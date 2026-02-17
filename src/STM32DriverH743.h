#pragma once

#ifdef STM32H743xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_sai.h"

#define SAI_SUPPORT 1

/// Pin configuration for STM32H743 SAI1 Block A
static const PinConfig h743Pins[4] = {
    {'E', 2, 6},  // SCK
    {'E', 3, 6},  // FS
    {'E', 4, 6},  // SD
    {'E', 6, 6}   // MCLK
};

/// Board-specific driver config for STM32H743
STM32SAIDriverConfig SAI_CONFIG = {
    SAI1_Block_A,                            // sai_block_tx
    DMA2_Stream0,                            // dma_tx_instance
    DMA_REQUEST_SAI1_A,                      // dma_tx_request
    DMA2_Stream0_IRQn,                       // dma_tx_irq
    SAI1_Block_B,                            // sai_block_rx
    DMA2_Stream1,                            // dma_rx_instance
    DMA_REQUEST_SAI1_B,                      // dma_rx_request
    DMA2_Stream1_IRQn,                       // dma_rx_irq
    h743Pins,                                // defaultPins
    sizeof(h743Pins) / sizeof(PinConfig),    // numPins
    []() { __HAL_RCC_SAI1_CLK_ENABLE(); },   // enableSAIClocks
    []() { __HAL_RCC_SAI1_CLK_DISABLE(); },  // disableSAIClocks
    []() {
      // enableDMAClocks
      __HAL_RCC_DMA2_CLK_ENABLE();
      __HAL_RCC_DMAMUX1_CLK_ENABLE();
    },
    []() {
      // disableDMAClocks
      __HAL_RCC_DMA2_CLK_DISABLE();
      __HAL_RCC_DMAMUX1_CLK_DISABLE();
    }};

#endif