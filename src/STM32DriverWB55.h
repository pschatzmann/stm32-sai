#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "stm32wbxx.h"

#define SAI_SUPPORT 1

/**
 * SAI driver implementation for STM32WB55. This configures SAI1 Block A for
 * audio input/output.
 */
class STM32SAIDriverWB55 {
 public:
  inline void initSAI(STM32AudioSAI* audio) {
    // Enable SAI1 peripheral clock
    RCC->APB2ENR |= RCC_APB2ENR_SAI1EN;
    // Reset SAI1 Block A registers
    SAI1_Block_A->CR1 = 0;
    SAI1_Block_A->CR2 = 0;
    SAI1_Block_A->FRCR = 0;
    SAI1_Block_A->SLOTR = 0;

    // Protocol
    uint32_t protocol_bits = (audio->getProtocol() << 5);
    // Data size
    uint32_t datasize_bits =
        ((audio->getBitsPerSample() == 16 ? 0x01 : 0x02) << 8);
    // Master/slave
    uint32_t master_bits = audio->isMaster() ? (1 << 0) : 0;
    // Mode
    // (For SAI, mode is typically set by Block A/B and transmitter/receiver)

    // Set CR1: protocol, data size, master/slave
    SAI1_Block_A->CR1 = protocol_bits | datasize_bits | master_bits;
    SAI1_Block_A->CR2 = 0;

    // Frame config: frame length, slots
    uint32_t frame_len = audio->getBitsPerSample() * audio->getChannels();
    uint32_t slots = audio->getChannels();
    SAI1_Block_A->FRCR = (frame_len << 8) | (slots - 1);

    // Slot config: slot size, slots
    uint32_t slot_size = audio->getBitsPerSample() - 1;
    SAI1_Block_A->SLOTR = (slot_size << 8) | (slots - 1);

    // Enable SAI
    SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
  }

  inline void deinitSAI(STM32AudioSAI* audio) {
    // Disable SAI1 and its clock
    SAI1_Block_A->CR1 &= ~SAI_xCR1_SAIEN;
    RCC->APB2ENR &= ~RCC_APB2ENR_SAI1EN;
  }

  inline void initDMA(STM32AudioSAI* audio) {
    // Enable DMA1 peripheral clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
  }

  inline void deinitDMA(STM32AudioSAI* audio) {
    // Disable DMA1 Channel1
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
  }

  inline bool configureGPIO(STM32AudioSAI* audio) {
    // Configure SAI GPIO pins for STM32WB55 using per-pin port/pin/AF
    // Defaults: GPIOA (SCK, FS, SD), GPIOB (MCLK), pins 5,6,7,9, AF6
    STM32AudioSAI::PinConfig defaultPins[4] = {
        {'A', 5, 6},  // SCK
        {'A', 6, 6},  // FS
        {'A', 7, 6},  // SD
        {'B', 9, 6}   // MCLK
    };
    bool success = true;
    for (int i = 0; i < 4; ++i) {
      STM32AudioSAI::PinConfig cfg =
          audio->getPinConfig((STM32AudioSAI::PinId)i);
      if (cfg.port == -1) cfg.port = defaultPins[i].port;
      if (cfg.pin == -1) cfg.pin = defaultPins[i].pin;
      if (cfg.af == -1) cfg.af = defaultPins[i].af;
      // Log the selected pin config
      Logger::instance().infof("SAI Pin %d: Port %c, Pin %d, AF %d", i,
                               cfg.port, cfg.pin, cfg.af);
      // Enable GPIO clock
      GPIO_TypeDef* gpio = nullptr;
      switch (cfg.port) {
        case 'A':
          RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
          gpio = GPIOA;
          break;
        case 'B':
          RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
          gpio = GPIOB;
          break;
        case 'C':
          RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
          gpio = GPIOC;
          break;
        case 'D':
          RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN;
          gpio = GPIOD;
          break;
        case 'E':
          RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN;
          gpio = GPIOE;
          break;
        default:
          Logger::instance().warnf("Invalid SAI pin port: %d for pin index %d",
                                   cfg.port, i);
          success = false;
          continue;  // skip if invalid port
      }
      if (!gpio) {
        Logger::instance().errorf("Failed to get GPIO pointer for port %c",
                                  cfg.port);
        success = false;
        continue;
      }
      // Set pin to alternate function mode
      gpio->MODER &= ~(0x3 << (cfg.pin * 2));
      gpio->MODER |= (0x2 << (cfg.pin * 2));
      // Set alternate function
      if (cfg.pin < 8) {
        gpio->AFR[0] &= ~(0xF << (cfg.pin * 4));
        gpio->AFR[0] |= (cfg.af << (cfg.pin * 4));
      } else {
        gpio->AFR[1] &= ~(0xF << ((cfg.pin - 8) * 4));
        gpio->AFR[1] |= (cfg.af << ((cfg.pin - 8) * 4));
      }
    }
    // Note: User should verify pin mappings and alternate function for their
    // board.
    return success;
  }

  // Double buffer read: bufferA and bufferB must be size/2 each
  inline size_t read(STM32AudioSAI* audio, void* bufferA, void* bufferB,
                     size_t size) {
    dmaTransferComplete = false;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1_Channel2->CPAR = (uint32_t)&SAI1_Block_A->DR;
    DMA1_Channel2->CMAR = (uint32_t)bufferA;
    DMA1_Channel2->CNDTR = size / 4;  // each buffer is size/2, so NDTR is half
    // Note: STM32WB DMA does not support true double buffer mode in hardware,
    // so swap manually in IRQ
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_PL_1;
    DMA1_Channel2->CCR |= DMA_CCR_EN;
    SAI1_Block_A->CR1 |= SAI_xCR1_DMAEN;
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaTransferComplete && (millis() - start < timeout));
    return dmaTransferComplete ? size : 0;
  }

  // Double buffer write: bufferA and bufferB must be size/2 each
  inline size_t write(STM32AudioSAI* audio, const void* bufferA,
                      const void* bufferB, size_t size) {
    dmaTransferComplete = false;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1_Channel1->CPAR = (uint32_t)&SAI1_Block_A->DR;
    DMA1_Channel1->CMAR = (uint32_t)bufferA;
    DMA1_Channel1->CNDTR = size / 4;  // each buffer is size/2, so NDTR is half
    // Note: STM32WB DMA does not support true double buffer mode in hardware,
    // so swap manually in IRQ
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CCR =
        DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE | DMA_CCR_PL_1;
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    SAI1_Block_A->CR1 |= SAI_xCR1_DMAEN;
    uint32_t start = millis();
    uint32_t timeout = audio->getIOTimoutMs();
    while (!dmaTransferComplete && (millis() - start < timeout));
    return dmaTransferComplete ? size : 0;
  }
  inline bool isRunning(const STM32AudioSAI* audio) {
    // Check if SAI1 Block A is enabled
    return (SAI1_Block_A->CR1 & SAI_xCR1_SAIEN) != 0;
  }
};

using STM32SAIDriver = STM32SAIDriverWB55;