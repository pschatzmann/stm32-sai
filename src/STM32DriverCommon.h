#include <Arduino.h>
#include "Logger.h"
#include "STM32AudioSAI.h"

struct STM32SAIDriverConfig {
  void* dma_instance;
  uint32_t dma_request;
  const STM32AudioSAI::PinConfig* defaultPins;
  int numPins;
};

class STM32SAIDriver {
public:
  SAI_HandleTypeDef hsai_a = {};
  DMA_HandleTypeDef hdma_sai_a = {};
  STM32SAIDriverConfig config;

  STM32SAIDriver(const STM32SAIDriverConfig& cfg) : config(cfg) {}

  void initSAI(STM32AudioSAI* audio) {
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
    }
  }

  void deinitSAI() {
    if (HAL_SAI_DeInit(&hsai_a) != HAL_OK) {
      Logger::instance().error("HAL_SAI_DeInit failed");
    }
    __HAL_RCC_SAI1_CLK_DISABLE();
  }

  void initDMA(STM32AudioSAI* audio) {
    hdma_sai_a.Instance = (decltype(hdma_sai_a.Instance))config.dma_instance;
    hdma_sai_a.Init.Request = config.dma_request;
    hdma_sai_a.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_sai_a.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_sai_a.Init.MemInc = DMA_MINC_ENABLE;
    hdma_sai_a.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_sai_a.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_sai_a.Init.Mode = DMA_CIRCULAR;
    hdma_sai_a.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_sai_a.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_sai_a) != HAL_OK) {
      Logger::instance().error("HAL_DMA_Init failed");
    }
    __HAL_LINKDMA(&hsai_a, hdmatx, hdma_sai_a);
  }

  void deinitDMA() {
    if (HAL_DMA_DeInit(&hdma_sai_a) != HAL_OK) {
      Logger::instance().error("HAL_DMA_DeInit failed");
    }
  }

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

  size_t read(STM32AudioSAI* audio, void* buffer, size_t size) {
    dmaRxTransferComplete = false;
    if (HAL_SAI_Receive_DMA(&hsai_a, (uint8_t*)buffer,
                            size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Receive_DMA failed");
      return 0;
    }
    uint32_t start = millis();
    return dmaRxTransferComplete ? size : 0;
  }

  size_t write(STM32AudioSAI* audio, const void* buffer, size_t size) {
    dmaTxTransferComplete = false;
    if (HAL_SAI_Transmit_DMA(&hsai_a, (uint8_t*)buffer,
                             size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Transmit_DMA failed");
      return 0;
    }
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaTxTransferComplete && (millis() - start < timeout));
    return dmaTxTransferComplete ? size : 0;
  }

  bool isRunning() {
    return (SAI1_Block_A->CR1 & SAI_xCR1_SAIEN) != 0;
  }

  // Map a sample rate in Hz to the corresponding SAI_AUDIO_FREQUENCY_xxx value
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
      default:     return SAI_AUDIO_FREQUENCY_48K; // fallback
    }
  }

  // Map bits per sample to the corresponding SAI_DATASIZE_xx value
  static uint32_t mapDataSize(uint8_t bits) {
    switch (bits) {
      case 8:  return SAI_DATASIZE_8;
      case 10: return SAI_DATASIZE_10;
      case 16: return SAI_DATASIZE_16;
      case 20: return SAI_DATASIZE_20;
      case 24: return SAI_DATASIZE_24;
      case 32: return SAI_DATASIZE_32;
      default: return SAI_DATASIZE_16; // fallback
    }
  }
};