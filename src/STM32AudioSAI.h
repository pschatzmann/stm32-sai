#pragma once
#include <Arduino.h>
#include <vector>
#include "PinConfig.h"
#include "STM32AudioLogger.h"

/// Callback for DMA transfer complete
extern volatile bool dmaTxTransferComplete;
extern volatile bool dmaRxTransferComplete;
/// Set by HAL_SAI_ErrorCallback (see the per-board .cpp files) so a timed-out
/// write()/read() can report *why* the transfer never completed - previously
/// any SAI/DMA-level error was silently swallowed by the default weak
/// HAL_SAI_ErrorCallback stub, and only the generic "still in progress"
/// symptom on the *next* call ever got logged.
extern volatile uint32_t saiLastErrorCode;
/// Incremented in HAL_SAI_TxCpltCallback/HAL_SAI_ErrorCallback (ISR context,
/// so just a counter bump - no logging there). Read/print these from normal
/// (non-ISR) code, e.g. periodically from loop(), to tell apart "the DMA IRQ
/// never fires at all" from "it fires but something after it hangs" from
/// "it's firing continuously/storming" (which can starve SysTick/millis()
/// entirely if the IRQ priority is too high, freezing the whole system).
extern volatile uint32_t saiTxCpltCount;
extern volatile uint32_t saiTxErrorCount;

/// Generic buffer to support consistent read/write operations
class Buffer {
 public:
  Buffer(size_t sz = 1024) : buffer(sz), bufferSize(sz), count(0) {}

  void resize(size_t sz) {
    buffer.resize(sz);
    bufferSize = sz;
    count = 0;
  }

  bool write(const uint8_t data) {
    if (count < bufferSize) {
      buffer[count] = data;
      ++count;
      return true;
    }
    return false;
  }

  size_t readBytes(uint8_t* outBuffer, size_t size) {
    size_t toRead = min(size, count);
    memcpy(outBuffer, buffer.data(), toRead);
    memmove(buffer.data(), buffer.data() + toRead, count - toRead);
    count -= toRead;
    return toRead;
  }

  const uint8_t* data() { return buffer.data(); }

  size_t available() const { return count; }
  size_t availableForWrite() const { return bufferSize - count; }
  size_t getBufferSize() const { return bufferSize; }
  bool isFull() const { return count >= bufferSize; }
  bool isEmpty() const { return count == 0; }
  void clear() { count = 0; }
  void advanceWriteIndex(size_t n) { count += n; }

 protected:
  std::vector<uint8_t> buffer;
  size_t bufferSize;
  size_t count;
};

/**
 * @brief STM32AudioSAI provides a flexible Arduino-style API for STM32 SAI
 * audio.
 *
 * This class abstracts the STM32 SAI hardware, allowing configuration of
 * protocol, data format, sample rate, channel count, and flexible pin/alternate
 * function assignment for multiple STM32 variants.
 *
 * - Supports I2S, PCM, and Free protocols
 * - Allows runtime configuration of SAI mode, master/slave, and data format
 * - Pin and alternate function assignment per signal (SCK, FS, SD, MCLK)
 * - Board-specific driver logic is delegated at compile time
 * - Double-buffered DMA for efficient audio streaming
 * - Compatible with Arduino Stream/Print API for easy integration
 *
 * Example usage:
 * @code
 * SAI.setSampleRate(48000);
 * SAI.setChannels(2);
 * SAI.setPin(STM32AudioSAI::SCK, PB3, 6); // Arduino PinName, AF6
 * SAI.begin();
 * @endcode
 *
 * @author Phil Schatzmann
 * @copyright MIT License
 */

class STM32AudioSAI : public Stream {
 public:
  /// Audio protocol types. PCM and TDM both use a single-pulse frame sync
  /// (one FS pulse per frame, not one toggle per channel like I2S) but
  /// differ in pulse width: PCM uses a fixed 13-bit-clock "long frame" pulse
  /// (classic PCM highway protocol - needs FrameLength > 13, i.e.
  /// wide-enough/enough slots), TDM uses a 1-bit-clock "short frame" pulse
  /// (the standard wiring for multi-channel TDM codecs/ADCs/DACs, and the
  /// one to use when PCM's fixed pulse width doesn't fit). Combine TDM with
  /// setChannels() (== slot count for a plain N-channel TDM device) or
  /// setSlotCount()/setActiveSlots() if the device needs more slots than
  /// active channels.
  enum Protocol { Free = 0x00, PCM = 0x01, I2S = 0x02, TDM = 0x03 };

  /// I2S Audio data format types
  enum DataFormat {
    Standard = 0x00,
    LeftJustified = 0x01,
    RightJustified = 0x02
  };

  /// SAI mode: input, output, or duplex (both)
  enum Mode { Input, Output, Duplex };

  /// Constructor
  STM32AudioSAI() = default;
  /// Deleted copy constructor
  STM32AudioSAI(const STM32AudioSAI&) = delete;
  /// Deleted assignment operator
  void operator=(const STM32AudioSAI&) = delete;

