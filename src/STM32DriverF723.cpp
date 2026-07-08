#if defined(STM32F723xx)
#include "STM32AudioLogger.h"
#include "STM32DriverF723.h"
#include "STM32Driver.h"
#include "STM32AudioSAI.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_sai.h"
#include "stm32f7xx_hal_dma.h"

// Global DMA handles for SAI2 Block A (TX) / Block B (RX) - needed for the
// HAL IRQ handlers below, mirrors the WB55 driver's pattern.
DMA_HandleTypeDef* hdma_sai_tx = nullptr;
DMA_HandleTypeDef* hdma_sai_rx = nullptr;

/// TX DMA IRQ (DMA2_Stream4, see STM32DriverF723.h's SAI_CONFIG)
void DMA2_Stream4_IRQHandler(void) {
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}

/// RX DMA IRQ (DMA2_Stream6, see STM32DriverF723.h's SAI_CONFIG)
void DMA2_Stream6_IRQHandler(void) {
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler)
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaTxTransferComplete = true;
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

#endif
