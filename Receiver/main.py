"""
main.py — FastAPI backend for Smart Balance Analyzer
Run: python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
"""

import json, time, logging
from contextlib import asynccontextmanager
from fastapi import FastAPI, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import asyncio
from typing import Set, Optional

logging.basicConfig(level=logging.INFO, format="%(asctime)s  %(message)s")
log = logging.getLogger(__name__)

# ── In-memory state ───────────────────────────────────────────
class State:
    def __init__(self):
        self.latest   = None
        self.last_seen= None
        self.count    = 0
    def update(self, d):
        self.latest    = d
        self.last_seen = time.time()
        self.count    += 1

state = State()

# ── Dashboard clients ─────────────────────────────────────────
clients: Set[WebSocket] = set()

async def broadcast(payload: str):
    dead = set()
    for ws in set(clients):
        try:   await ws.send_text(payload)
        except: dead.add(ws)
    clients.difference_update(dead)

# ── App ───────────────────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    log.info("Smart Balance Analyzer backend started")
    yield

app = FastAPI(title="Smart Balance Analyzer", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"],
                   allow_methods=["*"], allow_headers=["*"])

# ── WebSocket: ESP32 / MQTT bridge sends data here ────────────
@app.websocket("/ws/esp32")
async def ws_esp32(ws: WebSocket):
    await ws.accept()
    log.info("ESP32/bridge connected")
    try:
        while True:
            try:
                raw = await asyncio.wait_for(ws.receive_text(), timeout=5.0)
            except asyncio.TimeoutError:
                await ws.send_text('{"ping":true}')
                continue

            try:   data = json.loads(raw)
            except: continue

            state.update(data)
            payload = json.dumps({**data,
                                   "serverTime": state.last_seen,
                                   "count": state.count})
            await broadcast(payload)

    except Exception: pass
    log.info("ESP32/bridge disconnected")

# ── WebSocket: dashboard browsers connect here ────────────────
@app.websocket("/ws/dashboard")
async def ws_dashboard(ws: WebSocket):
    await ws.accept()
    clients.add(ws)
    log.info(f"Dashboard connected. Total: {len(clients)}")

    # Send latest data immediately on connect
    if state.latest:
        try:
            await ws.send_text(json.dumps({**state.latest,
                                            "serverTime": state.last_seen,
                                            "count": state.count}))
        except: pass

    try:
        while True:
            try:
                msg = await asyncio.wait_for(ws.receive_text(), timeout=30.0)
                if msg == "ping": await ws.send_text("pong")
            except asyncio.TimeoutError:
                try:   await ws.send_text('{"heartbeat":true}')
                except: break
    except Exception: pass

    clients.discard(ws)
    log.info(f"Dashboard disconnected. Total: {len(clients)}")

# ── REST ──────────────────────────────────────────────────────
@app.get("/api/status")
async def status():
    return JSONResponse({
        "connected": state.last_seen is not None and
                     time.time() - state.last_seen < 5,
        "last_seen": state.last_seen,
        "count": state.count,
        "data": state.latest,
        "dashboards": len(clients)
    })

@app.get("/")
async def root():
    return JSONResponse({"status": "ok", "dashboard": "http://localhost:3000"})
