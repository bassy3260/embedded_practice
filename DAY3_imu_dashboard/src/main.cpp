#include <Arduino.h>
#include <Wire.h>

const int MPU = 0x68;

// 상보필터 계수: 1에 가까울수록 자이로(적분값)를 더 믿음 -> 드리프트는 줄지만 반응이 느려짐
// 0에 가까울수록 가속도값을 더 믿음 -> 반응은 빠르지만 흔들림(노이즈)에 약함
const float ALPHA = 0.96f;

float roll = 0, pitch = 0, yaw = 0;   // 단위: deg
unsigned long lastMicros = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);        // SDA=21, SCL=22
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);          // 전원관리 레지스터
  Wire.write(0);             // 절전모드 해제
  Wire.endTransmission(true);

  lastMicros = micros();
}

void loop() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);          // 가속도부터 순서대로 읽기 시작
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 14, true);   // 가속도6 + 온도2 + 자이로6 = 14바이트

  int16_t rawAx = Wire.read() << 8 | Wire.read();
  int16_t rawAy = Wire.read() << 8 | Wire.read();
  int16_t rawAz = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();          // 온도는 이번엔 사용 안 함
  int16_t rawGx = Wire.read() << 8 | Wire.read();
  int16_t rawGy = Wire.read() << 8 | Wire.read();
  int16_t rawGz = Wire.read() << 8 | Wire.read();

  float ax = rawAx / 16384.0;        // g
  float ay = rawAy / 16384.0;
  float az = rawAz / 16384.0;
  float gx = rawGx / 131.0;          // deg/s
  float gy = rawGy / 131.0;
  float gz = rawGz / 131.0;

  unsigned long now = micros();
  float dt = (now - lastMicros) / 1000000.0f;
  lastMicros = now;

  // 1) 가속도만으로 구한 각도 (중력 방향 기준, 정적일 때 정확함)
  float rollAcc  = atan2(ay, az) * 180.0 / PI;
  float pitchAcc = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;

  // 1.5) 가속도 크기가 1g에서 많이 벗어나면 "지금은 움직이는 중"이라는 뜻.
  //      그때는 가속도값이 중력 방향을 제대로 못 재므로, 가속도 보정 비중을 0으로 낮춰서
  //      휙 뒤집을 때 값이 튀는 걸 줄인다. (정적일 때는 accMag ≈ 1.0)
  float accMag = sqrt(ax * ax + ay * ay + az * az);
  float accelWeight = (fabs(accMag - 1.0f) < 0.2f) ? (1 - ALPHA) : 0.0f;

  // 2) 상보필터: "자이로 적분값"과 "가속도 각도"를 가중합
  roll  = (roll  + gx * dt) * (1 - accelWeight) + rollAcc  * accelWeight;
  pitch = (pitch + gy * dt) * (1 - accelWeight) + pitchAcc * accelWeight;

  // yaw는 지자기(나침반) 센서가 없으면 자이로 적분만 가능 -> 시간이 지나면 서서히 틀어짐(drift)
  // 참고용으로만 사용 (정면 방향의 절대 기준은 아님)
  yaw += gz * dt;

  Serial.print(roll, 2);
  Serial.print(',');
  Serial.print(pitch, 2);
  Serial.print(',');
  Serial.println(yaw, 2);

  delay(20);   // 약 50Hz로 전송 -> 대시보드가 부드럽게 움직임
}
