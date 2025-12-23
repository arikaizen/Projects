// map.js - frontend glue for Asset Map
// This file initializes the Leaflet map, loads markers from the API, and
// posts new markers when the user clicks the map. Comments explain each step.

document.addEventListener("DOMContentLoaded", function () {
  // Initialize the map centered on a world view
  const map = L.map("map").setView([20, 0], 2);

  // Use OpenStreetMap tiles via Leaflet (no API keys required). This is
  // suitable for development and small projects. We can switch to Mapbox
  // or another provider later if needed.
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution:
      '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
  }).addTo(map);

  // Helper to add a marker object to the map; markerObj should have lat,lng,properties
  function addMarkerToMap(markerObj) {
    const marker = L.marker([markerObj.lat, markerObj.lng]).addTo(map);
    const props = markerObj.properties ? JSON.stringify(markerObj.properties) : "{}";
    marker.bindPopup(`ID: ${markerObj.id || "(n/a)"}<br/>${props}`);
  }

  // Load existing markers from the backend API and add them to the map
  fetch("/api/markers")
    .then((r) => r.json())
    .then((data) => {
      if (data && data.markers) {
        data.markers.forEach((m) => addMarkerToMap(m));
      }
    })
    .catch((err) => console.warn("Failed to load markers:", err));

  // When the user clicks the map we create a new marker and POST it to the API
  map.on("click", function (e) {
    const { lat, lng } = e.latlng;
    const payload = { lat: lat, lng: lng, properties: { source: "ui-click" } };

    fetch("/api/markers", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    })
      .then((r) => r.json())
      .then((data) => {
        if (data && data.marker) {
          // Add the returned marker (with id) to the map
          addMarkerToMap(data.marker);
        } else {
          console.warn("Unexpected response from POST /api/markers", data);
        }
      })
      .catch((err) => console.error("Failed to save marker:", err));
  });
});
