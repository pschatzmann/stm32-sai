#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example
#if defined(STM32WB55xx)

// TX DMA IRQ
extern "C" void DMA1_Channel1_IRQHandler(void) {
  if (DMA1->ISR & DMA_ISR_TCIF1) {
    DMA1->IFCR = DMA_IFCR_CTCIF1;  // Clear transfer complete flag
    handleDMATxComplete();
    dmaTransferComplete = true;
  }
}

// RX DMA IRQ (software double buffer)
extern "C" void DMA1_Channel2_IRQHandler(void) {
  if (DMA1->ISR & DMA_ISR_TCIF2) {
    DMA1->IFCR = DMA_IFCR_CTCIF2;  // Clear transfer complete flag
    handleDMARxComplete();
    dmaTransferComplete = true;
  }
}

#endif
