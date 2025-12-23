#!/usr/bin/env python3
"""
FastAPI-based backend for Asset Map

This module provides:
- FastAPI app serving a single-page frontend (Jinja2 template)
- Static files mounted for JS/CSS
- CSV-backed persistence for markers (data/markers.csv)

Comments are intentionally verbose to explain what each section does and why.
"""
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import JSONResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel, Field
from typing import List, Optional
import csv
import os
import threading
import json

# --- Configuration ---
# Root path for this small app. We keep everything under the "asset map/src/py" directory
BASE_DIR = os.path.dirname(__file__)
DATA_DIR = os.path.join(BASE_DIR, "data")
MARKERS_CSV = os.path.join(DATA_DIR, "markers.csv")

# Ensure the data directory exists
os.makedirs(DATA_DIR, exist_ok=True)

# A simple in-process lock to reduce race conditions when multiple requests
# are handled by the same process. Note: this does NOT protect against
# concurrent writes from multiple processes (e.g., multiple uvicorn workers).
# For production or multi-process safety, use a real DB or a file-locking library
# such as "portalocker" or switch to SQLite.
csv_lock = threading.Lock()

# --- FastAPI app setup ---
app = FastAPI(title="Asset Map API")

# Mount the static folder so the frontend (JS/CSS) can be served easily.
# The static and templates folders are relative to this file.
app.mount("/static", StaticFiles(directory=os.path.join(BASE_DIR, "static")), name="static")

templates = Jinja2Templates(directory=os.path.join(BASE_DIR, "templates"))

# --- Pydantic models ---
class Marker(BaseModel):
    """Marker model used for input validation on POST requests.

    - id: Optional; if not provided the server will generate one.
    - lat, lng: Required latitude and longitude.
    - properties: Optional dictionary with extra attributes (stored as JSON in CSV).
    """
    id: Optional[int] = Field(None, description="Optional numeric id")
    lat: float
    lng: float
    properties: Optional[dict] = Field(default_factory=dict)

# --- CSV helper functions ---

def _ensure_csv_exists():
    """Create the CSV with header if it does not exist yet."""
    if not os.path.exists(MARKERS_CSV):
        with open(MARKERS_CSV, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            # Columns: id, lat, lng, properties (properties stored as JSON string)
            writer.writerow(["id", "lat", "lng", "properties"])


def _read_markers_from_csv() -> List[dict]:
    """Read all markers from the CSV and return them as a list of dicts.

    We parse the JSON stored in the properties column back into a Python dict.
    """
    _ensure_csv_exists()
    markers = []
    with csv_lock:
        with open(MARKERS_CSV, "r", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    props = json.loads(row.get("properties") or "{}")
                except Exception:
                    props = {}
                markers.append({
                    "id": int(row["id"]) if row.get("id") else None,
                    "lat": float(row.get("lat")) if row.get("lat") else None,
                    "lng": float(row.get("lng")) if row.get("lng") else None,
                    "properties": props,
                })
    return markers


def _append_marker_to_csv(marker: Marker) -> dict:
    """Append a new marker to the CSV and return the stored record.

    This function generates a numeric id by finding the current max id in file
    and adding 1. For very large CSVs this is inefficient; it's OK for a small
    prototype. Consider SQLite for better performance and concurrency.
    """
    _ensure_csv_exists()
    with csv_lock:
        # Read current markers to determine the next id
        existing = _read_markers_from_csv()
        max_id = 0
        for m in existing:
            if m.get("id") is not None:
                max_id = max(max_id, m["id"])
        new_id = (max_id + 1) if marker.id is None else marker.id

        row = [
            str(new_id),
            f"{marker.lat}",
            f"{marker.lng}",
            json.dumps(marker.properties or {}),
        ]
        with open(MARKERS_CSV, "a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(row)

    return {"id": new_id, "lat": marker.lat, "lng": marker.lng, "properties": marker.properties}


# --- Routes ---

@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    """Render the single-page frontend. The template contains the interactive map.

    Note: We serve the frontend through FastAPI templates for simplicity. In a
    larger app you might host static files separately or build a SPA with a
    frontend bundler.
    """
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/api/markers")
async def get_markers():
    """Return all markers stored in the CSV as JSON.

    We return a top-level object with a "markers" array for ease of extension.
    """
    markers = _read_markers_from_csv()
    return JSONResponse({"markers": markers})


@app.post("/api/markers", status_code=201)
async def post_marker(marker: Marker):
    """Add a new marker to the CSV.

    Request body must validate against the Marker Pydantic model. We append
    the marker and return the stored record including the generated id.
    """
    try:
        stored = _append_marker_to_csv(marker)
    except Exception as e:
        # Convert internal errors to an HTTP 500 with a helpful message
        raise HTTPException(status_code=500, detail=str(e))
    return JSONResponse({"status": "ok", "marker": stored})


# --- If run directly ---
if __name__ == "__main__":
    # Run via: python app.py for convenience in development. For production use
    # "uvicorn app:app --host 0.0.0.0 --port 8000 --workers N" or a proper ASGI server.
    import uvicorn

    uvicorn.run("app:app", host="127.0.0.1", port=8000, log_level="info", reload=True)
