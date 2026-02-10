#include <STM32AudioSAI.h>

void setup() {
    Serial.begin(115200);
    if (SAI.begin()) {
        Serial.println("SAI audio started");
    } else {
        Serial.println("SAI audio failed to start");
    }
}

void loop() {
    // Example: write audio data
    // SAI.write(buffer, size);
}
