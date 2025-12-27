#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "Vector.h"
#include "Graph.h"

// Parcel struct
struct Parcel {
    int id;
    int priority; // 1 = Standard, 2 = Overnight
    double weight;
    int currentLocationId;
    int destinationId;
    std::string status; // "pending", "in_transit", "delivered"
    Vector<int> route; // Sequence of node IDs
    int routeIndex; // Current position in route
    double currentLat; // Real-time latitude
    double currentLon; // Real-time longitude
    int riderId; // ID of assigned rider (-1 if unassigned)

    Parcel() : id(-1), priority(1), weight(0.0), currentLocationId(-1), destinationId(-1), 
               status("pending"), routeIndex(0), currentLat(0.0), currentLon(0.0), riderId(-1) {}
    Parcel(int _id, int _priority, double _weight, int _location, int _destination, const std::string& _status = "pending")
        : id(_id), priority(_priority), weight(_weight), currentLocationId(_location), destinationId(_destination), 
          status(_status), routeIndex(0), currentLat(0.0), currentLon(0.0), riderId(-1) {}
};

// Max-Heap Priority Queue
class PriorityQueue {
private:
    Vector<Parcel> heap;
    int nextParcelId;

    // Helper functions
    int parent(int i) { return (i - 1) / 2; }
    int leftChild(int i) { return 2 * i + 1; }
    int rightChild(int i) { return 2 * i + 2; }

    // Swap two parcels
    void swap(int i, int j) {
        Parcel temp = heap[i];
        heap[i] = heap[j];
        heap[j] = temp;
    }

    // Compare: return true if parcel i should be higher priority than parcel j
    bool hasHigherPriority(int i, int j) {
        if (heap[i].priority != heap[j].priority) {
            return heap[i].priority > heap[j].priority; // Higher priority value = higher priority
        }
        return heap[i].id < heap[j].id; // If same priority, older parcel (lower ID) first
    }

    // Bubble up after insertion
    void bubbleUp(int i) {
        while (i > 0 && hasHigherPriority(i, parent(i))) {
            swap(i, parent(i));
            i = parent(i);
        }
    }

    // Bubble down after removal
    void bubbleDown(int i) {
        int smallest = i;
        int left = leftChild(i);
        int right = rightChild(i);

        if (left < heap.getSize() && hasHigherPriority(left, smallest)) {
            smallest = left;
        }
        if (right < heap.getSize() && hasHigherPriority(right, smallest)) {
            smallest = right;
        }

        if (smallest != i) {
            swap(i, smallest);
            bubbleDown(smallest);
        }
    }

public:
    PriorityQueue() : nextParcelId(1) {}

    // Insert a new parcel
    int insert(int priority, double weight, int locationId, int destinationId) {
        Parcel newParcel(nextParcelId, priority, weight, locationId, destinationId);
        heap.push_back(newParcel);
        bubbleUp(heap.getSize() - 1);
        return nextParcelId++;
    }

    // Get highest priority parcel without removing
    Parcel peek() {
        if (heap.getSize() > 0) {
            return heap[0];
        }
        return Parcel();
    }

    // Remove and return highest priority parcel
    Parcel extract() {
        if (heap.getSize() == 0) {
            return Parcel();
        }

        Parcel top = heap[0];
        heap[0] = heap[heap.getSize() - 1];
        heap.pop_back();

        if (heap.getSize() > 0) {
            bubbleDown(0);
        }

        return top;
    }

    // Get all parcels (for display)
    Vector<Parcel> getAllParcels() const {
        return heap;
    }

    // Get parcel by ID
    Parcel* getParcelById(int parcelId) {
        for (int i = 0; i < heap.getSize(); i++) {
            if (heap[i].id == parcelId) {
                return &heap[i];
            }
        }
        return nullptr;
    }

    // Update parcel status
    bool updateParcelStatus(int parcelId, const std::string& newStatus) {
        for (int i = 0; i < heap.getSize(); i++) {
            if (heap[i].id == parcelId) {
                heap[i].status = newStatus;
                return true;
            }
        }
        return false;
    }

    // Get sorted list (for display purposes - doesn't modify queue)
    Vector<Parcel> getSortedList() const {
        Vector<Parcel> sorted;
        Vector<Parcel> temp = heap;

        // Simple bubble sort for display (heap is already mostly sorted)
        for (int i = 0; i < temp.getSize(); i++) {
            sorted.push_back(temp[i]);
        }

        return sorted;
    }

    // Get size
    int getSize() const {
        return heap.getSize();
    }

    // Clear all parcels
    void clear() {
        heap.clear();
    }
};

#endif // PRIORITY_QUEUE_H
