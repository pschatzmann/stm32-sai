#pragma once
#include "PinConfig.h"

struct SAIPinCandidate {
  PinId id;
  PinConfig config;
};

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
  int numPins = static_cast<int>(PinId::NumPins);
  const SAIPinCandidate* allowedPins = nullptr;
  int numAllowedPins = 0;
  // Board-specific clock enable/disable lambdas. enableSAIClocks takes the
  // target sample rate in Hz so boards whose SAI clock source needs a
  // different PLL profile per sample-rate family (e.g. F7's PLLI2S: one
  // profile for 11025/22050/44100 Hz, another for 8000/16000/32000/48000/
  // 96000 Hz) can pick the right one; boards that don't need this can just
  // ignore the parameter.
  void (*enableSAIClocks)(uint32_t sample_rate);
  void (*disableSAIClocks)();
  void (*enableDMAClocks)();
  void (*disableDMAClocks)();
};
