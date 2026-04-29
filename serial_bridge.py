"""
ESP32 Serial → WebSocket Bridge
================================
Reads JSON from ESP32 over Serial and broadcasts to the dashboard.

Install deps:
    pip install pyserial websockets

Run:
    python serial_bridge.py

Then open dashboard.html, select WebSocket, URL = ws://localhost:8765, Connect.
"""

import asyncio
import json
import serial
import serial.tools.list_ports
import websockets

# ── CONFIG ──────────────────────────────────────────────────────────────────
SERIAL_PORT  = "COM3"        # Windows: "COM3" | Mac/Linux: "/dev/ttyUSB0"
BAUD_RATE    = 115200
WS_HOST      = "localhost"
WS_PORT      = 8765
# ─────────────────────────────────────────────────────────────────────────────

clients = set()

async def ws_handler(websocket):
    clients.add(websocket)
    print(f"[WS] Client connected  (total: {len(clients)})")
    try:
        await websocket.wait_closed()
    finally:
        clients.discard(websocket)
        print(f"[WS] Client disconnected (total: {len(clients)})")

async def broadcast(message: str):
    if clients:
        await asyncio.gather(*[c.send(message) for c in clients], return_exceptions=True)

async def serial_reader():
    print(f"[Serial] Opening {SERIAL_PORT} @ {BAUD_RATE} baud ...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"[Serial] Connected to {SERIAL_PORT}")
    except serial.SerialException as e:
        print(f"[Serial] ERROR: {e}")
        print("[Serial] Available ports:")
        for p in serial.tools.list_ports.comports():
            print(f"   {p.device}  —  {p.description}")
        return

    loop = asyncio.get_event_loop()
    while True:
        try:
            line = await loop.run_in_executor(None, ser.readline)
            line = line.decode("utf-8", errors="ignore").strip()

            if not line:
                continue

            print(f"[Serial] Raw: {line}")

            # Only forward lines that look like JSON
            if line.startswith("{"):
                try:
                    data = json.loads(line)
                    await broadcast(json.dumps(data))
                    print(f"[WS] Sent: {data}")
                except json.JSONDecodeError:
                    print(f"[Serial] Bad JSON: {line}")

        except Exception as e:
            print(f"[Serial] Read error: {e}")
            await asyncio.sleep(1)

async def main():
    print(f"[WS] Server starting on ws://{WS_HOST}:{WS_PORT}")
    async with websockets.serve(ws_handler, WS_HOST, WS_PORT):
        await serial_reader()   # runs forever

if __name__ == "__main__":
    asyncio.run(main())
