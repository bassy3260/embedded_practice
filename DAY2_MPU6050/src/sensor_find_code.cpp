#include <Arduino.h>
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);        // SDA=D21, SCL=D22
  Serial.println("I2C 스캔 시작");
}
void loop() {
  byte found = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("찾음: 0x");
      Serial.println(a, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("못 찾음 → 배선 확인");
  delay(2000);
}