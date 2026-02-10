#include "STM32AudioSAI.h"
#include "STM32DriverH743.h"

// DMA transfer complete flags (set in driver-specific DMA interrupt handler)
volatile bool dmaTxTransferComplete = false;
volatile bool dmaRxTransferComplete = false;
// RingBuffer instances are now members of STM32AudioSAI

// Board config and driver instance are now provided by the board-specific
// header
bool STM32AudioSAI::begin() {
  if (!configureGPIO()) {
    Logger::instance().error("GPIO configuration failed");
    return false;
  }
  // setup tx buffer with consistent block size for DMA writes based on buffer
  // ensure block size is a multiple of frame size
  int frameSize = channels * (bitsPerSample / 8);
  size_t dmaBlockSize = txBuffer.getBufferSize() / frameSize * frameSize;
  txBuffer.resize(dmaBlockSize);

  initSAI();
  initDMA();
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
  size_t written = 0;
  for (int i = 0; i < size; ++i) {
    written += write(buffer[i]);
  }
  return written;
}

void STM32AudioSAI::flush() {
  // Send any remaining data via DMA, zero-pad if needed
  if (txBuffer.available() > 0) {
    driver.write(this, txBuffer.data(), txBuffer.available());
    txBuffer.clear();
  }
}

size_t STM32AudioSAI::readBytes(uint8_t* buffer, size_t size) {
  // use consistent block size for DMA reads based on buffer size, channels, and
  // bits per sample
  size_t dmaBlockSize =
      rxBuffer.getBufferSize() / 4 * channels * (bitsPerSample / 8);
  while (rxBuffer.availableForWrite() >= dmaBlockSize) {
    driver.read(this, (void*)(rxBuffer.data() + rxBuffer.available()),
                dmaBlockSize);
    rxBuffer.advanceWriteIndex(dmaBlockSize);
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
void STM32AudioSAI::setPin(PinId id, int8_t port, int8_t pin, int8_t af) {
  pins[id].port = port;
  pins[id].pin = pin;
  pins[id].af = af;
}
int8_t STM32AudioSAI::getPinPort(PinId id) const { return pins[id].port; }
int8_t STM32AudioSAI::getPinNumber(PinId id) const { return pins[id].pin; }
int8_t STM32AudioSAI::getPinAF(PinId id) const { return pins[id].af; }
bool STM32AudioSAI::isDMATransferComplete() const {
  return dmaRxTransferComplete && dmaTxTransferComplete;
}
void STM32AudioSAI::initSAI() { driver.initSAI(this); }
void STM32AudioSAI::deinitSAI() { driver.deinitSAI(); }
void STM32AudioSAI::initDMA() { driver.initDMA(this); }
void STM32AudioSAI::deinitDMA() { driver.deinitDMA(); }
bool STM32AudioSAI::configureGPIO() { return driver.configureGPIO(this); }

// define a global object for easy use in sketches
STM32AudioSAI SAI;