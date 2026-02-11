#include <Arduino.h>
#include "Logger.h"
#include "STM32AudioSAI.h"

/**
 * @brief Configuration for STM32SAIDriver.
 * Holds DMA instance, DMA request, and default pin configuration for a board.
 */
struct STM32SAIDriverConfig {
  void* dma_instance;
  uint32_t dma_request;
  const STM32AudioSAI::PinConfig* defaultPins;
  int numPins;
};

/**
 * @brief STM32SAIDriver provides SAI and DMA initialization, configuration, and data transfer for STM32 boards.
 *
 * This class abstracts the SAI and DMA hardware, allowing board-specific configuration and providing
 * methods for initialization, deinitialization, GPIO setup, and DMA-based read/write operations.
 */
class STM32SAIDriver {
public:
  SAI_HandleTypeDef hsai_a = {};
  DMA_HandleTypeDef hdma_sai_a = {};
  STM32SAIDriverConfig config;

  STM32SAIDriver(const STM32SAIDriverConfig& cfg) : config(cfg) {}

  /**
   * @brief Initialize the SAI peripheral with the given audio configuration.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  bool initSAI(STM32AudioSAI* audio) {
    __HAL_RCC_SAI1_CLK_ENABLE();
    hsai_a.Instance = SAI1_Block_A;
    hsai_a.Init.AudioMode = audio->isMaster() ? SAI_MODEMASTER_TX : SAI_MODESLAVE_RX;
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

    hsai_a.FrameInit.FrameLength = audio->getBitsPerSample() * audio->getChannels();
    hsai_a.FrameInit.ActiveFrameLength = audio->getBitsPerSample();
    hsai_a.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
    hsai_a.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
    hsai_a.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

    hsai_a.SlotInit.FirstBitOffset = 0;
    hsai_a.SlotInit.SlotSize = (audio->getBitsPerSample() == 16) ? SAI_SLOTSIZE_16B : SAI_SLOTSIZE_32B;
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
    if (HAL_SAI_DeInit(&hsai_a) != HAL_OK) {
      Logger::instance().error("HAL_SAI_DeInit failed");
    }
    __HAL_RCC_SAI1_CLK_DISABLE();
  }

  /**
   * @brief Initialize the DMA peripheral for SAI transfers.
   * @param audio Pointer to STM32AudioSAI instance for configuration.
   * @return true if initialization succeeded, false otherwise.
   */
  bool initDMA(STM32AudioSAI* audio) {
    hdma_sai_a.Instance = (decltype(hdma_sai_a.Instance))config.dma_instance;
    hdma_sai_a.Init.Request = config.dma_request;
    hdma_sai_a.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_sai_a.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai_a.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai_a.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai_a.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_sai_a.Init.Mode = DMA_CIRCULAR;
    hdma_sai_a.Init.Priority = DMA_PRIORITY_HIGH;
    #ifdef DMA_FIFOMODE_DISABLE
    hdma_sai_a.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    #endif
    if (HAL_DMA_Init(&hdma_sai_a) != HAL_OK) {
      Logger::instance().error("HAL_DMA_Init failed");
      return false;
    }
    __HAL_LINKDMA(&hsai_a, hdmatx, hdma_sai_a);
    return true;
  }

  /**
   * @brief Deinitialize the DMA peripheral.
   */
  void deinitDMA() {
    if (HAL_DMA_DeInit(&hdma_sai_a) != HAL_OK) {
      Logger::instance().error("HAL_DMA_DeInit failed");
    }
  }

  /**
   * @brief Configure GPIO pins for SAI operation based on board config and user overrides.
   * @param audio Pointer to STM32AudioSAI instance for pin configuration.
   * @return true if all pins were configured successfully, false otherwise.
   */
  bool configureGPIO(STM32AudioSAI* audio) {
    bool success = true;
    for (int i = 0; i < config.numPins; ++i) {
      STM32AudioSAI::PinConfig cfg = audio->getPinConfig((STM32AudioSAI::PinId)i);
      if (cfg.port == -1) cfg.port = config.defaultPins[i].port;
      if (cfg.pin == -1) cfg.pin = config.defaultPins[i].pin;
      if (cfg.af == -1) cfg.af = config.defaultPins[i].af;
      Logger::instance().infof("SAI Pin %d: Port %c, Pin %d, AF %d", i, cfg.port, cfg.pin, cfg.af);
      // ... GPIO enabling and HAL_GPIO_Init logic ...
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
    dmaTxTransferComplete = false;
    if (HAL_SAI_Transmit_DMA(&hsai_a, (uint8_t*)buffer,
                             size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Transmit_DMA failed");
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
    return (SAI1_Block_A->CR1 & SAI_xCR1_SAIEN) != 0;
  }

  // Map a sample rate in Hz to the corresponding SAI_AUDIO_FREQUENCY_xxx value
  /**
   * @brief Map a sample rate in Hz to the corresponding SAI_AUDIO_FREQUENCY_xxx value.
   * @param rate Sample rate in Hz.
   * @return SAI_AUDIO_FREQUENCY_xxx value.
   */
  static uint32_t mapSampleRate(uint32_t rate) {
    switch (rate) {
      case 8000:   return SAI_AUDIO_FREQUENCY_8K;
      case 11025:  return SAI_AUDIO_FREQUENCY_11K;
      case 11000:  return SAI_AUDIO_FREQUENCY_11K;
      case 16000:  return SAI_AUDIO_FREQUENCY_16K;
      case 22050:  return SAI_AUDIO_FREQUENCY_22K;
      case 22000:  return SAI_AUDIO_FREQUENCY_22K;
      case 32000:  return SAI_AUDIO_FREQUENCY_32K;
      case 44100:  return SAI_AUDIO_FREQUENCY_44K;
      case 44000:  return SAI_AUDIO_FREQUENCY_44K;
      case 48000:  return SAI_AUDIO_FREQUENCY_48K;
      case 96000:  return SAI_AUDIO_FREQUENCY_96K;
      case 192000: return SAI_AUDIO_FREQUENCY_192K;
      default:
        Logger::instance().errorf("Unsupported sample rate: %u, using 44.1kHz fallback", rate);
        return SAI_AUDIO_FREQUENCY_44K; // fallback
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
      case 8:  return SAI_DATASIZE_8;
      case 10: return SAI_DATASIZE_10;
      case 16: return SAI_DATASIZE_16;
      case 20: return SAI_DATASIZE_20;
      case 24: return SAI_DATASIZE_24;
      case 32: return SAI_DATASIZE_32;
      default:
        Logger::instance().errorf("Unsupported data size: %u, using 16-bit fallback", bits);
        return SAI_DATASIZE_16; // fallback
    }
  }
};