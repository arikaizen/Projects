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

// Load status on page load
document.addEventListener('DOMContentLoaded', function() {
    fetchStatus();

    // Refresh status every 5 seconds
    setInterval(fetchStatus, 5000);
});
