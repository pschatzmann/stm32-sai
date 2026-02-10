#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "STM32DriverCommon.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_sai.h"

#define SAI_SUPPORT 1

static const STM32AudioSAI::PinConfig h743Pins[4] = {
  {'E', 2, 6},  // SCK
  {'E', 3, 6},  // FS
  {'E', 4, 6},  // SD
  {'E', 6, 6}   // MCLK
};

STM32SAIDriverConfig h743Config = {
  DMA2_Stream0,
  DMA_REQUEST_SAI1_A,
  h743Pins,
  4
};

STM32SAIDriver driver(h743Config);
