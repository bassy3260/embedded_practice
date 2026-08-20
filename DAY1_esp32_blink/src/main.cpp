#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
    Serial.println("Blinking LED...");
    delay(1000);
}