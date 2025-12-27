// Global state
let map;
let currentMode = 'view'; // 'view', 'addNode', 'addEdge'
let nodes = [];
let edges = [];
let markers = {};
let polylines = {};
let selectedNodeForEdge = null;

// Initialize map
function initMap() {
    // Center on Pakistan (you can change to your preferred location)
    map = L.map('map').setView([30.3753, 69.3451], 6);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors',
        maxZoom: 19
    }).addTo(map);

    // Map click handler
    map.on('click', handleMapClick);

    // Load existing data
    loadNodes();
    loadEdges();
}

// Handle map clicks
function handleMapClick(e) {
    const lat = e.latlng.lat;
    const lon = e.latlng.lng;

    if (currentMode === 'addNode') {
        document.getElementById('nodeLat').value = lat.toFixed(6);
        document.getElementById('nodeLon').value = lon.toFixed(6);
        document.getElementById('saveNode').disabled = false;
        updateStatus('Node position selected. Enter name and click Save.');

        // Show temporary marker
        if (window.tempMarker) {
            map.removeLayer(window.tempMarker);
        }
        window.tempMarker = L.marker([lat, lon], {
            opacity: 0.6
        }).addTo(map);
    }
}

// Load nodes from backend
async function loadNodes() {
    try {
        const response = await fetch('/api/nodes');
        nodes = await response.json();
        renderNodes();
        updateStats();
        updateNodeSelects();
    } catch (error) {
        console.error('Failed to load nodes:', error);
    }
}

// Load edges from backend
async function loadEdges() {
    try {
        const response = await fetch('/api/edges');
        edges = await response.json();
        renderEdges();
        updateStats();
    } catch (error) {
        console.error('Failed to load edges:', error);
    }
}

// Render nodes on map
function renderNodes() {
    // Clear existing markers
    Object.values(markers).forEach(marker => map.removeLayer(marker));
    markers = {};

    nodes.forEach(node => {
        const marker = L.marker([node.lat, node.lon]).addTo(map);

        marker.bindPopup(`
            <div class="node-popup">
                <h4>${node.name}</h4>
                <p>ID: ${node.id}</p>
                <p>Lat: ${node.lat.toFixed(6)}</p>
                <p>Lon: ${node.lon.toFixed(6)}</p>
            </div>
        `);

        markers[node.id] = marker;
    });
}

// Render edges on map
function renderEdges() {
    // Clear existing polylines
    Object.values(polylines).forEach(line => map.removeLayer(line));
    polylines = {};

    edges.forEach(edge => {
        const sourceNode = nodes.find(n => n.id === edge.source);
        const destNode = nodes.find(n => n.id === edge.destination);

        if (sourceNode && destNode) {
            const polyline = L.polyline([
                [sourceNode.lat, sourceNode.lon],
                [destNode.lat, destNode.lon]
            ], {
                color: '#3498db',
                weight: 3,
                opacity: 0.7
            }).addTo(map);

            polyline.bindPopup(`
                <div class="node-popup">
                    <h4>${sourceNode.name} ↔ ${destNode.name}</h4>
                    <p>Distance: ${edge.distance} km</p>
                    <p>Traffic: ${edge.trafficWeight}x</p>
                </div>
            `);

            polylines[`${edge.source}-${edge.destination}`] = polyline;
        }
    });
}

// Save node to backend
async function saveNode() {
    const name = document.getElementById('nodeName').value.trim();
    const lat = parseFloat(document.getElementById('nodeLat').value);
    const lon = parseFloat(document.getElementById('nodeLon').value);

    if (!name) {
        alert('Please enter a node name');
        return;
    }

    try {
        const response = await fetch('/api/nodes', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, lat, lon })
        });

        if (response.ok) {
            updateStatus('Node saved successfully!');
            document.getElementById('nodeName').value = '';
            document.getElementById('nodeLat').value = '';
            document.getElementById('nodeLon').value = '';
            document.getElementById('saveNode').disabled = true;

            if (window.tempMarker) {
                map.removeLayer(window.tempMarker);
                window.tempMarker = null;
            }

            await loadNodes();
            await loadEdges();
        } else {
            alert('Failed to save node');
        }
    } catch (error) {
        alert('Error saving node: ' + error.message);
    }
}

// Save edge to backend
async function saveEdge() {
    const source = parseInt(document.getElementById('edgeFrom').value);
    const destination = parseInt(document.getElementById('edgeTo').value);
    const distance = parseFloat(document.getElementById('edgeDistance').value);
    const trafficWeight = parseFloat(document.getElementById('edgeTraffic').value);

    if (!source || !destination) {
        alert('Please select both nodes');
        return;
    }

    if (source === destination) {
        alert('Cannot create edge to the same node');
        return;
    }

    try {
        const response = await fetch('/api/edges', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ source, destination, distance, trafficWeight })
        });

        if (response.ok) {
            updateStatus('Edge saved successfully!');
            document.getElementById('edgeFrom').value = '';
            document.getElementById('edgeTo').value = '';
            document.getElementById('edgeDistance').value = '1.0';
            document.getElementById('edgeTraffic').value = '1.0';

            await loadNodes();
            await loadEdges();
        } else {
            alert('Failed to save edge');
        }
    } catch (error) {
        alert('Error saving edge: ' + error.message);
    }
}

