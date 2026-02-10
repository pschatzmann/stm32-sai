#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example
#if defined(STM32H743xx)

#if defined(STM32H743xx)
// TX DMA IRQ
extern "C" void DMA2_Stream0_IRQHandler(void) {
  if (DMA2->LISR & DMA_LISR_TCIF0) {
    DMA2->LIFCR = DMA_LIFCR_CTCIF0;  // Clear transfer complete flag
  handleDMATxComplete();
  dmaTransferComplete = true;
  }
}
// RX DMA IRQ (true double buffer)
extern "C" void DMA2_Stream1_IRQHandler(void) {
  if (DMA2->LISR & DMA_LISR_TCIF1) {
    DMA2->LIFCR = DMA_LIFCR_CTCIF1;  // Clear transfer complete flag
    extern STM32AudioSAI SAI;
    handleDMARxComplete();
    dmaTransferComplete = true;
  }
}
#endif
#endif
