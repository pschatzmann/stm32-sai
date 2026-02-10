#include <Arduino.h>

#include "Logger.h"
#include "STM32AudioSAI.h"

class STM32DriverCommon {
 public:
  static void initSAI(SAI_HandleTypeDef* hsai, STM32AudioSAI* audio) {
    __HAL_RCC_SAI1_CLK_ENABLE();
    hsai->Instance = SAI1_Block_A;
    hsai->Init.AudioMode =
        audio->isMaster() ? SAI_MODEMASTER_TX : SAI_MODESLAVE_RX;
    hsai->Init.Synchro = SAI_ASYNCHRONOUS;
    hsai->Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    hsai->Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
    hsai->Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
    hsai->Init.AudioFrequency = (audio->getSampleRate() == 44100)
                                    ? SAI_AUDIO_FREQUENCY_44K
                                    : SAI_AUDIO_FREQUENCY_48K;
    hsai->Init.Protocol = (audio->getProtocol() == STM32AudioSAI::I2S)
                              ? SAI_FREE_PROTOCOL
                              : SAI_PCM_LONG;
    hsai->Init.DataSize =
        (audio->getBitsPerSample() == 16) ? SAI_DATASIZE_16 : SAI_DATASIZE_24;
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
      Logger::instance().error("HAL_SAI_Init failed");
    }
  }

  static void deinitSAI(SAI_HandleTypeDef* hsai) {
    if (HAL_SAI_DeInit(hsai) != HAL_OK) {
      Logger::instance().error("HAL_SAI_DeInit failed");
    }
    __HAL_RCC_SAI1_CLK_DISABLE();
  }

  static void initDMA(DMA_HandleTypeDef* hdma, SAI_HandleTypeDef* hsai,
                             STM32AudioSAI* audio, void* dma_instance, uint32_t dma_request) {
    hdma->Instance = (decltype(hdma->Instance))dma_instance;
    hdma->Init.Request = dma_request;
    hdma->Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma->Init.Mode = DMA_CIRCULAR;
    hdma->Init.Priority = DMA_PRIORITY_HIGH;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(hdma) != HAL_OK) {
      Logger::instance().error("HAL_DMA_Init failed");
    }
    __HAL_LINKDMA(hsai, hdmatx, *hdma);
  }

  static void deinitDMA(DMA_HandleTypeDef* hdma) {
    if (HAL_DMA_DeInit(hdma) != HAL_OK) {
      Logger::instance().error("HAL_DMA_DeInit failed");
    }
  }

  static bool configureGPIO(STM32AudioSAI* audio,
                                   const STM32AudioSAI::PinConfig* defaultPins,
                                   int numPins) {
    bool success = true;
    for (int i = 0; i < numPins; ++i) {
      STM32AudioSAI::PinConfig cfg =
          audio->getPinConfig((STM32AudioSAI::PinId)i);
      if (cfg.port == -1) cfg.port = defaultPins[i].port;
      if (cfg.pin == -1) cfg.pin = defaultPins[i].pin;
      if (cfg.af == -1) cfg.af = defaultPins[i].af;
      Logger::instance().infof("SAI Pin %d: Port %c, Pin %d, AF %d", i,
                               cfg.port, cfg.pin, cfg.af);
      // ... GPIO enabling and HAL_GPIO_Init logic ...
    }
    return success;
  }
  static size_t read(SAI_HandleTypeDef* hsai, STM32AudioSAI* audio,
                            void* buffer, size_t size) {
    dmaTransferComplete = false;
    if (HAL_SAI_Receive_DMA(hsai, (uint8_t*)buffer,
                            size / (audio->getBitsPerSample() / 8)) != HAL_OK) {
      Logger::instance().error("HAL_SAI_Receive_DMA failed");
      return 0;
    }
    uint32_t start = millis();
    return dmaTransferComplete ? size : 0;
  }

  static size_t write(SAI_HandleTypeDef* hsai, STM32AudioSAI* audio,
                             const void* buffer, size_t size) {
    dmaTransferComplete = false;
    if (HAL_SAI_Transmit_DMA(hsai, (uint8_t*)buffer,
                             size / (audio->getBitsPerSample() / 8)) !=
        HAL_OK) {
      Logger::instance().error("HAL_SAI_Transmit_DMA failed");
      return 0;
    }
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaTransferComplete && (millis() - start < timeout));
    return dmaTransferComplete ? size : 0;
  }

  static bool isRunning(volatile SAI_Block_TypeDef* block) {
    return (block->CR1 & SAI_xCR1_SAIEN) != 0;
  }
};