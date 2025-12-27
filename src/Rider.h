#ifndef RIDER_H
#define RIDER_H

#include "Vector.h"
#include "Graph.h"
#include "PriorityQueue.h"
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>

enum VehicleType {
    RIDER,  // Can carry up to 5 parcels, each up to 5 kg
    CAR     // Can carry up to 20 parcels, each from 6 kg to 20 kg
};

struct Rider {
    int id;
    std::string name;
    VehicleType vehicleType;          // Type of vehicle
    int currentLocationId;
    double currentLat;
    double currentLon;
    Vector<int> assignedParcelIds;  // IDs of parcels assigned to this rider
    int capacity;                     // Max parcels this vehicle can carry
    std::string status;              // "available", "delivering", "offline"
    Vector<int> plannedRoute;        // The combined route this rider is following
    Vector<int> routeHistory;        // History of visited nodes
    int routeIndex;                  // Current position in the route
    
    Rider() : id(0), vehicleType(RIDER), currentLocationId(-1), currentLat(0), currentLon(0), 
              capacity(5), status("available"), routeIndex(0) {}
    
    Rider(int rId, std::string rName, VehicleType vType, int locationId, double lat, double lon)
        : id(rId), name(rName), vehicleType(vType), currentLocationId(locationId), 
          currentLat(lat), currentLon(lon), status("available"), routeIndex(0) {
        // Set capacity based on vehicle type
        capacity = (vType == RIDER) ? 5 : 20;
        // Record starting location
        routeHistory.push_back(locationId);
    }
    
    // Check if vehicle can accept a parcel based on weight
    bool canAcceptParcel(double parcelWeight) const {
        if (status == "offline" || assignedParcelIds.getSize() >= capacity) {
            return false;
        }
        
        // Check weight compatibility
        if (vehicleType == RIDER) {
            return parcelWeight <= 5.0;
        } else { // CAR
            return parcelWeight >= 6.0 && parcelWeight <= 20.0;
        }
    }
    
    // Get number of available slots
    int getAvailableSlots() const {
        return capacity - assignedParcelIds.getSize();
    }
    
    // Get vehicle type as string
    std::string getVehicleTypeString() const {
        return (vehicleType == RIDER) ? "Rider" : "Car";
    }
};

class RiderManager {
private:
    Vector<Rider> riders;
    int nextRiderId;
    
    // Calculate distance between two coordinates (Haversine formula)
    double calculateDistance(double lat1, double lon1, double lat2, double lon2) const {
        double dx = lat2 - lat1;
        double dy = lon2 - lon1;
        return sqrt(dx * dx + dy * dy); // Simplified distance for small coordinates
    }
    
    // Check if two routes are similar (destinations close to each other)
    bool routesAreCompatible(const Vector<int>& route1, const Vector<int>& route2, Graph& graph) const {
        if (route1.getSize() == 0 || route2.getSize() == 0) return false;
        
        // Check if destinations are the same or close
        int dest1 = route1[route1.getSize() - 1];
        int dest2 = route2[route2.getSize() - 1];
        
        if (dest1 == dest2) return true;
        
        Node* node1 = graph.getNodeById(dest1);
        Node* node2 = graph.getNodeById(dest2);
        
        if (!node1 || !node2) return false;
        
        double dist = calculateDistance(node1->lat, node1->lon, node2->lat, node2->lon);
        return dist < 0.05; // Threshold for "close" destinations
    }
    
public:
    RiderManager() : nextRiderId(1) {}
    
    // Add a new vehicle (rider or car)
    int addRider(const std::string& name, VehicleType vehicleType, int locationId, double lat, double lon) {
        Rider rider(nextRiderId++, name, vehicleType, locationId, lat, lon);
        riders.push_back(rider);
        return rider.id;
    }
    
    // Get all riders
    const Vector<Rider>& getRiders() const {
        return riders;
    }
    
    // Save riders to file
    bool saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        for (int i = 0; i < riders.getSize(); i++) {
            const Rider& r = riders[i];
            file << r.id << "," << r.name << "," << (int)r.vehicleType << ","
                 << r.currentLocationId << "," << r.currentLat << "," << r.currentLon << ","
                 << r.capacity << "," << r.status << "," << r.routeIndex;
            
            // Save assigned parcel IDs
            file << ",";
            for (int j = 0; j < r.assignedParcelIds.getSize(); j++) {
                file << r.assignedParcelIds[j];
                if (j < r.assignedParcelIds.getSize() - 1) file << ";";
            }
            
            // Save planned route
            file << ",";
            for (int j = 0; j < r.plannedRoute.getSize(); j++) {
                file << r.plannedRoute[j];
                if (j < r.plannedRoute.getSize() - 1) file << ";";
            }
            
            // Save route history
            file << ",";
            for (int j = 0; j < r.routeHistory.getSize(); j++) {
                file << r.routeHistory[j];
                if (j < r.routeHistory.getSize() - 1) file << ";";
            }
            
            file << "\n";
        }
        
