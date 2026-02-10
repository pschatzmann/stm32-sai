
# STM32 SAI Audio Arduino Library

Flexible, robust STM32 SAI/I2S audio library for Arduino with double-buffered DMA, runtime configuration, and strong diagnostics.

## Features
- Double-buffered DMA for both read and write (low-latency, high-throughput audio)
- Encapsulated, reusable `DoubleBuffer` class for buffer management
- Robust error handling and diagnostics with singleton `Logger` (log levels, Print output)
- Global DMA complete handlers for seamless buffer switching
- Board-specific driver abstraction (STM32WB55, STM32H743; easily extendable)
- I2S, PCM, and Free protocol support
- Master/slave, input/output/duplex modes
- Runtime configuration: sample rate, bits per sample, channels
- Per-pin port/pin/AF assignment for SAI signals (SCK, FS, SD, MCLK)
- DMA transfer timeout handling
- All configuration and buffer logic encapsulated in public API

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
  SAI.setPin(STM32AudioSAI::SCK, 'B', 3, 6); // Port B, Pin 3, AF6
  if (!SAI.begin()) {
    Logger::instance().error("SAI initialization failed!");
    while (1); // Halt on error
  }
}

void loop() {
  // Audio processing here
  // Example: write audio data
  // SAI.write(buffer, size);
}
```

## API Overview
- `setSampleRate(uint32_t rate)`
- `setChannels(uint8_t ch)`
- `setBitsPerSample(uint8_t bits)`
- `setProtocol(Protocol p)`
- `setMode(Mode m)`
- `setPin(PinId id, int8_t port, int8_t pin, int8_t af = -1)`
- `write(const void* buffer, size_t size)`
- `read(void* buffer, size_t size)`
- `setIOTimoutMs(uint32_t ms)`
- `available()` / `availableForWrite()`
- `flush()`
- `isRunning()`

## Error Handling & Logging
- All board drivers propagate errors from `configureGPIO()`; `begin()` returns `false` on failure
- Diagnostics and warnings are logged via the singleton `Logger` (configurable log level, Print output)
- DMA transfer timeout returns 0 if not completed in time

## Notes
- Requires STM32 core for Arduino (STM32duino) and correct board selection
- Ensure your board supports SAI/I2S hardware
- Easily extensible for new STM32 variants: add a new driver class and update board selection

## License
MIT
