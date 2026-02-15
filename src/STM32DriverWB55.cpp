#include "Logger.h"
#include "STM32DriverWB55.h"
#include "STM32DriverCommon.h"
#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example
#if defined(STM32WB55xx)
#include "stm32wbxx_hal.h"
#include "stm32wbxx_hal_sai.h"
#include "stm32wbxx_hal_dma.h"

// Global DMA handle for SAI1 Block A (needed for HAL IRQ handler)
DMA_HandleTypeDef hdma_sai;

/// TX DMA IRQ
void DMA1_Channel1_IRQHandler(void) {
  HAL_DMA_IRQHandler(&hdma_sai);
}

/// RX DMA IRQ
void DMA1_Channel2_IRQHandler(void) {
  HAL_DMA_IRQHandler(&hdma_sai);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler)
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaTxTransferComplete = true;
  Logger::instance().debug("HAL_SAI_TxCpltCallback called: dmaTxTransferComplete set to true");
}

/// RX DMA IRQ is handled by HAL, so we just set the flag in the callback
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

#endif

