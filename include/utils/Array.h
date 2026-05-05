#pragma once
#include <cstring>
#include <stdexcept>

// ─────────────────────────────────────────────
//  Array<T> – heap-backed resizable array
//  NO STL. Manual memory management.
// ─────────────────────────────────────────────

template <typename T>
class Array {
public:
    Array() : data(nullptr), sz(0), cap(0) { reserve(8); }

    explicit Array(int initial) : data(nullptr), sz(0), cap(0) { reserve(initial); }

    Array(const Array& o) : data(nullptr), sz(0), cap(0) {
        reserve(o.cap);
        for (int i = 0; i < o.sz; i++) data[i] = o.data[i];
        sz = o.sz;
    }

    Array& operator=(const Array& o) {
        if (this == &o) return *this;
        delete[] data;
        data = nullptr; sz = 0; cap = 0;
        reserve(o.cap);
        for (int i = 0; i < o.sz; i++) data[i] = o.data[i];
        sz = o.sz;
        return *this;
    }

    ~Array() { delete[] data; }

    void push(const T& val) {
        if (sz >= cap) reserve(cap * 2);
        data[sz++] = val;
    }

    void pop() { if (sz > 0) --sz; }

    T& operator[](int i)       { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

    T& back()  { return data[sz - 1]; }
    T& front() { return data[0]; }

    int  size()    const { return sz; }
    bool empty()   const { return sz == 0; }

    void clear() { sz = 0; }

    void removeAt(int idx) {
        for (int i = idx; i < sz - 1; i++) data[i] = data[i + 1];
        --sz;
    }

    // Simple linear search
    int find(const T& val) const {
        for (int i = 0; i < sz; i++)
            if (data[i] == val) return i;
        return -1;
    }

private:
    T*  data;
    int sz;
    int cap;

    void reserve(int newCap) {
        if (newCap < 1) newCap = 1;
        T* newData = new T[newCap];
        for (int i = 0; i < sz; i++) newData[i] = data[i];
        delete[] data;
        data = newData;
        cap  = newCap;
    }
};
