# Smart Balance Analyzer

Live weight monitoring: ESP32 + HX711 load cell + 20x4 LCD publishes data over MQTT to a cloud dashboard. Works across any distance — no port forwarding, no VPN required.

**Live dashboard:** https://SMGTCodes.github.io/smart-balance-analyzer
**Repository:** https://github.com/SMGTCodes/smart-balance-analyzer

## Architecture

```
PC (ESP32 + load cell)
        │  USB serial
        ▼
Sender/sender.py
        │  MQTT publish
        ▼
broker.hivemq.com  (free public broker)
        │  MQTT subscribe
        ▼
Receiver/mqtt_bridge.py  (Railway)
        │  WebSocket
        ▼
Receiver/main.py — FastAPI backend  (Railway)
        │  WebSocket
        ▼
frontend/ — React dashboard  (GitHub Pages)
```

## Repository structure

```
sba/
├── firmware/
│   └── esp32_loadcell.ino     # Upload to ESP32 via Arduino IDE
├── Sender/
│   └── sender.py      # Run on the PC with the ESP32 plugged in
├── Receiver/
│   ├── main.py                 # FastAPI backend
│   ├── mqtt_bridge.py          # MQTT → WebSocket bridge
│   ├── requirements.txt
│   ├── Procfile                 # Railway deployment
│   └── runtime.txt              # Railway Python version
└── frontend/
    └── src/App.jsx              # React dashboard
```

## Quick start (local)

### ESP32 side
```bash
pip install pyserial paho-mqtt
python sender.py
```
Upload `firmware/esp32_loadcell.ino` first via Arduino IDE (Board: ESP32 Dev Module, Port: your COM port).

### Backend + dashboard (any machine)
```bash
cd Receiver
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload

# new terminal
source venv/bin/activate
python mqtt_bridge.py

# new terminal
cd ../frontend
npm install
npm start
```
Open `http://localhost:3000`.

## Configuration

`Sender/sender.py` and `Receiver/mqtt_bridge.py` must use the same MQTT topic:
```python
TOPIC = "balance/sba20"
```
