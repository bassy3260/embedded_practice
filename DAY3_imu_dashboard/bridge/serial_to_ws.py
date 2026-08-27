"""
ESP32가 시리얼로 보내는 "roll,pitch,yaw" 한 줄을 읽어서
로컬 웹소켓(ws://localhost:8765)으로 그대로 뿌려주는 브리지.

dashboard/index.html이 이 웹소켓에 접속해서 3D 박스를 회전시킨다.

사용법:
    pip install -r requirements.txt
    python serial_to_ws.py --port COM7

(포트 번호는 장치관리자 또는
 `pio device monitor` 실행 시 뜨는 COM 번호를 참고)

"""

import argparse
import asyncio
import json

import serial
import websockets

connected_clients: set = set()


async def handle_client(websocket):
    connected_clients.add(websocket)
    print(f"[bridge] 대시보드 연결됨 (현재 {len(connected_clients)}개)")
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.discard(websocket)
        print(f"[bridge] 대시보드 연결 종료 (현재 {len(connected_clients)}개)")


async def broadcast(payload: str) -> None:
    if not connected_clients:
        return
    dead = []
    # 순회 중 다른 코루틴(handle_client)이 connected_clients에 add/discard 하면
    # "Set changed size during iteration" 에러로 브리지가 죽어버리므로 복사본을 순회한다.
    for client in list(connected_clients):
        try:
            await client.send(payload)
        except websockets.ConnectionClosed:
            dead.append(client)
    for client in dead:
        connected_clients.discard(client)


async def read_serial_loop(port: str, baud: int) -> None:
    ser = serial.Serial(port, baud, timeout=1)
    print(f"[bridge] 시리얼 연결: {port} @ {baud}bps")

    loop = asyncio.get_running_loop()
    raw_shown = 0     # 디버그용: 처음 들어오는 원본 줄 몇 개를 그대로 보여줌
    ok_count = 0       # 정상 파싱된 줄 수 (1초에 한 번씩만 로그)
    last_log = 0.0
    try:
        while True:
            line = await loop.run_in_executor(None, ser.readline)
            line = line.decode(errors="ignore").strip()
            if not line:
                continue

            if raw_shown < 5:
                print(f"[bridge] 원본 수신: {line!r}")
                raw_shown += 1

            parts = line.split(",")
            if len(parts) != 3:
                print(f"[bridge] 형식 불일치 (콤마 3개 아님), 건너뜀: {line!r}")
                continue
            try:
                roll, pitch, yaw = (float(p) for p in parts)
            except ValueError:
                print(f"[bridge] 숫자 변환 실패, 건너뜀: {line!r}")
                continue

            ok_count += 1
            now = loop.time()
            if now - last_log > 1.0:
                print(f"[bridge] 전송중 roll={roll:.1f} pitch={pitch:.1f} yaw={yaw:.1f} "
                      f"(누적 {ok_count}줄, 구독자 {len(connected_clients)}명)")
                last_log = now

            payload = json.dumps({"roll": roll, "pitch": pitch, "yaw": yaw})
            await broadcast(payload)
    finally:
        ser.close()


async def main(port: str, baud: int, ws_port: int) -> None:
    server = await websockets.serve(handle_client, "localhost", ws_port)
    print(f"[bridge] 웹소켓 서버 시작: ws://localhost:{ws_port}")
    print("[bridge] dashboard/index.html을 브라우저로 열어서 확인하세요.")

    try:
        await read_serial_loop(port, baud)
    finally:
        server.close()
        await server.wait_closed()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32 시리얼 -> 웹소켓 브리지")
    parser.add_argument("--port", required=True, help="시리얼 포트 (예: COM7)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--ws-port", type=int, default=8765)
    args = parser.parse_args()

    try:
        asyncio.run(main(args.port, args.baud, args.ws_port))
    except KeyboardInterrupt:
        print("\n[bridge] 종료")
