#include <Arduino.h>
#include <Wire.h>

const int MPU = 0x68;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);        // SDA=21, SCL=22
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);          // 전원관리 레지스터
  Wire.write(0);             // 절전모드 해제
  Wire.endTransmission(true);
  Serial.println("MPU-6050 6축 시작");
}

void loop() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);          // 가속도부터 순서대로 읽기 시작
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 14, true);   // 가속도6 + 온도2 + 자이로6 = 14바이트

  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  int16_t rawT = Wire.read() << 8 | Wire.read();   // 온도
  int16_t gx = Wire.read() << 8 | Wire.read();
  int16_t gy = Wire.read() << 8 | Wire.read();
  int16_t gz = Wire.read() << 8 | Wire.read();

  float tempC = rawT / 340.0 + 36.53;   // 데이터시트 공식

  Serial.print("가속도(g) X:"); Serial.print(ax / 16384.0, 2);
  Serial.print(" Y:"); Serial.print(ay / 16384.0, 2);
  Serial.print(" Z:"); Serial.print(az / 16384.0, 2);

  Serial.print(" | 자이로(°/s) X:"); Serial.print(gx / 131.0, 1);
  Serial.print(" Y:"); Serial.print(gy / 131.0, 1);
  Serial.print(" Z:"); Serial.print(gz / 131.0, 1);

  Serial.print(" | 온도:"); Serial.print(tempC, 1); Serial.println("C");

  delay(300);
}