  /// Initialize SAI and DMA
  bool begin();
  /// Deinitialize SAI and DMA
  void end();
  /// Read audio data (blocking or non-blocking)
  size_t readBytes(uint8_t* buffer, size_t size);
  /// Write audio data (blocking or non-blocking, single buffer, accumulates
  /// until 1024 bytes)
  int read() override;
  /// not implemented
  int peek() override { return -1; }
  // Write single byte via buffer
  size_t write(uint8_t b) override;
  /// Write multiple bytes via buffer
  size_t write(const uint8_t* buffer, size_t size);
  /// Flush any partially filled buffer to DMA (pads with zeros if needed)
  void flush();
  /// Returns the number of bytes available to read from the RX buffer
  int available() override;
  /// Returns the number of bytes available for writing to the TX buffer
  int availableForWrite() override;
  /// Set audio sample rate
  void setSampleRate(uint32_t rate);
  /// Get audio sample rate
  uint32_t getSampleRate() const;
  /// Set bits per audio sample
  void setBitsPerSample(uint8_t bits);
  /// Get bits per audio sample
  uint8_t getBitsPerSample() const;
  /// Set number of audio channels
  void setChannels(uint8_t ch);
  /// Get number of audio channels
  uint8_t getChannels() const;
  /// Set audio protocol (I2S, PCM, etc)
  void setProtocol(Protocol p);
  /// Get audio protocol
  Protocol getProtocol() const;
  /// Set SAI master/slave mode
  void setMaster(bool m);
  /// Get SAI master/slave mode
  bool isMaster() const;
  /// Set audio data format
  void setDataFormat(DataFormat f);
  /// Get audio data format
  DataFormat getDataFormat() const;
  /// Check if SAI is running
  bool isRunning() const;
  /// Set SAI mode (input/output/duplex)
  void setMode(Mode m);
  /// Get SAI mode
  Mode getMode() const;
  /// Set IO timeout in ms (0 = non-blocking)
  void setIOTimoutMs(uint32_t ms);
  /// Get IO timeout in ms
  uint32_t getIOTimoutMs() const;
  /// Set pin configuration for a SAI signal using an Arduino PinName.
  /// @return true when the supplied pin encoding is valid, false otherwise.
  bool setPin(PinId id, PinName pin, int8_t af = -1);
  /// Backwards-compatible setter using legacy port/pin values.
  /// @return true when the supplied pin encoding is valid, false otherwise.
  bool setPin(PinId id, int8_t port, int8_t pin, int8_t af = -1);
  /// ESP32-style convenience API: bclk, ws, dout, din, mclk.
  /// Use -1 for unused pins. Must be called before begin().
  bool setPins(int bclk, int ws, int dout, int din = -1, int mclk = -1);
  /// Get pin port for a SAI signal
  int8_t getPinPort(PinId id) const;
  /// Get pin number for a SAI signal
  int8_t getPinNumber(PinId id) const;
  /// Get pin alternate function for a SAI signal
  int8_t getPinAF(PinId id) const;
  /// Check if DMA transfer is complete
  bool isDMATransferComplete() const;
  /// Override the SAI frame's slot count - only needed when the TDM device
  /// has more slots in its frame than active audio channels, e.g. the
  /// STM32F723E-Discovery's WM8994, which needs a 4-slot frame even for
  /// 2-channel audio. For a plain N-channel TDM device, slot count == channel
  /// count and this doesn't need to be called. 0 (default) means "use
  /// getChannels()", matching every board's prior behavior.
  void setSlotCount(uint8_t count) { slotCount = count; }
  /// 0 (unset) resolves to channels, preserving existing behavior
  uint8_t getSlotCount() const { return slotCount == 0 ? channels : slotCount; }
  /// Override which of the frame's slots actually carry data (SAI_SLOTACTIVE_x
  /// bitmask) - e.g. only slots 0+2 for the WM8994's headphone-only TDM
  /// routing. Only needed alongside setSlotCount() when active channels don't
  /// occupy the first N slots. 0 (default) means "slots 0..channels-1",
  /// matching every board's prior behavior.
  void setActiveSlots(uint32_t mask) { activeSlots = mask; }
  /// 0 (unset) resolves to the first `channels` slots (0b1, 0b11, 0b111, ...)
  uint32_t getActiveSlots() const {
    if (activeSlots != 0) return activeSlots;
    return (1u << channels) - 1u;
  }
  /// Get the pin configuration for a given PinId
  PinConfig getPinConfig(PinId id) const {
    return pins[static_cast<size_t>(id)];
  }

  void setLogLevel(STM32AudioLogger::Level level) {
    STM32AudioLogger::instance().setLevel(level);
  }

 protected:
  Mode mode = Duplex;
  Protocol protocol = I2S;
  bool master = true;
  DataFormat dataFormat = Standard;
  uint32_t sampleRate = 44100;
  uint8_t bitsPerSample = 16;
  uint8_t channels = 2;
  uint32_t ioTimeoutMs = 1000;  ///< IO timeout in milliseconds
  uint8_t slotCount = 0;        ///< 0 = use channels (see getSlotCount())
  uint32_t activeSlots = 0;     ///< 0 = use channels (see getActiveSlots())
  PinConfig pins[static_cast<size_t>(PinId::NumPins)];
  Buffer txBuffer{512};
  Buffer rxBuffer{2048};
  std::vector<uint8_t> rxRawBuffer;  ///< scratch space for TDM slot stripping
                                      ///< in readBytes() (see .cpp)

  bool initSAI();
  void deinitSAI();
  bool initDMA();
  void deinitDMA();
  bool configureGPIO();
};

// define a global object for easy use in sketches
extern STM32AudioSAI SAI;
