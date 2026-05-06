#pragma once
#include <cstdint>

// ─────────────────────────────────────────────
//  Doubly-Linked List used by the LRU cache
//  Provides O(1) insertion, removal, and move-to-front
// ─────────────────────────────────────────────

struct DLLNode {
    uint32_t  pageId;
    int       frameIndex;   // index into BufferPool::frames[]
    DLLNode*  prev;
    DLLNode*  next;

    DLLNode(uint32_t pid = 0, int fi = -1)
        : pageId(pid), frameIndex(fi), prev(nullptr), next(nullptr) {}
};

class DoublyLinkedList {
public:
    DLLNode* head;   // Most-Recently Used end
    DLLNode* tail;   // Least-Recently Used end
    int      size;

    DoublyLinkedList() : head(nullptr), tail(nullptr), size(0) {}
    ~DoublyLinkedList() { clear(); }

    // Insert at MRU (head)
    void insertFront(DLLNode* node) {
        node->prev = nullptr;
        node->next = head;
        if (head) head->prev = node;
        head = node;
        if (!tail) tail = node;
        ++size;
    }

    // Remove an arbitrary node in O(1)
    void remove(DLLNode* node) {
        if (node->prev) node->prev->next = node->next;
        else            head = node->next;

        if (node->next) node->next->prev = node->prev;
        else            tail = node->prev;

        node->prev = node->next = nullptr;
        --size;
    }

    // Move an existing node to the MRU front
    void moveToFront(DLLNode* node) {
        if (node == head) return;
        remove(node);
        insertFront(node);
    }

    // Remove and return the LRU (tail) node
    DLLNode* evictLRU() {
        if (!tail) return nullptr;
        DLLNode* victim = tail;
        remove(victim);
        return victim;
    }

    void clear() {
        DLLNode* cur = head;
        while (cur) {
            DLLNode* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        head = tail = nullptr;
        size = 0;
    }
};
