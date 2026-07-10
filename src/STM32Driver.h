#include <Arduino.h>
#include <string.h>
#include <vector>

#include "DriverConfig.h"
#include "PinConfig.h"
#include "PortNames.h"
#include "STM32AudioLogger.h"
#include "STM32AudioSAI.h"

extern DMA_HandleTypeDef* hdma_sai_tx;
extern DMA_HandleTypeDef* hdma_sai_rx;
// -1 = neither half of the circular TX buffer is free yet; 0/1 = that half
// was just vacated by the half-complete/full-complete DMA callback (see
// STM32DriverF723.cpp) and is ready for write() to refill.
extern volatile int8_t saiTxFreeHalf;

/**
 * @brief STM32SAIDriver provides SAI and DMA initialization, configuration, and
 * data transfer for STM32 boards.
 *
 * This class abstracts the SAI and DMA hardware, allowing board-specific
 * configuration and providing methods for initialization, deinitialization,
 * GPIO setup, and DMA-based read/write operations.
 */
class STM32SAIDriver {
 public:
  STM32SAIDriver(const STM32SAIDriverConfig& cfg) : config(cfg) {
    if (config.sai_block_tx == config.sai_block_rx) {
      hdma_sai_tx = &dma_out;
      hdma_sai_rx = &dma_out;
      p_hsai_in = &hsai_a;
      p_hsai_out = &hsai_a;
    } else {
      hdma_sai_tx = &dma_out;
      hdma_sai_rx = &dma_in;
      p_hsai_out = &hsai_a;
      p_hsai_in = &hsai_b;
    }
  }

  /**
   * @brief Initialize the SAI peripheral for TX and RX with the given audio
   * configuration.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  bool initSAI(STM32AudioSAI* audio) {
    if (p_hsai_out != p_hsai_in) {
      // Only touch the block(s) the requested mode actually needs - matches
      // initDMA() below, which already gates on getMode(). Previously this
      // unconditionally initialized RX (Block B) as an independent SAI_MODEMASTER_RX
      // even for Output-only use, on boards (like the F723 Discovery) where
      // TX/RX are separate SAI blocks: that block's clock generator got
      // enabled and left running with no GPIO ever routed to its pins
      // (configureGPIO() only wires up the pins the *used* direction needs),
      // an unnecessary and potentially interfering half-configured block.
      bool rctx = true, rcrx = true;
      if (audio->getMode() == STM32AudioSAI::Output ||
          audio->getMode() == STM32AudioSAI::Duplex) {
        STM32AudioLogger::instance().debug("initSAI: Initializing TX");
        rctx = initSAICommon(p_hsai_out, config.sai_block_tx, audio, true);
      }
      if (audio->getMode() == STM32AudioSAI::Input ||
          audio->getMode() == STM32AudioSAI::Duplex) {
        STM32AudioLogger::instance().debug("initSAI: Initializing RX");
        rcrx = initSAICommon(p_hsai_in, config.sai_block_rx, audio, false);
      }
      return rctx && rcrx;
    } else {
      STM32AudioLogger::instance().debug(
          "initSAI: Initializing TX and RX together (same SAI block)");
      return initSAICommon(p_hsai_out, config.sai_block_tx, audio, true);
    }
  }

  /**
   * @brief Deinitialize the SAI peripheral.
   */
  void deinitSAI() {
    STM32AudioLogger::instance().debug("deinitSAI: Entered");
    // deinit tx
    if (HAL_SAI_DeInit(p_hsai_out) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_SAI_DeInit failed");
    }
    // deinit rx if different block
    if (p_hsai_in != p_hsai_out) {
      if (HAL_SAI_DeInit(p_hsai_in) != HAL_OK) {
        STM32AudioLogger::instance().error("HAL_SAI_DeInit failed");
      }
    }

    if (config.disableSAIClocks) {
      config.disableSAIClocks();
    }
  }

