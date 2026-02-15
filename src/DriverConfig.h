#pragma once
#include "PinConfig.h"
/**
 * @brief Configuration for STM32SAIDriver.
 * Holds DMA instance, DMA request, and default pin configuration for a board.
 */
struct STM32SAIDriverConfig {
  // TX DMA config (set to nullptr/0 if not used)
  void* dma_tx_instance;
  uint32_t dma_tx_request;
  IRQn_Type dma_tx_irq;
  // RX DMA config (set to nullptr/0 if not used)
  void* dma_rx_instance;
  uint32_t dma_rx_request;
  IRQn_Type dma_rx_irq;
  // Pin config
  const PinConfig* defaultPins;
  int numPins = 4;
};
