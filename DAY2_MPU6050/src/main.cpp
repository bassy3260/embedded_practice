#include <Arduino.h>
#include <Wire.h>

const int MPU = 0x68;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);        // SDA=21, SCL=22
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);          // 전원관리 레지스터
  Wire.write(0);             // 절전모드 해제 (센서 깨우기)
  Wire.endTransmission(true);
  Serial.println("MPU-6050 시작");
}

void loop() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);          // 가속도 데이터 시작 주소
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);

  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();

  Serial.print("X: "); Serial.print(ax / 16384.0);
  Serial.print("  Y: "); Serial.print(ay / 16384.0);
  Serial.print("  Z: "); Serial.println(az / 16384.0);

  delay(300);
}