  bool initDMA(STM32AudioSAI* audio) {
    STM32AudioLogger::instance().debug("initDMA: Entered");
    bool success = true;

    if (config.enableDMAClocks) {
      config.enableDMAClocks();
    } else {
      STM32AudioLogger::instance().error(
          "No clock enable function provided in config");
    }

    switch (audio->getMode()) {
      case STM32AudioSAI::Output: {
        if (!initDMATx(audio)) {
          STM32AudioLogger::instance().error("DMA TX init failed");
          success = false;
        }
        break;
      }
      case STM32AudioSAI::Input: {
        if (!initDMARx(audio)) {
          STM32AudioLogger::instance().error("DMA RX init failed");
          success = false;
        }
        break;
      }
      case STM32AudioSAI::Duplex: {
        if (!initDMATx(audio)) {
          STM32AudioLogger::instance().error("DMA TX init failed");
          success = false;
        }
        if (!initDMARx(audio)) {
          STM32AudioLogger::instance().error("DMA RX init failed");
          success = false;
        }
        break;
      }
    }
    return success;
  }

  /**
   * @brief Deinitialize the DMA peripheral.
   */
  void deinitDMA() {
    STM32AudioLogger::instance().debug("deinitDMA: Entered");
    if (HAL_DMA_DeInit(hdma_sai_tx) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_DMA_DeInit failed");
    }
    // Only deinit RX if it's a different DMA instance than TX
    if (hdma_sai_rx != hdma_sai_tx) {
      if (HAL_DMA_DeInit(hdma_sai_rx) != HAL_OK) {
        STM32AudioLogger::instance().error("HAL_DMA_DeInit RX failed");
      }
    }
    // If the config provides a clock disable function, call it
    if (config.disableDMAClocks) {
      config.disableDMAClocks();
    }
  }

