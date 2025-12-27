#ifndef GRAPH_H
#define GRAPH_H

#include "Vector.h"
#include <string>
#include <fstream>
#include <sstream>

struct Node {
    int id;
    std::string name;
    double lat;
    double lon;

    Node() : id(-1), name(""), lat(0.0), lon(0.0) {}
    Node(int _id, const std::string& _name, double _lat, double _lon) 
        : id(_id), name(_name), lat(_lat), lon(_lon) {}
};

struct Edge {
    int source;
    int destination;
    double distance;
    double trafficWeight;
    bool blocked;

    Edge() : source(-1), destination(-1), distance(0.0), trafficWeight(1.0), blocked(false) {}
    Edge(int src, int dest, double dist, double traffic = 1.0, bool blk = false)
        : source(src), destination(dest), distance(dist), trafficWeight(traffic), blocked(blk) {}
};

class Graph {
private:
    Vector<Node> nodes;
    Vector<Vector<Edge>> adjacencyList; // adjacencyList[nodeIndex] = Vector of edges
    int nextNodeId;

    // Find node index by ID
    int findNodeIndex(int nodeId) const {
        for (int i = 0; i < nodes.getSize(); i++) {
            if (nodes[i].id == nodeId) {
                return i;
            }
        }
        return -1;
    }

public:
    Graph() : nextNodeId(1) {}

    // Add a node to the graph
    bool addNode(const std::string& name, double lat, double lon) {
        Node newNode(nextNodeId++, name, lat, lon);
        nodes.push_back(newNode);
        adjacencyList.push_back(Vector<Edge>()); // Add empty edge list
        return true;
    }

    // Add node with specific ID (for loading from file)
    bool addNodeWithId(int id, const std::string& name, double lat, double lon) {
        Node newNode(id, name, lat, lon);
        nodes.push_back(newNode);
        adjacencyList.push_back(Vector<Edge>());
        if (id >= nextNodeId) {
            nextNodeId = id + 1;
        }
        return true;
    }

    // Add an edge between two nodes
    bool addEdge(int sourceId, int destId, double distance, double trafficWeight = 1.0) {
        int srcIndex = findNodeIndex(sourceId);
        int destIndex = findNodeIndex(destId);

        if (srcIndex == -1 || destIndex == -1) {
            return false;
        }

        Edge newEdge(sourceId, destId, distance, trafficWeight);
        adjacencyList[srcIndex].push_back(newEdge);

        // For undirected graph, add reverse edge
        Edge reverseEdge(destId, sourceId, distance, trafficWeight);
        adjacencyList[destIndex].push_back(reverseEdge);

        return true;
    }

    // Get all nodes
    const Vector<Node>& getNodes() const {
        return nodes;
    }

    // Get all edges
    Vector<Edge> getAllEdges() const {
        Vector<Edge> allEdges;
        for (int i = 0; i < adjacencyList.getSize(); i++) {
            for (int j = 0; j < adjacencyList[i].getSize(); j++) {
                // Only add edge once (check if source < destination to avoid duplicates)
                if (adjacencyList[i][j].source < adjacencyList[i][j].destination) {
                    allEdges.push_back(adjacencyList[i][j]);
                }
            }
        }
        return allEdges;
    }

    // Get edges for a specific node
    const Vector<Edge>& getEdges(int nodeId) const {
        int index = findNodeIndex(nodeId);
        if (index == -1) {
            static Vector<Edge> empty;
            return empty;
        }
        return adjacencyList[index];
    }

    // Save map to file
    bool saveMap(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // Save nodes
        file << "NODES\n";
        for (int i = 0; i < nodes.getSize(); i++) {
            file << nodes[i].id << "," 
                 << nodes[i].name << "," 
                 << nodes[i].lat << "," 
                 << nodes[i].lon << "\n";
        }

        // Save edges (only once per edge)
        file << "EDGES\n";
        Vector<Edge> edges = getAllEdges();
        for (int i = 0; i < edges.getSize(); i++) {
            file << edges[i].source << "," 
                 << edges[i].destination << "," 
                 << edges[i].distance << "," 
                 << edges[i].trafficWeight << ","
                 << edges[i].blocked << "\n";
        }

        file.close();
        return true;
    }

