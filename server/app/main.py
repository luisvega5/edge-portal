from fastapi import FastAPI, Query
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pathlib import Path
from datetime import datetime, timezone
from pydantic import BaseModel
import random

app = FastAPI(title="Edge Portal")

class PowerStateResponse(BaseModel):
    powerOn: bool

class TempResponse(BaseModel):
    value_c: float
    timestamp: str

ROOT_DIR = Path(__file__).resolve().parents[2]    # /app
CLIENT_DIR = ROOT_DIR / "client"                  # /app/client

app.mount("/static", StaticFiles(directory=str(CLIENT_DIR)), name="static")

@app.get("/")
def index():
    return FileResponse(str(CLIENT_DIR / "index.html"))

@app.get("/health")
def health():
    return {"status": "ok"}

@app.post("/power", response_model=PowerStateResponse)
def set_power_state(powerOn: bool = Query(...)):
    # TODO: Replace with real hardware control
    return PowerStateResponse(powerOn=powerOn)

@app.get("/temperature", response_model=TempResponse)
def get_temperature():
    # TODO: Replace with real sensor read
    value_c = round(20.0 + random.random() * 5.0, 2)
    ts = datetime.now(timezone.utc).isoformat()
    return TempResponse(value_c=value_c, timestamp=ts)
