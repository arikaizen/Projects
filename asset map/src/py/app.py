#!/usr/bin/env python3
"""
FastAPI backend with CSV persistence for markers.

This module implements a small local-first API for an interactive map UI.

High-level responsibilities:
- Manage a simple CSV-backed store of markers (id, lat, lng, properties).
- Provide REST endpoints for listing, creating, updating, and deleting markers.
- Serve the frontend template and static assets (JS/CSS) that implement the interactive map.

Important notes:
- CSV persistence is intentionally simple and suitable for local development or small prototypes.
- An in-process threading.Lock reduces races inside one process. For multi-process or production
  use cases, migrate to SQLite or a proper DB.
- Functions below include docstrings describing their purpose, inputs, outputs, side effects,
  and places to extend or replace with a different persistence backend.
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

# --- Configuration & filesystem setup ---
BASE_DIR = os.path.dirname(__file__)
DATA_DIR = os.path.join(BASE_DIR, "data")
MARKERS_CSV = os.path.join(DATA_DIR, "markers.csv")

# Ensure the data directory exists on startup.
os.makedirs(DATA_DIR, exist_ok=True)

# In-process lock to reduce race conditions when multiple requests are handled
# by the same Python process. This is NOT sufficient across multiple processes.
csv_lock = threading.Lock()

# --- CSV helper functions ---
def _ensure_csv_exists():
    """
    Ensure the CSV file exists and contains the header row.

    Purpose:
    - Create the CSV file with header if it does not exist.
    - This is a no-op if the file already exists.

    Side-effects:
    - Creates the file MARKERS_CSV with header: id,lat,lng,properties

    Why important:
    - Downstream read/write functions assume the header exists.
    - Keeps initialization logic in one place.
    """
    if not os.path.exists(MARKERS_CSV):
        with open(MARKERS_CSV, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["id", "lat", "lng", "properties"])


def _read_markers_from_csv() -> List[dict]:
    """
    Read all markers from the CSV and return a list of marker dicts.

    Purpose:
    - Parse the CSV into Python objects for the API to return to clients.

    Details:
    - Parses the `properties` column as JSON and converts numeric fields.
    - Uses csv_lock to protect concurrent reads/writes within the same process.

    Returns:
    - List[dict], each dict contains keys: id (int), lat (float), lng (float), properties (dict)

    Side-effects:
    - Calls _ensure_csv_exists() to create the file if missing.
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
                    # If parsing fails, fall back to empty dict to avoid crashing the API.
                    props = {}
                markers.append({
                    "id": int(row["id"]) if row.get("id") else None,
                    "lat": float(row.get("lat")) if row.get("lat") else None,
                    "lng": float(row.get("lng")) if row.get("lng") else None,
                    "properties": props
                })
    return markers


def _write_all_markers_to_csv(markers: List[dict]):
    """
    Overwrite the CSV with the provided markers list.

    Purpose:
    - Persist an in-memory list of markers (used by update/delete operations which
      rewrite the entire CSV).

    Input:
    - markers: List[dict]. Each dict must include keys: id, lat, lng, properties (dict).

    Side-effects:
    - Replaces content of MARKERS_CSV. Uses csv_lock to protect writes.
    - Serializes properties to JSON for storage.

    Performance note:
    - Rewriting is fine for small datasets; for large datasets, switch to a DB.
    """
    _ensure_csv_exists()
    with csv_lock:
        with open(MARKERS_CSV, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["id", "lat", "lng", "properties"])
            for m in markers:
                props_json = json.dumps(m.get("properties") or {})
                writer.writerow([str(m.get("id")), str(m.get("lat")), str(m.get("lng")), props_json])


