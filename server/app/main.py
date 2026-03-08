import os
import requests
from datetime import datetime, timezone
from fastapi import HTTPException
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

TEMP_DAEMON_URL = os.getenv("TEMP_DAEMON_URL", "http://localhost:7070")

@app.get("/temperature", response_model=TempResponse)
def get_temperature():
    try:
        r = requests.get(f"{TEMP_DAEMON_URL}/read", timeout=1.5)
        r.raise_for_status()
        data = r.json()

        if data.get("status") != "ok":
            raise HTTPException(status_code=502, detail=f"Daemon error: {data.get('error', 'unknown')}")

        temp_c = data.get("temp_c")
        if temp_c is None:
            raise HTTPException(status_code=502, detail="Daemon response missing temp_c")

        ts = data.get("timestamp") or datetime.now(timezone.utc).isoformat()
        return TempResponse(value_c=round(float(temp_c), 2), timestamp=ts)

    except requests.exceptions.RequestException as e:
        raise HTTPException(status_code=502, detail=f"Temperature daemon unavailable: {e}")
    except ValueError as e:
        raise HTTPException(status_code=502, detail=f"Bad daemon response: {e}")
