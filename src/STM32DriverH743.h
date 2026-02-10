#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "stm32h7xx.h"

#define SAI_SUPPORT 1

/**
 * SAI driver implementation for STM32H743. This configures SAI1 Block A for
 * audio input/output.
 */
class STM32SAIDriverH743 {
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

    SAI1_Block_A->CR1 = protocol_bits | datasize_bits | master_bits;
    SAI1_Block_A->CR2 = 0;

    uint32_t frame_len = audio->getBitsPerSample() * audio->getChannels();
    uint32_t slots = audio->getChannels();
    SAI1_Block_A->FRCR = (frame_len << 8) | (slots - 1);

    uint32_t slot_size = audio->getBitsPerSample() - 1;
    SAI1_Block_A->SLOTR = (slot_size << 8) | (slots - 1);

    SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
  }

  inline void deinitSAI(STM32AudioSAI* audio) {
    // Disable SAI1 and its clock
    SAI1_Block_A->CR1 &= ~SAI_xCR1_SAIEN;
    RCC->APB2ENR &= ~RCC_APB2ENR_SAI1EN;
  }

  inline void initDMA(STM32AudioSAI* audio) {
    // Enable DMA2 peripheral clock
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  }

  inline void deinitDMA(STM32AudioSAI* audio) {
    // Disable DMA2 Stream0
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
  }

  inline bool configureGPIO(STM32AudioSAI* audio) {
    // Configure SAI GPIO pins for STM32H743 using per-pin port/pin/AF
    // Defaults: GPIOE, pins 2(SCK), 3(FS), 4(SD), 6(MCLK), AF6
    STM32AudioSAI::PinConfig defaultPins[4] = {
        {'E', 2, 6},  // SCK
        {'E', 3, 6},  // FS
        {'E', 4, 6},  // SD
        {'E', 6, 6}   // MCLK
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
          RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
          gpio = GPIOA;
          break;
        case 'B':
          RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
          gpio = GPIOB;
          break;
        case 'C':
          RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
          gpio = GPIOC;
          break;
        case 'D':
          RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
          gpio = GPIOD;
          break;
        case 'E':
          RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
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
    return success;
  }

  // Double buffer read: bufferA and bufferB must be size/2 each
  inline size_t read(STM32AudioSAI* audio, void* bufferA, void* bufferB,
                     size_t size) {
    dmaTransferComplete = false;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    DMA2_Stream1->CR &= ~DMA_SxCR_EN;
    DMA2_Stream1->PAR = (uint32_t)&SAI1_Block_A->DR;
    DMA2_Stream1->M0AR = (uint32_t)bufferA;
    DMA2_Stream1->M1AR = (uint32_t)bufferB;
    DMA2_Stream1->NDTR = size / 4;  // each buffer is size/2, so NDTR is half
    DMA2_Stream1->CR =
        DMA_SxCR_DBM | DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_PL_1 | (0 << 6);
    NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    DMA2_Stream1->CR |= DMA_SxCR_EN;
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
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    DMA2_Stream0->PAR = (uint32_t)&SAI1_Block_A->DR;
    DMA2_Stream0->M0AR = (uint32_t)bufferA;
    DMA2_Stream0->M1AR = (uint32_t)bufferB;
    DMA2_Stream0->NDTR = size / 4;  // each buffer is size/2, so NDTR is half
    DMA2_Stream0->CR = DMA_SxCR_DBM | DMA_SxCR_MINC | DMA_SxCR_DIR_0 |
                       DMA_SxCR_TCIE | DMA_SxCR_PL_1 | (0 << 6);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    DMA2_Stream0->CR |= DMA_SxCR_EN;
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
using STM32SAIDriver = STM32SAIDriverH743;