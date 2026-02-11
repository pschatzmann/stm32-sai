#pragma once
#include "STM32DriverCommon.h"
#include "stm32wbxx_hal.h"

/// Pin configuration for STM32WB55 SAI1 Block A
static const STM32AudioSAI::PinConfig WB55_SAI_PINS[] = {
    {'A', 5, 6},  // SCK
    {'A', 6, 6},  // FS
    {'A', 7, 6},  // SD
    {'B', 9, 6}   // MCLK
};

/// Board-specific driver config for STM32WB55
static const STM32SAIDriverConfig WB55_SAI_CONFIG = {
    DMA1_Channel1,         // dma_instance
    DMA_REQUEST_SAI1_A,    // dma_request
    WB55_SAI_PINS,         // defaultPins
    4                      // numPins
};

/// Board-specific driver instance for STM32WB55
STM32SAIDriver driver(WB55_SAI_CONFIG);