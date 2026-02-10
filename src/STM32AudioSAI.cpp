#include "STM32AudioSAI.h"

// Board-specific SAI initialization
#if defined(STM32WB55xx)
#include "STM32DriverWB55.h"
#elif defined(STM32H743xx)
#include "STM32DriverH743.h"
#endif

// Global driver instance (selects correct implementation at compile time)
STM32SAIDriver driver;
// DMA transfer complete flag (set in driver-specific DMA interrupt handler)
volatile bool dmaTransferComplete = false;
DoubleBuffer txBuf;
DoubleBuffer rxBuf;

void handleDMATxComplete() {
  // Called from DMA complete callback (internal, not user)
  txBuf.swap();
  txBuf.setBufferReady(txBuf.getActiveBufferIndex(), true);
  // If both buffers are ready, DMA is idle
  if (txBuf.isBufferReady(0) && txBuf.isBufferReady(1)) {
    txBuf.setDMARunning(false);
  }
}

// DMA RX complete handler for double-buffered read
void handleDMARxComplete() {
  rxBuf.swap();
  rxBuf.setBufferReady(rxBuf.getActiveBufferIndex(), true);
  // If both buffers are ready, DMA is idle
  if (rxBuf.isBufferReady(0) && rxBuf.isBufferReady(1)) {
    rxBuf.setDMARunning(false);
  }
}

bool STM32AudioSAI::begin() {
  if (!configureGPIO()) {
    Logger::instance().error("GPIO configuration failed");
    return false;
  }
  initSAI();
  initDMA();
  return isRunning();
}

void STM32AudioSAI::end() { deinitSAI(); }
int STM32AudioSAI::available()  { return rxBuf.getFillLevel(); }
/// Returns the number of bytes available for writing to the TX buffer
int STM32AudioSAI::availableForWrite()  {
  return txBuf.getBufferSize() - txBuf.getFillLevel();
}
// Stream single-byte read implementation
int STM32AudioSAI::read() {
  uint8_t b;
  size_t n = readBytes(&b, 1);
  if (n == 1) return b;
  return -1;
}

// Stream single-byte write implementation
size_t STM32AudioSAI::write(uint8_t b) {
  return write(&b, 1);
}

// Double-buffered write implementation
size_t STM32AudioSAI::write(const uint8_t* buffer, size_t size) {
  size_t written = 0;
  // bufIdx: index of the inactive buffer (the one not being sent by DMA)
  int bufIdx = (txBuf.getActiveBufferIndex() == 0) ? 1 : 0;
  uint32_t start = millis();
  while (written < size) {
    // Wait until the inactive buffer is ready to be written to
    while (!txBuf.isBufferReady(bufIdx)) {
      if (ioTimeoutMs == 0) return written;  // No wait: return immediately
      if ((millis() - start) > ioTimeoutMs)
        return written;  // Timeout: return partial
      yield();
    }
    // Calculate available space in the inactive buffer
    size_t space = txBuf.getBufferSize() - txBuf.getFillLevel();
    // Copy as much as possible to the buffer
    size_t toCopy = min(space, size - written);
    memcpy(txBuf.getInactiveBuffer() + txBuf.getFillLevel(),
           (const uint8_t*)buffer + written, toCopy);
    txBuf.setFillLevel(txBuf.getFillLevel() + toCopy);
    written += toCopy;
    // If the buffer is now full, mark it as not ready and reset fill level
    if (txBuf.getFillLevel() == txBuf.getBufferSize()) {
      txBuf.setBufferReady(bufIdx, false);
      txBuf.setFillLevel(0);
      // If DMA is not running, start a new DMA transfer for both buffers
      if (!txBuf.isDMARunning()) {
        txBuf.setDMARunning(true);
        txBuf.setBufferReady(txBuf.getActiveBufferIndex(), false);
        driver.write(this, txBuf.getActiveBuffer(), txBuf.getInactiveBuffer(),
                     txBuf.getBufferSize() * 2);
      }
      // Switch to the other buffer for the next write
      bufIdx = (txBuf.getActiveBufferIndex() == 0) ? 1 : 0;
    }
  }
  return written;
}

