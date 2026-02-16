#pragma once
#include "PinConfig.h"
/**
 * @brief Configuration for STM32SAIDriver.
 * Holds DMA instance, DMA request, and default pin configuration for a board.
 */
struct STM32SAIDriverConfig {
  SAI_Block_TypeDef* sai_block_tx; // SAI block instance for TX (e.g., SAI1_Block_A)
  // TX DMA config (set to nullptr/0 if not used)
  void* dma_tx_instance;
  uint32_t dma_tx_request;
  IRQn_Type dma_tx_irq;
  // RX DMA config (set to nullptr/0 if not used)
  SAI_Block_TypeDef* sai_block_rx; // SAI block instance for TX (e.g., SAI1_Block_A)
  void* dma_rx_instance;
  uint32_t dma_rx_request;
  IRQn_Type dma_rx_irq;
  // Pin config
  const PinConfig* defaultPins;
  int numPins = 4;
  // Board-specific clock enable/disable lambdas
  void (*enableSAIClocks)();
  void (*disableSAIClocks)();
  void (*enableDMAClocks)();
  void (*disableDMAClocks)();
};
