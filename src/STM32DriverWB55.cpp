#if defined(STM32WB55xx)
#include "STM32AudioLogger.h"
#include "STM32DriverWB55.h"
#include "STM32Driver.h"
#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example
#include "stm32wbxx_hal.h"
#include "stm32wbxx_hal_sai.h"
#include "stm32wbxx_hal_dma.h"

// Global DMA handles for SAI1 Block A TX and RX (needed for HAL IRQ handler)
DMA_HandleTypeDef* hdma_sai_tx = nullptr;
DMA_HandleTypeDef* hdma_sai_rx = nullptr;


/// TX DMA IRQ

void DMA1_Stream0_IRQHandler(void) {
  STM32AudioLogger::instance().debug("DMA1_Stream0_IRQHandler called");
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}


void DMA1_Stream1_IRQHandler(void) {
  STM32AudioLogger::instance().debug("DMA2_Stream1_IRQHandler called");
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// TX DMA IRQ

void DMA1_Channel1_IRQHandler(void) {
  STM32AudioLogger::instance().debug("DMA1_Channel1_IRQHandler called");
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}

/// RX DMA IRQ

void DMA1_Channel2_IRQHandler(void) {
  STM32AudioLogger::instance().debug("DMA1_Channel2_IRQHandler called");
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler)
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaTxTransferComplete = true;
  STM32AudioLogger::instance().debug("HAL_SAI_TxCpltCallback called: dmaTxTransferComplete set to true");
}

/// RX DMA IRQ is handled by HAL, so we just set the flag in the callback
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
  STM32AudioLogger::instance().debug("HAL_SAI_TxCpltCallback called: dmaTxTransferComplete set to true");
}

#endif