        file.close();
        return true;
    }
    
    // Load riders from file
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        riders.clear();
        std::string line;
        int maxId = 0;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            std::string token;
            Rider rider;
            
            // Parse fields
            std::getline(ss, token, ','); rider.id = std::stoi(token);
            std::getline(ss, rider.name, ',');
            std::getline(ss, token, ','); rider.vehicleType = (VehicleType)std::stoi(token);
            std::getline(ss, token, ','); rider.currentLocationId = std::stoi(token);
            std::getline(ss, token, ','); rider.currentLat = std::stod(token);
            std::getline(ss, token, ','); rider.currentLon = std::stod(token);
            std::getline(ss, token, ','); rider.capacity = std::stoi(token);
            std::getline(ss, rider.status, ',');
            std::getline(ss, token, ','); rider.routeIndex = std::stoi(token);
            
            // Parse assigned parcel IDs
            std::getline(ss, token, ',');
            if (!token.empty()) {
                std::stringstream parcelStream(token);
                std::string parcelId;
                while (std::getline(parcelStream, parcelId, ';')) {
                    rider.assignedParcelIds.push_back(std::stoi(parcelId));
                }
            }
            
            // Parse planned route
            std::getline(ss, token, ',');
            if (!token.empty()) {
                std::stringstream routeStream(token);
                std::string nodeId;
                while (std::getline(routeStream, nodeId, ';')) {
                    rider.plannedRoute.push_back(std::stoi(nodeId));
                }
            }
            
            // Parse route history
            std::getline(ss, token, ',');
            if (!token.empty()) {
                std::stringstream historyStream(token);
                std::string nodeId;
                while (std::getline(historyStream, nodeId, ';')) {
                    rider.routeHistory.push_back(std::stoi(nodeId));
                }
            }
            
            riders.push_back(rider);
            if (rider.id > maxId) maxId = rider.id;
        }
        
        nextRiderId = maxId + 1;
        file.close();
        return true;
    }
    
    // Restore riders from snapshot (for undo)
    void restoreRiders(const Vector<Rider>& snapshotRiders) {
        riders.clear();
        for (int i = 0; i < snapshotRiders.getSize(); i++) {
            riders.push_back(snapshotRiders[i]);
        }
        // Update nextRiderId to be one more than the highest ID
        nextRiderId = 1;
        for (int i = 0; i < riders.getSize(); i++) {
            if (riders[i].id >= nextRiderId) {
                nextRiderId = riders[i].id + 1;
            }
        }
    }
    
    // Get rider by ID
    Rider* getRiderById(int riderId) {
        for (int i = 0; i < riders.getSize(); i++) {
            if (riders[i].id == riderId) {
                return &riders[i];
            }
        }
        return nullptr;
    }
    
    // Find best rider for a parcel based on:
    // 1. Proximity to pickup location
    // 2. Route compatibility (similar destination)
    // 3. Available capacity
    // 4. Priority (riders with fewer parcels preferred for high-priority parcels)
    int findBestRider(const Parcel& parcel, Graph& graph) {
        int bestRiderId = -1;
        double bestScore = -1000000.0;
        
        Node* pickupNode = graph.getNodeById(parcel.currentLocationId);
        if (!pickupNode) return -1;
        
        for (int i = 0; i < riders.getSize(); i++) {
            Rider& rider = riders[i];
            
            // Skip if rider can't accept this parcel based on weight
            if (!rider.canAcceptParcel(parcel.weight)) continue;
            
            double score = 0.0;
            
            // Factor 1: Proximity to rider's current location (weight: 40%)
            double proximity = calculateDistance(
                rider.currentLat, rider.currentLon,
                pickupNode->lat, pickupNode->lon
            );
            score += (1.0 / (proximity + 0.01)) * 40.0; // Closer is better
            
            // Factor 2: Route compatibility (weight: 40%)
            if (rider.plannedRoute.getSize() > 0) {
                if (routesAreCompatible(rider.plannedRoute, parcel.route, graph)) {
                    score += 40.0;
                } else {
                    score += 10.0; // Small bonus for having any route
                }
            } else {
                // If rider has no route yet, they're available - bonus for first parcel
                score += 30.0;
            }
            
            // Factor 3: Priority matching (weight: 20%)
            if (parcel.priority == 2) { // Overnight/High priority
                // Prefer riders with fewer parcels for high-priority
                int loadFactor = rider.assignedParcelIds.getSize();
                score += (5.0 - loadFactor) * 4.0; // Max 20 points if empty
            } else {
                // Standard priority can use busier riders
                score += 10.0;
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestRiderId = rider.id;
            }
        }
        
        return bestRiderId;
    }
    
    // Assign a parcel to a rider
    bool assignParcelToRider(int riderId, int parcelId, const Vector<int>& parcelRoute, Graph& graph, double parcelWeight) {
        Rider* rider = getRiderById(riderId);
        if (!rider || !rider->canAcceptParcel(parcelWeight)) return false;
        
        rider->assignedParcelIds.push_back(parcelId);
        
        // If this is the first parcel, set the rider's route
        if (rider->plannedRoute.getSize() == 0 && parcelRoute.getSize() > 0) {
            rider->plannedRoute = parcelRoute;
            rider->routeIndex = 0;
            rider->status = "delivering";
        } else if (parcelRoute.getSize() > 0) {
            // Extend rider's route if new parcel destination is beyond current route
            int newDest = parcelRoute[parcelRoute.getSize() - 1];
            int currentDest = rider->plannedRoute[rider->plannedRoute.getSize() - 1];
            
            if (newDest != currentDest) {
                // Find where the new route intersects with existing route
                int intersectionIdx = -1;
                for (int i = 0; i < rider->plannedRoute.getSize(); i++) {
                    for (int j = 0; j < parcelRoute.getSize(); j++) {
                        if (rider->plannedRoute[i] == parcelRoute[j]) {
                            intersectionIdx = i;
                            break;
                        }
                    }
                    if (intersectionIdx != -1) break;
                }
                
                // If new destination is not in current route, extend the route
                bool destInRoute = false;
                for (int i = 0; i < rider->plannedRoute.getSize(); i++) {
                    if (rider->plannedRoute[i] == newDest) {
                        destInRoute = true;
                        break;
                    }
                }
                
                if (!destInRoute) {
                    // Calculate route from current destination to new destination
                    Vector<Node> extensionPath = graph.dijkstra(currentDest, newDest);
                    // Add new nodes to rider's route (skip first node as it's already in route)
                    for (int i = 1; i < extensionPath.getSize(); i++) {
                        rider->plannedRoute.push_back(extensionPath[i].id);
                    }
                }
            }
        }
        
        return true;
    }
    
    // Remove parcel from rider (when delivered)
    void removeParcelFromRider(int riderId, int parcelId) {
        Rider* rider = getRiderById(riderId);
        if (!rider) return;
        
        // Remove parcel from assigned list
        for (int i = 0; i < rider->assignedParcelIds.getSize(); i++) {
            if (rider->assignedParcelIds[i] == parcelId) {
                // Shift remaining elements
                for (int j = i; j < rider->assignedParcelIds.getSize() - 1; j++) {
                    rider->assignedParcelIds[j] = rider->assignedParcelIds[j + 1];
                }
                rider->assignedParcelIds.pop_back();
                break;
            }
        }
        
        // If no more parcels, reset rider
        if (rider->assignedParcelIds.getSize() == 0) {
            rider->status = "available";
            rider->plannedRoute.clear();
            rider->routeIndex = 0;
        }
    }
    
    // Move one rider at a time along their route (for simulation control)
    bool advanceNextRider(Graph& graph) {
        // Find next rider that needs to move
        for (int i = 0; i < riders.getSize(); i++) {
            Rider& rider = riders[i];
            
            if (rider.status == "delivering" && rider.plannedRoute.getSize() > 0) {
                // Check if at first location
                if (rider.routeIndex == 0) {
                    std::cout << "⚠️ ALERT: " << rider.name << " is at starting location (" 
                              << rider.currentLocationId << ")" << std::endl;
                }
                
                // Move to next node in route
                if (rider.routeIndex < rider.plannedRoute.getSize() - 1) {
                    rider.routeIndex++;
                    rider.currentLocationId = rider.plannedRoute[rider.routeIndex];
                    
                    // Add to history
                    rider.routeHistory.push_back(rider.currentLocationId);
                    
                    // Update position
                    Node* node = graph.getNodeById(rider.currentLocationId);
                    if (node) {
                        rider.currentLat = node->lat;
                        rider.currentLon = node->lon;
                        
                        std::cout << "📍 " << rider.name << " moved to " << node->name 
                                  << " (" << rider.routeIndex + 1 << "/" << rider.plannedRoute.getSize() << ")" << std::endl;
                    }
                    
                    // Check if at last location
                    if (rider.routeIndex == rider.plannedRoute.getSize() - 1) {
                        std::cout << "🏁 ALERT: " << rider.name << " reached final destination!" << std::endl;
                    }
                    
                    return true; // Rider moved
                }
            }
        }
        return false; // No rider moved
    }
    
    // Move all riders along their routes
    void advanceRiders(Graph& graph) {
        for (int i = 0; i < riders.getSize(); i++) {
            Rider& rider = riders[i];
            
            if (rider.status == "delivering" && rider.plannedRoute.getSize() > 0) {
                // Move to next node in route
                if (rider.routeIndex < rider.plannedRoute.getSize() - 1) {
                    rider.routeIndex++;
                    rider.currentLocationId = rider.plannedRoute[rider.routeIndex];
                    
                    // Update position
                    Node* node = graph.getNodeById(rider.currentLocationId);
                    if (node) {
                        rider.currentLat = node->lat;
                        rider.currentLon = node->lon;
                    }
                }
            }
        }
    }
    
    // Get rider at a specific location
    Vector<int> getRidersAtLocation(int locationId) const {
        Vector<int> riderIds;
        for (int i = 0; i < riders.getSize(); i++) {
            if (riders[i].currentLocationId == locationId) {
                riderIds.push_back(riders[i].id);
            }
        }
        return riderIds;
    }
};

#endif // RIDER_H
