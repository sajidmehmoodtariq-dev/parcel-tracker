#ifndef STACK_H
#define STACK_H

#include "Vector.h"

template<typename T>
class Stack {
private:
    Vector<T> data;

public:
    Stack() {}

    void push(const T& item) {
        data.push_back(item);
    }

    T pop() {
        if (data.getSize() == 0) {
            return T();
        }
        T item = data[data.getSize() - 1];
        data.pop_back();
        return item;
    }

    T peek() const {
        if (data.getSize() == 0) {
            return T();
        }
        return data[data.getSize() - 1];
    }

    bool isEmpty() const {
        return data.getSize() == 0;
    }

    int getSize() const {
        return data.getSize();
    }

    void clear() {
        data.clear();
    }
};

#endif // STACK_H
