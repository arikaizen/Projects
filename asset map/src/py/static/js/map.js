// map.js - interactive map frontend using Leaflet + FastAPI endpoints
//
// This file is heavily commented to explain the purpose of each function and
// what happens at each stage (load, add, update, delete).
//
// High level flow:
// 1. Initialize the Leaflet map and tile layer (OpenStreetMap).
// 2. Load markers from the backend via GET /api/markers and add them to the map.
// 3. User clicks map -> prompt for optional name -> POST /api/markers -> add marker.
// 4. Markers are draggable; drag event triggers PUT /api/markers/:id to persist position.
// 5. Popup contains Edit and Delete buttons that call PUT and DELETE respectively.

document.addEventListener("DOMContentLoaded", function () {
  const apiBase = "/api/markers";

  // Initialize the Leaflet map centered on a world view (lat=20, lng=0, zoom=2).
  // Purpose: provide a global view where users can click anywhere and add markers.
  const map = L.map("map").setView([20, 0], 2);

  // Add OpenStreetMap tile layer. This is free and open-source.
  // Attribution is required by the tile provider.
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution:
      '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
  }).addTo(map);

  // Maintain a mapping from marker id -> Leaflet marker instance.
  // Purpose: allow easy lookup to update/remove markers when the backend changes.
  const leafletMarkers = new Map();

  // makePopupHtml - purpose:
  // Build the HTML shown inside each marker's popup. The popup includes:
  // - ID display
  // - Name display (from marker.properties.name)
  // - Edit and Delete buttons
  //
  // Important: this returns plain HTML; event handlers for buttons are attached
  // when popup opens (see addMarkerToMap).
  function makePopupHtml(markerObj) {
    const name = (markerObj.properties && markerObj.properties.name) ? markerObj.properties.name : "";
    return `
      <div>
        <strong>ID:</strong> ${markerObj.id}<br/>
        <strong>Name:</strong> <span id="name-${markerObj.id}">${escapeHtml(name)}</span><br/>
        <div style="margin-top:6px;">
          <button data-action="edit" data-id="${markerObj.id}">Edit</button>
          <button data-action="delete" data-id="${markerObj.id}">Delete</button>
        </div>
      </div>
    `;
  }

  // escapeHtml - purpose:
  // Simple utility to escape user-provided strings when injecting into HTML.
  // Prevents basic HTML injection inside popups.
  function escapeHtml(s) {
    if (!s) return "";
    return s.replace(/[&<>'\"]/g, (c) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  }

  // addMarkerToMap - purpose:
  // Add a marker object (from the backend) to the Leaflet map.
  //
  // Steps:
  // - Remove existing Leaflet marker for the id (if present) to avoid duplicates.
  // - Create a draggable Leaflet marker at the provided lat/lng.
  // - Bind a popup (HTML created by makePopupHtml).
  // - Wire dragend event to persist new coordinates via PUT to the backend.
  // - Wire popupopen event to attach click handlers to Edit/Delete buttons.
  //
  // Inputs:
  // - markerObj: { id, lat, lng, properties }
  function addMarkerToMap(markerObj) {
    // Remove duplicate if present
    if (leafletMarkers.has(markerObj.id)) {
      const existing = leafletMarkers.get(markerObj.id);
      existing.remove();
      leafletMarkers.delete(markerObj.id);
    }

    const marker = L.marker([markerObj.lat, markerObj.lng], { draggable: true }).addTo(map);

    // Attach popup HTML based on the marker's current state.
    marker.bindPopup(makePopupHtml(markerObj));

    // dragend handler: persist position change to backend
    marker.on("dragend", function (e) {
      const latlng = e.target.getLatLng();
      // Build payload: include properties to preserve metadata (e.g., name).
      const payload = { lat: latlng.lat, lng: latlng.lng, properties: markerObj.properties || {} };

      // PUT to update marker coordinates. On success, update local markerObj and popup.
      fetch(`${apiBase}/${markerObj.id}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      })
        .then((r) => {
          if (!r.ok) throw new Error("Failed to update marker");
          return r.json();
        })
        .then((data) => {
          if (data && data.marker) {
            markerObj.lat = data.marker.lat;
            markerObj.lng = data.marker.lng;
            markerObj.properties = data.marker.properties;
            marker.setPopupContent(makePopupHtml(markerObj));
          }
        })
        .catch((err) => {
          console.error("Update failed:", err);
          // Optionally inform the user or revert marker position.
        });
    });

    // When popup opens, attach handlers to the Edit/Delete buttons.
    // We attach handlers here because popup HTML is injected dynamically.
    marker.on("popupopen", function () {
      const popupEl = document.querySelector(`.leaflet-popup-content`);
      if (!popupEl) return;

      // Edit button handler: prompt user for a new name, then send PUT to backend.
      const editBtn = popupEl.querySelector(`button[data-action="edit"][data-id="${markerObj.id}"]`);
      if (editBtn) {
        editBtn.addEventListener("click", function () {
          const newName = prompt("Enter a name/label for this marker:", (markerObj.properties && markerObj.properties.name) || "");
          if (newName === null) return; // user cancelled
          const updatedProps = Object.assign({}, markerObj.properties || {}, { name: newName });
          const payload = { lat: markerObj.lat, lng: markerObj.lng, properties: updatedProps };
          fetch(`${apiBase}/${markerObj.id}`, {
            method: "PUT",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
          })
            .then((r) => {
              if (!r.ok) throw new Error("Failed to update marker properties");
              return r.json();
            })
            .then((data) => {
              if (data && data.marker) {
                markerObj.properties = data.marker.properties;
                const nameSpan = document.getElementById(`name-${markerObj.id}`);
                if (nameSpan) nameSpan.textContent = markerObj.properties.name || "";
              }
            })
            .catch((err) => {
              console.error("Failed to save name:", err);
            });
        });
      }

      // Delete button handler: confirm and call DELETE endpoint.
      const delBtn = popupEl.querySelector(`button[data-action="delete"][data-id="${markerObj.id}"]`);
      if (delBtn) {
        delBtn.addEventListener("click", function () {
          if (!confirm("Delete this marker?")) return;
          fetch(`${apiBase}/${markerObj.id}`, {
            method: "DELETE"
          })
            .then((r) => {
              if (r.status === 204) {
                // Remove from map and local state on success.
                marker.remove();
                leafletMarkers.delete(markerObj.id);
              } else {
                throw new Error("Failed to delete marker");
              }
            })
            .catch((err) => {
              console.error("Delete failed:", err);
            });
        });
      }
    });

    // Store marker for later lookup (update/remove)
    leafletMarkers.set(markerObj.id, marker);
  }

  // loadMarkers - purpose:
  // Fetch all markers from the server and add them to the map.
  //
  // Steps:
  // - GET /api/markers
  // - For each returned marker, call addMarkerToMap
  function loadMarkers() {
    fetch(apiBase)
      .then((r) => r.json())
      .then((data) => {
        if (data && data.markers) {
          data.markers.forEach((m) => addMarkerToMap(m));
        }
      })
      .catch((err) => console.warn("Failed to load markers:", err));
  }

  // map click handler - purpose:
  // Allow users to click anywhere on the map to create a new marker.
  //
  // Flow:
  // - User clicks: prompt for optional name.
  // - Build payload (lat, lng, properties) and POST to backend.
  // - On success, add returned marker to the map.
  map.on("click", function (e) {
    const { lat, lng } = e.latlng;
    const name = prompt("Enter a name/label for this marker (optional):", "");
    const properties = {};
    if (name) properties.name = name;
    properties.source = "ui-click";

    const payload = { lat: lat, lng: lng, properties: properties };

    fetch(apiBase, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    })
      .then((r) => r.json())
      .then((data) => {
        if (data && data.marker) {
          addMarkerToMap(data.marker);
        } else {
          console.warn("Unexpected POST response", data);
        }
      })
      .catch((err) => {
        console.error("Failed to save marker:", err);
      });
  });

  // Kick off initial load of markers.
  loadMarkers();
});
