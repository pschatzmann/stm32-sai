#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "stm32wbxx_hal.h"

#define SAI_SUPPORT 1

/**
 * SAI driver implementation for STM32WB55. This configures SAI1 Block A for
 * audio input/output.
 */
class STM32SAIDriverWB55 : public STM32DriverCommon {
 public:
  SAI_HandleTypeDef hsai_a = {};
  DMA_HandleTypeDef hdma_sai_a = {};

  inline void initSAI(STM32AudioSAI* audio) {
    STM32DriverCommon::initSAI(&hsai_a, audio);
  }

  inline void deinitSAI(STM32AudioSAI* /*audio*/) {
    STM32DriverCommon::deinitSAI(&hsai_a);
  }

  inline void initDMA(STM32AudioSAI* audio) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    STM32DriverCommon::initDMA(&hdma_sai_a, &hsai_a, audio,
                                      DMA1_Channel1, DMA_REQUEST_SAI1_A);
  }

  inline void deinitDMA(STM32AudioSAI* /*audio*/) {
    STM32DriverCommon::deinitDMA(&hdma_sai_a);
  }

  inline bool configureGPIO(STM32AudioSAI* audio) {
    static const STM32AudioSAI::PinConfig defaultPins[4] = {
        {'A', 5, 6},  // SCK
        {'A', 6, 6},  // FS
        {'A', 7, 6},  // SD
        {'B', 9, 6}   // MCLK
    };
    return STM32DriverCommon::configureGPIO(audio, defaultPins, 4);
  }

  // Single buffer write
  inline size_t read(STM32AudioSAI* audio, void* buffer, size_t size) {
    return STM32DriverCommon::read(&hsai_a, audio, buffer, size);
  }

  inline size_t write(STM32AudioSAI* audio, const void* buffer, size_t size) {
    return STM32DriverCommon::write(&hsai_a, audio, buffer, size);
  }

  inline bool isRunning(const STM32AudioSAI* /*audio*/) {
    return STM32DriverCommon::isRunning(SAI1_Block_A);
  }