"""
Asset Map App for SIEM
Interactive world map for tracking and tagging asset locations with persistence.
"""

from . import BaseApp
from typing import Dict, Any


class AssetMapApp(BaseApp):
    """
    Asset Map application for SIEM.

    Features:
    - Interactive world map with country zoom
    - Add/edit/delete location markers
    - Tag assets with metadata
    - Persistent storage of markers
    - Click to add markers
    - Drag to reposition
    """

    @property
    def name(self) -> str:
        return "Asset Map"

    @property
    def id(self) -> str:
        return "asset-map"

    @property
    def description(self) -> str:
        return "Interactive world map for tracking asset locations"

    @property
    def icon(self) -> str:
        return "🗺️"

    def render(self) -> Dict[str, Any]:
        """Render the asset map app interface"""
        html = """
        <div class="search-page-container" style="max-width: 1400px;">
            <h1 class="search-page-title">🗺️ Asset Map</h1>

            <div style="background: white; border-radius: 10px; padding: 20px; box-shadow: 0 4px 20px rgba(0,0,0,0.2); margin-bottom: 20px;">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
                    <div>
                        <h3 style="margin: 0;">Global Asset Locations</h3>
                        <p style="margin: 5px 0 0 0; color: #666; font-size: 0.9em;">
                            Click map to add • Drag markers to move • Click markers to edit/delete
                        </p>
                    </div>
                    <div>
                        <button onclick="clearAllMarkers()" style="padding: 8px 16px; background: #f56565; color: white; border: none; border-radius: 5px; cursor: pointer; margin-right: 10px;">
                            Clear All
                        </button>
                        <button onclick="exportMarkers()" style="padding: 8px 16px; background: #48bb78; color: white; border: none; border-radius: 5px; cursor: pointer;">
                            Export JSON
                        </button>
                    </div>
                </div>

                <div id="asset-map" style="height: 600px; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,0.1);"></div>

                <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; margin-top: 20px;">
                    <div style="padding: 15px; background: #f8f9fa; border-radius: 8px; text-align: center;">
                        <div style="font-size: 1.5em; font-weight: bold; color: #667eea;" id="marker-count">0</div>
                        <div style="color: #666; font-size: 0.9em;">Total Assets</div>
                    </div>
                    <div style="padding: 15px; background: #f8f9fa; border-radius: 8px; text-align: center;">
                        <div style="font-size: 1.5em; font-weight: bold; color: #48bb78;" id="country-count">0</div>
                        <div style="color: #666; font-size: 0.9em;">Countries</div>
                    </div>
                    <div style="padding: 15px; background: #f8f9fa; border-radius: 8px; text-align: center;">
                        <div style="font-size: 1.5em; font-weight: bold; color: #f6ad55;" id="tagged-count">0</div>
                        <div style="color: #666; font-size: 0.9em;">With IP</div>
                    </div>
                    <div style="padding: 15px; background: #f8f9fa; border-radius: 8px; text-align: center;">
                        <div style="font-size: 1.5em; font-weight: bold; color: #9f7aea;" id="critical-count">0</div>
                        <div style="color: #666; font-size: 0.9em;">With Contact</div>
                    </div>
                </div>
            </div>

            <a href="#dashboard" class="back-link" onclick="hideApp()">← Back to Dashboard</a>
        </div>
        """

        css = """
        .leaflet-container {
            font-family: inherit;
        }

        .asset-popup {
            min-width: 200px;
        }

        .asset-popup input,
        .asset-popup select,
        .asset-popup textarea {
            width: 100%;
            padding: 5px;
            margin: 5px 0;
            border: 1px solid #ddd;
            border-radius: 3px;
            box-sizing: border-box;
        }

        .asset-popup button {
            padding: 6px 12px;
            margin: 5px 5px 0 0;
            border: none;
            border-radius: 3px;
            cursor: pointer;
            font-size: 0.9em;
        }

        .btn-save {
            background: #667eea;
            color: white;
        }

        .btn-delete {
            background: #f56565;
            color: white;
        }

        .btn-cancel {
            background: #e2e8f0;
            color: #333;
        }
        """

        js = """
        // Leaflet map initialization
        let assetMap;
        let markers = {};
        let markerId = 1;

        // Load Leaflet CSS
        const leafletCSS = document.createElement('link');
        leafletCSS.rel = 'stylesheet';
        leafletCSS.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
        document.head.appendChild(leafletCSS);

        // Load Leaflet JS
        const leafletJS = document.createElement('script');
        leafletJS.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
        leafletJS.onload = initAssetMap;
        document.body.appendChild(leafletJS);

        function initAssetMap() {
            // Initialize map centered on world view with bounds to limit to single world map
            assetMap = L.map('asset-map', {
                minZoom: 2,
                maxBounds: [[-90, -180], [90, 180]],
                maxBoundsViscosity: 1.0
            }).setView([20, 0], 2);

            // Add OpenStreetMap tiles
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                maxZoom: 19,
                attribution: '© OpenStreetMap contributors',
                noWrap: true
            }).addTo(assetMap);

            // Load saved markers from localStorage
            loadMarkers();

            // Add click handler to create new markers
            assetMap.on('click', function(e) {
                addMarker(e.latlng.lat, e.latlng.lng);
            });

            updateStats();
        }

        function addMarker(lat, lng, data = null) {
            const id = data?.id || markerId++;
            const address = data?.address || '';
            const ip_address = data?.ip_address || '';
            const owner = data?.owner || '';
            const site = data?.site || '';
            const contact_info = data?.contact_info || '';

            const marker = L.marker([lat, lng], {
                draggable: true
            }).addTo(assetMap);

            // Marker drag event
            marker.on('dragend', function(e) {
                const pos = e.target.getLatLng();
                markers[id].lat = pos.lat;
                markers[id].lng = pos.lng;
                saveMarkers();
            });

            // Create popup content
            const popupContent = createPopupContent(id, address, ip_address, owner, site, contact_info);
            marker.bindPopup(popupContent);

            // Store marker data
            markers[id] = {
                id: id,
                marker: marker,
                lat: lat,
                lng: lng,
                address: address,
                ip_address: ip_address,
                owner: owner,
                site: site,
                contact_info: contact_info
            };

            saveMarkers();
            updateStats();

            return marker;
        }

        function createPopupContent(id, address, ip_address, owner, site, contact_info) {
            return `
                <div class="asset-popup">
                    <h4 style="margin: 0 0 10px 0;">Asset Details</h4>
                    <input type="text" id="address-${id}" placeholder="Address" value="${address}" />
                    <input type="text" id="ip-${id}" placeholder="IP Address (optional)" value="${ip_address}" />
                    <input type="text" id="owner-${id}" placeholder="Owner" value="${owner}" />
                    <input type="text" id="site-${id}" placeholder="Site" value="${site}" />
                    <textarea id="contact-${id}" placeholder="Contact Info" rows="2">${contact_info}</textarea>
                    <button class="btn-save" onclick="updateMarker(${id})">Save</button>
                    <button class="btn-delete" onclick="deleteMarker(${id})">Delete</button>
                    <button class="btn-cancel" onclick="assetMap.closePopup()">Cancel</button>
                </div>
            `;
        }

        function updateMarker(id) {
            const markerData = markers[id];
            markerData.address = document.getElementById(`address-${id}`).value;
            markerData.ip_address = document.getElementById(`ip-${id}`).value;
            markerData.owner = document.getElementById(`owner-${id}`).value;
            markerData.site = document.getElementById(`site-${id}`).value;
            markerData.contact_info = document.getElementById(`contact-${id}`).value;

            const popupContent = createPopupContent(
                id,
                markerData.address,
                markerData.ip_address,
                markerData.owner,
                markerData.site,
                markerData.contact_info
            );
            markerData.marker.setPopupContent(popupContent);

            saveMarkers();
            updateStats();
            assetMap.closePopup();
        }

        function deleteMarker(id) {
            if (confirm('Delete this asset marker?')) {
                assetMap.removeLayer(markers[id].marker);
                delete markers[id];
                saveMarkers();
                updateStats();
            }
        }

        function clearAllMarkers() {
            if (confirm('Delete all markers? This cannot be undone.')) {
                Object.values(markers).forEach(m => assetMap.removeLayer(m.marker));
                markers = {};
                saveMarkers();
                updateStats();
            }
        }

        function saveMarkers() {
            const data = Object.values(markers).map(m => ({
                id: m.id,
                lat: m.lat,
                lng: m.lng,
                address: m.address,
                ip_address: m.ip_address,
                owner: m.owner,
                site: m.site,
                contact_info: m.contact_info
            }));
            localStorage.setItem('siem_asset_markers', JSON.stringify(data));
        }

        function loadMarkers() {
            const saved = localStorage.getItem('siem_asset_markers');
            if (saved) {
                const data = JSON.parse(saved);
                data.forEach(m => {
                    addMarker(m.lat, m.lng, m);
                    if (m.id >= markerId) markerId = m.id + 1;
                });
            }
        }

        function exportMarkers() {
            const data = Object.values(markers).map(m => ({
                id: m.id,
                lat: m.lat,
                lng: m.lng,
                address: m.address,
                ip_address: m.ip_address,
                owner: m.owner,
                site: m.site,
                contact_info: m.contact_info
            }));

            const blob = new Blob([JSON.stringify(data, null, 2)], {type: 'application/json'});
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'asset-map-export.json';
            a.click();
            URL.revokeObjectURL(url);
        }

        function updateStats() {
            const markerArray = Object.values(markers);
            document.getElementById('marker-count').textContent = markerArray.length;

            // Count unique countries (rough estimate based on coordinates)
            const countries = new Set();
            markerArray.forEach(m => {
                const region = Math.floor(m.lng / 30) + '_' + Math.floor(m.lat / 30);
                countries.add(region);
            });
            document.getElementById('country-count').textContent = countries.size;

            // Count assets with IP addresses
            const withIP = markerArray.filter(m => m.ip_address && m.ip_address.trim().length > 0).length;
            document.getElementById('tagged-count').textContent = withIP;

            // Count assets with contact info
            const withContact = markerArray.filter(m => m.contact_info && m.contact_info.trim().length > 0).length;
            document.getElementById('critical-count').textContent = withContact;
        }
        """

        return {
            "html": html,
            "js": js,
            "css": css
        }


# Export the app instance
app = AssetMapApp()
