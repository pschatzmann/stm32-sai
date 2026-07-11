#include "STM32AudioSAI.h"
#include "STM32DriverAll.h"
#include "STM32Driver.h"

// Global driver instance (configured in board-specific header)
STM32SAIDriver driver{SAI_CONFIG};

// DMA transfer complete flags (set in driver-specific DMA interrupt handler).
// Must start true: it really means "no transfer currently in flight", and
// write()/read() reject the call outright when it's false - starting it
// false meant the very first write()/read() ever made (before any transfer
// had a chance to complete and set it true) was always rejected, forever,
// since nothing else ever primes it back to true.
volatile bool dmaTxTransferComplete = true;
volatile bool dmaRxTransferComplete = true;
volatile uint32_t saiLastErrorCode = 0;
volatile uint32_t saiTxCpltCount = 0;
volatile uint32_t saiTxErrorCount = 0;
volatile int8_t saiTxFreeHalf = -1;
uint8_t* saiTxCircBufPtr = nullptr;
volatile size_t saiTxCircHalfBytes = 0;

// RingBuffer instances are now members of STM32AudioSAI

// Board config and driver instance are now provided by the board-specific
// header
bool STM32AudioSAI::begin() {
  if (!configureGPIO()) {
    STM32AudioLogger::instance().error("GPIO configuration failed");
    return false;
  }
  // TEMP DIAGNOSTIC: reverted to the old channels-based frameSize (matching
  // yesterday's working baseline) to isolate whether this change (or the
  // write() padding below) is interacting with the WM8994 register-write
  // regression - unclear why either would, since both only run after/during
  // playback, well after WM8994::init() has already run, but ActiveFrameLength
  // was already ruled out by direct A/B test so this is the next suspect.
  int frameSize = channels * (bitsPerSample / 8);
  size_t dmaBlockSize = txBuffer.getBufferSize() / frameSize * frameSize;
  txBuffer.resize(dmaBlockSize);

  if (!initSAI()) return false;
  if (!initDMA()) return false;

  return isRunning();
}

void STM32AudioSAI::end() { deinitSAI(); }

int STM32AudioSAI::available() { return rxBuffer.available(); }
int STM32AudioSAI::availableForWrite() { return txBuffer.availableForWrite(); }

int STM32AudioSAI::read() {
  uint8_t b;
  size_t n = readBytes(&b, 1);
  if (n == 1) return b;
  return -1;
}

size_t STM32AudioSAI::write(uint8_t b) {
  if (!txBuffer.write(b)) return 0;
  if (txBuffer.isFull()) {
    driver.write(this, txBuffer.data(), txBuffer.available());
    txBuffer.clear();
  }
  return 1;
}

size_t STM32AudioSAI::write(const uint8_t* buffer, size_t size) {
  uint8_t bytesPerSample = bitsPerSample / 8;
  uint8_t activeBytes = channels * bytesPerSample;
  uint8_t slotCountVal = getSlotCount();

  if (slotCountVal <= channels || activeBytes == 0) {
    // plain case (every board except TDM-wired ones like the F723
    // Discovery's WM8994): unchanged pass-through behavior.
    size_t written = 0;
    for (size_t i = 0; i < size; ++i) {
      written += write(buffer[i]);
    }
    return written;
  }

  // TDM padding: the SAI hardware pulls one FIFO word per *active* slot,
  // every frame. If we only ever feed `channels` real samples per frame,
  // the extra slots silently "steal" every-other real sample across slot
  // positions instead of getting silence, starving the real channels to
  // half their intended sample rate (audible as a distorted/aliased
  // signal) - insert zero bytes for the inactive slots so every hardware
  // frame gets a full, fresh set of samples. Returns the number of *input*
  // bytes actually consumed (excludes the padding bytes it also wrote).
  size_t consumed = 0;
  uint8_t paddedBytes = slotCountVal * bytesPerSample;
  while (consumed + activeBytes <= size) {
    for (uint8_t b = 0; b < activeBytes; ++b) {
      if (write(buffer[consumed + b]) == 0) return consumed;
    }
    for (uint8_t b = activeBytes; b < paddedBytes; ++b) {
      if (write((uint8_t)0) == 0) return consumed;
    }
    consumed += activeBytes;
  }
  return consumed;
}

void STM32AudioSAI::flush() {
  // Send any remaining data via DMA, zero-pad if needed
  if (txBuffer.available() > 0) {
    driver.write(this, txBuffer.data(), txBuffer.available());
    txBuffer.clear();
  }
}

