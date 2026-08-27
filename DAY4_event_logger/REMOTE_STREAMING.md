# 원격 스트리밍 옵션 — 같은 공유기 밖에서 센서 데이터 보기 (정리 문서, 미구현)

> DAY3 대화에서 나온 질문 정리: "MCU → 웹소켓 → 웹으로 데이터 주는 거는 같은 와이파이일 때만 되는 거야?"
> → "그럼 같은 공유기가 아니면 MQTT 같은 걸 쓰면 되나?"
> 아직 코드는 없고, 어떤 선택지가 있고 각각 언제 쓰는지만 정리해둔 상태.

---

## 1. 현재 DAY3 구조는 "같은 PC 전용"이다

DAY3는 WiFi를 아예 안 쓴다. 실제 경로는 이렇다.

```
ESP32 ──USB 시리얼(COM7)──► PC의 bridge/serial_to_ws.py ──ws://localhost:8765──► 같은 PC 브라우저
```

- `src/main.cpp` : `roll,pitch,yaw`를 **시리얼(USB 케이블)** 로만 출력
- `bridge/serial_to_ws.py` : 웹소켓 서버를 `"localhost"` 에만 바인딩
- `dashboard/index.html` : 브라우저도 `ws://localhost:8765` 로 접속

즉 지금은 "같은 와이파이"조차 아니고 **브리지와 브라우저가 같은 PC**, ESP32는 그 PC에 USB로 꽂혀 있어야만 동작한다.

---

## 2. 도달 범위별 정리

| 어디까지 보고 싶나 | 필요한 것 | 방법 |
|---|---|---|
| 같은 PC | (지금 그대로) | DAY3 구조 |
| **같은 공유기(LAN)** 안 다른 기기 | 서버를 `0.0.0.0`에 바인딩 + LAN IP로 접속 | 아래 **방법 A** |
| **인터넷 너머** (다른 네트워크) | 공용 IP를 가진 중개 지점 필요 (사설 IP끼리는 직접 연결 불가) | 아래 **방법 B / C / D** |

"같은 LAN"은 SSID가 2.4G/5G로 갈라져 있어도 **같은 공유기에 붙어 있으면** OK. 문제는 인터넷 너머로 나갈 때다.

---

## 3. 방법 A — ESP32가 직접 WiFi + 웹소켓 서버 (같은 LAN)

보드가 `esp32dev`라 **WiFi가 칩에 내장**돼 있다. 부품 추가 없이 펌웨어만 바꾸면 PC 브리지(`serial_to_ws.py`) 자체를 없앨 수 있다.

```
ESP32(웹소켓 서버) ──ws://<esp32-ip>:81──► 같은 공유기에 붙은 브라우저
```

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    links2004/WebSockets @ ^2.4.1
```

### main.cpp 델타

```cpp
#include <WiFi.h>
#include <WebSocketsServer.h>

const char* WIFI_SSID = "공유기_이름";
const char* WIFI_PASS = "비밀번호";
WebSocketsServer wsServer(81);          // ws://<esp32-ip>:81

// setup(): Serial.begin 다음
WiFi.begin(WIFI_SSID, WIFI_PASS);
while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print('.'); }
Serial.printf("\nWiFi OK  ws://%s:81\n", WiFi.localIP().toString().c_str());
wsServer.begin();

// loop(): 맨 위에
wsServer.loop();

// loop(): 기존 Serial.print(roll...) 자리 (대체 또는 병행)
char buf[64];
snprintf(buf, sizeof(buf), "{\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f}", roll, pitch, yaw);
wsServer.broadcastTXT(buf);
```

### dashboard/index.html 델타

```js
const WS_URL = "ws://192.168.0.XX:81";   // 시리얼 모니터에 찍힌 ESP32 IP
```

### 주의점

- **같은 공유기(LAN)** 안에서만 됨.
- 대시보드를 `file://` 또는 `http://` 로 열어야 함. `https://` 페이지는 `ws://`(비암호) 접속을 브라우저가 막는다 (mixed content).
- IP가 DHCP로 바뀌어 귀찮으면 `ESPmDNS` 로 `imu.local` 고정 가능.
- ESP32가 HTML까지 서빙하게 하면(`http://<esp32-ip>` 접속) 대시보드 파일도 필요 없어지는데, LittleFS 세팅이 더 붙는다. 위 방식이 최소 변경.

---

## 4. 방법 B — MQTT (over WebSocket), 인터넷 너머

인터넷 너머 문제에 맞는 도구. **ESP32도 브라우저도 브로커로 "나가는 연결(outbound)"만 하기 때문에** 공유기 포트포워딩이 필요 없다.

```
ESP32 ──publish──►  MQTT 브로커(공용 IP)  ◄──subscribe── 브라우저
        topic: imu/oreo/attitude
```

### 핵심 주의점

