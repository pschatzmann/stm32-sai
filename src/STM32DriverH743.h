#pragma once

#ifdef STM32H743xx
#include "DriverConfig.h"
#include "PinConfig.h"
#include "stm32h7xx.h"
#include "stm32h7xx_hal_sai.h"

#define SAI_SUPPORT 1

/// Pin configuration for STM32H743 SAI1 Block A (TX) / Block B (RX).
/// The prior defaults in this header were shifted one role off; these values
/// match the ST pin database for SAI1.
static const PinConfig H743_SAI_DEFAULT_PINS[5] = {
  {digitalPinToPinName(PE5), 6},  // SCK
  {digitalPinToPinName(PE4), 6},  // FS
  {digitalPinToPinName(PE6), 6},  // SD (TX/out, Block A)
  {digitalPinToPinName(PE3), 6},  // SD_RX (RX/in, Block B)
  {digitalPinToPinName(PE2), 6}   // MCLK
};

static const SAIPinCandidate H743_SAI_ALLOWED_PINS[] = {
    {PinId::SCK, {digitalPinToPinName(PE5), 6}},
    {PinId::FS, {digitalPinToPinName(PE4), 6}},
    {PinId::SD, {digitalPinToPinName(PB2), 6}},
    {PinId::SD, {digitalPinToPinName(PC1), 6}},
    {PinId::SD, {digitalPinToPinName(PD6), 6}},
    {PinId::SD, {digitalPinToPinName(PE6), 6}},
    {PinId::SD_RX, {digitalPinToPinName(PE3), 6}},
    {PinId::SD_RX, {digitalPinToPinName(PF6), 6}},
    {PinId::MCLK, {digitalPinToPinName(PE2), 6}},
    {PinId::MCLK, {digitalPinToPinName(PG7), 6}},
};

/// Board-specific driver config for STM32H743
/// const gives this internal linkage per translation unit (C++ default for
/// namespace-scope const globals) - without it, both STM32AudioSAI.cpp and
/// this file's own .cpp (which each transitively include this header) would
/// get their own externally-linked SAI_CONFIG and fail to link as a
/// duplicate definition (matches the WB55 config's existing const usage).
const STM32SAIDriverConfig SAI_CONFIG = {
    SAI1_Block_A,                            // sai_block_tx
    DMA2_Stream0,                            // dma_tx_instance
    DMA_REQUEST_SAI1_A,                      // dma_tx_request
    DMA2_Stream0_IRQn,                       // dma_tx_irq
    SAI1_Block_B,                            // sai_block_rx
    DMA2_Stream1,                            // dma_rx_instance
    DMA_REQUEST_SAI1_B,                      // dma_rx_request
    DMA2_Stream1_IRQn,                       // dma_rx_irq
    H743_SAI_DEFAULT_PINS,                   // defaultPins
    sizeof(H743_SAI_DEFAULT_PINS) / sizeof(PinConfig),  // numPins
    H743_SAI_ALLOWED_PINS,                   // allowedPins
    sizeof(H743_SAI_ALLOWED_PINS) / sizeof(SAIPinCandidate),  // numAllowedPins
    [](uint32_t sample_rate) { __HAL_RCC_SAI1_CLK_ENABLE(); },   // enableSAIClocks
    []() { __HAL_RCC_SAI1_CLK_DISABLE(); },  // disableSAIClocks
    []() {
      // enableDMAClocks
      __HAL_RCC_DMA2_CLK_ENABLE();
      __HAL_RCC_DMAMUX1_CLK_ENABLE();
    },
    []() {
      // disableDMAClocks
      __HAL_RCC_DMA2_CLK_DISABLE();
      __HAL_RCC_DMAMUX1_CLK_DISABLE();
    }};

#endif