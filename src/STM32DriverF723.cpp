#if defined(STM32F723xx)
#include "STM32AudioLogger.h"
#include "STM32DriverF723.h"
#include "STM32Driver.h"
#include "STM32AudioSAI.h"

#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_sai.h"
#include "stm32f7xx_hal_dma.h"
#include <string.h>

// Global DMA handles for SAI2 Block A (TX) / Block B (RX) - needed for the
// HAL IRQ handlers below, mirrors the WB55 driver's pattern.
DMA_HandleTypeDef* hdma_sai_tx = nullptr;
DMA_HandleTypeDef* hdma_sai_rx = nullptr;

// extern "C" is required here: this is a .cpp file, so without it these
// definitions get C++ name-mangled and silently fail to override the weak C
// symbols the startup code's vector table / HAL expect (DMA2_StreamX_IRQHandler
// for the NVIC vector table, HAL_SAI_*Callback for HAL's own weak stubs).
// The mangled definitions still get compiled in as dead code, but the real
// interrupt vector keeps pointing at the default handler - an infinite
// self-loop (`b.n` to itself) - so once a real SAI TX DMA completion
// interrupt fires, the CPU freezes there forever. Confirmed via a GDB
// backtrace on real hardware: PC stuck in WWDG_IRQHandler (an alias for the
// same default-handler address) called from inside STM32SAIDriver::write()'s
// busy-wait, right where the real DMA2_Stream4 completion interrupt should
// have landed.
extern "C" {

/// TX DMA IRQ (DMA2_Stream4, see STM32DriverF723.h's SAI_CONFIG)
void DMA2_Stream4_IRQHandler(void) {
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}

/// RX DMA IRQ (DMA2_Stream6, see STM32DriverF723.h's SAI_CONFIG)
void DMA2_Stream6_IRQHandler(void) {
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler)
/// Fires when the CIRCULAR TX buffer's first half has just finished
/// transmitting (DMA is now sending the second half) - the first half is
/// free for write() to refill with the next chunk.
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (saiTxCircBufPtr && saiTxCircHalfBytes) {
    memset(saiTxCircBufPtr, 0, saiTxCircHalfBytes);
  }
  saiTxFreeHalf = 0;
}

/// Fires when the CIRCULAR TX buffer's second half has just finished
/// transmitting (DMA has wrapped back to the start, re-sending the first
/// half) - the second half is free for write() to refill.
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (saiTxCircBufPtr && saiTxCircHalfBytes) {
    memset(saiTxCircBufPtr + saiTxCircHalfBytes, 0, saiTxCircHalfBytes);
  }
  saiTxFreeHalf = 1;
  dmaTxTransferComplete = true;
  saiTxCpltCount++;
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

/// Only capture the error code here - do NOT log/print from ISR context (see
/// the note on readToTransmit in arduino-audio-tools' I2SDriverSTM32.h: Serial
/// I/O relies on interrupts that are prio-blocked while this ISR runs).
/// STM32SAIDriver::write()'s timeout path (safe, non-ISR context) reports it.
void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
  saiLastErrorCode = hsai->ErrorCode;
  saiTxErrorCount++;
}

}  // extern "C"

#endif
