"""
Search App for SIEM
Provides advanced search functionality for events, alerts, and logs.
"""

from . import BaseApp
from typing import Dict, Any


class SearchApp(BaseApp):
    """
    Advanced search application for SIEM.

    Features:
    - Multi-line search queries
    - Search across events, alerts, and logs
    - Real-time results
    """

    @property
    def name(self) -> str:
        return "Search"

    @property
    def id(self) -> str:
        return "search-app"

    @property
    def description(self) -> str:
        return "Advanced search for events, alerts, and logs"

    @property
    def icon(self) -> str:
        return "🔍"

    def render(self) -> Dict[str, Any]:
        """Render the search app interface"""
        html = """
        <div class="search-page-container">
            <h1 class="search-page-title">🔍 Search Everything</h1>
            <div class="search-bar-wrapper">
                <textarea id="main-search-input" class="main-search-bar" rows="3"
                          placeholder="Type your search query...&#10;Use Shift+Enter for new line&#10;Press Enter to search"></textarea>
                <button onclick="executeSearch()" class="main-search-button">Search</button>
            </div>
            <div id="search-results" class="search-results">
                <p style="color: #999; text-align: center;">Enter a search query to see results</p>
            </div>
            <a href="#dashboard" class="back-link" onclick="hideApp()">← Back to Dashboard</a>
        </div>
        """

        js = """
        async function executeSearch() {
            const searchInput = document.getElementById('main-search-input');
            const query = searchInput.value.trim();
            const resultsDiv = document.getElementById('search-results');

            if (!query) {
                resultsDiv.innerHTML = '<p style="color: #999;">Please enter a search term</p>';
                return;
            }

            searchInput.classList.add('expanded');
            resultsDiv.innerHTML = '<p style="color: #667eea;">Searching...</p>';

            try {
                // Query the SIEM logs API
                const response = await fetch(`/api/logs?q=${encodeURIComponent(query)}&limit=100`);
                const data = await response.json();

                if (data.logs && data.logs.length > 0) {
                    let resultsHTML = `<h3>Search Results for: "${query}" (${data.returned} of ${data.total} total logs)</h3>`;
                    resultsHTML += '<div style="margin-top: 20px;">';

                    data.logs.forEach((log, index) => {
                        const eventId = log.event_id || 'N/A';
                        const level = log.level || 'Unknown';
                        const channel = log.channel || 'Unknown';
                        const computer = log.computer || 'Unknown';
                        const source = log.source || 'Unknown';
                        const receivedAt = log.received_at || 'Unknown';

                        let levelColor = '#667eea';
                        if (level === '1') levelColor = '#f56565';  // Critical
                        else if (level === '2') levelColor = '#f6ad55';  // Error
                        else if (level === '3') levelColor = '#ecc94b';  // Warning
                        else if (level === '4') levelColor = '#48bb78';  // Information

                        resultsHTML += `
                            <div style="padding: 15px; border-left: 4px solid ${levelColor}; background: #f8f9fa; margin-bottom: 10px; border-radius: 5px;">
                                <div style="display: flex; justify-content: space-between; margin-bottom: 5px;">
                                    <strong>Event ID: ${eventId}</strong>
                                    <span style="color: #666; font-size: 0.9em;">${receivedAt}</span>
                                </div>
                                <div style="color: #666; font-size: 0.9em; margin-top: 5px;">
                                    <span><strong>Channel:</strong> ${channel}</span> |
                                    <span><strong>Computer:</strong> ${computer}</span> |
                                    <span><strong>Source:</strong> ${source}</span> |
                                    <span><strong>Level:</strong> ${level}</span>
                                </div>
                                <details style="margin-top: 10px;">
                                    <summary style="cursor: pointer; color: #667eea;">View Raw JSON</summary>
                                    <pre style="background: #2d3748; color: #e2e8f0; padding: 10px; border-radius: 5px; margin-top: 5px; overflow-x: auto;">${JSON.stringify(log, null, 2)}</pre>
                                </details>
                            </div>
                        `;
                    });

                    resultsHTML += '</div>';
                    resultsDiv.innerHTML = resultsHTML;
                } else {
                    resultsDiv.innerHTML = `
                        <h3>Search Results for: "${query}"</h3>
                        <p style="color: #666; margin-top: 20px;">No logs found matching your query.</p>
                        <p style="color: #999; font-size: 0.9em;">Total logs in database: ${data.total}</p>
                    `;
                }
            } catch (error) {
                console.error('Search error:', error);
                resultsDiv.innerHTML = `
                    <p style="color: #f56565;">Error performing search: ${error.message}</p>
                    <p style="color: #666; font-size: 0.9em;">Make sure the SIEM server is running and accepting log connections.</p>
                `;
            }

            setTimeout(() => {
                searchInput.classList.remove('expanded');
            }, 400);
        }

        // Enable Enter key for search
        const mainSearchInput = document.getElementById('main-search-input');
        if (mainSearchInput) {
            mainSearchInput.addEventListener('keydown', function(e) {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    executeSearch();
                }
            });
        }
        """

        return {
            "html": html,
            "js": js,
            "css": ""
        }


# Export the app instance
app = SearchApp()