// Flush any partially filled buffer by zero-padding and sending it via DMA
void STM32AudioSAI::flush() {
  // bufIdx: index of the inactive buffer (the one not being sent by DMA)
  int bufIdx = (txBuf.getActiveBufferIndex() == 0) ? 1 : 0;
  // Only flush if there is data in the buffer and it is ready
  if (txBuf.getFillLevel() > 0 && txBuf.isBufferReady(bufIdx)) {
    // Zero-pad the remainder of the buffer to ensure full buffer size
    memset(txBuf.getInactiveBuffer() + txBuf.getFillLevel(), 0,
           txBuf.getBufferSize() - txBuf.getFillLevel());
    // Mark the buffer as not ready (so it can't be written to)
    txBuf.setBufferReady(bufIdx, false);
    // Reset fill level for next use
    txBuf.setFillLevel(0);
    // If DMA is not running, start a new DMA transfer for both buffers
    if (!txBuf.isDMARunning()) {
      txBuf.setDMARunning(true);
      txBuf.setBufferReady(txBuf.getActiveBufferIndex(), false);
      driver.write(this, txBuf.getActiveBuffer(), txBuf.getInactiveBuffer(),
                   txBuf.getBufferSize() * 2);
    }
  }
}

// Double-buffered read implementation
size_t STM32AudioSAI::readBytes(uint8_t* buffer, size_t size) {
  size_t read = 0;
  // bufIdx: index of the inactive buffer (the one not being filled by DMA)
  int bufIdx = (rxBuf.getActiveBufferIndex() == 0) ? 1 : 0;
  uint32_t start = millis();
  while (read < size) {
    // Wait until the inactive buffer is ready to be read from
    while (!rxBuf.isBufferReady(bufIdx)) {
      if (ioTimeoutMs == 0) return read;  // No wait: return immediately
      if ((millis() - start) > ioTimeoutMs)
        return read;  // Timeout: return partial
      yield();
    }
    // Determine how much data is available in the buffer
    size_t available = rxBuf.getFillLevel();
    // Copy as much as possible to the output buffer
    size_t toCopy = min(available, size - read);
    memcpy((uint8_t*)buffer + read, rxBuf.getInactiveBuffer(), toCopy);
    // If only part of the buffer was read, move remaining data to the front
    if (toCopy < available) {
      memmove(rxBuf.getInactiveBuffer(), rxBuf.getInactiveBuffer() + toCopy,
              available - toCopy);
    }
    rxBuf.setFillLevel(available - toCopy);
    read += toCopy;
    // If buffer is now empty, mark as not ready and trigger DMA to refill
    if (rxBuf.getFillLevel() == 0) {
      rxBuf.setBufferReady(bufIdx, false);
      if (!rxBuf.isDMARunning()) {
        rxBuf.setDMARunning(true);
        rxBuf.setBufferReady(rxBuf.getActiveBufferIndex(), false);
        driver.read(this, rxBuf.getActiveBuffer(), rxBuf.getInactiveBuffer(),
                    rxBuf.getBufferSize() * 2);
      }
      // Switch to the other buffer for the next read
      bufIdx = (rxBuf.getActiveBufferIndex() == 0) ? 1 : 0;
    }
  }
  return read;
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
bool STM32AudioSAI::isRunning() const { return driver.isRunning(this); }
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
  return dmaTransferComplete;
}
void STM32AudioSAI::initSAI() { driver.initSAI(this); }
void STM32AudioSAI::deinitSAI() { driver.deinitSAI(this); }
void STM32AudioSAI::initDMA() { driver.initDMA(this); }
void STM32AudioSAI::deinitDMA() { driver.deinitDMA(this); }
bool STM32AudioSAI::configureGPIO() { return driver.configureGPIO(this); }

// define a global object for easy use in sketches
STM32AudioSAI SAI;