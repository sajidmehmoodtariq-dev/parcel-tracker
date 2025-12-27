#include <crow.h>
#include <fstream>
#include <sstream>
#include "src/Graph.h"

// Global graph instance
Graph cityGraph;
const std::string MAP_FILE = "data/map_data.txt";

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

int main() {
    crow::SimpleApp app;

    // Load existing map data on startup
    if (cityGraph.loadMap(MAP_FILE)) {
        std::cout << "Map data loaded successfully!" << std::endl;
    } else {
        std::cout << "No existing map data found. Starting fresh." << std::endl;
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