  /**
   * @brief Configure GPIO pins for SAI operation based on board config and
   * user overrides.
   * @param audio Pointer to STM32AudioSAI instance for pin configuration.
   * @return true if all pins were configured successfully, false otherwise.
   */
  bool configureGPIO(STM32AudioSAI* audio) {
    STM32AudioLogger::instance().debug("configureGPIO: Entered");
    static const char* i2sPinNames[static_cast<size_t>(PinId::NumPins)] = {
        "SCK", "FS", "SD", "SD_RX", "MCLK"};

    bool success = true;
    for (int i = 0; i < config.numPins; ++i) {
      // Defensive: check bounds for i2sPinNames
      const char* pinName =
          (i >= 0 && i < (int)(sizeof(i2sPinNames) / sizeof(i2sPinNames[0])))
              ? i2sPinNames[i]
              : "?";
      PinConfig cfg;
      // Defensive: getPinConfig may not initialize all fields
      cfg = audio->getPinConfig(static_cast<PinId>(i));
      if (cfg.pin == NC && config.defaultPins) cfg = config.defaultPins[i];
      if (cfg.af == -1 && config.defaultPins) cfg.af = config.defaultPins[i].af;
      if (cfg.pin != NC && STM_VALID_PINNAME(cfg.pin)) {
        STM32AudioLogger::instance().infof(
            "SAI Pin %s (%d): Port %c, Pin %d, AF %d", pinName, i,
            static_cast<char>('A' + STM_PORT(cfg.pin)), STM_PIN(cfg.pin),
            cfg.af);
      } else {
        STM32AudioLogger::instance().infof(
            "SAI Pin %s (%d): invalid pin, AF %d", pinName, i, cfg.af);
      }
      GPIO_InitTypeDef GPIO_InitStruct = {0};
      GPIO_TypeDef* gpio_port = nullptr;
      if (cfg.pin != NC && STM_VALID_PINNAME(cfg.pin)) {
        gpio_port = set_GPIO_Port_Clock(STM_PORT(cfg.pin));
      }
      if (gpio_port) {
        // These SAI pins are never touched via Arduino pinMode()/digitalWrite(),
        // which is normally what enables a port's GPIO clock on this core (see
        // set_GPIO_Port_Clock() in SrcWrapper's PortNames.c) - without this,
        // HAL_GPIO_Init below silently writes to a clock-gated peripheral and
        // the pin never actually switches to AF mode.
        GPIO_InitStruct.Pin = STM_GPIO_PIN(cfg.pin);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = cfg.af;
        HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);
      } else {
        STM32AudioLogger::instance().errorf(
            "Invalid GPIO port for SAI Pin %s (%d)", pinName, i);
        success = false;
      }
    }
    return success;
  }

  /**
   * @brief Perform a DMA-based SAI receive operation.
   * @param audio Pointer to STM32AudioSAI instance.
   * @param buffer Pointer to destination buffer.
   * @param size Number of bytes to receive.
   * @return Number of bytes received (size if successful, 0 on error).
   */
  size_t read(STM32AudioSAI* audio, void* buffer, size_t size) {
    STM32AudioLogger::instance().debugf("read: %d", (int)size);
    dmaRxTransferComplete = false;
    if (HAL_SAI_Receive_DMA(p_hsai_in, (uint8_t*)buffer,
                            size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_SAI_Receive_DMA failed");
      return 0;
    }
    // Wait for transfer to complete or timeout
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaRxTransferComplete && (millis() - start < timeout));
    return dmaRxTransferComplete ? size : 0;
  }

  /**
   * @brief Perform a DMA-based SAI transmit operation.
   * @param audio Pointer to STM32AudioSAI instance.
   * @param buffer Pointer to source buffer.
   * @param size Number of bytes to transmit.
   * @return Number of bytes transmitted (size if successful, 0 on error).
   */
  size_t write(STM32AudioSAI* audio, const void* buffer, size_t size) {
    STM32AudioLogger::instance().debugf("write: %d", (int)size);

    if (!txCircStarted) {
      // First chunk: build a 2x buffer (one half per ping-pong slot),
      // duplicate this first chunk into both halves (so the second half
      // has *something* sane queued before its own refill ever lands),
      // and kick off a single CIRCULAR transfer that then runs
      // indefinitely - later chunks just refill whichever half the
      // half/full-complete callbacks mark free, they never start a new
      // transfer.
      txCircHalfBytes = size;
      txCircBuf.assign(size * 2, 0);
      memcpy(txCircBuf.data(), buffer, size);
      memcpy(txCircBuf.data() + size, buffer, size);
      saiTxFreeHalf = -1;
      uint32_t nwords = (size * 2) / (audio->getBitsPerSample() / 8);
      dmaTxTransferComplete = false;
      HAL_StatusTypeDef hal_status =
          HAL_SAI_Transmit_DMA(p_hsai_out, txCircBuf.data(), nwords);
      if (hal_status != HAL_OK) {
        STM32AudioLogger::instance().errorf(
            "HAL_SAI_Transmit_DMA (circular start) failed: status=%d",
            (int)hal_status);
        return 0;
      }
      txCircStarted = true;
      return size;
    }

    if (size != txCircHalfBytes) {
      STM32AudioLogger::instance().errorf(
          "Circular TX chunk size changed (%u -> %u) - not supported once "
          "started",
          (unsigned)txCircHalfBytes, (unsigned)size);
      return 0;
    }

    // Wait for either half to become free (set from
    // HAL_SAI_TxHalfCpltCallback/HAL_SAI_TxCpltCallback - see
    // STM32DriverF723.cpp).
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (saiTxFreeHalf < 0 && (millis() - start < timeout));
    if (saiTxFreeHalf < 0) {
      STM32AudioLogger::instance().errorf(
          "Circular TX refill timed out after %lums: hsai state=%d, "
          "HAL error=0x%lX, last SAI error callback=0x%lX",
          (unsigned long)timeout, (int)p_hsai_out->State,
          (unsigned long)p_hsai_out->ErrorCode,
          (unsigned long)saiLastErrorCode);
      return 0;
    }
    memcpy(txCircBuf.data() + ((size_t)saiTxFreeHalf * txCircHalfBytes),
           buffer, size);
    saiTxFreeHalf = -1;
    return size;
  }

  /**
   * @brief Check if the SAI peripheral is currently enabled and running.
   * @return true if SAI is enabled, false otherwise.
   */
  bool isRunning() {
    STM32AudioLogger::instance().debug("isRunning: Entered");
    if (!p_hsai_out->Instance) return false;
    return true;
    // return (((SAI_TypeDef*)p_hsai_out->Instance)->CR1 & SAI_xCR1_SAIEN) != 0;
  }

  // Map a sample rate in Hz to the corresponding SAI_AUDIO_FREQUENCY_xxx
  // value
  /**
   * @brief Map a sample rate in Hz to the corresponding
   * SAI_AUDIO_FREQUENCY_xxx value.
   * @param rate Sample rate in Hz.
   * @return SAI_AUDIO_FREQUENCY_xxx value.
   */
  static uint32_t mapSampleRate(uint32_t rate) {
    switch (rate) {
      case 8000:
        return SAI_AUDIO_FREQUENCY_8K;
      case 11025:
        return SAI_AUDIO_FREQUENCY_11K;
      case 11000:
        return SAI_AUDIO_FREQUENCY_11K;
      case 16000:
        return SAI_AUDIO_FREQUENCY_16K;
      case 22050:
        return SAI_AUDIO_FREQUENCY_22K;
      case 22000:
        return SAI_AUDIO_FREQUENCY_22K;
      case 32000:
        return SAI_AUDIO_FREQUENCY_32K;
      case 44100:
        return SAI_AUDIO_FREQUENCY_44K;
      case 44000:
        return SAI_AUDIO_FREQUENCY_44K;
      case 48000:
        return SAI_AUDIO_FREQUENCY_48K;
      case 96000:
        return SAI_AUDIO_FREQUENCY_96K;
      case 192000:
        return SAI_AUDIO_FREQUENCY_192K;
      default:
        STM32AudioLogger::instance().errorf(
            "Unsupported sample rate: %u, using 44.1kHz fallback", rate);
        return SAI_AUDIO_FREQUENCY_44K;  // fallback
    }
  }

  // Map bits per sample to the corresponding SAI_DATASIZE_xx value
  /**
   * @brief Map bits per sample to the corresponding SAI_DATASIZE_xx value.
   * @param bits Number of bits per audio sample.
   * @return SAI_DATASIZE_xx value.
   */
  static uint32_t mapDataSize(uint8_t bits) {
    switch (bits) {
      case 8:
        return SAI_DATASIZE_8;
      case 10:
        return SAI_DATASIZE_10;
      case 16:
        return SAI_DATASIZE_16;
      case 20:
        return SAI_DATASIZE_20;
      case 24:
        return SAI_DATASIZE_24;
      case 32:
        return SAI_DATASIZE_32;
      default:
        STM32AudioLogger::instance().errorf(
            "Unsupported data size: %u, using 16-bit fallback", bits);
        return SAI_DATASIZE_16;  // fallback
    }
  }

 protected:
  // --- Circular double-buffered TX state (see write() above) ---
  std::vector<uint8_t> txCircBuf;
  size_t txCircHalfBytes = 0;
  bool txCircStarted = false;

  DMA_HandleTypeDef dma_out;
  DMA_HandleTypeDef dma_in;
  SAI_HandleTypeDef hsai_a = {};
  SAI_HandleTypeDef hsai_b = {};
  SAI_HandleTypeDef* p_hsai_in = nullptr;   // Use external global DMA handles
                                            // defined in STM32DriverWB55.cpp
  SAI_HandleTypeDef* p_hsai_out = nullptr;  // Use external global DMA handles
                                            // defined in STM32DriverWB55.cpp

  STM32SAIDriverConfig config;

  /**
   * @brief Initialize the DMA peripheral for SAI transfers.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  // Initialize DMA for TX (write)
  bool initDMATx(STM32AudioSAI* audio) {
    STM32AudioLogger::instance().debug("initDMATx: Entered");
    if (!config.dma_tx_instance) {
      STM32AudioLogger::instance().error("DMA TX instance not configured");
      return false;
    }
    hdma_sai_tx->Instance =
        (decltype(hdma_sai_tx->Instance))config.dma_tx_instance;
    // DMAMUX-equipped chips (H7/WB/L4+/G4) select the DMA request via
    // DMA_InitTypeDef::Request; classic stream/channel DMA (F1-F4, F7 - no
    // DMAMUX1 macro) uses ::Channel instead - the two fields don't coexist.
#if defined(DMAMUX1) || defined(DMAMUX)
    hdma_sai_tx->Init.Request = config.dma_tx_request;
#else
    hdma_sai_tx->Init.Channel = config.dma_tx_request;
#endif
    hdma_sai_tx->Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_sai_tx->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai_tx->Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai_tx->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai_tx->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    // CIRCULAR, double-buffered via the half/full-transfer-complete
    // callbacks (see writeCircularTx() below) - matches the ST BSP's own
    // SAIx_Out_Init/BSP_AUDIO_OUT_MspInit exactly (FIFOMode/threshold/burst
    // included). The previous NORMAL-mode, one-shot-per-chunk approach left
    // an audible gap at every chunk boundary (SAI DMA request disabled
    // between the outgoing chunk finishing and the next HAL_SAI_Transmit_DMA
    // call being issued from software) and turned out to be a genuine cause
    // of not just gaps but total silence when paired with this codec/board -
    // CIRCULAR keeps the DMA request continuously active across chunk
    // boundaries, since the "chunk" being refilled is just one half of an
    // already-running transfer.
    hdma_sai_tx->Init.Mode = DMA_CIRCULAR;
    hdma_sai_tx->Init.Priority = DMA_PRIORITY_HIGH;
#ifdef DMA_FIFOMODE_ENABLE
    hdma_sai_tx->Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_sai_tx->Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_sai_tx->Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_sai_tx->Init.PeriphBurst = DMA_PBURST_SINGLE;
#endif
    __HAL_LINKDMA(p_hsai_out, hdmatx, *hdma_sai_tx);
    HAL_DMA_DeInit(hdma_sai_tx);

    if (HAL_DMA_Init(hdma_sai_tx) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_DMA_Init (TX) failed");
      return false;
    }
    // Explicitly link the SAI handle to the DMA handle before starting DMA
    p_hsai_out->hdmatx = hdma_sai_tx;
    // Priority 0 is the highest possible on this MCU - non-preemptible by
    // anything, including SysTick (millis()). If this ISR ever fires faster
    // than expected (e.g. retriggers immediately), it can starve SysTick
    // completely, freezing millis() and everything that depends on it -
    // including write()'s own timeout loop below. A moderate priority still
    // services audio promptly but leaves SysTick able to run.
    HAL_NVIC_SetPriority(config.dma_tx_irq, 5, 0);
    HAL_NVIC_EnableIRQ(config.dma_tx_irq);
    return true;
  }

  // Initialize DMA for RX (read)
  bool initDMARx(STM32AudioSAI* audio) {
    STM32AudioLogger::instance().debug("initDMARx: Entered");
    if (!config.dma_rx_instance) {
      STM32AudioLogger::instance().error("DMA RX instance not configured");
      return false;
    }
    hdma_sai_rx->Instance =
        (decltype(hdma_sai_rx->Instance))config.dma_rx_instance;
#if defined(DMAMUX1) || defined(DMAMUX)
    hdma_sai_rx->Init.Request = config.dma_rx_request;
#else
    hdma_sai_rx->Init.Channel = config.dma_rx_request;
#endif
    hdma_sai_rx->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_sai_rx->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai_rx->Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai_rx->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai_rx->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    // See the matching note in initDMATx: NORMAL, not CIRCULAR, to match
    // read()'s fresh-call-per-chunk usage.
    hdma_sai_rx->Init.Mode = DMA_NORMAL;
    hdma_sai_rx->Init.Priority = DMA_PRIORITY_HIGH;
#ifdef DMA_FIFOMODE_DISABLE
    hdma_sai_rx->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
    __HAL_LINKDMA(p_hsai_in, hdmarx, *hdma_sai_rx);
    HAL_DMA_DeInit(hdma_sai_rx);

    if (HAL_DMA_Init(hdma_sai_rx) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_DMA_Init (RX) failed");
      return false;
    }
    p_hsai_out->hdmarx = hdma_sai_rx;
    HAL_NVIC_SetPriority(config.dma_rx_irq, 5, 0);
    HAL_NVIC_EnableIRQ(config.dma_rx_irq);

    return true;
  }

  /**
   * @brief Common SAI initialization logic for TX and RX.
   * @param hsai Pointer to SAI handle (TX or RX)
   * @param block SAI block instance
   * @param audio Pointer to STM32AudioSAI instance
   * @param isTx true for TX, false for RX
   * @return true if initialization succeeded, false otherwise.
   */
  bool initSAICommon(SAI_HandleTypeDef* hsai, SAI_Block_TypeDef* block,
                     STM32AudioSAI* audio, bool isTx) {
    STM32AudioLogger::instance().debug(isTx ? "initSAITx: Entered"
                                            : "initSAIRx: Entered");
    if (config.enableSAIClocks) {
      config.enableSAIClocks(audio->getSampleRate());
    } else {
      STM32AudioLogger::instance().error(
          "No clock enable function provided in config");
    }
    hsai->Instance = block;
    hsai->Init.AudioMode = audio->isMaster()
                               ? (isTx ? SAI_MODEMASTER_TX : SAI_MODEMASTER_RX)
                               : SAI_MODESLAVE_RX;
    hsai->Init.Synchro = SAI_ASYNCHRONOUS;
    // Matches the ST BSP's SAIx_Out_Init exactly (stm32f723e_discovery_audio.c) -
    // DISABLE means outputs aren't driven until the first FS edge after
    // enable, avoiding a glitch; ENABLE was an unverified guess that never
    // matched the confirmed-working reference.
    hsai->Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
    hsai->Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai->Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai->Init.AudioFrequency = mapSampleRate(audio->getSampleRate());
    // Init.Protocol (SAI_xCR1.PRTCFG) only ever selects between FREE/AC97/
    // SPDIF hardware-framing modes - it is a different field from the
    // I2S/PCM/TDM distinction below, which we always implement ourselves via
    // manual FrameInit/SlotInit (matching how ST's own HAL_SAI_InitProtocol()
    // helper does it internally, see SAI_InitPCM() etc. in the HAL source).
    // Previously non-I2S protocols wrote SAI_PCM_LONG (value 3) into this
    // field, which is not one of PRTCFG's three valid values and left the
    // register in a reserved state.
    hsai->Init.Protocol = SAI_FREE_PROTOCOL;
    hsai->Init.DataSize = mapDataSize(audio->getBitsPerSample());
    hsai->Init.FirstBit = SAI_FIRSTBIT_MSB;
    hsai->Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    // slotCount/activeSlots default to channels/"first N slots" (see
    // STM32AudioSAI::getSlotCount()/getActiveSlots()), matching every
    // existing board unchanged - only boards/protocols that explicitly call
    // setSlotCount()/setActiveSlots() (TDM-wired codecs, e.g. the F723
    // Discovery's WM8994) get a frame layout different from plain
    // 1-slot-per-channel.
    uint8_t slotCount = audio->getSlotCount();
    hsai->FrameInit.FrameLength = audio->getBitsPerSample() * slotCount;
    STM32AudioSAI::Protocol proto = audio->getProtocol();
    if (proto == STM32AudioSAI::TDM || proto == STM32AudioSAI::PCM) {
      // PCM/TDM framing: single-pulse FS (SAI_FS_STARTFRAME) per frame,
      // instead of one edge per channel like I2S below - register values
      // match ST's own HAL_SAI_InitProtocol()/SAI_InitPCM() exactly.
      // TDM uses the "short frame" 1-bit-clock pulse (SAI_PCM_SHORT), the
      // wire format every generic multi-channel TDM codec/ADC/DAC expects.
      // PCM uses the "long frame" pulse (SAI_PCM_LONG): a fixed 13-bit-clock
      // width regardless of slot count/size, per the classic PCM highway
      // protocol - not scaled to FrameLength, so it only fits when
      // FrameLength > 13 (e.g. 16-bit-or-wider slots); narrower/fewer slots
      // will produce an FS pulse as wide as (or wider than) the frame
      // itself, which is invalid - use TDM instead for narrow-slot framing.
      hsai->FrameInit.FSDefinition = SAI_FS_STARTFRAME;
      hsai->FrameInit.FSPolarity = SAI_FS_ACTIVE_HIGH;
      if (proto == STM32AudioSAI::TDM) {
        hsai->FrameInit.ActiveFrameLength = 1;
      } else {
        hsai->FrameInit.ActiveFrameLength = 13;
        if (13 >= hsai->FrameInit.FrameLength) {
          STM32AudioLogger::instance().errorf(
              "PCM long-frame FS pulse (13 bit-clocks) doesn't fit in a "
              "%lu-bit-clock frame (bitsPerSample=%u, slotCount=%u) - use "
              "TDM protocol instead for narrow/few slots",
              (unsigned long)hsai->FrameInit.FrameLength,
              audio->getBitsPerSample(), slotCount);
        }
      }
    } else {
      // ActiveFrameLength marks the FS/WS active-low duration, which for
      // I2S-style timing must be half the total frame (left channel bits),
      // not a single slot's width - only ever matched by coincidence when
      // slotCount==2 (bitsPerSample == FrameLength/2 in that case), which is
      // why this was previously hardcoded to bitsPerSample without issue on
      // 2-slot boards. Matches the ST BSP's SAIx_Out_Init exactly (e.g.
      // FrameLength=64/ActiveFrameLength=32 for the F723 Discovery's 4-slot
      // TDM frame). Confirmed via A/B testing that this value has no effect
      // on the separate WM8994 register-retention issue - that one turned out
      // to be caused by init() failures being silently swallowed, see
      // AudioDriverWM8994Class::begin() in arduino-audio-driver.
      hsai->FrameInit.ActiveFrameLength = hsai->FrameInit.FrameLength / 2;
      hsai->FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
      hsai->FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    }
    hsai->FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
    hsai->SlotInit.FirstBitOffset = 0;
    hsai->SlotInit.SlotSize =
        (audio->getBitsPerSample() == 16) ? SAI_SLOTSIZE_16B : SAI_SLOTSIZE_32B;
    hsai->SlotInit.SlotNumber = slotCount;
    hsai->SlotInit.SlotActive = audio->getActiveSlots();
    if (HAL_SAI_Init(hsai) != HAL_OK) {
      STM32AudioLogger::instance().error(isTx ? "HAL_SAI_Init (TX) failed"
                                              : "HAL_SAI_Init (RX) failed");
      return false;
    }
    // Matches the ST BSP's SAIx_Out_Init exactly (stm32f723e_discovery_audio.c),
    // which enables the block immediately after HAL_SAI_Init instead of
    // waiting for the first DMA transfer. Without this, SAIEN stays clear
    // (no MCLK/SCK/FS output at all) until HAL_SAI_Transmit_DMA's own
    // "enable if not already enabled" logic fires on the *first* data write -
    // which on this board happens well after begin() returns, i.e. after any
    // codec bring-up that runs between i2s.begin() and the first write().
    // The WM8994 derives its internal SYSCLK from this MCLK pin, so its
    // write-sequencer-driven analog bring-up (headphone output, DC servo,
    // charge pump, output mixer) has no clock to run on during that entire
    // window even though every plain register write still ACKs normally -
    // this was the actual root cause of the "clean I2C/DMA, no sound"
    // symptom, not the codec driver code itself.
    __HAL_SAI_ENABLE(hsai);
    return true;
  }
};