// Update node selects
function updateNodeSelects() {
    const fromSelect = document.getElementById('edgeFrom');
    const toSelect = document.getElementById('edgeTo');
    const deleteNodeSelect = document.getElementById('deleteNodeSelect');

    fromSelect.innerHTML = '<option value="">Select node...</option>';
    toSelect.innerHTML = '<option value="">Select node...</option>';
    deleteNodeSelect.innerHTML = '<option value="">Select node to delete...</option>';

    nodes.forEach(node => {
        const option1 = document.createElement('option');
        option1.value = node.id;
        option1.textContent = node.name;
        fromSelect.appendChild(option1);

        const option2 = document.createElement('option');
        option2.value = node.id;
        option2.textContent = node.name;
        toSelect.appendChild(option2);

        const option3 = document.createElement('option');
        option3.value = node.id;
        option3.textContent = `${node.name} (ID: ${node.id})`;
        deleteNodeSelect.appendChild(option3);
    });
}

// Update statistics
function updateStats() {
    document.getElementById('nodeCount').textContent = nodes.length;
    document.getElementById('edgeCount').textContent = edges.length;
    updateEdgeSelect();
}

// Update edge select dropdown
function updateEdgeSelect() {
    const deleteEdgeSelect = document.getElementById('deleteEdgeSelect');
    deleteEdgeSelect.innerHTML = '<option value="">Select edge to delete...</option>';

    edges.forEach(edge => {
        const sourceNode = nodes.find(n => n.id === edge.source);
        const destNode = nodes.find(n => n.id === edge.destination);
        
        if (sourceNode && destNode) {
            const option = document.createElement('option');
            option.value = `${edge.source}-${edge.destination}`;
            option.textContent = `${sourceNode.name} ↔ ${destNode.name} (${edge.distance}km)`;
            deleteEdgeSelect.appendChild(option);
        }
    });
}

// Delete node
async function deleteNode() {
    const nodeId = parseInt(document.getElementById('deleteNodeSelect').value);
    
    if (!nodeId) {
        alert('Please select a node to delete');
        return;
    }

    if (!confirm('Are you sure you want to delete this node? All connected edges will also be removed.')) {
        return;
    }

    try {
        const response = await fetch(`/api/nodes/${nodeId}`, {
            method: 'DELETE'
        });

        if (response.ok) {
            updateStatus('Node deleted successfully!');
            document.getElementById('deleteNodeSelect').value = '';
            await loadNodes();
            await loadEdges();
        } else {
            alert('Failed to delete node');
        }
    } catch (error) {
        alert('Error deleting node: ' + error.message);
    }
}

// Delete edge
async function deleteEdge() {
    const edgeValue = document.getElementById('deleteEdgeSelect').value;
    
    if (!edgeValue) {
        alert('Please select an edge to delete');
        return;
    }

    if (!confirm('Are you sure you want to delete this edge?')) {
        return;
    }

    const [source, destination] = edgeValue.split('-').map(Number);

    try {
        const response = await fetch(`/api/edges/${source}/${destination}`, {
            method: 'DELETE'
        });

        if (response.ok) {
            updateStatus('Edge deleted successfully!');
            document.getElementById('deleteEdgeSelect').value = '';
            await loadNodes();
            await loadEdges();
        } else {
            alert('Failed to delete edge');
        }
    } catch (error) {
        alert('Error deleting edge: ' + error.message);
    }
}

// Update status message
function updateStatus(message) {
    document.getElementById('status').textContent = message;
}

// Set mode
function setMode(mode) {
    currentMode = mode;

    document.querySelectorAll('.btn-primary, .btn-secondary').forEach(btn => {
        btn.classList.remove('active');
    });

    if (mode === 'addNode') {
        document.getElementById('addNodeMode').classList.add('active');
        updateStatus('Click on the map to place a new node');
    } else {
        updateStatus('View mode - Click nodes and edges to see details');
    }
}

// Event listeners
document.getElementById('addNodeMode').addEventListener('click', () => setMode('addNode'));
document.getElementById('viewMode').addEventListener('click', () => setMode('view'));
document.getElementById('saveNode').addEventListener('click', saveNode);
document.getElementById('saveEdge').addEventListener('click', saveEdge);
document.getElementById('deleteNode').addEventListener('click', deleteNode);
document.getElementById('deleteEdge').addEventListener('click', deleteEdge);

// Initialize
window.addEventListener('DOMContentLoaded', () => {
    initMap();
    updateStatus('Ready - Select a mode to begin');
});
