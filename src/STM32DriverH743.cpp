#if defined(STM32H743xx)
#include "STM32DriverH743.h"
#include "STM32AudioLogger.h"
#include "STM32Driver.h"
#include "STM32AudioSAI.h"

// DMA Interrupt Handler Integration Example using STM32 HAL
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_sai.h"
#include "stm32h7xx_hal_dma.h"

// Global DMA handles for SAI1 Block A (TX) / Block B (RX) - needed for the
// HAL IRQ handlers below. STM32Driver.h/STM32AudioSAI.cpp declare/reference
// these exact extern names; the previous single shared `hdma_sai` local here
// never actually defined them, which would fail to link ("undefined
// reference to hdma_sai_tx") the moment anything pulled in STM32AudioSAI.cpp
// - apparently never caught because H743 had never been link-tested before.
DMA_HandleTypeDef* hdma_sai_tx = nullptr;
DMA_HandleTypeDef* hdma_sai_rx = nullptr;

// extern "C" is required here: this is a .cpp file, so without it these
// definitions get C++ name-mangled and silently fail to override the weak C
// symbols the startup code's vector table / HAL expect (DMA2_StreamX_IRQHandler
// for the NVIC vector table, HAL_SAI_*Callback for HAL's own weak stubs). The
// mangled definitions still get compiled in as dead code, but the real
// interrupt vector keeps pointing at the default handler - an infinite
// self-loop - so once a real SAI TX DMA completion interrupt fires, the CPU
// freezes there forever. Found via a GDB backtrace on the F723 board hitting
// this exact bug; applying the same fix here since H743 had never actually
// been hardware-tested either.
extern "C" {

/// TX DMA IRQ
void DMA2_Stream0_IRQHandler(void) {
  if (hdma_sai_tx) HAL_DMA_IRQHandler(hdma_sai_tx);
}
/// RX DMA IRQ
void DMA2_Stream1_IRQHandler(void) {
  if (hdma_sai_rx) HAL_DMA_IRQHandler(hdma_sai_rx);
}

/// HAL DMA callbacks (called by HAL_DMA_IRQHandler). Don't log/print here -
/// this runs in ISR context and Serial I/O relies on interrupts that are
/// prio-blocked while this ISR runs (see the equivalent note in
/// arduino-audio-tools' I2SDriverSTM32.h).
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaTxTransferComplete = true;
}

/// RX DMA IRQ is handled by HAL, so we just set the flag in the callback
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
  dmaRxTransferComplete = true;
}

}  // extern "C"

#endif
