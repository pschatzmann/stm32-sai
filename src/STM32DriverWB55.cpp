#if defined(STM32WB55xx)
#include "STM32AudioLogger.h"
#include "STM32DriverWB55.h"
#include "STM32Driver.h"
#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example
#include "stm32wbxx_hal.h"
#include "stm32wbxx_hal_sai.h"
#include "stm32wbxx_hal_dma.h"
#include <string.h>

// Global DMA handles for SAI1 Block A TX and RX (needed for HAL IRQ handler)
DMA_HandleTypeDef* hdma_sai_tx = nullptr;
DMA_HandleTypeDef* hdma_sai_rx = nullptr;

// extern "C" is required here: this is a .cpp file, so without it these
// definitions get C++ name-mangled and silently fail to override the weak C
// symbols the startup code's vector table / HAL expect (DMA1_ChannelX_IRQHandler
// for the NVIC vector table, HAL_SAI_*Callback for HAL's own weak stubs). The
// mangled definitions still get compiled in as dead code, but the real
// interrupt vector keeps pointing at the default handler - an infinite
// self-loop - so once a real SAI TX DMA completion interrupt fires, the CPU
// freezes there forever. Found via a GDB backtrace on the F723 board hitting
// this exact bug (same missing extern "C" there); applying the same fix here
// since WB55 had never actually been hardware-tested either.
extern "C" {

/// TX DMA IRQ
void DMA1_Channel1_IRQHandler(void) {
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}

/// RX DMA IRQ
void DMA1_Channel2_IRQHandler(void) {
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler). Don't log/print here -
/// this runs in ISR context and Serial I/O relies on interrupts that are
/// prio-blocked while this ISR runs (see the equivalent note in
/// arduino-audio-tools' I2SDriverSTM32.h).
/// Fires when the CIRCULAR TX buffer's first half has just finished
/// transmitting - that half is now free for write() to refill (see
/// STM32Driver.h's write(), which busy-waits on saiTxFreeHalf). Without
/// this, saiTxFreeHalf never leaves its initial -1 and every write() call
/// after the first one times out - previously missing here (only the F723
/// driver had it), so TX (and therefore Duplex) never actually worked past
/// the first chunk on this board.
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  if (saiTxCircBufPtr && saiTxCircHalfBytes) {
    memset(saiTxCircBufPtr, 0, saiTxCircHalfBytes);
  }
  saiTxFreeHalf = 0;
}

/// Fires when the second half completes (DMA wraps back to the start) -
/// that half is now free for write() to refill.
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  if (saiTxCircBufPtr && saiTxCircHalfBytes) {
    memset(saiTxCircBufPtr + saiTxCircHalfBytes, 0, saiTxCircHalfBytes);
  }
  saiTxFreeHalf = 1;
  dmaTxTransferComplete = true;
}

/// RX DMA IRQ is handled by HAL, so we just set the flag in the callback
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

}  // extern "C"

#endif

