// Route Finder - Main Page
let map;
let nodes = [];
let edges = [];
let markers = {};
let routeLine = null;

// Initialize map
function initMap() {
    map = L.map('map').setView([30.3753, 69.3451], 6);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© OpenStreetMap contributors',
        maxZoom: 19
    }).addTo(map);

    loadNodes();
    loadEdges();
}

// Load nodes
async function loadNodes() {
    try {
        const response = await fetch('/api/nodes');
        nodes = await response.json();
        renderNodes();
        updateNodeSelects();
    } catch (error) {
        console.error('Failed to load nodes:', error);
    }
}

// Load edges
async function loadEdges() {
    try {
        const response = await fetch('/api/edges');
        edges = await response.json();
        updateEdgeSelect();
    } catch (error) {
        console.error('Failed to load edges:', error);
    }
}

// Render nodes
function renderNodes() {
    Object.values(markers).forEach(marker => map.removeLayer(marker));
    markers = {};

    nodes.forEach(node => {
        const marker = L.marker([node.lat, node.lon]).addTo(map);
        marker.bindPopup(`<b>${node.name}</b><br>ID: ${node.id}`);
        markers[node.id] = marker;
    });
}

// Update node selects
function updateNodeSelects() {
    const startSelect = document.getElementById('startNode');
    const endSelect = document.getElementById('endNode');

    startSelect.innerHTML = '<option value="">Select start node...</option>';
    endSelect.innerHTML = '<option value="">Select end node...</option>';

    nodes.forEach(node => {
        startSelect.innerHTML += `<option value="${node.id}">${node.name} (ID: ${node.id})</option>`;
        endSelect.innerHTML += `<option value="${node.id}">${node.name} (ID: ${node.id})</option>`;
    });
}

// Update edge select
function updateEdgeSelect() {
    const edgeSelect = document.getElementById('blockEdge');
    edgeSelect.innerHTML = '<option value="">Select edge...</option>';

    edges.forEach(edge => {
        const fromNode = nodes.find(n => n.id === edge.source);
        const toNode = nodes.find(n => n.id === edge.destination);
        if (fromNode && toNode) {
            const status = edge.blocked ? '🚫 BLOCKED' : '✅';
            edgeSelect.innerHTML += `<option value="${edge.source}-${edge.destination}">${status} ${fromNode.name} ↔ ${toNode.name}</option>`;
        }
    });
}

// Find route
async function findRoute() {
    const startId = document.getElementById('startNode').value;
    const endId = document.getElementById('endNode').value;

    if (!startId || !endId) {
        alert('Please select both start and end nodes');
        return;
    }

    try {
        const response = await fetch(`/api/get_route?start=${startId}&end=${endId}`);
        const data = await response.json();

        if (data.success && data.path) {
            drawRoute(data.path);
            showRouteInfo(data.path);
        } else {
            alert('No route found!');
            clearRoute();
        }
    } catch (error) {
        console.error('Failed to find route:', error);
        alert('Error finding route');
    }
}

// Draw route
function drawRoute(path) {
    if (routeLine) {
        map.removeLayer(routeLine);
    }

    const coordinates = path.map(node => [node.lat, node.lon]);
    routeLine = L.polyline(coordinates, {
        color: 'red',
        weight: 5,
        opacity: 0.7
    }).addTo(map);

    map.fitBounds(routeLine.getBounds());
}

// Show route info
function showRouteInfo(path) {
    const routeInfo = document.getElementById('routeInfo');
    const stops = path.map(node => node.name).join(' → ');
    routeInfo.innerHTML = `
        <div class="route-info">
            <strong>Route Found!</strong><br>
            ${stops}<br>
            <small>Total stops: ${path.length}</small>
        </div>
    `;
}

// Clear route
function clearRoute() {
    if (routeLine) {
        map.removeLayer(routeLine);
        routeLine = null;
    }
    document.getElementById('routeInfo').innerHTML = '';
}

// Toggle edge block
async function toggleEdgeBlock() {
    const edgeValue = document.getElementById('blockEdge').value;
    if (!edgeValue) {
        alert('Please select an edge');
        return;
    }

    const [source, dest] = edgeValue.split('-').map(Number);

    try {
        const response = await fetch(`/api/edges/${source}/${dest}/toggle_block`, {
            method: 'POST'
        });

        const data = await response.json();
        if (data.success) {
            alert('Edge block toggled! Reload to see changes.');
            loadEdges();
            
            // If there's a route displayed, recalculate it
            const startId = document.getElementById('startNode').value;
            const endId = document.getElementById('endNode').value;
            if (startId && endId && routeLine) {
                findRoute();
            }
        }
    } catch (error) {
        console.error('Failed to toggle block:', error);
        alert('Error toggling block');
    }
}

// Event listeners
document.getElementById('findRoute').addEventListener('click', findRoute);
document.getElementById('clearRoute').addEventListener('click', clearRoute);
document.getElementById('toggleBlock').addEventListener('click', toggleEdgeBlock);

// Initialize
window.addEventListener('DOMContentLoaded', () => {
    initMap();
});