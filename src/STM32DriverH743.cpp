#include "STM32AudioSAI.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sai.h"
#include "stm32h7xx_hal_dma.h"


// DMA Interrupt Handler Integration Example using STM32 HAL
#if defined(STM32H743xx)

// Global DMA handle for SAI1 Block A (needed for HAL IRQ handler)
DMA_HandleTypeDef hdma_sai_a;

// TX DMA IRQ
extern "C" void DMA2_Stream0_IRQHandler(void) {
  HAL_DMA_IRQHandler(&hdma_sai_a);
}

// HAL DMA callbacks (called by HAL_DMA_IRQHandler)
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaTxTransferComplete = true;
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

#endif
