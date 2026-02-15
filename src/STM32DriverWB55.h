#pragma once

#ifdef STM32WB55xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32wbxx.h"
#include "stm32wbxx_hal.h"

// Pin configuration for STM32WB55 SAI1 Block A
const PinConfig WB55_SAI_PINS[4] = {
    {'A', 5, 6},  // SCK
    {'A', 6, 6},  // FS
    {'A', 7, 6},  // SD
    {'B', 9, 6}   // MCLK
};

// Board-specific driver config for STM32WB55
const STM32SAIDriverConfig SAI_CONFIG = {
    DMA1_Channel1,       // dma_tx_instance
    DMA_REQUEST_SAI1_A,  // dma_tx_request
    DMA1_Channel1_IRQn,  // dma_tx_irq
    DMA1_Channel2,       // dma_rx_instance
    DMA_REQUEST_SAI1_A,  // dma_rx_request (same request for SAI1_A RX)
    DMA1_Channel2_IRQn,  // dma_rx_irq
    WB55_SAI_PINS,       // defaultPins
    sizeof(WB55_SAI_PINS) / sizeof(PinConfig),  // numPins
    []() {
        Logger::instance().debug("enable clocks");
      __HAL_RCC_DMAMUX1_CLK_ENABLE();
      __HAL_RCC_DMA1_CLK_ENABLE();
    },  // enableClocks
    []() {
        Logger::instance().debug("disable clocks");
      __HAL_RCC_DMA1_CLK_DISABLE();
      __HAL_RCC_DMAMUX1_CLK_DISABLE();
    }  // disableClocks
};

#endif