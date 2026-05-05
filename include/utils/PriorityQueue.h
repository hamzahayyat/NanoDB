#pragma once
#include <cstring>

// ─────────────────────────────────────────────
//  PriorityQueue<T> – binary min-heap
//  Lower priority number = executed FIRST
//  (admin queries use priority 0; user queries use priority 10)
// ─────────────────────────────────────────────

template <typename T>
struct PQItem {
    T   value;
    int priority;   // 0 = highest
    int seqNum;     // tie-break by arrival order

    bool operator<(const PQItem& o) const {
        if (priority != o.priority) return priority < o.priority;
        return seqNum < o.seqNum;
    }
};

template <typename T>
class PriorityQueue {
public:
    static constexpr int MAX_PQ = 256;

    PriorityQueue() : sz(0), counter(0) {}

    void enqueue(const T& val, int priority) {
        if (sz >= MAX_PQ) return;
        data[sz] = {val, priority, counter++};
        bubbleUp(sz);
        ++sz;
    }

    T dequeue() {
        T ret = data[0].value;
        --sz;
        if (sz > 0) {
            data[0] = data[sz];
            sinkDown(0);
        }
        return ret;
    }

    bool empty() const { return sz == 0; }
    int  size()  const { return sz; }

private:
    PQItem<T> data[MAX_PQ];
    int sz;
    int counter;

    void bubbleUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (data[i] < data[parent]) {
                PQItem<T> tmp = data[i];
                data[i] = data[parent];
                data[parent] = tmp;
                i = parent;
            } else break;
        }
    }

    void sinkDown(int i) {
        while (true) {
            int left = 2 * i + 1, right = 2 * i + 2, smallest = i;
            if (left  < sz && data[left]  < data[smallest]) smallest = left;
            if (right < sz && data[right] < data[smallest]) smallest = right;
            if (smallest == i) break;
            PQItem<T> tmp = data[i];
            data[i] = data[smallest];
            data[smallest] = tmp;
            i = smallest;
        }
    }
};
