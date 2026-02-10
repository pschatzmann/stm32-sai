#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "STM32DriverCommon.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_sai.h"

#define SAI_SUPPORT 1
// Use global DMA handle for HAL IRQ compatibility
extern DMA_HandleTypeDef hdma_sai_a;

/**
 * SAI driver implementation for STM32H743. This configures SAI1 Block A for
 * audio input/output.
 */

class STM32SAIDriverH743 {
 public:
  SAI_HandleTypeDef hsai_a = {};

  inline void initSAI(STM32AudioSAI* audio) {
    STM32DriverCommon::initSAI(&hsai_a, audio);
  }

  inline void deinitSAI(STM32AudioSAI* /*audio*/) {
    STM32DriverCommon::deinitSAI(&hsai_a);
  }

  inline void initDMA(STM32AudioSAI* audio) {
  __HAL_RCC_DMA2_CLK_ENABLE();
  STM32DriverCommon::initDMA(&hdma_sai_a, &hsai_a, audio, DMA2_Stream0, DMA_REQUEST_SAI1_A);
  }

  inline void deinitDMA(STM32AudioSAI* /*audio*/) {
    STM32DriverCommon::deinitDMA(&hdma_sai_a);
  }

  inline bool configureGPIO(STM32AudioSAI* audio) {
    static const STM32AudioSAI::PinConfig defaultPins[4] = {
        {'E', 2, 6},  // SCK
        {'E', 3, 6},  // FS
        {'E', 4, 6},  // SD
        {'E', 6, 6}   // MCLK
    };
    return STM32DriverCommon::configureGPIO(audio, defaultPins, 4);
  }

  inline size_t read(STM32AudioSAI* audio, void* buffer, size_t size) {
    return STM32DriverCommon::read(&hsai_a, audio, buffer, size);
  }

  inline size_t write(STM32AudioSAI* audio, const void* buffer, size_t size) {
    return STM32DriverCommon::write(&hsai_a, audio, buffer, size);
  }

  inline bool isRunning(const STM32AudioSAI* audio) {
    return STM32DriverCommon::isRunning(SAI1_Block_A);
  }
};

using STM32SAIDriver = STM32SAIDriverH743;