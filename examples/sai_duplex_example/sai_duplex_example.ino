#include <STM32AudioSAI.h>

// Example: simultaneous audio in + out (e.g. a passthrough/echo effect).
// On boards where TX and RX are separate SAI blocks with separate data pins
// (e.g. the STM32F723E-Discovery's SAI2 Block A/Block B), begin() wires up
// both directions automatically - no extra pin setup needed.

uint8_t inBuffer[1024];

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SAI.setMode(STM32AudioSAI::Duplex);
  SAI.setSampleRate(44100);
  SAI.setBitsPerSample(16);
  SAI.setChannels(2);
  SAI.setProtocol(STM32AudioSAI::I2S);
  SAI.setMaster(true);
  SAI.setDataFormat(STM32AudioSAI::Standard);

  if (SAI.begin()) {
    Serial.println("SAI audio duplex started");
  } else {
    Serial.println("SAI audio failed to start");
  }
}

void loop() {
  // Example: read audio in, then write it straight back out (passthrough)
  size_t byte_count = SAI.readBytes(inBuffer, sizeof(inBuffer));
  if (byte_count > 0) {
    SAI.write(inBuffer, byte_count);
  }
}
