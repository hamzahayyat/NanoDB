#pragma once
#include <cstdint>
#include <cstring>

// ─────────────────────────────────────────────
//  Page – fixed-size unit of storage (4 KB)
// ─────────────────────────────────────────────
static constexpr int PAGE_SIZE      = 4096;   // bytes per page
static constexpr int MAX_PAGES      = 512;    // buffer pool capacity

struct PageHeader {
    uint32_t pageId;
    uint32_t recordCount;
    uint32_t freeOffset;      // next write position inside data[]
    bool     isDirty;
    uint8_t  _pad[3];
};

struct Page {
    PageHeader header;
    char       data[PAGE_SIZE - sizeof(PageHeader)];

    Page() { clear(); }

    void clear() {
        std::memset(this, 0, sizeof(Page));
    }

    // Returns how many bytes are still available
    int freeSpace() const {
        return static_cast<int>(sizeof(data)) - static_cast<int>(header.freeOffset);
    }

    // Write raw bytes; returns start offset or -1 if full
    int write(const void* src, uint32_t len) {
        if (static_cast<int>(len) > freeSpace()) return -1;
        std::memcpy(data + header.freeOffset, src, len);
        int off = static_cast<int>(header.freeOffset);
        header.freeOffset += len;
        header.recordCount++;
        header.isDirty = true;
        return off;
    }

    // Read raw bytes from offset
    void read(void* dst, uint32_t offset, uint32_t len) const {
        std::memcpy(dst, data + offset, len);
    }
};