    // Load map from file
    bool loadMap(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // Clear existing data
        nodes.clear();
        adjacencyList.clear();
        nextNodeId = 1;

        std::string line;
        std::string section = "";

        while (std::getline(file, line)) {
            if (line == "NODES") {
                section = "NODES";
                continue;
            } else if (line == "EDGES") {
                section = "EDGES";
                continue;
            }

            if (line.empty()) continue;

            std::stringstream ss(line);
            std::string token;

            if (section == "NODES") {
                Vector<std::string> tokens;
                while (std::getline(ss, token, ',')) {
                    tokens.push_back(token);
                }
                if (tokens.getSize() >= 4) {
                    int id = std::stoi(tokens[0]);
                    std::string name = tokens[1];
                    double lat = std::stod(tokens[2]);
                    double lon = std::stod(tokens[3]);
                    addNodeWithId(id, name, lat, lon);
                }
            } else if (section == "EDGES") {
                Vector<std::string> tokens;
                while (std::getline(ss, token, ',')) {
                    tokens.push_back(token);
                }
                if (tokens.getSize() >= 3) {
                    int source = std::stoi(tokens[0]);
                    int dest = std::stoi(tokens[1]);
                    double distance = std::stod(tokens[2]);
                    double traffic = tokens.getSize() >= 4 ? std::stod(tokens[3]) : 1.0;
                    bool blocked = tokens.getSize() >= 5 ? (std::stoi(tokens[4]) != 0) : false;
                    
                    // Add edge with blocked status
                    int srcIdx = findNodeIndex(source);
                    int destIdx = findNodeIndex(dest);
                    if (srcIdx != -1 && destIdx != -1) {
                        Edge newEdge(source, dest, distance, traffic, blocked);
                        adjacencyList[srcIdx].push_back(newEdge);
                        Edge reverseEdge(dest, source, distance, traffic, blocked);
                        adjacencyList[destIdx].push_back(reverseEdge);
                    }
                }
            }
        }

        file.close();
        return true;
    }

    // Get node by ID
    Node* getNodeById(int nodeId) {
        int index = findNodeIndex(nodeId);
        if (index == -1) return nullptr;
        return &nodes[index];
    }

    // Delete node
    bool deleteNode(int nodeId) {
        int index = findNodeIndex(nodeId);
        if (index == -1) return false;

        // Remove all edges connected to this node
        for (int i = 0; i < adjacencyList.getSize(); i++) {
            for (int j = adjacencyList[i].getSize() - 1; j >= 0; j--) {
                if (adjacencyList[i][j].destination == nodeId || 
                    adjacencyList[i][j].source == nodeId) {
                    // Remove edge by shifting elements
                    for (int k = j; k < adjacencyList[i].getSize() - 1; k++) {
                        adjacencyList[i][k] = adjacencyList[i][k + 1];
                    }
                    adjacencyList[i].pop_back();
                }
            }
        }

        // Remove the node itself
        for (int i = index; i < nodes.getSize() - 1; i++) {
            nodes[i] = nodes[i + 1];
            adjacencyList[i] = adjacencyList[i + 1];
        }
        nodes.pop_back();
        adjacencyList.pop_back();

        return true;
    }

    // Block/Unblock edge
    bool toggleEdgeBlock(int sourceId, int destId) {
        int srcIndex = findNodeIndex(sourceId);
        int destIndex = findNodeIndex(destId);

        if (srcIndex == -1 || destIndex == -1) return false;

        bool found = false;
        // Update both directions
        for (int i = 0; i < adjacencyList[srcIndex].getSize(); i++) {
            if (adjacencyList[srcIndex][i].destination == destId) {
                adjacencyList[srcIndex][i].blocked = !adjacencyList[srcIndex][i].blocked;
                found = true;
                break;
            }
        }
        for (int i = 0; i < adjacencyList[destIndex].getSize(); i++) {
            if (adjacencyList[destIndex][i].destination == sourceId) {
                adjacencyList[destIndex][i].blocked = !adjacencyList[destIndex][i].blocked;
                break;
            }
        }
        return found;
    }

    // Dijkstra's algorithm
    Vector<Node> dijkstra(int startId, int endId) {
        Vector<Node> path;
        int startIdx = findNodeIndex(startId);
        int endIdx = findNodeIndex(endId);

        if (startIdx == -1 || endIdx == -1) return path;

        int n = nodes.getSize();
        Vector<double> dist;
        Vector<int> prev;
        Vector<bool> visited;

        // Initialize
        for (int i = 0; i < n; i++) {
            dist.push_back(999999999.0); // infinity
            prev.push_back(-1);
            visited.push_back(false);
        }
        dist[startIdx] = 0;

        // Dijkstra
        for (int count = 0; count < n; count++) {
            int u = -1;
            double minDist = 999999999.0;

            // Find unvisited node with minimum distance
            for (int i = 0; i < n; i++) {
                if (!visited[i] && dist[i] < minDist) {
                    minDist = dist[i];
                    u = i;
                }
            }

            if (u == -1) break;
            visited[u] = true;

            // Update distances to neighbors
            for (int i = 0; i < adjacencyList[u].getSize(); i++) {
                Edge& edge = adjacencyList[u][i];
                if (edge.blocked) continue; // Skip blocked edges

                int v = findNodeIndex(edge.destination);
                if (v != -1 && !visited[v]) {
                    double weight = edge.distance * edge.trafficWeight;
                    if (dist[u] + weight < dist[v]) {
                        dist[v] = dist[u] + weight;
                        prev[v] = u;
                    }
                }
            }
        }

        // Reconstruct path
        if (prev[endIdx] == -1 && startIdx != endIdx) {
            return path; // No path found
        }

        Vector<int> pathIndices;
        for (int at = endIdx; at != -1; at = prev[at]) {
            pathIndices.push_back(at);
        }

        // Reverse path
        for (int i = pathIndices.getSize() - 1; i >= 0; i--) {
            path.push_back(nodes[pathIndices[i]]);
        }

        return path;
    }
};

#endif // GRAPH_H
