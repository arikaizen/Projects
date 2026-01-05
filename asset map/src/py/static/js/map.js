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

  // Navigation link handling
  const navLinks = document.querySelectorAll('.nav-link');
  navLinks.forEach(link => {
    link.addEventListener('click', function(e) {
      e.preventDefault();

      // Update active state
      navLinks.forEach(l => l.classList.remove('active'));
      this.classList.add('active');

      // Handle navigation
      const section = this.getAttribute('href').substring(1);
      console.log('Navigating to:', section);
    });
  });

  // Handle dropdown items
  const dropdownItems = document.querySelectorAll('.dropdown-item');
  dropdownItems.forEach(item => {
    item.addEventListener('click', function(e) {
      e.preventDefault();
      const href = this.getAttribute('href');

      if (href === '#search-app') {
        showSearchApp();
      } else {
        console.log('Opening app:', href);
      }
    });
  });

  // Enable Enter key for navbar search
  const searchInput = document.getElementById('nav-search-input');
  if (searchInput) {
    searchInput.addEventListener('keypress', function(e) {
      if (e.key === 'Enter') {
        searchAssets();
      }
    });
  }

  // Enable Enter key for main search bar (Shift+Enter for new line, Enter to search)
  const mainSearchInput = document.getElementById('main-search-input');
  if (mainSearchInput) {
    mainSearchInput.addEventListener('keydown', function(e) {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        executeSearch();
      }
      // Shift+Enter will create new line (default behavior)
    });
  }
});

// Show Search App
function showSearchApp() {
  document.getElementById('search-app-page').classList.remove('hidden');
  document.getElementById('main-content').style.display = 'none';
  document.getElementById('main-search-input').focus();
}

// Hide Search App
function hideSearchApp() {
  document.getElementById('search-app-page').classList.add('hidden');
  document.getElementById('main-content').style.display = 'block';
}

// Execute Search from Search App
function executeSearch() {
  const searchInput = document.getElementById('main-search-input');
  const query = searchInput.value.trim();
  const resultsDiv = document.getElementById('search-results');

  if (!query) {
    resultsDiv.innerHTML = '<p style="color: #999;">Please enter a search term</p>';
    return;
  }

  // Add expanded class for animation
  searchInput.classList.add('expanded');

  // Search through markers
  fetch('/api/markers')
    .then(r => r.json())
    .then(data => {
      const markers = data.markers || [];
      const matches = markers.filter(marker => {
        const name = marker.properties?.name || '';
        const idStr = String(marker.id);
        return name.toLowerCase().includes(query.toLowerCase()) || idStr.includes(query);
      });

      if (matches.length > 0) {
        let resultsHTML = `<h3>Found ${matches.length} asset(s) matching "${query}"</h3><div style="margin-top: 20px;">`;

        matches.forEach(marker => {
          const name = marker.properties?.name || 'Unnamed';
          resultsHTML += `
            <div style="padding: 15px; border-left: 4px solid #1f6feb; background: #f8f9fa; margin-bottom: 10px; cursor: pointer;"
                 onclick="hideSearchApp(); setTimeout(() => { const map = document.querySelector('#map')._leaflet_map; if (map) map.setView([${marker.lat}, ${marker.lng}], 15); }, 100);">
              <strong>${name}</strong> (ID: ${marker.id})<br/>
              <small style="color: #666;">Location: ${marker.lat.toFixed(4)}, ${marker.lng.toFixed(4)}</small>
            </div>
          `;
        });

        resultsHTML += '</div>';
        resultsDiv.innerHTML = resultsHTML;
      } else {
        resultsDiv.innerHTML = `
          <h3>No results found</h3>
          <p style="color: #666; margin-top: 20px;">No assets match "${query}". Try a different search term.</p>
        `;
      }

      // Remove expanded class after animation
      setTimeout(() => {
        searchInput.classList.remove('expanded');
      }, 400);
    })
    .catch(err => {
      console.error('Search failed:', err);
      resultsDiv.innerHTML = '<p style="color: red;">Search failed. Please try again.</p>';
      setTimeout(() => {
        searchInput.classList.remove('expanded');
      }, 400);
    });
}

// Search assets functionality (navbar search)
// Purpose: Search through markers by name or properties and highlight matching ones
function searchAssets() {
  const searchInput = document.getElementById('nav-search-input');
  const query = searchInput.value.trim().toLowerCase();

  if (!query) {
    alert('Please enter a search term');
    return;
  }

  const statusEl = document.getElementById('status');
  let matchCount = 0;

  // Search through all markers
  fetch('/api/markers')
    .then(r => r.json())
    .then(data => {
      const markers = data.markers || [];
      const matches = markers.filter(marker => {
        const name = marker.properties?.name || '';
        const idStr = String(marker.id);
        return name.toLowerCase().includes(query) || idStr.includes(query);
      });

      matchCount = matches.length;

      if (matches.length > 0) {
        // Zoom to first match
        const firstMatch = matches[0];
        const map = document.querySelector('#map')._leaflet_map;
        if (map) {
          map.setView([firstMatch.lat, firstMatch.lng], 12);
        }
        statusEl.textContent = `Found ${matchCount} match(es) for "${query}". Zoomed to first result.`;
      } else {
        statusEl.textContent = `No matches found for "${query}".`;
      }
    })
    .catch(err => {
      console.error('Search failed:', err);
      statusEl.textContent = 'Search failed. Please try again.';
    });
}
