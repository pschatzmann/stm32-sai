#pragma once
#include <Arduino.h>

#include <vector>

using sai_pin_t = int16_t;  // Use int16_t to allow -1 for "use default"

// Optional user callback for DMA transfer complete
extern volatile bool dmaTxTransferComplete;
extern volatile bool dmaRxTransferComplete;

// Generic double buffer utility for DMA
class DoubleBuffer {
 public:
  DoubleBuffer(size_t sz = 1024)
      : bufferSize(sz),
        bufferA(sz),
        bufferB(sz),
        activeBuffer(0),
        fillLevel(0) {
    bufferReady[0] = bufferReady[1] = true;
    dmaRunning = false;
  }

  void resize(size_t sz) {
    bufferSize = sz;
    bufferA.resize(sz);
    bufferB.resize(sz);
    fillLevel = 0;
  }
  void swap() { activeBuffer = (activeBuffer == 0) ? 1 : 0; }
  uint8_t* getActiveBuffer() { return (activeBuffer == 0) ? bufferA.data() : bufferB.data(); }
  uint8_t* getInactiveBuffer() {
    return (activeBuffer == 0) ? bufferB.data() : bufferA.data();
  }
  size_t getBufferSize() const { return bufferSize; }
  size_t getFillLevel() const { return fillLevel; }
  void setFillLevel(size_t v) { fillLevel = v; }
  bool isBufferReady(int idx) const { return bufferReady[idx]; }
  void setBufferReady(int idx, bool v) { bufferReady[idx] = v; }
  int getActiveBufferIndex() const { return activeBuffer; }
  bool isDMARunning() const { return dmaRunning; }
  void setDMARunning(bool v) { dmaRunning = v; }

 private:
  size_t bufferSize;
  std::vector<uint8_t> bufferA;
  std::vector<uint8_t> bufferB;
  volatile int activeBuffer;
  size_t fillLevel;
  volatile bool bufferReady[2];
  volatile bool dmaRunning;
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
 * SAI.setPin(STM32AudioSAI::SCK, 'B', 3, 6); // Port B, Pin 3, AF6
 * SAI.begin();
 * @endcode
 */

class STM32AudioSAI : public Stream {
 public:
  /// Audio protocol types
  enum Protocol { Free = 0x00, PCM = 0x01, I2S = 0x02 };

  /// I2S Audio data format types
  enum DataFormat {
    Standard = 0x00,
    LeftJustified = 0x01,
    RightJustified = 0x02
  };

  /// SAI mode: input, output, or duplex (both)
  enum Mode { Input, Output, Duplex };

  /// Pin identifiers for SAI signals
  enum PinId { SCK, FS, SD, MCLK, NumPins };
  struct PinConfig {
    int8_t port;  ///< Port letter as ASCII ('A'=65, ...)
    int8_t pin;   ///< Pin number
    int8_t af;    ///< Alternate function
    PinConfig(int8_t pt = -1, int8_t pn = -1, int8_t a = -1)
        : port(pt), pin(pn), af(a) {}
  };

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
  size_t write(uint8_t b) override ;
  size_t write(const uint8_t* buffer, size_t size);
  /// Flush any partially filled buffer to DMA (pads with zeros if needed)
  void flush();
  /// Returns the number of bytes available to read from the RX buffer
  int available() override;
  /// Returns the number of bytes available for writing to the TX buffer
  int availableForWrite();
  /// Get pin configuration for a SAI signal
  PinConfig getPinConfig(PinId id) const { return pins[id]; }
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
  /// Set pin configuration for a SAI signal
  void setPin(PinId id, int8_t port, int8_t pin, int8_t af = -1);
  /// Get pin port for a SAI signal
  int8_t getPinPort(PinId id) const;
  /// Get pin number for a SAI signal
  int8_t getPinNumber(PinId id) const;
  /// Get pin alternate function for a SAI signal
  int8_t getPinAF(PinId id) const;
  /// Check if DMA transfer is complete
  bool isDMATransferComplete() const;

 private:
  Mode mode = Duplex;
  Protocol protocol = I2S;
  bool master = true;
  DataFormat dataFormat = Standard;
  uint32_t sampleRate = 44100;
  uint8_t bitsPerSample = 16;
  uint8_t channels = 2;
  uint32_t ioTimeoutMs = 1000;  ///< IO timeout in milliseconds
  PinConfig pins[NumPins];

  void initSAI();
  void deinitSAI();
  void initDMA();
  void deinitDMA();
  bool configureGPIO();
};

// define a global object for easy use in sketches
extern STM32AudioSAI SAI;
