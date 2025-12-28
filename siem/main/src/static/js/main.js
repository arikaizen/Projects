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

    // Enable Enter key for search
    document.getElementById('nav-search-input').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            performSearch();
        }
    });
});
