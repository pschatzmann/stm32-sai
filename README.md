# STM32 SAI Audio Arduino Library

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://mit-license.org/)

Many STM microcontrollers provide the SAI API for Audio Processing.
This is the most capable and flexible audio peripheral. Supports I2S, PCM, TDM, AC'97, and free protocol modes. It has two semi-independent blocks (Block A / Block B) that can be configured as master/slave or TX/RX pairs and it is available on mid-to-high-end parts (F4, F7, H7, WB55, etc.).

This project provides a high level, flexible, robust STM32 SAI audio library for Arduino with DMA, runtime configuration, and strong diagnostics. Now uses a unified, config-driven driver and a simple buffer for audio streaming.

## Features

- DMA for both read and write (low-latency, high-throughput audio)
- Simple, reusable `Buffer` class for audio data management
- Robust error handling and diagnostics with singleton `Logger` (log levels, Print output)
- HAL-based DMA completion and callback handling (no manual IRQ flag logic)
- Unified, config-driven driver for all supported boards (STM32WB55, STM32H743, STM32F723; easily extendable)
- I2S, PCM, TDM, and Free protocol support
- Master/slave, input/output/duplex modes
- Runtime configuration: sample rate, bits per sample, channels
- Per-pin port/pin/AF assignment for SAI signals (SCK, FS, SD, MCLK)
- DMA transfer timeout handling
- All configuration and buffer logic encapsulated in public API

## Supported Boards

This library currently supports:

- STM32WB55 series
- STM32H743 series
- STM32F723 series (default config targets SAI2 on the F723 Discovery; TX/RX use separate SAI blocks, and pin routing can be overridden via `setPin()`/`setPins()`)

Other STM32 boards with SAI/I2S hardware can be supported by adding a board config and updating the driver config table. Contributions for additional boards are welcome!

For porting details, see [ADDING_NEW_VARIANT.md](ADDING_NEW_VARIANT.md).

## Getting Started

1. Copy or clone this library into your Arduino `libraries` folder.
2. Open the Arduino IDE and select your STM32 board.
3. See the `examples/` folder for usage.

## Example

```cpp
#include <STM32AudioSAI.h>

void setup() {
  SAI.setSampleRate(48000);
  SAI.setChannels(2);
  SAI.setBitsPerSample(16);
  SAI.setProtocol(STM32AudioSAI::I2S);
  // ESP32-style convenience mapping: bclk, ws, dout, din, mclk.
  // Use -1 for unused pins. Must be called before begin().
  // SAI.setPins(bclk, ws, dout, din, mclk);
  // Optional: override one of the board-supported SAI pins.
  // If the AF is omitted, the library resolves it from the board pin table.
  if (!SAI.begin()) {
    STM32AudioLogger::instance().error("SAI initialization failed!");
    while (1); // Halt on error
  }
}

void loop() {
  // Audio processing here
  // Example: write audio data
  // SAI.write(buffer, size);
  // Example: read audio data
  // SAI.readBytes(buffer, size);
}
```

## TDM

For a multi-channel TDM codec/ADC/DAC, set the protocol to `TDM` and the
channel count to the device's slot count - TDM uses a single-pulse frame
sync per frame instead of I2S's per-channel toggling FS:

```cpp
SAI.setProtocol(STM32AudioSAI::TDM);
SAI.setChannels(8);  // one TDM slot per channel
```

If the device's frame has more slots than active audio channels (e.g. a
codec that only uses 2 of its 4 TDM slots), also call `setSlotCount()` and
`setActiveSlots()` - see `examples/sai_tdm_example`.

`PCM` is the classic PCM highway protocol: same single-pulse frame sync as
TDM, but a fixed 13-bit-clock pulse width instead of TDM's 1-bit-clock pulse,
which only fits when the frame is wide enough (`bitsPerSample * slotCount >
13`). Use `TDM` instead of `PCM` for narrow or few slots.

## API Overview

- `setSampleRate(uint32_t rate)`
- `setChannels(uint8_t ch)`
- `setBitsPerSample(uint8_t bits)`
- `setProtocol(Protocol p)` - `Free`, `PCM`, `I2S`, `TDM`
- `setMode(Mode m)` - `Input`, `Output`, `Duplex`
- `setMaster(bool m)`
- `setDataFormat(DataFormat f)` - `Standard`, `LeftJustified`, `RightJustified`
- `setSlotCount(uint8_t count)` / `setActiveSlots(uint32_t mask)` - TDM frames with more slots than active channels
- `bool setPins(int bclk, int ws, int dout, int din = -1, int mclk = -1)` - ESP32-style convenience API (`-1` disables an unused pin)
- `bool setPin(PinId id, PinName pin, int8_t af = -1)` - stores a pin override (`af = -1` means AF auto-detect during `begin()`)
- `bool setPin(PinId id, int8_t port, int8_t pin, int8_t af = -1)` - legacy compatibility overload
- `write(const uint8_t* buffer, size_t size)`
- `readBytes(uint8_t* buffer, size_t size)`
- `setIOTimoutMs(uint32_t ms)`
- `available()` / `availableForWrite()`
- `flush()`
- `isRunning()`
- `setLogLevel(STM32AudioLogger::Level level)`

## Error Handling & Logging

- All board drivers propagate errors from `configureGPIO()`; `begin()` returns `false` on failure
- User pin overrides are checked against board candidate pin tables during `begin()` (GPIO configuration stage)
- Diagnostics and warnings are logged via the singleton `Logger` (configurable log level, Print output)
- DMA transfer timeout returns 0 if not completed in time

## Notes

- Requires STM32 core for Arduino (STM32duino) and correct board selection
- Ensure your board supports SAI/I2S hardware
- Easily extensible for new STM32 variants: add a new board config and update the driver config table


## License

MIT
