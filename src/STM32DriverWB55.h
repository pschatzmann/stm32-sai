#pragma once

#ifdef STM32WB55xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32wbxx.h"
#include "stm32wbxx_hal.h"

// Pin configuration for STM32WB55 SAI1 Block A (TX) / Block B (RX).
// SCK/FS/SD/MCLK (Block A) are the library's original pins, corrected here
// from AF6 to AF13 - AF6 does not map to SAI1 on this chip (stm32wbxx_hal_
// gpio_ex.h only defines GPIO_AF13_SAI1; AF6 is MCO/LSCO/RF_DTBx), so these
// pins would not actually have been routed to the SAI peripheral before.
// SD_RX (Block B) is a BEST-EFFORT PLACEHOLDER, not verified against any
// datasheet/schematic/reference board - STM32WB55's actual SAI1_SD_B pin
// depends on the specific package, and no P-NUCLEO-WB55 audio BSP exists to
// cross-check against (unlike the F723 pins, which were confirmed against
// ST's own BSP). Confirm against your board before relying on Input/Duplex
// mode, and override with setPin(STM32AudioSAI::SD_RX, ...) if it's wrong.
const PinConfig WB55_SAI_PINS[5] = {
    {'A', 5, 13},  // SCK
    {'A', 6, 13},  // FS
    {'A', 7, 13},  // SD (TX/out, Block A)
    {'B', 2, 13},  // SD_RX (RX/in, Block B) - UNVERIFIED, see note above
    {'B', 9, 13}   // MCLK
};

// Board-specific driver config for STM32WB55 - TX on SAI1 Block A, RX on
// SAI1 Block B (separate blocks, like H743/F723 - see the PinConfig note
// above re: SD_RX's unverified default).
const STM32SAIDriverConfig SAI_CONFIG = {
  SAI1_Block_A,        // sai_block_tx
  DMA1_Channel1,       // dma_tx_instance
  DMA_REQUEST_SAI1_A,  // dma_tx_request
  DMA1_Channel1_IRQn,  // dma_tx_irq
  SAI1_Block_B,        // sai_block_rx
  DMA1_Channel2,       // dma_rx_instance
  DMA_REQUEST_SAI1_B,  // dma_rx_request
  DMA1_Channel2_IRQn,  // dma_rx_irq
  WB55_SAI_PINS,       // defaultPins
  sizeof(WB55_SAI_PINS) / sizeof(PinConfig),  // numPins
    [](uint32_t sample_rate) {
      // enableSAIClocks: WB55 routes SAI1 straight from the main PLL with no
      // per-sample-rate divider selection, so sample_rate is unused here.
      STM32AudioLogger::instance().debug("enable SAI clocks");
      RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
      PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
      PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL;
      if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        STM32AudioLogger::instance().error("SAI1 clock configuration failed");
      }
      __HAL_RCC_SAI1_CLK_ENABLE();
    },
    []() {
      // disableSAIClocks
      STM32AudioLogger::instance().debug("disable clocks");
      __HAL_RCC_SAI1_CLK_DISABLE();
    },
    []() {
      // enableDMAClocks
      STM32AudioLogger::instance().debug("enable DMA clocks");
      __HAL_RCC_DMAMUX1_CLK_ENABLE();
      __HAL_RCC_DMA1_CLK_ENABLE();
    },
    []() {
      // disableDMAClocks
      STM32AudioLogger::instance().debug("disable clocks");
      __HAL_RCC_DMA1_CLK_DISABLE();
      __HAL_RCC_DMAMUX1_CLK_DISABLE();
    }};

#endif