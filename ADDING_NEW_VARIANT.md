# Adding Support for a New STM32 Variant

This guide explains how to add a new board/MCU variant to this library.

## 1) Identify the target and constraints

Collect:
- MCU and package (for example: STM32H743ZI)
- Board name / Arduino variant
- Which SAI instance is wired (for example `SAI1` or `SAI2`)
- Whether TX/RX use the same block or separate blocks
- DMA instance/request/IRQ for TX and RX

## 2) Build the SAI pin map (required)

Create two datasets for the variant:
- **Default pins**: one `PinConfig` per `PinId`
- **Allowed pins**: all valid `(PinId, PinName, AF)` combinations

Use authoritative machine-readable sources when possible (STM32 pin database / CubeMX-derived data), not manual transcription.

`PinId` order is:
- `SCK`
- `FS`
- `SD` (Block A / TX)
- `SD_RX` (Block B / RX)
- `MCLK`

Notes:
- If a user calls `setPin(...)` with AF = `-1`, AF is auto-resolved from allowed pins.
- If user pin/AF is not in allowed pins, `configureGPIO()` rejects it and `begin()` fails.

## 3) Add a new board driver header

Create a new file in src, for example:
- `src/STM32DriverXXXX.h`

In that file:
1. Guard with target macro (`#ifdef STM32...`)
2. Define `SAI_SUPPORT 1`
3. Add default pin array:
   - `static const PinConfig XXXX_SAI_DEFAULT_PINS[5] = {...};`
4. Add allowed pin table:
   - `static const SAIPinCandidate XXXX_SAI_ALLOWED_PINS[] = {...};`
5. Define `const STM32SAIDriverConfig SAI_CONFIG = {...};`

Populate `STM32SAIDriverConfig` fields carefully:
- `sai_block_tx`, `sai_block_rx`
- `dma_tx_instance`, `dma_tx_request`, `dma_tx_irq`
- `dma_rx_instance`, `dma_rx_request`, `dma_rx_irq`
- `defaultPins`, `numPins`
- `allowedPins`, `numAllowedPins`
- clock/DMA enable/disable lambdas

## 4) Register the new driver header

Update [src/STM32DriverAll.h](src/STM32DriverAll.h) to include the new header so the variant can provide `SAI_CONFIG`.

## 5) Validate pin behavior

Expected behavior:
- `setPin(...)` returns `false` for invalid pin encoding
- valid pin with AF omitted should auto-pick AF from candidate table
- invalid `(PinId, PinName, AF)` should be rejected in `configureGPIO()`

Recommended checks:
- Output mode (`setMode(Output)`)
- Input mode (`setMode(Input)`)
- Duplex mode (`setMode(Duplex)`)
- If TX and RX use separate blocks, verify `SD` and `SD_RX` independently

## 6) Verify DMA/clock configuration

Confirm:
- TX and RX DMA can initialize and run
- IRQ handlers fire for the configured DMA channels/streams
- SAI peripheral clocks are enabled/disabled correctly
- sample-rate dependent clock setup (if needed) is handled in `enableSAIClocks`

## 7) Add or adapt examples

Ensure at least one example compiles/runs on the new board.
If pin routing is different from existing boards, include a short example with explicit `setPin(...)` calls.

## 8) Update documentation

Update [README.md](README.md):
- add the board to Supported Boards
- mention any important caveats (for example, package-dependent SAI pins)

## 9) Common pitfalls

- Swapped SAI roles (`MCLK_A` vs `SCK_A`, `SD_A` vs `SD_B`)
- Correct pin but wrong AF number
- Candidate list missing legitimate alternatives for the selected package
- TX/RX DMA request mismatch with selected SAI block
- Variant compiles but fails at runtime because GPIO clocks are not enabled

## Minimal checklist

- [ ] New `src/STM32DriverXXXX.h` exists
- [ ] `SAI_CONFIG` is fully defined
- [ ] Default pins are valid for target board
- [ ] Allowed pin table covers all intended SAI alternatives
- [ ] `src/STM32DriverAll.h` includes new header
- [ ] README updated
- [ ] Example verified on target