def _append_marker_to_csv(record: dict) -> dict:
    """
    Append a single marker record to the CSV and return the stored record with an assigned id.

    Purpose:
    - Add new markers to persistence while preserving existing records.

    Input:
    - record: dict with keys lat (float), lng (float), and optional properties (dict)
              and optionally 'id' (int). If id is omitted, the function assigns one.

    Returns:
    - dict representing the stored marker with assigned id.

    Side-effects:
    - Reads existing rows to compute the next id, then appends a new CSV row.
    - Uses csv_lock to protect read/append within the same process.

    Concurrency caveat:
    - This approach is simple but not robust across multiple processes. Use SQLite
      or another DB for multi-process safety.
    """
    _ensure_csv_exists()
    with csv_lock:
        existing = _read_markers_from_csv()
        max_id = 0
        for e in existing:
            if e.get("id") is not None:
                max_id = max(max_id, e["id"])
        assigned_id = (max_id + 1) if record.get("id") is None else record.get("id")
        row = [str(assigned_id), str(record["lat"]), str(record["lng"]), json.dumps(record.get("properties") or {})]
        with open(MARKERS_CSV, "a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(row)
    return {"id": assigned_id, "lat": record["lat"], "lng": record["lng"], "properties": record.get("properties") or {}}


def _update_marker_in_csv(marker_id: int, updated: dict) -> Optional[dict]:
    """
    Update the marker with the given id and persist the change.

    Purpose:
    - Support updating location or properties for an existing marker.

    Input:
    - marker_id: int id of the marker to update
    - updated: dict with fields to update (lat, lng, properties). Fields not present
               will be left unchanged (in this implementation we overwrite provided fields).

    Returns:
    - The updated marker dict if found and updated, otherwise None.

    Implementation detail:
    - Reads all markers into memory, updates the target, then rewrites the CSV.
      This is straightforward and fine for small collections.
    """
    markers = _read_markers_from_csv()
    found = False
    for m in markers:
        if m.get("id") == marker_id:
            # Update fields explicitly supplied in the 'updated' object.
            if "lat" in updated and updated["lat"] is not None:
                m["lat"] = float(updated["lat"])
            if "lng" in updated and updated["lng"] is not None:
                m["lng"] = float(updated["lng"])
            if "properties" in updated:
                # Replace properties wholesale for simplicity; merge if desired.
                m["properties"] = updated.get("properties") or {}
            found = True
            break
    if not found:
        return None
    _write_all_markers_to_csv(markers)
    return m


def _delete_marker_in_csv(marker_id: int) -> bool:
    """
    Delete the marker with the given id from CSV.

    Purpose:
    - Remove a marker permanently from persistence.

    Input:
    - marker_id: int

    Returns:
    - True if the marker was found and removed; False if not found.

    Side-effects:
    - Rewrites entire CSV without the deleted marker.
    """
    markers = _read_markers_from_csv()
    new_markers = [m for m in markers if m.get("id") != marker_id]
    if len(new_markers) == len(markers):
        # No change -> id not found
        return False
    _write_all_markers_to_csv(new_markers)
    return True

# --- FastAPI app setup ---
app = FastAPI(title="Asset Map (CSV persistence)")

# Serve static assets and templates from the package directory
app.mount("/static", StaticFiles(directory=os.path.join(BASE_DIR, "static")), name="static")
templates = Jinja2Templates(directory=os.path.join(BASE_DIR, "templates"))

# Pydantic model defines the expected shape of marker input payloads
class Marker(BaseModel):
    """
    Input/validation model for markers.

    Fields:
    - id: Optional int (client-provided or server-assigned)
    - lat, lng: floats (required)
    - properties: optional dict for arbitrary metadata (stored as JSON in CSV)
    """
    id: Optional[int] = Field(None, description="Optional ID. Server assigns one if not provided.")
    lat: float
    lng: float
    properties: Optional[dict] = Field(default_factory=dict)

# --- Routes / HTTP handlers ---
@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    """
    Render the frontend single-page app.

    Purpose:
    - Return the index.html template which bootstraps the Leaflet-based frontend.
    - The frontend will call the API endpoints below to manage markers.
    """
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/api/markers")
async def get_markers():
    """
    HTTP GET /api/markers

    Purpose:
    - Return a JSON object with a "markers" array containing all stored markers.

    Behavior:
    - Reads the CSV via _read_markers_from_csv() and returns the result.
    - Fast and simple for development; not paginated for now.
    """
    markers = _read_markers_from_csv()
    return JSONResponse({"markers": markers})


@app.post("/api/markers", status_code=201)
async def post_marker(marker: Marker):
    """
    HTTP POST /api/markers

    Purpose:
    - Create a new marker from the request body and persist it to CSV.

    Input:
    - JSON body matching Marker Pydantic model (lat, lng, optional properties/id).

    Returns:
    - JSON with {"status":"ok","marker": stored_marker}

    Errors:
    - Returns 500 if CSV write fails.
    """
    try:
        stored = _append_marker_to_csv(marker.dict())
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    return JSONResponse({"status": "ok", "marker": stored})


@app.put("/api/markers/{marker_id}")
async def put_marker(marker_id: int, marker: Marker):
    """
    HTTP PUT /api/markers/{marker_id}

    Purpose:
    - Update an existing marker's lat, lng, or properties.

    Input:
    - marker_id path parameter and a JSON body (Marker model).

    Returns:
    - JSON with {"status":"ok","marker": updated_marker} or 404 if not found.
    """
    updated = {"lat": marker.lat, "lng": marker.lng, "properties": marker.properties}
    result = _update_marker_in_csv(marker_id, updated)
    if result is None:
        raise HTTPException(status_code=404, detail="Marker not found")
    return JSONResponse({"status": "ok", "marker": result})


@app.delete("/api/markers/{marker_id}", status_code=204)
async def delete_marker(marker_id: int):
    """
    HTTP DELETE /api/markers/{marker_id}

    Purpose:
    - Remove a marker by id.

    Returns:
    - HTTP 204 on success; 404 if the marker does not exist.
    """
    ok = _delete_marker_in_csv(marker_id)
    if not ok:
        raise HTTPException(status_code=404, detail="Marker not found")
    # JSONResponse with None and status_code 204 to comply with the previous implementation
    return JSONResponse(content=None, status_code=204)


# --- Convenience: run with 'python app.py' for development ---
if __name__ == "__main__":
    import uvicorn
    # Uses uvicorn with reload for development convenience.
    uvicorn.run("app:app", host="127.0.0.1", port=8000, reload=True)
