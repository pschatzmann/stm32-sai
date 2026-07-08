#pragma once

#ifdef STM32F723xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"

#define SAI_SUPPORT 1

// Pin configuration for the STM32F723E-Discovery's WM8994 codec on SAI2.
// Matches the ST BSP (stm32f723e_discovery_audio.h): Block A = TX/out on
// PI4 (MCLK)/PI5 (SCK)/PI6 (SD)/PI7 (FS), all AF10 (GPIO_AF10_SAI2). Block
// B = RX/in, SD on PG10 - not listed here since this library's PinConfig
// only has one SD slot (see PinId in PinConfig.h), so only output mode is
// supported until that's extended with a separate RX-data PinId.
static const PinConfig F723_SAI_PINS[4] = {
    {'I', 5, 10},  // SCK
    {'I', 7, 10},  // FS
    {'I', 6, 10},  // SD (TX/out)
    {'I', 4, 10}   // MCLK
};

/// Board-specific driver config for the STM32F723E-Discovery (SAI2, output
/// only - see the PinConfig note above for why RX/duplex isn't wired up).
/// enableSAIClocks below picks the right PLLI2S profile per sample-rate
/// family, matching the ST BSP.
/// const gives this internal linkage per translation unit (C++ default for
/// namespace-scope const globals) - both STM32AudioSAI.cpp and this file's
/// own .cpp transitively include this header, so without const each would
/// get its own externally-linked SAI_CONFIG and the linker would reject it
/// as a duplicate definition.
const STM32SAIDriverConfig SAI_CONFIG = {
    SAI2_Block_A,                            // sai_block_tx
    DMA2_Stream4,                            // dma_tx_instance
    DMA_CHANNEL_3,                           // dma_tx_request (-> Init.Channel, no DMAMUX on F7)
    DMA2_Stream4_IRQn,                       // dma_tx_irq
    SAI2_Block_B,                            // sai_block_rx
    DMA2_Stream6,                            // dma_rx_instance
    DMA_CHANNEL_3,                           // dma_rx_request (-> Init.Channel)
    DMA2_Stream6_IRQn,                       // dma_rx_irq
    F723_SAI_PINS,                           // defaultPins
    sizeof(F723_SAI_PINS) / sizeof(PinConfig),  // numPins
    [](uint32_t sample_rate) {
      // enableSAIClocks: route SAI2 from PLLI2S, per the ST BSP's
      // BSP_AUDIO_OUT_ClockConfig(). The BSP uses one PLLI2S profile for the
      // 44.1kHz sample-rate family (11025/22050/44100 Hz) and a different
      // one for the 48kHz family (8000/16000/32000/48000/96000 Hz) - pick
      // between them based on the actual requested sample_rate instead of
      // hardcoding one, so both families get an accurate audio clock.
      STM32AudioLogger::instance().debugf("enable SAI2 clocks for %u Hz",
                                           (unsigned)sample_rate);
      RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
      PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
      PeriphClkInitStruct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLI2S;
      switch (sample_rate) {
        case 11025:
        case 22050:
        case 44100:
          // PLLSAI_VCO/PLLI2SQ = 429/2 = 214.5MHz, /PLLI2SDivQ(19) = 11.289MHz
          PeriphClkInitStruct.PLLI2S.PLLI2SN = 429;
          PeriphClkInitStruct.PLLI2S.PLLI2SQ = 2;
          PeriphClkInitStruct.PLLI2SDivQ = 19;
          break;
        case 8000:
        case 16000:
        case 32000:
        case 48000:
        case 96000:
        default:
          // PLLSAI_VCO/PLLI2SQ = 344/7 = 49.142MHz, /PLLI2SDivQ(1) = 49.142MHz
          PeriphClkInitStruct.PLLI2S.PLLI2SN = 344;
          PeriphClkInitStruct.PLLI2S.PLLI2SQ = 7;
          PeriphClkInitStruct.PLLI2SDivQ = 1;
          break;
      }
      if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        STM32AudioLogger::instance().error("SAI2 clock configuration failed");
      }
      __HAL_RCC_SAI2_CLK_ENABLE();
    },
    []() {
      // disableSAIClocks
      STM32AudioLogger::instance().debug("disable SAI2 clocks");
      __HAL_RCC_SAI2_CLK_DISABLE();
    },
    []() {
      // enableDMAClocks
      STM32AudioLogger::instance().debug("enable DMA2 clocks");
      __HAL_RCC_DMA2_CLK_ENABLE();
    },
    []() {
      // disableDMAClocks
      STM32AudioLogger::instance().debug("disable DMA2 clocks");
      __HAL_RCC_DMA2_CLK_DISABLE();
    }};

#endif
