#include <Arduino.h>

#include "DriverConfig.h"
#include "PinConfig.h"
#include "PortNames.h"
#include "STM32AudioLogger.h"
#include "STM32AudioSAI.h"

extern DMA_HandleTypeDef* hdma_sai_tx;
extern DMA_HandleTypeDef* hdma_sai_rx;

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
      STM32AudioLogger::instance().debug(
          "initSAI: Initializing TX and RX separately");
      bool rctx = initSAICommon(p_hsai_out, config.sai_block_tx, audio, true);
      bool rcrx = initSAICommon(p_hsai_in, config.sai_block_rx, audio, false);
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
        "SCK", "FS", "SD", "MCLK"};

    bool success = true;
    for (int i = 0; i < config.numPins; ++i) {
      // Defensive: check bounds for i2sPinNames
      const char* pinName =
          (i >= 0 && i < (int)(sizeof(i2sPinNames) / sizeof(i2sPinNames[0])))
              ? i2sPinNames[i]
              : "?";
      PinConfig cfg = {-1, -1, -1};
      // Defensive: getPinConfig may not initialize all fields
      cfg = audio->getPinConfig(static_cast<PinId>(i));
      if (cfg.port == -1 && config.defaultPins)
        cfg.port = config.defaultPins[i].port;
      if (cfg.pin == -1 && config.defaultPins)
        cfg.pin = config.defaultPins[i].pin;
      if (cfg.af == -1 && config.defaultPins) cfg.af = config.defaultPins[i].af;
      // Print port as character if in valid range, else as integer
      if (cfg.port >= 'A' && cfg.port <= 'Z') {
        STM32AudioLogger::instance().infof(
            "SAI Pin %s (%d): Port %c, Pin %d, AF %d", pinName, i, cfg.port,
            cfg.pin, cfg.af);
      } else {
        STM32AudioLogger::instance().infof(
            "SAI Pin %s (%d): Port %d (invalid), Pin %d, AF %d", pinName, i,
            cfg.port, cfg.pin, cfg.af);
      }
      GPIO_InitTypeDef GPIO_InitStruct = {0};
      GPIO_TypeDef* gpio_port = nullptr;
      if (cfg.port >= 'A' && cfg.port <= 'Z') {
        gpio_port = (GPIO_TypeDef*)(GPIOA_BASE + 0x400U * (cfg.port - 'A'));
      }
      if (gpio_port) {
        // These SAI pins are never touched via Arduino pinMode()/digitalWrite(),
        // which is normally what enables a port's GPIO clock on this core (see
        // set_GPIO_Port_Clock() in SrcWrapper's PortNames.c) - without this,
        // HAL_GPIO_Init below silently writes to a clock-gated peripheral and
        // the pin never actually switches to AF mode.
        set_GPIO_Port_Clock(cfg.port - 'A');
        GPIO_InitStruct.Pin = 1U << cfg.pin;
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
    if (!dmaTxTransferComplete) {
      STM32AudioLogger::instance().warn(
          "HAL_SAI_Transmit_DMA called while previous transfer still in "
          "progress");
      return 0;
    }
    dmaTxTransferComplete = false;
    uint32_t nwords = size / (audio->getBitsPerSample() / 8);
    HAL_StatusTypeDef hal_status =
        HAL_SAI_Transmit_DMA(p_hsai_out, (uint8_t*)buffer, nwords);
    if (hal_status != HAL_OK) {
      STM32AudioLogger::instance().errorf(
          "HAL_SAI_Transmit_DMA failed: status=%d, buffer=%p, size=%u, "
          "nwords=%lu",
          (int)hal_status, buffer, (unsigned)size, (unsigned long)nwords);
      return 0;
    }
    /// Wait for transfer to complete or timeout
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaTxTransferComplete && (millis() - start < timeout));
    return dmaTxTransferComplete ? size : 0;
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
    hdma_sai_tx->Init.Mode = DMA_CIRCULAR;
    hdma_sai_tx->Init.Priority = DMA_PRIORITY_HIGH;
#ifdef DMA_FIFOMODE_DISABLE
    hdma_sai_tx->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
    __HAL_LINKDMA(p_hsai_out, hdmatx, *hdma_sai_tx);
    HAL_DMA_DeInit(hdma_sai_tx);

    if (HAL_DMA_Init(hdma_sai_tx) != HAL_OK) {
      STM32AudioLogger::instance().error("HAL_DMA_Init (TX) failed");
      return false;
    }
    // Explicitly link the SAI handle to the DMA handle before starting DMA
    p_hsai_out->hdmatx = hdma_sai_tx;
    HAL_NVIC_SetPriority(config.dma_tx_irq, 0, 0);
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
    hdma_sai_rx->Init.Mode = DMA_CIRCULAR;
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
    HAL_NVIC_SetPriority(config.dma_rx_irq, 0, 0);
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
    hsai->Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai->Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai->Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai->Init.AudioFrequency = mapSampleRate(audio->getSampleRate());
    hsai->Init.Protocol = (audio->getProtocol() == STM32AudioSAI::I2S)
                              ? SAI_FREE_PROTOCOL
                              : SAI_PCM_LONG;
    hsai->Init.DataSize = mapDataSize(audio->getBitsPerSample());
    hsai->Init.FirstBit = SAI_FIRSTBIT_MSB;
    hsai->Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    hsai->FrameInit.FrameLength =
        audio->getBitsPerSample() * audio->getChannels();
    hsai->FrameInit.ActiveFrameLength = audio->getBitsPerSample();
    hsai->FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai->FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai->FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;
    hsai->SlotInit.FirstBitOffset = 0;
    hsai->SlotInit.SlotSize =
        (audio->getBitsPerSample() == 16) ? SAI_SLOTSIZE_16B : SAI_SLOTSIZE_32B;
    hsai->SlotInit.SlotNumber = audio->getChannels();
    hsai->SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;
    if (HAL_SAI_Init(hsai) != HAL_OK) {
      STM32AudioLogger::instance().error(isTx ? "HAL_SAI_Init (TX) failed"
                                              : "HAL_SAI_Init (RX) failed");
      return false;
    }
    return true;
  }
};
