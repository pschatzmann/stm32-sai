#include <STM32AudioSAI.h>

void setup() {
  Serial.begin(115200);
  while(!Serial);

  SAI.setMode(STM32AudioSAI::Input);
  // SAI.setSampleRate(44100);
  // SAI.setBitsPerSample(16);
  // SAI.setChannels(2);
  // SAI.setProtocol(STM32AudioSAI::I2S);
  // SAI.setMaster(true);
  // SAI.setDataFormat(STM32AudioSAI::Standard);

  if (SAI.begin()) {
    Serial.println("SAI audio input started");
  } else {
    Serial.println("SAI audio failed to start");
  }
}

void loop() {
  // Example: read audio data
  int16_t buffer[256];
  size_t result = SAI.readBytes((uint8_t*)buffer, sizeof(buffer));
  // Print all samples to Serial
  for (size_t i = 0; i < result / sizeof(int16_t); i+=2) {
    Serial.print(buffer[i]);
    Serial.print(", ");
    Serial.println(buffer[i+1]);
  }
}
