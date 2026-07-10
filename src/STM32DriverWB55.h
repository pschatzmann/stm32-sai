#pragma once

#ifdef STM32WB55xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32wbxx.h"
#include "stm32wbxx_hal.h"

// Pin configuration for STM32WB55 SAI1 Block A (TX) / Block B (RX).
// These defaults and allowed candidates are derived from the ST CubeMX pin
// database. The earlier defaults in this header had the correct AF but not
// the correct signal-to-pin role mapping.
const PinConfig WB55_SAI_DEFAULT_PINS[5] = {
  {digitalPinToPinName(PA8), 13},   // SCK
  {digitalPinToPinName(PA9), 13},   // FS
  {digitalPinToPinName(PA10), 13},  // SD (TX/out, Block A)
  {digitalPinToPinName(PA5), 13},   // SD_RX (RX/in, Block B)
  {digitalPinToPinName(PA3), 13}    // MCLK
};

static const SAIPinCandidate WB55_SAI_ALLOWED_PINS[] = {
    {PinId::SCK, {digitalPinToPinName(PA8), 13}},
    {PinId::SCK, {digitalPinToPinName(PB10), 13}},
    {PinId::SCK, {digitalPinToPinName(PB13), 13}},
    {PinId::FS, {digitalPinToPinName(PA9), 13}},
    {PinId::FS, {digitalPinToPinName(PB9), 13}},
    {PinId::FS, {digitalPinToPinName(PB12), 13}},
    {PinId::SD, {digitalPinToPinName(PA10), 13}},
    {PinId::SD, {digitalPinToPinName(PB15), 13}},
    {PinId::SD, {digitalPinToPinName(PC3), 13}},
    {PinId::SD, {digitalPinToPinName(PD6), 13}},
    {PinId::SD_RX, {digitalPinToPinName(PA5), 13}},
    {PinId::SD_RX, {digitalPinToPinName(PA13), 13}},
    {PinId::SD_RX, {digitalPinToPinName(PB5), 13}},
    {PinId::MCLK, {digitalPinToPinName(PA3), 13}},
    {PinId::MCLK, {digitalPinToPinName(PB8), 13}},
    {PinId::MCLK, {digitalPinToPinName(PB14), 13}},
    {PinId::MCLK, {digitalPinToPinName(PE2), 13}},
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
  WB55_SAI_DEFAULT_PINS,       // defaultPins
  sizeof(WB55_SAI_DEFAULT_PINS) / sizeof(PinConfig),  // numPins
  WB55_SAI_ALLOWED_PINS,       // allowedPins
  sizeof(WB55_SAI_ALLOWED_PINS) / sizeof(SAIPinCandidate),  // numAllowedPins
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