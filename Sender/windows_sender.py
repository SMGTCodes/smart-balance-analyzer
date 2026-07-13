import json, sys, time, serial, serial.tools.list_ports
import paho.mqtt.client as mqtt

#  SETTINGS
SERIAL_PORT = "COM6"               # your ESP32 COM port
BAUD        = 115200
BROKER      = "broker.hivemq.com"  # free public MQTT broker
PORT        = 1883
TOPIC       = "balance/sba20"      # must match mqtt_bridge.py


pkt    = [0]
online = [False]

def on_connect(client, ud, flags, rc):
    if rc == 0:
        online[0] = True
        print(f"[MQTT] Connected to {BROKER}")
        print(f"[MQTT] Sending on topic: {TOPIC}")
        print(f"[MQTT] Mac dashboard will now show live data!\n")
    else:
        print(f"[MQTT] Connection failed rc={rc} — retrying...")

def on_disconnect(client, ud, rc):
    online[0] = False
    print("[MQTT] Disconnected — reconnecting...")

def list_ports():
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device:10s}  {p.description}")

def main():
    if "--list" in sys.argv:
        list_ports(); return

    print("=" * 45)
    print("  Smart Balance Analyzer — Windows Sender")
    print(f"  Port  : {SERIAL_PORT}")
    print(f"  Broker: {BROKER}")
    print(f"  Topic : {TOPIC}")
    print("=" * 45 + "\n")

    # Start MQTT
    client = mqtt.Client(client_id=f"sba-win-{int(time.time())}")
    client.on_connect    = on_connect
    client.on_disconnect = on_disconnect
    client.reconnect_delay_set(1, 10)
    client.connect_async(BROKER, PORT, 60)
    client.loop_start()
    print(f"[MQTT] Connecting to {BROKER}...")

    # Serial loop
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD, timeout=2)
            print(f"[COM]  Opened {SERIAL_PORT}")
        except serial.SerialException as e:
            print(f"[COM]  Cannot open {SERIAL_PORT}: {e}")
            print("       → Close Arduino Serial Monitor if open (it locks the port)")
            print("       → Check ESP32 is plugged in")
            time.sleep(3)
            continue

        try:
            while True:
                try:
                    raw = ser.readline()
                except serial.SerialException:
                    break

                if not raw: continue
                line = raw.decode("utf-8", errors="ignore").strip()

                # Print ESP32 debug lines
                if not line.startswith("{"):
                    if line: print(f"[ESP32] {line}")
                    continue

                # Validate JSON
                try: data = json.loads(line)
                except: continue

                # Publish to MQTT
                if online[0]:
                    client.publish(TOPIC, line, qos=0)
                    pkt[0] += 1
                    if pkt[0] % 10 == 0:
                        print(f"[PKT #{pkt[0]}]  "
                              f"{data.get('totalWeight',0):.2f} kg  "
                              f"→ {TOPIC}")
                else:
                    print("[MQTT] Waiting for broker connection...")
                    time.sleep(1)

        finally:
            try: ser.close()
            except: pass
        print("[COM]  Reconnecting in 3s...")
        time.sleep(3)

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: print("\nStopped.")
