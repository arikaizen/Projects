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
        function executeSearch() {
            const searchInput = document.getElementById('main-search-input');
            const query = searchInput.value.trim();
            const resultsDiv = document.getElementById('search-results');

            if (!query) {
                resultsDiv.innerHTML = '<p style="color: #999;">Please enter a search term</p>';
                return;
            }

            searchInput.classList.add('expanded');

            // Simulate search
            resultsDiv.innerHTML = `
                <h3>Search Results for: "${query}"</h3>
                <div style="margin-top: 20px;">
                    <div style="padding: 15px; border-left: 4px solid #667eea; background: #f8f9fa; margin-bottom: 10px;">
                        <strong>Event #1234</strong> - Security alert detected at 10:23 AM
                    </div>
                    <div style="padding: 15px; border-left: 4px solid #667eea; background: #f8f9fa; margin-bottom: 10px;">
                        <strong>Log Entry #5678</strong> - System activity logged at 11:45 AM
                    </div>
                    <div style="padding: 15px; border-left: 4px solid #667eea; background: #f8f9fa;">
                        <strong>Alert #9012</strong> - Suspicious activity detected at 2:15 PM
                    </div>
                </div>
                <p style="margin-top: 20px; color: #666;">Showing 3 results. Search functionality ready for backend integration.</p>
            `;

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
