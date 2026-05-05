#pragma once
#include <stdexcept>
#include <cstring>

// ─────────────────────────────────────────────
//  Stack<T> – raw-pointer dynamic array stack
//  NO STL. Manual memory management.
// ─────────────────────────────────────────────

template <typename T>
class Stack {
public:
    Stack() : data(nullptr), topIdx(-1), cap(0) { reserve(16); }

    ~Stack() { delete[] data; }

    void push(const T& val) {
        if (topIdx + 1 >= cap) reserve(cap * 2);
        data[++topIdx] = val;
    }

    T pop() {
        if (isEmpty()) throw std::underflow_error("Stack underflow");
        return data[topIdx--];
    }

    T& peek() {
        if (isEmpty()) throw std::underflow_error("Stack empty");
        return data[topIdx];
    }

    bool isEmpty() const { return topIdx < 0; }
    int  size()    const { return topIdx + 1; }

    void clear() { topIdx = -1; }

private:
    T*  data;
    int topIdx;
    int cap;

    void reserve(int newCap) {
        T* newData = new T[newCap];
        for (int i = 0; i <= topIdx; i++) newData[i] = data[i];
        delete[] data;
        data = newData;
        cap  = newCap;
    }
};
