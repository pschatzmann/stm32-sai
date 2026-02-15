#pragma once
#include "Arduino.h"  // for millis() in DMA timeout handling
#include "Logger.h"
#include "STM32DriverCommon.h"
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
  DMA2_Stream0,            // dma_tx_instance
  DMA_REQUEST_SAI1_A,      // dma_tx_request
  DMA2_Stream0_IRQn,       // dma_tx_irq (update if different)
  DMA2_Stream1,            // dma_rx_instance (update as needed)
  DMA_REQUEST_SAI1_A,      // dma_rx_request (update as needed)
  DMA2_Stream1_IRQn,       // dma_rx_irq (update as needed)
  h743Pins,                // defaultPins
  4                        // numPins
};

