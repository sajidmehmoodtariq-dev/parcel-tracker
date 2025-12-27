#include <crow.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include "src/Graph.h"
#include "src/PriorityQueue.h"
#include "src/HashTable.h"
#include "src/Stack.h"
#include "src/Rider.h"

// Notification structure
struct Notification {
    std::string type; // "pickup" or "delivery"
    int parcelId;
    int riderId;
    std::string riderName;
    std::string locationName;
    std::string timestamp;
    
    Notification() {}
    Notification(std::string t, int pId, int rId, std::string rName, std::string loc, std::string ts)
        : type(t), parcelId(pId), riderId(rId), riderName(rName), locationName(loc), timestamp(ts) {}
};

// Structure to save state for undo
struct SystemSnapshot {
    Vector<Parcel> parcels;
    Vector<Rider> riders;
    Vector<Notification> notifications;
};

// Global instances
Graph cityGraph;
PriorityQueue parcelQueue;
HashTable<int, Parcel> parcelTracker; // ParcelID -> Parcel
Stack<SystemSnapshot> undoStack; // For undo operations
RiderManager riderManager; // Rider management system
Vector<Notification> notifications; // Recent notifications
const std::string MAP_FILE = "data/map_data.txt";
const std::string PARCELS_FILE = "data/parcels_data.txt";
const std::string RIDERS_FILE = "data/riders_data.txt";

// Helper function to read file content
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Save parcels to file
bool saveParcels(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    Vector<int> parcelIds = parcelTracker.getAllKeys();
    for (int i = 0; i < parcelIds.getSize(); i++) {
        Parcel* p = parcelTracker.get(parcelIds[i]);
        if (p) {
            file << p->id << "," << p->priority << "," << p->weight << ","
                 << p->currentLocationId << "," << p->destinationId << ","
                 << p->status << "," << p->routeIndex << "," << p->riderId << ","
                 << p->currentLat << "," << p->currentLon;
            
            // Save route
            file << ",";
            for (int j = 0; j < p->route.getSize(); j++) {
                file << p->route[j];
                if (j < p->route.getSize() - 1) file << ";";
            }
            
            file << "\n";
        }
    }
    
    file.close();
    return true;
}

// Load parcels from file
bool loadParcels(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    std::string line;
    int count = 0;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string token;
        Parcel parcel;
        
        // Parse fields
        std::getline(ss, token, ','); parcel.id = std::stoi(token);
        std::getline(ss, token, ','); parcel.priority = std::stoi(token);
        std::getline(ss, token, ','); parcel.weight = std::stod(token);
        std::getline(ss, token, ','); parcel.currentLocationId = std::stoi(token);
        std::getline(ss, token, ','); parcel.destinationId = std::stoi(token);
        std::getline(ss, parcel.status, ',');
        std::getline(ss, token, ','); parcel.routeIndex = std::stoi(token);
        std::getline(ss, token, ','); parcel.riderId = std::stoi(token);
        std::getline(ss, token, ','); parcel.currentLat = std::stod(token);
        std::getline(ss, token, ','); parcel.currentLon = std::stod(token);
        
        // Parse route
        std::getline(ss, token, ',');
        if (!token.empty()) {
            std::stringstream routeStream(token);
            std::string nodeId;
            while (std::getline(routeStream, nodeId, ';')) {
                parcel.route.push_back(std::stoi(nodeId));
            }
        }
        
        parcelTracker.insert(parcel.id, parcel);
        // Don't add to priority queue - we just need tracker for persistence
        count++;
    }
    
    file.close();
    return count > 0;
}