- **브라우저는 순수 MQTT(TCP)를 못 함** → 브로커가 **MQTT over WebSocket** 포트를 열어줘야 함 (보통 `8083`/`8084`, `wss://broker/mqtt`). ESP32는 일반 MQTT 포트(`1883`/`8883`).
- 공용이므로 **TLS(mqtts / wss) + 인증** 권장. 토픽 이름도 남이 못 맞추게 유니크하게.
- 50Hz 그대로 공용 브로커에 쏘면 과함 → **10~20Hz로 줄이는 게** 현실적. LAN보다 지연·지터도 크다.

### 브로커 선택

| 용도 | 브로커 |
|---|---|
| 잠깐 테스트 | `broker.emqx.io` / `test.mosquitto.org` (무인증, 공개됨 — 아무나 구독 가능) |
| 제대로 | **HiveMQ Cloud 무료 티어** (TLS+계정, WS 지원), 또는 VPS에 Mosquitto 직접 (`listener 9001` + `protocol websockets`) |

### ESP32 델타 (방법 A의 WebSocket 라이브러리 대신)

```ini
lib_deps = knolleary/PubSubClient @ ^2.8
```

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
WiFiClientSecure net;
PubSubClient mqtt(net);

// setup()
net.setInsecure();                                  // 데모용. 실제로는 루트 CA 심기
mqtt.setServer("xxxxx.s1.eu.hivemq.cloud", 8883);
mqtt.connect("esp32-oreo", "mqttUser", "mqttPass");

// loop(): 맨 위
mqtt.loop();
// 20Hz마다
mqtt.publish("imu/oreo/attitude", buf);             // buf = 그 JSON 문자열
```

### 브라우저 델타 (raw WebSocket 대신 mqtt.js)

```js
import mqtt from "https://unpkg.com/mqtt/dist/mqtt.esm.js";
const client = mqtt.connect("wss://xxxxx.s1.eu.hivemq.cloud:8884/mqtt", { username, password });
client.on("connect", () => client.subscribe("imu/oreo/attitude"));
client.on("message", (t, payload) => {
  const { roll, pitch, yaw } = JSON.parse(payload.toString());
  // 기존 3D 렌더 코드 재사용
});
```

(CSP나 오프라인 때문에 unpkg 로드가 막히면 `mqtt.min.js`를 로컬에 받아서 참조)

---

## 5. 방법 C — 터널 (코드 변경 없이 임시 노출)

방법 A로 만든 LAN 구성을 **하나도 안 바꾸고** 외부에 노출한다. 제일 빠른 임시 방법.

- `ngrok http 8765` → 임시 공개 URL 발급
- Cloudflare Tunnel / Tailscale → 좀 더 상시로 쓸 만함

데모·발표용으로 잠깐 보여줄 때 적합. 상시 서비스로는 부적합(URL이 매번 바뀌거나 세션 제한).

---

## 6. 방법 D — 클라우드 WS 릴레이 / 매니지드 서비스

- **직접 릴레이**: Fly.io / Railway 등에 20줄짜리 Node 웹소켓 서버를 올리고, ESP32(ws 클라이언트) + 브라우저(ws 클라이언트) 둘 다 거기에 접속. 지금 `serial_to_ws.py`의 `broadcast` 로직을 클라우드로 옮기는 느낌.
- **매니지드**: Ably, PubNub, Firebase RTDB — 브로커/서버 운영 부담 없음. 무료 티어 있음.

---

## 7. 정리 / 추천

| 상황 | 선택 |
|---|---|
| 같은 공유기 안에서만 보면 됨 | **방법 A** (ESP32 직접 웹소켓, PC 브리지 제거) |
| 발표·데모로 잠깐 외부에 보여주기 | **방법 C** (터널) |
| 인터넷 너머에서 상시 스트리밍 | **방법 B (MQTT over WS)** 또는 **방법 D (WS 릴레이)** |

- 임시 = 터널
- 상시 = MQTT(over WS) 또는 WS 릴레이

---

## 8. 해야 할 일 (TODO)

- [ ] 방법 A 먼저 구현 — ESP32 WiFi 접속 + `WebSocketsServer`로 대시보드 직접 연결, PC 브리지 제거
- [ ] `dashboard/index.html`의 `WS_URL`을 설정값으로 빼서 localhost / ESP32 IP / 브로커를 쉽게 전환
- [ ] `ESPmDNS`로 `imu.local` 고정해서 IP 안 외우기
- [ ] 방법 B 테스트 — HiveMQ Cloud 무료 계정 + `PubSubClient` publish + 브라우저 mqtt.js subscribe
- [ ] 전송률을 20Hz로 낮췄을 때 대시보드가 여전히 부드러운지 확인 (필요하면 브라우저에서 보간)
- [ ] TLS 루트 CA를 펌웨어에 심어서 `setInsecure()` 제거
- [ ] DAY4 이벤트 로거(`README.md`)와 합칠지 결정 — "원격 브로커로 이벤트만 push"하면 두 주제가 자연스럽게 연결됨
