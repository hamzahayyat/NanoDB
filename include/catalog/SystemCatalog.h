#pragma once
#include "../utils/Logger.h"
#include "../schema/Row.h"
#include <cstring>
#include <cstdio>
#include <cstdint>

// ─────────────────────────────────────────────
//  System Catalog – stores table metadata
//  Uses a custom open-addressing hash map
//  for O(1) amortized lookups
// ─────────────────────────────────────────────

static constexpr int CATALOG_SIZE = 64;

struct TableEntry {
    bool    occupied;
    char    name[64];
    Schema  schema;
    char    dataFile[128];    // e.g. "data/customer.bin"
    uint32_t rootPageId;      // first page of table data

    TableEntry() : occupied(false), rootPageId(0) {
        name[0] = dataFile[0] = '\0';
    }
};

class SystemCatalog {
public:
    SystemCatalog() {
        for (int i = 0; i < CATALOG_SIZE; i++) table[i] = TableEntry();
    }

    // Register a table
    void registerTable(const char* name, const Schema& schema,
                       const char* dataFile, uint32_t rootPageId = 0)
    {
        int idx = probe(name);
        table[idx].occupied   = true;
        table[idx].schema     = schema;
        table[idx].rootPageId = rootPageId;
        std::strncpy(table[idx].name,     name,     63);
        std::strncpy(table[idx].dataFile, dataFile, 127);
        Logger::log("[CATALOG] Registered table '%s' -> '%s'", name, dataFile);
    }

    // Lookup; returns nullptr if not found
    TableEntry* findTable(const char* name) {
        int start = hash(name);
        for (int i = 0; i < CATALOG_SIZE; i++) {
            int idx = (start + i) % CATALOG_SIZE;
            if (!table[idx].occupied) return nullptr;
            if (std::strcmp(table[idx].name, name) == 0) return &table[idx];
        }
        return nullptr;
    }

    void printAll() const {
        std::printf("=== System Catalog ===\n");
        for (int i = 0; i < CATALOG_SIZE; i++) {
            if (table[i].occupied)
                std::printf("  [%d] %s -> %s\n", i, table[i].name, table[i].dataFile);
        }
    }

private:
    TableEntry table[CATALOG_SIZE];

    int hash(const char* s) const {
        unsigned long h = 5381;
        while (*s) h = ((h << 5) + h) + static_cast<unsigned char>(*s++);
        return static_cast<int>(h % static_cast<unsigned long>(CATALOG_SIZE));
    }

    int probe(const char* name) {
        int start = hash(name);
        for (int i = 0; i < CATALOG_SIZE; i++) {
            int idx = (start + i) % CATALOG_SIZE;
            if (!table[idx].occupied || std::strcmp(table[idx].name, name) == 0)
                return idx;
        }
        return start; // fallback (should not reach)
    }
};

#include "../utils/Logger.h"
