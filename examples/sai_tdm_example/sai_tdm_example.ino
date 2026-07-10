#include <STM32AudioSAI.h>

// Example: generic multi-channel TDM playback (e.g. a multi-channel TDM
// DAC/amplifier bank or audio codec wired for TDM instead of I2S).
//
// TDM uses a single-pulse frame sync (SAI_FS_STARTFRAME) instead of I2S's
// per-channel toggling FS - just set the protocol to TDM and the channel
// count to the device's slot count. setSlotCount()/setActiveSlots() are
// only needed if the device has *more* slots in its frame than active audio
// channels (see STM32AudioSAI::setSlotCount() for that case, e.g. a codec
// that only uses 2 of its 4 TDM slots).

static float phase = 0.0f;
static const uint8_t kChannels = 8;
static const uint32_t kSampleRate = 48000;
static const size_t kFramesPerWrite = 32;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SAI.setMode(STM32AudioSAI::Output);
  SAI.setProtocol(STM32AudioSAI::TDM);
  SAI.setSampleRate(kSampleRate);
  SAI.setBitsPerSample(16);
  SAI.setChannels(kChannels);  // one TDM slot per channel
  SAI.setMaster(true);
  // SAI.setSlotCount(16);              // only if the frame has more slots
  // SAI.setActiveSlots(0x00FF);        // ...than active channels

  if (SAI.begin()) {
    Serial.println("SAI TDM output started");
  } else {
    Serial.println("SAI TDM output failed to start");
    while (true);
  }
}

int16_t nextSineSample(float freq, float sampleRate) {
  int16_t sample = (int16_t)(32767 * sinf(phase));
  phase += 2.0f * 3.14159265f * freq / sampleRate;
  if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
  return sample;
}

void loop() {
  // kFramesPerWrite frames, kChannels interleaved slots each
  int16_t buffer[kFramesPerWrite * kChannels];
  for (size_t frame = 0; frame < kFramesPerWrite; ++frame) {
    int16_t sample = nextSineSample(440.0f, kSampleRate);
    for (uint8_t ch = 0; ch < kChannels; ++ch) {
      buffer[frame * kChannels + ch] = sample;
    }
  }
  SAI.write((const uint8_t*)buffer, sizeof(buffer));
}
