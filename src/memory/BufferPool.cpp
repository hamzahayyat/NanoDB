#include "../../include/memory/BufferPool.h"
#include "../../include/utils/Logger.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

// ─── Constructor ────────────────────────────────────────────────────────────
BufferPool::BufferPool(const char* dbFile, int poolSize)
    : poolCapacity(poolSize), numFrames(0), nextPageId(0), evictions(0)
{
    std::strncpy(filePath, dbFile, 255);
    filePath[255] = '\0';

    for (int i = 0; i < MAX_PAGES; i++) nodeMap[i] = nullptr;
    for (int i = 0; i < HASH_SIZE;  i++) hashTable[i] = HashEntry();

    // Determine highest existing page from file size
    FILE* f = std::fopen(filePath, "rb");
    if (f) {
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fclose(f);
        if (sz > 0) nextPageId = static_cast<uint32_t>(sz / sizeof(Page));
    }
}

// ─── Destructor ──────────────────────────────────────────────────────────────
BufferPool::~BufferPool() {
    flushAll();
    // lruList automatically frees its nodes in its destructor
}

// ─── Hash Helpers ────────────────────────────────────────────────────────────
int BufferPool::hashProbe(uint32_t pageId) const {
    return static_cast<int>(pageId % static_cast<uint32_t>(HASH_SIZE));
}

int BufferPool::hashFind(uint32_t pageId) const {
    int idx = hashProbe(pageId);
    for (int i = 0; i < HASH_SIZE; i++) {
        int probe = (idx + i) % HASH_SIZE;
        if (!hashTable[probe].occupied) return -1;
        if (hashTable[probe].pageId == pageId) return hashTable[probe].frameIndex;
    }
    return -1;
}

void BufferPool::hashInsert(uint32_t pageId, int frameIndex) {
    int idx = hashProbe(pageId);
    for (int i = 0; i < HASH_SIZE; i++) {
        int probe = (idx + i) % HASH_SIZE;
        if (!hashTable[probe].occupied) {
            hashTable[probe].pageId = pageId;
            hashTable[probe].frameIndex = frameIndex;
            hashTable[probe].occupied = true;
            return;
        }
    }
}

void BufferPool::hashRemove(uint32_t pageId) {
    int idx = hashProbe(pageId);
    for (int i = 0; i < HASH_SIZE; i++) {
        int probe = (idx + i) % HASH_SIZE;
        if (!hashTable[probe].occupied) return;
        if (hashTable[probe].pageId == pageId) {
            hashTable[probe].occupied = false;
            // Rehash subsequent entries to avoid chain breakage
            int j = (probe + 1) % HASH_SIZE;
            while (hashTable[j].occupied) {
                HashEntry tmp = hashTable[j];
                hashTable[j].occupied = false;
                hashInsert(tmp.pageId, tmp.frameIndex);
                j = (j + 1) % HASH_SIZE;
            }
            return;
        }
    }
}

// ─── Disk I/O ────────────────────────────────────────────────────────────────
void BufferPool::writeToDisk(uint32_t pageId, const Page* page) {
    FILE* f = std::fopen(filePath, "r+b");
    if (!f) f = std::fopen(filePath, "w+b");
    if (!f) return;
    long offset = static_cast<long>(pageId) * static_cast<long>(sizeof(Page));
    std::fseek(f, offset, SEEK_SET);
    std::fwrite(page, sizeof(Page), 1, f);
    std::fclose(f);
}

bool BufferPool::readFromDisk(uint32_t pageId, Page* page) {
    FILE* f = std::fopen(filePath, "rb");
    if (!f) return false;
    long offset = static_cast<long>(pageId) * static_cast<long>(sizeof(Page));
    std::fseek(f, offset, SEEK_SET);
    size_t n = std::fread(page, sizeof(Page), 1, f);
    std::fclose(f);
    return (n == 1);
}

// ─── Eviction ────────────────────────────────────────────────────────────────
int BufferPool::evictOne() {
    DLLNode* victim = lruList.evictLRU();
    if (!victim) return -1;

    int fi = victim->frameIndex;
    uint32_t pid = victim->pageId;

    if (frames[fi].header.isDirty) {
        writeToDisk(pid, &frames[fi]);
        Logger::log("[LRU] Page %u evicted (dirty → disk), frame %d", pid, fi);
    } else {
        Logger::log("[LRU] Page %u evicted (clean), frame %d", pid, fi);
    }

    hashRemove(pid);
    nodeMap[fi] = nullptr;
    delete victim;

    evictions++;
    numFrames--;
    return fi;
}

int BufferPool::findFreeFrame() {
    for (int i = 0; i < poolCapacity; i++) {
        if (nodeMap[i] == nullptr) return i;
    }
    return -1;
}

// ─── Public API ──────────────────────────────────────────────────────────────
Page* BufferPool::fetchPage(uint32_t pageId) {
    int fi = hashFind(pageId);
    if (fi != -1) {
        lruList.moveToFront(nodeMap[fi]);
        Logger::log("[CACHE HIT] Page %u at frame %d", pageId, fi);
        return &frames[fi];
    }

    // Cache miss – find a free frame (evict if full)
    fi = findFreeFrame();
    if (fi == -1) fi = evictOne();
    if (fi == -1) return nullptr;

    frames[fi].clear();
    readFromDisk(pageId, &frames[fi]);
    frames[fi].header.pageId = pageId;

    DLLNode* node = new DLLNode(pageId, fi);
    lruList.insertFront(node);
    nodeMap[fi] = node;
    hashInsert(pageId, fi);
    numFrames++;

    Logger::log("[CACHE MISS] Page %u loaded to frame %d", pageId, fi);
    return &frames[fi];
}

uint32_t BufferPool::allocatePage() {
    uint32_t pid = nextPageId++;
    int fi = findFreeFrame();
    if (fi == -1) fi = evictOne();
    if (fi == -1) return UINT32_MAX;

    frames[fi].clear();
    frames[fi].header.pageId = pid;
    frames[fi].header.isDirty = true;

    DLLNode* node = new DLLNode(pid, fi);
    lruList.insertFront(node);
    nodeMap[fi] = node;
    hashInsert(pid, fi);
    numFrames++;

    Logger::log("[ALLOC] New page %u at frame %d", pid, fi);
    return pid;
}

void BufferPool::markDirty(uint32_t pageId) {
    int fi = hashFind(pageId);
    if (fi != -1) frames[fi].header.isDirty = true;
}

void BufferPool::flushAll() {
    DLLNode* cur = lruList.head;
    while (cur) {
        int fi = cur->frameIndex;
        if (frames[fi].header.isDirty) {
            writeToDisk(frames[fi].header.pageId, &frames[fi]);
            frames[fi].header.isDirty = false;
            Logger::log("[FLUSH] Page %u written to disk", frames[fi].header.pageId);
        }
        cur = cur->next;
    }
}