// Advance time - move riders and their parcels along routes
void advanceTime() {
    // Save complete system state before advancing (for undo)
    SystemSnapshot snapshot;
    Vector<int> parcelIds = parcelTracker.getAllKeys();
    for (int i = 0; i < parcelIds.getSize(); i++) {
        Parcel* p = parcelTracker.get(parcelIds[i]);
        if (p) snapshot.parcels.push_back(*p);
    }
    snapshot.riders = riderManager.getRiders();
    snapshot.notifications = notifications;
    undoStack.push(snapshot);
    
    // Move one rider at a time (simulation control)
    riderManager.advanceNextRider(cityGraph);
    
    // Save riders state
    riderManager.saveToFile(RIDERS_FILE);
    
    // Update all parcels based on their assigned riders
    for (int i = 0; i < parcelIds.getSize(); i++) {
        Parcel* parcel = parcelTracker.get(parcelIds[i]);
        
        if (parcel && parcel->status == "in_transit" && parcel->riderId > 0) {
            // Get rider's current position
            Rider* rider = riderManager.getRiderById(parcel->riderId);
            if (rider) {
                int oldLocationId = parcel->currentLocationId;
                
                // Update parcel to rider's position
                parcel->currentLocationId = rider->currentLocationId;
                parcel->currentLat = rider->currentLat;
                parcel->currentLon = rider->currentLon;
                
                // Update route index to match rider's progress
                for (int j = 0; j < parcel->route.getSize(); j++) {
                    if (parcel->route[j] == rider->currentLocationId) {
                        parcel->routeIndex = j;
                        break;
                    }
                }
                
                // Check if parcel was just picked up (moved from source)
                if (parcel->routeIndex == 0 && oldLocationId != parcel->currentLocationId) {
                    Node* sourceNode = cityGraph.getNodeById(parcel->route[0]);
                    if (sourceNode) {
                        std::time_t now = std::time(nullptr);
                        char timeStr[100];
                        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
                        notifications.push_back(Notification("pickup", parcel->id, rider->id, 
                            rider->name, sourceNode->name, std::string(timeStr)));
                    }
                }
                
                // Check if reached destination
                if (parcel->currentLocationId == parcel->destinationId) {
                    Node* destNode = cityGraph.getNodeById(parcel->destinationId);
                    if (destNode) {
                        std::time_t now = std::time(nullptr);
                        char timeStr[100];
                        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
                        notifications.push_back(Notification("delivery", parcel->id, rider->id,
                            rider->name, destNode->name, std::string(timeStr)));
                    }
                    parcel->status = "delivered";
                    riderManager.removeParcelFromRider(parcel->riderId, parcel->id);
                    parcel->riderId = -1;
                }
            }
        }
    }
    
    // Save parcels state
    saveParcels(PARCELS_FILE);
}

