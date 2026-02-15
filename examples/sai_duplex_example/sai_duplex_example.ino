#include <STM32AudioSAI.h>

void setup() {
  Serial.begin(115200);
  while(!Serial);

  SAI.setMode(STM32AudioSAI::Duplex);
  SAI.setSampleRate(44100);
  SAI.setBitsPerSample(16);
  SAI.setChannels(2);
  SAI.setProtocol(STM32AudioSAI::I2S);
  SAI.setMaster(true);
  SAI.setDataFormat(STM32AudioSAI::Standard);
  SAI.configureGPIO();
  if (SAI.begin()) {
    Serial.println("SAI audio duplex started");
  } else {
    Serial.println("SAI audio failed to start");
  }
}

void loop() {
  // Example: duplex audio (read and write)
  uint16_t inBuffer[256];
  uint16_t outBuffer[256];
  size_t byte_count = SAI.read(inBuffer, sizeof(inBuffer));
  // Process inBuffer...
  SAI.write(outBuffer, byte_count);
}
