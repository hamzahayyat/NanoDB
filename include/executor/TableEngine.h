#pragma once
#include "../memory/BufferPool.h"
#include "../schema/Row.h"
#include "../executor/AVLTree.h"
#include "../utils/Logger.h"
#include "../utils/Array.h"
#include <cstring>
#include <cstdio>
#include <ctime>

// ─────────────────────────────────────────────
//  TableEngine – manages storage of rows
//  into pages via the BufferPool and maintains
//  an AVL index on the primary key column
// ─────────────────────────────────────────────

class TableEngine {
public:
    Schema    schema;
    AVLTree   index;       // primary key index
    char      indexColName[64];
    int       indexColIdx;

    explicit TableEngine(const Schema& s, const char* dbFile, int poolSize = MAX_PAGES)
        : schema(s), indexColIdx(-1), pool(dbFile, poolSize)
    {
        indexColName[0] = '\0';
        rowsInserted = 0;
        // Default: index first INT column
        for (int i = 0; i < s.numCols; i++) {
            if (s.cols[i].type == ValueType::INT) {
                std::strncpy(indexColName, s.cols[i].name, 63);
                indexColIdx = i;
                break;
            }
        }
    }

    ~TableEngine() { pool.flushAll(); }

    // ── INSERT ───────────────────────────────────
    bool insertRow(const Row& row) {
        int rowSz = schema.rowByteSize();
        char* buf = new char[rowSz]();
        row.serialize(buf, schema);

        // Find a page with enough space
        Page* pg = nullptr;
        uint32_t pgId = 0;
        for (uint32_t pid = 0; pid < pool.nextPageId; pid++) {
            Page* p = pool.fetchPage(pid);
            if (p && p->freeSpace() >= rowSz) {
                pg = p; pgId = pid; break;
            }
        }
        if (!pg) {
            pgId = pool.allocatePage();
            pg   = pool.fetchPage(pgId);
        }

        if (!pg) { delete[] buf; return false; }

        int offset = pg->write(buf, rowSz);
        delete[] buf;
        if (offset < 0) return false;

        pool.markDirty(pgId);

        // Update AVL index on primary key
        if (indexColIdx >= 0 && row.fields[indexColIdx] &&
            row.fields[indexColIdx]->type == ValueType::INT)
        {
            int key = static_cast<IntValue*>(row.fields[indexColIdx])->val;
            index.insert(key, {pgId, offset});
        }

        rowsInserted++;
        return true;
    }

    // ── SEQUENTIAL SCAN (no index) ───────────────
    // Calls cb(row) for every row satisfying filter (nullptr = all)
    template <typename Filter, typename Callback>
    int seqScan(Filter filter, Callback cb) {
        int count = 0;
        int rowSz = schema.rowByteSize();

        for (uint32_t pid = 0; pid < pool.nextPageId; pid++) {
            Page* pg = pool.fetchPage(pid);
            if (!pg) continue;

            uint32_t off = 0;
            for (uint32_t r = 0; r < pg->header.recordCount; r++) {
                if (static_cast<int>(off) + rowSz > static_cast<int>(pg->header.freeOffset)) break;

                Row row;
                row.deserialize(pg->data + off, schema);
                if (filter(row)) { cb(row); count++; }
                off += rowSz;
            }
        }
        return count;
    }

    // ── INDEX SEARCH (AVL) ───────────────────────
    bool indexSearch(int key, Row& outRow) {
        AVLRecord* rec = index.search(key);
        if (!rec) return false;

        Page* pg = pool.fetchPage(rec->pageId);
        if (!pg) return false;

        outRow.deserialize(pg->data + rec->rowOffset, schema);
        Logger::log("[INDEX] AVL lookup key=%d found at page=%u offset=%d", key, rec->pageId, rec->rowOffset);
        return true;
    }

    // ── UPDATE ───────────────────────────────────
    template <typename Filter>
    int updateRows(Filter filter, const char* colName, Value* newVal) {
        int count = 0;
        int rowSz = schema.rowByteSize();
        int colIdx = schema.colIndex(colName);
        if (colIdx < 0) return 0;

        for (uint32_t pid = 0; pid < pool.nextPageId; pid++) {
            Page* pg = pool.fetchPage(pid);
            if (!pg) continue;

            uint32_t off = 0;
            for (uint32_t r = 0; r < pg->header.recordCount; r++) {
                if (static_cast<int>(off) + rowSz > static_cast<int>(pg->header.freeOffset)) break;

                Row row;
                row.deserialize(pg->data + off, schema);
                if (filter(row)) {
                    // Replace field
                    delete row.fields[colIdx];
                    row.fields[colIdx] = newVal->clone();
                    // Re-serialize in place
                    char* buf = new char[rowSz]();
                    row.serialize(buf, schema);
                    pg->write(buf, 0); // just re-place (write at off directly)
                    std::memcpy(pg->data + off, buf, rowSz);
                    pg->header.isDirty = true;
                    pool.markDirty(pid);
                    delete[] buf;
                    count++;
                }
                off += rowSz;
            }
        }
        return count;
    }

    int  getRowsInserted()  const { return rowsInserted; }
    int  getEvictionCount() const { return pool.evictionCount(); }
    void flushAll() { pool.flushAll(); }

    // Expose nextPageId for table scan bounds
    uint32_t pageCount() const { return pool.nextPageId; }

public:
    BufferPool pool;
    int        rowsInserted;
};
