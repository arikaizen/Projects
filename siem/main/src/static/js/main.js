// Fetch and display system status
async function fetchStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        document.getElementById('status-display').innerHTML = `
            <p><strong>Status:</strong> ${data.status}</p>
            <p><strong>Message:</strong> ${data.message}</p>
        `;
    } catch (error) {
        document.getElementById('status-display').innerHTML = `
            <p style="color: red;">Error fetching status: ${error.message}</p>
        `;
    }
}

// Send data to the server
async function sendData() {
    const input = document.getElementById('data-input').value;
    const responseDiv = document.getElementById('response-display');

    if (!input.trim()) {
        responseDiv.innerHTML = '<p style="color: red;">Please enter some data</p>';
        return;
    }

    try {
        const jsonData = JSON.parse(input);
        const response = await fetch('/api/data', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(jsonData)
        });

        const result = await response.json();
        responseDiv.innerHTML = `
            <p><strong>Response:</strong></p>
            <pre>${JSON.stringify(result, null, 2)}</pre>
        `;
    } catch (error) {
        responseDiv.innerHTML = `
            <p style="color: red;">Error: ${error.message}</p>
            <p>Please ensure your input is valid JSON</p>
        `;
    }
}

// Search functionality
function performSearch() {
    const searchInput = document.getElementById('nav-search-input');
    const query = searchInput.value.trim();

    if (!query) {
        alert('Please enter a search term');
        return;
    }

    // Display search results in the response display
    const responseDiv = document.getElementById('response-display');
    responseDiv.innerHTML = `
        <p><strong>Search Results for:</strong> "${query}"</p>
        <p style="color: #666;">Search functionality ready for implementation</p>
        <p style="color: #666;">This will search through events, alerts, and logs</p>
    `;

    // Scroll to results
    responseDiv.scrollIntoView({ behavior: 'smooth' });
}

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

    // Remove expanded class after animation
    setTimeout(() => {
        searchInput.classList.remove('expanded');
    }, 400);
}

// Navigation link handling
document.addEventListener('DOMContentLoaded', function() {
    fetchStatus();

    // Refresh status every 5 seconds
    setInterval(fetchStatus, 5000);

    // Handle navigation links
    const navLinks = document.querySelectorAll('.nav-link');
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();

            // Update active state
            navLinks.forEach(l => l.classList.remove('active'));
            this.classList.add('active');

            // Handle navigation (placeholder for now)
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
    document.getElementById('nav-search-input').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            performSearch();
        }
    });

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
