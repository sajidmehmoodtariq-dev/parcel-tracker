#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "Vector.h"
#include "PriorityQueue.h"

// HashTable entry
template<typename K, typename V>
struct HashEntry {
    K key;
    V value;
    bool occupied;

    HashEntry() : key(K()), value(V()), occupied(false) {}
    HashEntry(K k, V v) : key(k), value(v), occupied(true) {}
};

// HashTable class
template<typename K, typename V>
class HashTable {
private:
    Vector<HashEntry<K, V>> table;
    int capacity;
    int size;

    // Hash function for integers
    int hash(int key) {
        return key % capacity;
    }

    // Resize when load factor > 0.7
    void resize() {
        int oldCapacity = capacity;
        capacity = capacity * 2;
        Vector<HashEntry<K, V>> oldTable = table;
        
        table.clear();
        for (int i = 0; i < capacity; i++) {
            table.push_back(HashEntry<K, V>());
        }
        size = 0;

        for (int i = 0; i < oldCapacity; i++) {
            if (oldTable[i].occupied) {
                insert(oldTable[i].key, oldTable[i].value);
            }
        }
    }

public:
    HashTable(int initialCapacity = 100) : capacity(initialCapacity), size(0) {
        for (int i = 0; i < capacity; i++) {
            table.push_back(HashEntry<K, V>());
        }
    }

    // Insert key-value pair
    void insert(K key, V value) {
        if (size >= capacity * 0.7) {
            resize();
        }

        int index = hash(key);
        int originalIndex = index;

        // Linear probing
        while (table[index].occupied && table[index].key != key) {
            index = (index + 1) % capacity;
            if (index == originalIndex) {
                resize();
                insert(key, value);
                return;
            }
        }

        if (!table[index].occupied) {
            size++;
        }
        table[index] = HashEntry<K, V>(key, value);
    }

    // Get value by key
    V* get(K key) {
        int index = hash(key);
        int originalIndex = index;

        while (table[index].occupied) {
            if (table[index].key == key) {
                return &table[index].value;
            }
            index = (index + 1) % capacity;
            if (index == originalIndex) break;
        }

        return nullptr;
    }

    // Check if key exists
    bool contains(K key) {
        return get(key) != nullptr;
    }

    // Remove key
    bool remove(K key) {
        int index = hash(key);
        int originalIndex = index;

        while (table[index].occupied) {
            if (table[index].key == key) {
                table[index].occupied = false;
                size--;
                return true;
            }
            index = (index + 1) % capacity;
            if (index == originalIndex) break;
        }

        return false;
    }

    // Get size
    int getSize() const {
        return size;
    }

    // Clear all entries
    void clear() {
        for (int i = 0; i < capacity; i++) {
            table[i].occupied = false;
        }
        size = 0;
    }

    // Get all keys
    Vector<K> getAllKeys() {
        Vector<K> keys;
        for (int i = 0; i < capacity; i++) {
            if (table[i].occupied) {
                keys.push_back(table[i].key);
            }
        }
        return keys;
    }
};

#endif // HASHTABLE_H
