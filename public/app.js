async function testConnection() {
    const statusDiv = document.getElementById('status');
    statusDiv.textContent = 'Connecting...';
    statusDiv.className = 'info';
    
    try {
        const response = await fetch('/api/test');
        const data = await response.json();
        
        if (data.status === 'connected') {
            statusDiv.textContent = '✓ ' + data.message;
            statusDiv.className = 'success';
            alert('Backend is connected!');
        } else {
            statusDiv.textContent = '✗ Unexpected response';
            statusDiv.className = 'error';
        }
    } catch (error) {
        statusDiv.textContent = '✗ Connection failed: ' + error.message;
        statusDiv.className = 'error';
        alert('Failed to connect to backend');
    }
}