size_t STM32AudioSAI::readBytes(uint8_t* buffer, size_t size) {
  uint8_t bytesPerSample = bitsPerSample / 8;
  uint8_t activeBytes = channels * bytesPerSample;
  uint8_t slotCountVal = getSlotCount();

  if (slotCountVal <= channels || activeBytes == 0) {
    // plain case (every board except TDM-wired ones with more slots than
    // active channels, e.g. the F723 Discovery's WM8994): unchanged
    // pass-through behavior, also covers the common "N-channel TDM device"
    // case where slot count == channel count.
    size_t dmaBlockSize = rxBuffer.getBufferSize() / 4 * activeBytes;
    while (rxBuffer.availableForWrite() >= dmaBlockSize) {
      driver.read(this, (void*)(rxBuffer.data() + rxBuffer.available()),
                  dmaBlockSize);
      rxBuffer.advanceWriteIndex(dmaBlockSize);
    }
    return rxBuffer.readBytes(buffer, size);
  }

  // TDM slot stripping: mirrors write()'s padding above. The SAI hardware
  // moves one FIFO word per *frame slot*, active or not (verified for TX on
  // the F723 Discovery's WM8994; assumed symmetric here for RX, since both
  // directions share the same frame/slot register configuration), so a raw
  // DMA block is slotCount-wide per frame - pull that raw, slotCount-wide
  // data into a scratch buffer, then keep only the first `channels` slots'
  // worth of bytes from every frame.
  uint8_t paddedBytes = slotCountVal * bytesPerSample;
  size_t framesPerBlock = rxBuffer.getBufferSize() / 4 / activeBytes;
  if (framesPerBlock == 0) framesPerBlock = 1;
  size_t rawBlockSize = framesPerBlock * paddedBytes;
  if (rxRawBuffer.size() != rawBlockSize) rxRawBuffer.resize(rawBlockSize);

  size_t activeBlockSize = framesPerBlock * activeBytes;
  while (rxBuffer.availableForWrite() >= activeBlockSize) {
    driver.read(this, rxRawBuffer.data(), rawBlockSize);
    for (size_t f = 0; f < framesPerBlock; ++f) {
      memcpy((void*)(rxBuffer.data() + rxBuffer.available()),
             rxRawBuffer.data() + f * paddedBytes, activeBytes);
      rxBuffer.advanceWriteIndex(activeBytes);
    }
  }
  return rxBuffer.readBytes(buffer, size);
}

void STM32AudioSAI::setSampleRate(uint32_t rate) { sampleRate = rate; }
uint32_t STM32AudioSAI::getSampleRate() const { return sampleRate; }
void STM32AudioSAI::setBitsPerSample(uint8_t bits) { bitsPerSample = bits; }
uint8_t STM32AudioSAI::getBitsPerSample() const { return bitsPerSample; }
void STM32AudioSAI::setChannels(uint8_t ch) { channels = ch; }
uint8_t STM32AudioSAI::getChannels() const { return channels; }
void STM32AudioSAI::setProtocol(Protocol p) { protocol = p; }
STM32AudioSAI::Protocol STM32AudioSAI::getProtocol() const { return protocol; }
void STM32AudioSAI::setMaster(bool m) { master = m; }
bool STM32AudioSAI::isMaster() const { return master; }
void STM32AudioSAI::setDataFormat(DataFormat f) { dataFormat = f; }
STM32AudioSAI::DataFormat STM32AudioSAI::getDataFormat() const {
  return dataFormat;
}
bool STM32AudioSAI::isRunning() const { return driver.isRunning(); }
void STM32AudioSAI::setMode(Mode m) { mode = m; }
STM32AudioSAI::Mode STM32AudioSAI::getMode() const { return mode; }
void STM32AudioSAI::setIOTimoutMs(uint32_t ms) { ioTimeoutMs = ms; }
uint32_t STM32AudioSAI::getIOTimoutMs() const { return ioTimeoutMs; }
bool STM32AudioSAI::setPin(PinId id, PinName pin, int8_t af) {
  if (pin == NC || !STM_VALID_PINNAME(pin)) {
    STM32AudioLogger::instance().error("setPin: invalid pin");
    return false;
  }
  pins[static_cast<size_t>(id)] = PinConfig(pin, af);
  return true;
}
bool STM32AudioSAI::setPin(PinId id, int8_t port, int8_t pin, int8_t af) {
  if (port < 'A' || port > 'K' || pin < 0 || pin > 15) {
    STM32AudioLogger::instance().error("setPin: invalid legacy port/pin");
    return false;
  }
  return setPin(id, static_cast<PinName>(((port - 'A') << 4) | (pin & 0x0F)), af);
}

bool STM32AudioSAI::setPins(int bclk, int ws, int dout, int din, int mclk) {
  auto applyArduinoPin = [&](PinId id, int pin) -> bool {
    if (pin < 0) {
      // Explicitly disabled/unused pin: do not fall back to board defaults.
      pins[static_cast<size_t>(id)] = PinConfig(NC, SAI_PIN_DISABLED_AF);
      return true;
    }
    return setPin(id, digitalPinToPinName(pin), -1);
  };

  bool ok = true;
  ok = applyArduinoPin(PinId::SCK, bclk) && ok;
  ok = applyArduinoPin(PinId::FS, ws) && ok;
  ok = applyArduinoPin(PinId::SD, dout) && ok;
  ok = applyArduinoPin(PinId::SD_RX, din) && ok;
  ok = applyArduinoPin(PinId::MCLK, mclk) && ok;
  return ok;
}
int8_t STM32AudioSAI::getPinPort(PinId id) const {
  PinName pin = pins[static_cast<size_t>(id)].pin;
  return pin == NC ? -1 : static_cast<int8_t>(STM_PORT(pin) + 'A');
}
int8_t STM32AudioSAI::getPinNumber(PinId id) const {
  PinName pin = pins[static_cast<size_t>(id)].pin;
  return pin == NC ? -1 : static_cast<int8_t>(STM_PIN(pin));
}
int8_t STM32AudioSAI::getPinAF(PinId id) const { return pins[static_cast<size_t>(id)].af; }
bool STM32AudioSAI::isDMATransferComplete() const {
  return dmaRxTransferComplete && dmaTxTransferComplete;
}
bool STM32AudioSAI::initSAI() { return driver.initSAI(this); }
void STM32AudioSAI::deinitSAI() { driver.deinitSAI(); }
bool STM32AudioSAI::initDMA() { return driver.initDMA(this); }
void STM32AudioSAI::deinitDMA() { driver.deinitDMA(); }
bool STM32AudioSAI::configureGPIO() { return driver.configureGPIO(this); }

// define a global object for easy use in sketches
STM32AudioSAI SAI;