#include <Arduino.h>

#include "DriverConfig.h"
#include "Logger.h"
#include "PinConfig.h"
#include "STM32AudioSAI.h"

extern DMA_HandleTypeDef hdma_sai;

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
  SAI_HandleTypeDef hsai_a = {};
  // Use external global DMA handle defined in STM32DriverWB55.cpp

  STM32SAIDriverConfig config;

  STM32SAIDriver(const STM32SAIDriverConfig& cfg) : config(cfg) {}

  /**
   * @brief Initialize the SAI peripheral with the given audio configuration.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  bool initSAI(STM32AudioSAI* audio) {
    Logger::instance().debug("initSAI: Entered");
    __HAL_RCC_SAI1_CLK_ENABLE();

    hsai_a.Instance = SAI1_Block_A;
    hsai_a.Init.AudioMode =
        audio->isMaster() ? SAI_MODEMASTER_TX : SAI_MODESLAVE_RX;
    hsai_a.Init.Synchro = SAI_ASYNCHRONOUS;
    hsai_a.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai_a.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai_a.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai_a.Init.AudioFrequency = mapSampleRate(audio->getSampleRate());
    hsai_a.Init.Protocol = (audio->getProtocol() == STM32AudioSAI::I2S)
                               ? SAI_FREE_PROTOCOL
                               : SAI_PCM_LONG;
    hsai_a.Init.DataSize = mapDataSize(audio->getBitsPerSample());
    hsai_a.Init.FirstBit = SAI_FIRSTBIT_MSB;
    hsai_a.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;

    hsai_a.FrameInit.FrameLength =
        audio->getBitsPerSample() * audio->getChannels();
    hsai_a.FrameInit.ActiveFrameLength = audio->getBitsPerSample();
    hsai_a.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai_a.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai_a.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

    hsai_a.SlotInit.FirstBitOffset = 0;
    hsai_a.SlotInit.SlotSize =
        (audio->getBitsPerSample() == 16) ? SAI_SLOTSIZE_16B : SAI_SLOTSIZE_32B;
    hsai_a.SlotInit.SlotNumber = audio->getChannels();
    hsai_a.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

    if (HAL_SAI_Init(&hsai_a) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Init failed");
      return false;
    }
    return true;
  }

  /**
   * @brief Deinitialize the SAI peripheral.
   */
  void deinitSAI() {
    Logger::instance().debug("deinitSAI: Entered");
    if (HAL_SAI_DeInit(&hsai_a) != HAL_OK) {
      Logger::instance().error("HAL_SAI_DeInit failed");
    }
    __HAL_RCC_SAI1_CLK_DISABLE();
  }

  bool initDMA(STM32AudioSAI* audio) {
    Logger::instance().debug("initDMA: Entered");
    bool success = true;
    if (config.enableClocks) {
      config.enableClocks();
    } else {
      Logger::instance().error("No clock enable function provided in config");
    }

    switch (audio->getMode()) {
      case STM32AudioSAI::Output: {
        if (!initDMATx(audio)) {
          Logger::instance().error("DMA TX init failed");
          success = false;
        }
        break;
      }
      case STM32AudioSAI::Input: {
        if (!initDMARx(audio)) {
          Logger::instance().error("DMA RX init failed");
          success = false;
        }
        break;
      }
      case STM32AudioSAI::Duplex: {
        if (!initDMATx(audio)) {
          Logger::instance().error("DMA TX init failed");
          success = false;
        }
        if (!initDMARx(audio)) {
          Logger::instance().error("DMA RX init failed");
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
    Logger::instance().debug("deinitDMA: Entered");
    if (HAL_DMA_DeInit(&hdma_sai) != HAL_OK) {
      Logger::instance().error("HAL_DMA_DeInit failed");
    }
    if (config.disableClocks) {
      config.disableClocks();
    } else {
      Logger::instance().error("No clock disable function provided in config");
    }
  }

  /**
   * @brief Configure GPIO pins for SAI operation based on board config and
   * user overrides.
   * @param audio Pointer to STM32AudioSAI instance for pin configuration.
   * @return true if all pins were configured successfully, false otherwise.
   */
  bool configureGPIO(STM32AudioSAI* audio) {
    Logger::instance().debug("configureGPIO: Entered");
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
        Logger::instance().infof("SAI Pin %s (%d): Port %c, Pin %d, AF %d",
                                 pinName, i, cfg.port, cfg.pin, cfg.af);
      } else {
        Logger::instance().infof(
            "SAI Pin %s (%d): Port %d (invalid), Pin %d, AF %d", pinName, i,
            cfg.port, cfg.pin, cfg.af);
      }
      GPIO_InitTypeDef GPIO_InitStruct = {0};
      GPIO_TypeDef* gpio_port = nullptr;
      if (cfg.port >= 'A' && cfg.port <= 'Z') {
        gpio_port = (GPIO_TypeDef*)(GPIOA_BASE + 0x400U * (cfg.port - 'A'));
      }
      if (gpio_port) {
        GPIO_InitStruct.Pin = 1U << cfg.pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = cfg.af;
        HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);
      } else {
        Logger::instance().errorf("Invalid GPIO port for SAI Pin %s (%d)",
                                  pinName, i);
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
    Logger::instance().debugf("read: %d", (int)size);
    dmaRxTransferComplete = false;
    if (HAL_SAI_Receive_DMA(&hsai_a, (uint8_t*)buffer,
                            size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Receive_DMA failed");
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
    Logger::instance().debugf("write: %d", (int)size);
    if (!dmaTxTransferComplete) {
      Logger::instance().warn(
          "HAL_SAI_Transmit_DMA called while previous transfer still in "
          "progress");
      return 0;
    }
    dmaTxTransferComplete = false;
    uint32_t nwords = size / (audio->getBitsPerSample() / 8);
    HAL_StatusTypeDef hal_status =
        HAL_SAI_Transmit_DMA(&hsai_a, (uint8_t*)buffer, nwords);
    if (hal_status != HAL_OK) {
      Logger::instance().errorf(
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
    Logger::instance().debug("isRunning: Entered");
    if (!hsai_a.Instance) return false;
    return true;
    // return (((SAI_TypeDef*)hsai_a.Instance)->CR1 & SAI_xCR1_SAIEN) != 0;
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
        Logger::instance().errorf(
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
        Logger::instance().errorf(
            "Unsupported data size: %u, using 16-bit fallback", bits);
        return SAI_DATASIZE_16;  // fallback
    }
  }

 protected:
  /**
   * @brief Initialize the DMA peripheral for SAI transfers.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  // Initialize DMA for TX (write)
  bool initDMATx(STM32AudioSAI* audio) {
    Logger::instance().debug("initDMATx: Entered");
    if (!config.dma_tx_instance) {
      Logger::instance().error("DMA TX instance not configured");
      return false;
    }

    hdma_sai.Instance = (decltype(hdma_sai.Instance))config.dma_tx_instance;
    hdma_sai.Init.Request = config.dma_tx_request;
    hdma_sai.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_sai.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_sai.Init.Mode = DMA_CIRCULAR;
    hdma_sai.Init.Priority = DMA_PRIORITY_HIGH;
#ifdef DMA_FIFOMODE_DISABLE
    hdma_sai.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
    __HAL_LINKDMA(&hsai_a, hdmatx, hdma_sai);
    HAL_DMA_DeInit(&hdma_sai);

    if (HAL_DMA_Init(&hdma_sai) != HAL_OK) {
      Logger::instance().error("HAL_DMA_Init (TX) failed");
      return false;
    }
    // Explicitly link the SAI handle to the DMA handle before starting DMA
    hsai_a.hdmatx = &hdma_sai;
    HAL_NVIC_SetPriority(config.dma_tx_irq, 0, 0);
    HAL_NVIC_EnableIRQ(config.dma_tx_irq);
    return true;
  }

  // Initialize DMA for RX (read)
  bool initDMARx(STM32AudioSAI* audio) {
    Logger::instance().debug("initDMARx: Entered");
    if (!config.dma_rx_instance) {
      Logger::instance().error("DMA RX instance not configured");
      return false;
    }
    hdma_sai.Instance = (decltype(hdma_sai.Instance))config.dma_rx_instance;
    hdma_sai.Init.Request = config.dma_rx_request;
    hdma_sai.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_sai.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_sai.Init.Mode = DMA_CIRCULAR;
    hdma_sai.Init.Priority = DMA_PRIORITY_HIGH;
#ifdef DMA_FIFOMODE_DISABLE
    hdma_sai.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
    __HAL_LINKDMA(&hsai_a, hdmarx, hdma_sai);
    HAL_DMA_DeInit(&hdma_sai);

    if (HAL_DMA_Init(&hdma_sai) != HAL_OK) {
      Logger::instance().error("HAL_DMA_Init (RX) failed");
      return false;
    }
    hsai_a.hdmarx = &hdma_sai;
    HAL_NVIC_SetPriority(config.dma_rx_irq, 0, 0);
    HAL_NVIC_EnableIRQ(config.dma_rx_irq);

    return true;
  }
};
