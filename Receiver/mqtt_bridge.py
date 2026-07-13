import asyncio, json, time, threading, os
import websockets
import paho.mqtt.client as mqtt

#  SETTINGS — must match windows_sender.py
BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC     = "balance/sba20"

# On Railway, uvicorn runs on $PORT (dynamic). Locally it's 8000.
# The bridge must connect to the SAME port uvicorn is using.
BACKEND_PORT = os.environ.get("PORT", "8000")
LOCAL_WS  = f"ws://localhost:{BACKEND_PORT}/ws/esp32"

pkt_count = 0
loop      = None
queue     = None

# WebSocket sender
async def ws_sender():
    global queue
    queue = asyncio.Queue()
    while True:
        try:
            async with websockets.connect(LOCAL_WS) as ws:
                print(f"[WS]   Connected to local backend")
                while True:
                    payload = await queue.get()
                    await ws.send(payload)
        except Exception as e:
            print(f"[WS]   Backend not ready: {e}")
            print(f"[WS]   Is uvicorn running? Retrying in 3s...")
            await asyncio.sleep(3)

# MQTT callbacks
def on_connect(client, ud, flags, rc):
    if rc == 0:
        client.subscribe(TOPIC)
        print(f"[MQTT] Connected to {BROKER}")
        print(f"[MQTT] Subscribed to: {TOPIC}")
        print(f"[MQTT] Waiting for Windows ESP32 data...\n")
    else:
        print(f"[MQTT] Failed rc={rc}")

def on_message(client, ud, msg):
    global pkt_count
    try:
        payload = msg.payload.decode("utf-8")
        data    = json.loads(payload)
    except: return

    pkt_count += 1

    # Forward to backend
    if loop and queue:
        asyncio.run_coroutine_threadsafe(queue.put(payload), loop)

    if pkt_count % 10 == 0:
        print(f"[PKT #{pkt_count}]  "
              f"Total={data.get('totalWeight',0):.2f}kg  "
              f"← {TOPIC}")

def on_disconnect(client, ud, rc):
    print("[MQTT] Disconnected — reconnecting...")

# MQTT thread
def mqtt_thread():
    client = mqtt.Client(client_id=f"sba-mac-{int(time.time())}")
    client.on_connect    = on_connect
    client.on_message    = on_message
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(1, 10)
    print(f"[MQTT] Connecting to {BROKER}:{PORT}...")
    client.connect(BROKER, PORT, 60)
    client.loop_forever()

# Main
if __name__ == "__main__":
    print("\n" + "=" * 45)
    print("  Smart Balance Analyzer — MQTT Bridge (Mac)")
    print(f"  Broker : {BROKER}:{PORT}")
    print(f"  Topic  : {TOPIC}")
    print(f"  Backend: {LOCAL_WS}")
    print("=" * 45 + "\n")

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    t = threading.Thread(target=mqtt_thread, daemon=True)
    t.start()

    try:
        loop.run_until_complete(ws_sender())
    except KeyboardInterrupt:
        print("\nStopped.")
