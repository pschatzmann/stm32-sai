#include <STM32AudioSAI.h>

static float phase = 0.0f;

void setup() {
  Serial.begin(115200);
  if (SAI.begin()) {
    Serial.println("SAI audio output started");
  } else {
    Serial.println("SAI audio failed to start");
  }
}

int16_t nextSineSample(float freq, float sampleRate) {
  int16_t sample = (int16_t)(32767 * sinf(phase));
  phase += 2.0f * 3.14159265f * freq / sampleRate;
  if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
  return sample;
}

void loop() {
  int16_t buffer[256];
  for (size_t i = 0; i < 256; i += 2) {
    buffer[i] = nextSineSample(440.0f, 44100.0f);
    buffer[i + 1] = buffer[i];
  }
  SAI.write(buffer, sizeof(buffer));
}