int main() {
    crow::SimpleApp app;

    // Load existing map data on startup
    if (cityGraph.loadMap(MAP_FILE)) {
        std::cout << "Map data loaded successfully!" << std::endl;
    } else {
        std::cout << "No existing map data found. Starting fresh." << std::endl;
    }
    
    // Load existing rider data on startup
    if (riderManager.loadFromFile(RIDERS_FILE)) {
        std::cout << "Rider data loaded successfully! " << riderManager.getRiders().getSize() << " riders restored." << std::endl;
    } else {
        std::cout << "Riders will be created on-demand when parcels are sent" << std::endl;
    }
    
    // Load existing parcel data on startup
    if (loadParcels(PARCELS_FILE)) {
        std::cout << "Parcel data loaded successfully! " << parcelTracker.getSize() << " parcels restored." << std::endl;
    } else {
        std::cout << "No existing parcel data found. Starting fresh." << std::endl;
    }

    Graph* graphPtr = &cityGraph;

    // ====================
    // API ENDPOINTS
    // ====================

    // Test endpoint
    CROW_ROUTE(app, "/api/test")
    ([](){
        crow::json::wvalue response;
        response["status"] = "connected";
        response["message"] = "Backend is connected!";
        return response;
    });

    // GET all nodes
    CROW_ROUTE(app, "/api/nodes").methods(crow::HTTPMethod::GET)
    ([graphPtr](){
        crow::json::wvalue response;
        const Vector<Node>& nodes = graphPtr->getNodes();
        
        response = crow::json::wvalue::list();
        for (int i = 0; i < nodes.getSize(); i++) {
            response[i]["id"] = nodes[i].id;
            response[i]["name"] = nodes[i].name;
            response[i]["lat"] = nodes[i].lat;
            response[i]["lon"] = nodes[i].lon;
        }
        
        return response;
    });

    // POST new node
    CROW_ROUTE(app, "/api/nodes").methods(crow::HTTPMethod::POST)
    ([graphPtr](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("name") || !body.has("lat") || !body.has("lon")) {
            return crow::response(400, "Invalid request body");
        }

        std::string name = body["name"].s();
        double lat = body["lat"].d();
        double lon = body["lon"].d();

        if (graphPtr->addNode(name, lat, lon)) {
            graphPtr->saveMap(MAP_FILE); // Persist changes
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "Node added successfully";
            return crow::response(201, response);
        }

        return crow::response(500, "Failed to add node");
    });

    // DELETE node
    CROW_ROUTE(app, "/api/nodes/<int>").methods(crow::HTTPMethod::Delete)
    ([graphPtr](int nodeId){
        if (graphPtr->deleteNode(nodeId)) {
            graphPtr->saveMap(MAP_FILE); // Persist changes
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "Node deleted successfully";
            return crow::response(200, response);
        }
        return crow::response(404, "Node not found");
    });

    // GET all edges
    CROW_ROUTE(app, "/api/edges").methods(crow::HTTPMethod::GET)
    ([graphPtr](){
        crow::json::wvalue response;
        Vector<Edge> edges = graphPtr->getAllEdges();
        
        response = crow::json::wvalue::list();
        for (int i = 0; i < edges.getSize(); i++) {
            response[i]["source"] = edges[i].source;
            response[i]["destination"] = edges[i].destination;
            response[i]["distance"] = edges[i].distance;
            response[i]["trafficWeight"] = edges[i].trafficWeight;
            response[i]["blocked"] = edges[i].blocked;
        }
        
        return response;
    });

    // POST new edge
    CROW_ROUTE(app, "/api/edges").methods(crow::HTTPMethod::POST)
    ([graphPtr](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("source") || !body.has("destination") || !body.has("distance")) {
            return crow::response(400, "Invalid request body");
        }

        int source = body["source"].i();
        int destination = body["destination"].i();
        double distance = body["distance"].d();
        double trafficWeight = body.has("trafficWeight") ? body["trafficWeight"].d() : 1.0;

        if (graphPtr->addEdge(source, destination, distance, trafficWeight)) {
            graphPtr->saveMap(MAP_FILE); // Persist changes
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "Edge added successfully";
            return crow::response(201, response);
        }

        return crow::response(500, "Failed to add edge");
    });

    // DELETE edge
    CROW_ROUTE(app, "/api/edges/<int>/<int>").methods(crow::HTTPMethod::Delete)
    ([graphPtr](int sourceId, int destId){
        // Need to implement deleteEdge in Graph class
        // For now, reload and remove the edge
        Vector<Edge> edges = graphPtr->getAllEdges();
        bool found = false;
        
        for (int i = 0; i < edges.getSize(); i++) {
            if ((edges[i].source == sourceId && edges[i].destination == destId) ||
                (edges[i].source == destId && edges[i].destination == sourceId)) {
                found = true;
                break;
            }
        }
        
        if (found) {
            // Save current nodes
            const Vector<Node>& nodes = graphPtr->getNodes();
            std::string tempFile = "temp_map.txt";
            
            // Rebuild graph without this edge
            Graph newGraph;
            for (int i = 0; i < nodes.getSize(); i++) {
                newGraph.addNodeWithId(nodes[i].id, nodes[i].name, nodes[i].lat, nodes[i].lon);
            }
            
            for (int i = 0; i < edges.getSize(); i++) {
                if (!((edges[i].source == sourceId && edges[i].destination == destId) ||
                      (edges[i].source == destId && edges[i].destination == sourceId))) {
                    newGraph.addEdge(edges[i].source, edges[i].destination, 
                                   edges[i].distance, edges[i].trafficWeight);
                }
            }
            
            newGraph.saveMap(MAP_FILE);
            graphPtr->loadMap(MAP_FILE);
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "Edge deleted successfully";
            return crow::response(200, response);
        }
        
        return crow::response(404, "Edge not found");
    });

    // GET route (Dijkstra)
    CROW_ROUTE(app, "/api/get_route")
    ([graphPtr](const crow::request& req){
        auto start = req.url_params.get("start");
        auto end = req.url_params.get("end");

        if (!start || !end) {
            return crow::response(400, "Missing start or end parameter");
        }

        int startId = std::stoi(start);
        int endId = std::stoi(end);

        Vector<Node> path = graphPtr->dijkstra(startId, endId);

        crow::json::wvalue response;
        if (path.getSize() == 0) {
            response["success"] = false;
            response["message"] = "No path found";
            return crow::response(404, response);
        }

        response["success"] = true;
        response["path"] = crow::json::wvalue::list();
        for (int i = 0; i < path.getSize(); i++) {
            crow::json::wvalue node;
            node["id"] = path[i].id;
            node["name"] = path[i].name;
            node["lat"] = path[i].lat;
            node["lon"] = path[i].lon;
            response["path"][i] = std::move(node);
        }

        return crow::response(200, response);
    });

    // POST toggle edge block
    CROW_ROUTE(app, "/api/edges/<int>/<int>/toggle_block").methods(crow::HTTPMethod::POST)
    ([graphPtr](int sourceId, int destId){
        if (graphPtr->toggleEdgeBlock(sourceId, destId)) {
            graphPtr->saveMap(MAP_FILE);
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "Edge block toggled";
            return crow::response(200, response);
        }
        return crow::response(404, "Edge not found");
    });

    // ====================
    // PARCEL API ENDPOINTS
    // ====================

    // POST new parcel
    CROW_ROUTE(app, "/api/new_parcel").methods(crow::HTTPMethod::POST)
    ([graphPtr](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("priority") || !body.has("weight") || !body.has("location") || !body.has("destination")) {
            return crow::response(400, "Invalid request body");
        }

        int priority = body["priority"].i(); // 1 = Standard, 2 = Overnight
        double weight = body["weight"].d();
        int location = body["location"].i();
        int destination = body["destination"].i();

        if (priority < 1 || priority > 2 || weight <= 0) {
            return crow::response(400, "Invalid priority or weight");
        }

        if (location == destination) {
            return crow::response(400, "Pickup and destination cannot be the same");
        }

        // Calculate route using Dijkstra
        Vector<Node> path = graphPtr->dijkstra(location, destination);
        
        if (path.getSize() == 0) {
            return crow::response(400, "No route found between locations");
        }

        int parcelId = parcelQueue.insert(priority, weight, location, destination);

        // Create parcel with route
        Parcel newParcel(parcelId, priority, weight, location, destination, "in_transit");
        for (int i = 0; i < path.getSize(); i++) {
            newParcel.route.push_back(path[i].id);
        }
        newParcel.routeIndex = 0;
        
        // Set initial position
        Node* startNode = graphPtr->getNodeById(location);
        if (startNode) {
            newParcel.currentLat = startNode->lat;
            newParcel.currentLon = startNode->lon;
        }
        
        // Add to tracker first
        parcelTracker.insert(parcelId, newParcel);
        
        // Find if any existing rider's route contains source or destination
        int assignedRiderId = -1;
        std::string riderName = "None";
        const Vector<Rider>& allRiders = riderManager.getRiders();
        
        for (int i = 0; i < allRiders.getSize(); i++) {
            const Rider& rider = allRiders[i];
            if (!rider.canAcceptParcel(weight)) continue;
            
            // Check if parcel source and destination are on rider's route
            int sourceIndexInRoute = -1;
            int destIndexInRoute = -1;
            
            for (int j = 0; j < rider.plannedRoute.getSize(); j++) {
                if (rider.plannedRoute[j] == location) {
                    sourceIndexInRoute = j;
                }
                if (rider.plannedRoute[j] == destination) {
                    destIndexInRoute = j;
                }
            }
            
            // Only consider if both points are on route OR can be extended
            bool routeCompatible = false;
            
            // Case 1: Both source and destination on route, and source comes before destination (forward direction)
            if (sourceIndexInRoute != -1 && destIndexInRoute != -1) {
                if (sourceIndexInRoute < destIndexInRoute) {
                    routeCompatible = true;
                }
                // If source comes after destination, it's backwards - need new rider
            }
            // Case 2: Source on route, destination not on route (can extend)
            else if (sourceIndexInRoute != -1 && destIndexInRoute == -1) {
                routeCompatible = true;
            }
            // Case 3: Destination on route, source not on route (can extend from beginning)
            else if (sourceIndexInRoute == -1 && destIndexInRoute != -1) {
                routeCompatible = true;
            }
            
            if (routeCompatible) {
                // Check priority compatibility - don't assign urgent parcels to riders with standard parcels
                bool priorityCompatible = true;
                
                // If this is an urgent parcel (priority 2), check if rider has any standard parcels
                if (priority == 2) {
                    for (int k = 0; k < rider.assignedParcelIds.getSize(); k++) {
                        Parcel* existingParcel = parcelTracker.get(rider.assignedParcelIds[k]);
                        if (existingParcel && existingParcel->priority == 1) {
                            // Rider has standard parcels, don't assign urgent parcel here
                            priorityCompatible = false;
                            break;
                        }
                    }
                }
                // If this is a standard parcel (priority 1), check if rider has any urgent parcels
                else if (priority == 1) {
                    for (int k = 0; k < rider.assignedParcelIds.getSize(); k++) {
                        Parcel* existingParcel = parcelTracker.get(rider.assignedParcelIds[k]);
                        if (existingParcel && existingParcel->priority == 2) {
                            // Rider has urgent parcels, don't assign standard parcel here
                            priorityCompatible = false;
                            break;
                        }
                    }
                }
                
                if (priorityCompatible) {
                    assignedRiderId = rider.id;
                    break;
                }
            }
        }
        
        // If no suitable vehicle found, create a new one at source location
        if (assignedRiderId == -1) {
            Node* sourceNode = graphPtr->getNodeById(location);
            if (sourceNode) {
                // Determine vehicle type based on weight
                VehicleType vehicleType = (weight <= 5.0) ? RIDER : CAR;
                std::string vehicleTypeStr = (vehicleType == RIDER) ? "Rider" : "Car";
                std::string newRiderName = vehicleTypeStr + " " + std::to_string(riderManager.getRiders().getSize() + 1);
                assignedRiderId = riderManager.addRider(newRiderName, vehicleType, location, sourceNode->lat, sourceNode->lon);
                riderName = newRiderName;
                // Save riders after creating new one
                riderManager.saveToFile(RIDERS_FILE);
            }
        } else {
            Rider* existingRider = riderManager.getRiderById(assignedRiderId);
            if (existingRider) {
                riderName = existingRider->name;
            }
        }
        
        // Assign parcel to rider
        if (assignedRiderId > 0) {
            if (riderManager.assignParcelToRider(assignedRiderId, parcelId, newParcel.route, cityGraph, weight)) {
                Parcel* trackedParcel = parcelTracker.get(parcelId);
                if (trackedParcel) {
                    trackedParcel->riderId = assignedRiderId;
                }
                // Save riders after assignment (route might have been extended)
                riderManager.saveToFile(RIDERS_FILE);
            }
        }
        
        // Save parcels after creation
        saveParcels(PARCELS_FILE);

        crow::json::wvalue response;
        response["success"] = true;
        response["parcelId"] = parcelId;
        response["riderId"] = assignedRiderId;
        response["riderName"] = riderName;
        response["message"] = "Parcel created and assigned to " + riderName;
        response["routeLength"] = path.getSize();
        return crow::response(201, response);
    });

    // GET all parcels
    CROW_ROUTE(app, "/api/parcels")
    ([](){
        // Get parcels from tracker instead of queue (for persistence)
        Vector<int> parcelIds = parcelTracker.getAllKeys();
        
        crow::json::wvalue response;
        response = crow::json::wvalue::list();

        for (int i = 0; i < parcelIds.getSize(); i++) {
            Parcel* parcel = parcelTracker.get(parcelIds[i]);
            if (parcel) {
                response[i]["id"] = parcel->id;
                response[i]["priority"] = parcel->priority;
                response[i]["weight"] = parcel->weight;
                response[i]["currentLocationId"] = parcel->currentLocationId;
                response[i]["destinationId"] = parcel->destinationId;
                response[i]["status"] = parcel->status;
            }
        }

        return response;
    });

    // GET parcel count
    CROW_ROUTE(app, "/api/parcels/count")
    ([](){
        crow::json::wvalue response;
        response["count"] = parcelTracker.getSize();
        return response;
    });
    
    // GET recent notifications
    CROW_ROUTE(app, "/api/notifications")
    ([](){
        crow::json::wvalue response;
        response = crow::json::wvalue::list();
        
        // Return last 10 notifications
        int start = notifications.getSize() > 10 ? notifications.getSize() - 10 : 0;
        for (int i = start; i < notifications.getSize(); i++) {
            int idx = i - start;
            response[idx]["type"] = notifications[i].type;
            response[idx]["parcelId"] = notifications[i].parcelId;
            response[idx]["riderId"] = notifications[i].riderId;
            response[idx]["riderName"] = notifications[i].riderName;
            response[idx]["locationName"] = notifications[i].locationName;
            response[idx]["timestamp"] = notifications[i].timestamp;
        }
        
        return response;
    });
    
    // POST clear notifications
    CROW_ROUTE(app, "/api/notifications/clear").methods(crow::HTTPMethod::POST)
    ([](){
        notifications.clear();
        crow::json::wvalue response;
        response["success"] = true;
        return crow::response(200, response);
    });

    // GET all rider positions (for live tracking)
    CROW_ROUTE(app, "/api/all_positions")
    ([](){
        const Vector<Rider>& riders = riderManager.getRiders();
        
        crow::json::wvalue response;
        response = crow::json::wvalue::list();
        
        for (int i = 0; i < riders.getSize(); i++) {
            const Rider& rider = riders[i];
            if (rider.status != "offline" && rider.assignedParcelIds.getSize() > 0) {
                response[i]["riderId"] = rider.id;
                response[i]["riderName"] = rider.name;
                response[i]["vehicleType"] = rider.getVehicleTypeString();
                response[i]["lat"] = rider.currentLat;
                response[i]["lon"] = rider.currentLon;
                response[i]["status"] = rider.status;
                response[i]["parcelCount"] = rider.assignedParcelIds.getSize();
                response[i]["capacity"] = rider.capacity;
                
                // Add parcel IDs for detail view
                response[i]["parcelIds"] = crow::json::wvalue::list();
                for (int j = 0; j < rider.assignedParcelIds.getSize(); j++) {
                    response[i]["parcelIds"][j] = rider.assignedParcelIds[j];
                }
            }
        }
        
        return response;
    });

    // GET parcel by tracking ID
    CROW_ROUTE(app, "/api/track/<int>")
    ([](int parcelId){
        Parcel* parcel = parcelTracker.get(parcelId);
        
        if (!parcel) {
            return crow::response(404, "Parcel not found");
        }
        
        crow::json::wvalue response;
        response["id"] = parcel->id;
        response["priority"] = parcel->priority;
        response["weight"] = parcel->weight;
        response["currentLocationId"] = parcel->currentLocationId;
        response["destinationId"] = parcel->destinationId;
        response["status"] = parcel->status;
        response["currentLat"] = parcel->currentLat;
        response["currentLon"] = parcel->currentLon;
        response["routeProgress"] = parcel->routeIndex;
        response["routeLength"] = parcel->route.getSize();
        
        return crow::response(200, response);
    });

    // POST advance time (simulation step)
    CROW_ROUTE(app, "/api/advance_time").methods(crow::HTTPMethod::POST)
    ([](){
        advanceTime();
        
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "Time advanced";
        return crow::response(200, response);
    });

    // POST undo last movement
    CROW_ROUTE(app, "/api/undo").methods(crow::HTTPMethod::POST)
    ([](){
        if (undoStack.isEmpty()) {
            return crow::response(400, "Nothing to undo");
        }
        
        SystemSnapshot previousState = undoStack.pop();
        
        // Restore all parcels
        parcelTracker.clear();
        for (int i = 0; i < previousState.parcels.getSize(); i++) {
            parcelTracker.insert(previousState.parcels[i].id, previousState.parcels[i]);
        }
        
        // Restore all riders
        riderManager.restoreRiders(previousState.riders);
        
        // Restore notifications
        notifications = previousState.notifications;
        
        // Save restored state
        riderManager.saveToFile(RIDERS_FILE);
        saveParcels(PARCELS_FILE);
        
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "Undo successful";
        return crow::response(200, response);
    });

    // GET all riders
    CROW_ROUTE(app, "/api/riders")
    ([](){
        const Vector<Rider>& riders = riderManager.getRiders();
        
        crow::json::wvalue response;
        response = crow::json::wvalue::list();
        
        for (int i = 0; i < riders.getSize(); i++) {
            // Count only active (in_transit) parcels
            int activeParcelCount = 0;
            for (int j = 0; j < riders[i].assignedParcelIds.getSize(); j++) {
                Parcel* parcel = parcelTracker.get(riders[i].assignedParcelIds[j]);
                if (parcel && parcel->status == "in_transit") {
                    activeParcelCount++;
                }
            }
            
            response[i]["id"] = riders[i].id;
            response[i]["name"] = riders[i].name;
            response[i]["vehicleType"] = riders[i].getVehicleTypeString();
            response[i]["currentLocationId"] = riders[i].currentLocationId;
            response[i]["lat"] = riders[i].currentLat;
            response[i]["lon"] = riders[i].currentLon;
            response[i]["status"] = riders[i].status;
            response[i]["parcelCount"] = activeParcelCount;
            response[i]["capacity"] = riders[i].capacity;
            response[i]["availableSlots"] = riders[i].capacity - activeParcelCount;
        }
        
        return response;
    });

    // GET rider details with assigned parcels
    CROW_ROUTE(app, "/api/rider/<int>")
    ([graphPtr](int riderId){
        Rider* rider = riderManager.getRiderById(riderId);
        
        if (!rider) {
            return crow::response(404, "Rider not found");
        }
        
        crow::json::wvalue response;
        response["id"] = rider->id;
        response["name"] = rider->name;
        response["vehicleType"] = rider->getVehicleTypeString();
        response["currentLocationId"] = rider->currentLocationId;
        response["status"] = rider->status;
        response["parcelCount"] = rider->assignedParcelIds.getSize();
        response["capacity"] = rider->capacity;
        response["routeIndex"] = rider->routeIndex;
        
        // Add planned route
        response["plannedRoute"] = crow::json::wvalue::list();
        for (int i = 0; i < rider->plannedRoute.getSize(); i++) {
            response["plannedRoute"][i] = rider->plannedRoute[i];
        }
        
        // Add route history
        response["routeHistory"] = crow::json::wvalue::list();
        for (int i = 0; i < rider->routeHistory.getSize(); i++) {
            response["routeHistory"][i] = rider->routeHistory[i];
        }
        
        // Get current location name
        Node* location = graphPtr->getNodeById(rider->currentLocationId);
        if (location) {
            response["currentLocationName"] = location->name;
        }
        
        // Add assigned parcels details (only in_transit parcels)
        response["parcels"] = crow::json::wvalue::list();
        int activeParcelCount = 0;
        for (int i = 0; i < rider->assignedParcelIds.getSize(); i++) {
            int parcelId = rider->assignedParcelIds[i];
            Parcel* parcel = parcelTracker.get(parcelId);
            if (parcel && parcel->status == "in_transit") {
                response["parcels"][activeParcelCount]["id"] = parcel->id;
                response["parcels"][activeParcelCount]["priority"] = parcel->priority;
                response["parcels"][activeParcelCount]["weight"] = parcel->weight;
                response["parcels"][activeParcelCount]["status"] = parcel->status;
                response["parcels"][activeParcelCount]["destinationId"] = parcel->destinationId;
                
                // Get destination name
                Node* dest = graphPtr->getNodeById(parcel->destinationId);
                if (dest) {
                    response["parcels"][activeParcelCount]["destinationName"] = dest->name;
                }
                activeParcelCount++;
            }
        }
        
        return crow::response(200, response);
    });

    // ====================
    // STATIC FILE SERVING
    // ====================

    // Serve route.html
    CROW_ROUTE(app, "/route")
    ([](){
        std::string content = read_file("public/route.html");
        if (content.empty()) {
            return crow::response(404, "Route page not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve send-parcel.html
    CROW_ROUTE(app, "/send-parcel")
    ([](){
        std::string content = read_file("public/send-parcel.html");
        if (content.empty()) {
            return crow::response(404, "Send parcel page not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve warehouse.html
    CROW_ROUTE(app, "/warehouse")
    ([](){
        std::string content = read_file("public/warehouse.html");
        if (content.empty()) {
            return crow::response(404, "Warehouse page not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve tracking.html
    CROW_ROUTE(app, "/tracking")
    ([](){
        std::string content = read_file("public/tracking.html");
        if (content.empty()) {
            return crow::response(404, "Tracking page not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve admin.html
    CROW_ROUTE(app, "/admin")
    ([](){
        std::string content = read_file("public/admin.html");
        if (content.empty()) {
            return crow::response(404, "Admin page not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve index.html
    CROW_ROUTE(app, "/")
    ([](){
        std::string content = read_file("public/index.html");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve CSS files
    CROW_ROUTE(app, "/styles.css")
    ([](){
        std::string content = read_file("public/styles.css");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/css");
        return res;
    });

    CROW_ROUTE(app, "/admin.css")
    ([](){
        std::string content = read_file("public/admin.css");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "text/css");
        return res;
    });

    // Serve JS files
    CROW_ROUTE(app, "/app.js")
    ([](){
        std::string content = read_file("public/app.js");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res;
    });

    CROW_ROUTE(app, "/admin.js")
    ([](){
        std::string content = read_file("public/admin.js");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res;
    });

    CROW_ROUTE(app, "/route.js")
    ([](){
        std::string content = read_file("public/route.js");
        if (content.empty()) {
            return crow::response(404, "File not found");
        }
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res;
    });

    std::cout << "========================================" << std::endl;
    std::cout << "Parcel Tracker Server - Phase 2" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Main Page: http://localhost:18080" << std::endl;
    std::cout << "Admin Dashboard: http://localhost:18080/admin" << std::endl;
    std::cout << "========================================" << std::endl;
    
    app.port(18080).multithreaded().run();
    
    return 0;
}
