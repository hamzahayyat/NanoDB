#pragma once
#include "Page.h"
#include "DoublyLinkedList.h"
#include <cstdint>
#include <cstring>

// ─────────────────────────────────────────────
//  BufferPool  – fixed-size array of frames
//  with LRU eviction via a Doubly-Linked List
//  and a hand-rolled open-address hash map for
//  O(1) pageId → frameIndex lookups.
// ─────────────────────────────────────────────

static constexpr int HASH_SIZE = MAX_PAGES * 2;   // load factor ~0.5

struct HashEntry {
    uint32_t pageId;
    int      frameIndex;
    bool     occupied;
    HashEntry() : pageId(0), frameIndex(-1), occupied(false) {}
};

class BufferPool {
public:
    explicit BufferPool(const char* dbFile, int poolSize = MAX_PAGES);
    ~BufferPool();

    // Fetch a page (load from disk if not cached)
    Page* fetchPage(uint32_t pageId);

    // Allocate a new page, returns its id
    uint32_t allocatePage();

    // Mark page dirty so it is flushed on eviction
    void     markDirty(uint32_t pageId);

    // Force flush all dirty pages to disk
    void     flushAll();

    int evictionCount() const { return evictions; }

private:
    Page         frames[MAX_PAGES];
    DLLNode*     nodeMap[MAX_PAGES];   // frameIndex → DLL node
    HashEntry    hashTable[HASH_SIZE];
    DoublyLinkedList lruList;

    char  filePath[256];
    int   poolCapacity;
    int   numFrames;           // frames currently loaded
public:
    uint32_t nextPageId;
private:
    int   evictions;

    // Hash map ops
    int   hashFind(uint32_t pageId) const;
    void  hashInsert(uint32_t pageId, int frameIndex);
    void  hashRemove(uint32_t pageId);
    int   hashProbe(uint32_t pageId) const;

    // Disk I/O
    void  writeToDisk(uint32_t pageId, const Page* page);
    bool  readFromDisk(uint32_t pageId, Page* page);

    // Evict one frame; returns freed frameIndex
    int   evictOne();

    int   findFreeFrame();
};
