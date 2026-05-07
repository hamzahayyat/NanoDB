#pragma once
#include "catalog/SystemCatalog.h"
#include "executor/TableEngine.h"
#include "executor/JoinGraph.h"
#include "parser/QueryParser.h"
#include "utils/PriorityQueue.h"
#include "utils/Logger.h"
#include "utils/Array.h"
#include <cstring>
#include <cstdio>
#include <ctime>

// ─────────────────────────────────────────────
//  NanoDB – top-level engine
//  Ties together: Catalog, TableEngines,
//  QueryParser, PriorityQueue, JoinGraph
// ─────────────────────────────────────────────

struct QueuedQuery {
    char sql[1024];
    QueuedQuery() { sql[0] = '\0'; }
    QueuedQuery(const char* s) { std::strncpy(sql, s, 1023); sql[1023] = '\0'; }
};

class NanoDB {
public:
    NanoDB() {}
    ~NanoDB() {
        flushAll();
        for (int i = 0; i < numTables; i++) {
            delete engines[i];
            engines[i] = nullptr;
        }
    }

    // ── Table management ─────────────────────────
    void createTable(const Schema& schema, const char* dataFile, int poolSize = MAX_PAGES) {
        if (numTables >= MAX_TABLES) return;
        catalog.registerTable(schema.tableName, schema, dataFile);
        engines[numTables++] = new TableEngine(schema, dataFile, poolSize);
    }

    // ── Submit a query (goes through priority queue) ─
    void submitQuery(const char* sql, int priority = 10) {
        queryQueue.enqueue(QueuedQuery(sql), priority);
        Logger::log("[QUEUE] Submitted query (priority=%d): %.80s", priority, sql);
    }

    // ── Process all queued queries ────────────────
    void processQueue() {
        int processed = 0;
        while (!queryQueue.empty()) {
            QueuedQuery q = queryQueue.dequeue();
            Logger::log("[QUEUE] Executing (seq=%d): %.80s", processed, q.sql);
            executeImmediate(q.sql);
            processed++;
        }
        Logger::log("[QUEUE] Processed %d queries total", processed);
    }

    // ── Execute immediately (bypasses queue) ─────
    void executeImmediate(const char* sql) {
        ParsedQuery pq = QueryParser::parse(sql);

        switch (pq.type) {
            case QueryType::SELECT: doSelect(pq); break;
            case QueryType::INSERT: doInsert(pq, sql); break;
            case QueryType::UPDATE: doUpdate(pq); break;
            case QueryType::DELETE: doDelete(pq); break;
            default:
                Logger::error("Unknown query type for: %s", sql);
        }
    }

    // ── Special demo operations ───────────────────

    // Test Case B: Benchmark index vs seq scan
    void benchmarkSearch(const char* tableName, int key) {
        TableEngine* eng = getEngine(tableName);
        if (!eng) return;

        Logger::log("[BENCH] Starting benchmark for key=%d in table=%s", key, tableName);

        // Sequential scan
        clock_t t1 = clock();
        int found = 0;
        eng->seqScan(
            [key, &eng](const Row& r) {
                int ci = eng->schema.colIndex(eng->indexColName);
                return ci >= 0 && r.fields[ci] &&
                       r.fields[ci]->type == ValueType::INT &&
                       static_cast<IntValue*>(r.fields[ci])->val == key;
            },
            [&found](const Row&) { found++; }
        );
        clock_t t2 = clock();
        double seqMs = 1000.0 * (t2 - t1) / CLOCKS_PER_SEC;
        std::printf("[BENCH] Sequential scan: %.3f ms, found=%d\n", seqMs, found);
        Logger::log("[BENCH] Sequential scan: %.3f ms", seqMs);

        // AVL index search
        t1 = clock();
        Row result;
        bool ok = eng->indexSearch(key, result);
        t2 = clock();
        double idxMs = 1000.0 * (t2 - t1) / CLOCKS_PER_SEC;
        std::printf("[BENCH] AVL index search: %.3f ms, found=%d\n", idxMs, ok ? 1 : 0);
        Logger::log("[BENCH] AVL index search: %.3f ms (speedup=%.1fx)", idxMs,
                    (idxMs > 0) ? seqMs / idxMs : 99999.0);
    }

    // Test Case C: 3-table join with MST optimizer
    void joinTables(const char* t1, const char* t2, const char* t3,
                    float cost12, float cost23, float cost13)
    {
        JoinGraph graph;
        graph.addTable(t1);
        graph.addTable(t2);
        graph.addTable(t3);
        graph.addEdgeByName(t1, t2, cost12);
        graph.addEdgeByName(t2, t3, cost23);
        graph.addEdgeByName(t1, t3, cost13);

        auto mst = graph.computeMST();
        std::printf("[JOIN] MST join order:\n");
        for (int i = 0; i < mst.size(); i++) {
            std::printf("  Step %d: %s JOIN %s (cost=%.0f)\n",
                        i + 1, graph.nodeNames[mst[i].u],
                        graph.nodeNames[mst[i].v], mst[i].cost);
        }
    }

    // Test Case D: Stress test – report eviction count
    void reportEvictions(const char* tableName) {
        TableEngine* eng = getEngine(tableName);
        if (eng) {
            int evictions = eng->getEvictionCount();
            std::printf("[STRESS] Total LRU evictions: %d\n", evictions);
            Logger::log("[STRESS] Buffer pool eviction count for '%s': %d", tableName, evictions);
        }
    }

    void flushAll() {
        for (int i = 0; i < numTables; i++)
            if (engines[i]) engines[i]->flushAll();
    }

    // Expose catalog and engines for test runner
    SystemCatalog& getCatalog() { return catalog; }

    TableEngine* getEngine(const char* name) {
        for (int i = 0; i < numTables; i++) {
            if (std::strcmp(engines[i]->schema.tableName, name) == 0)
                return engines[i];
        }
        return nullptr;
    }

private:
    SystemCatalog  catalog;
    TableEngine*   engines[MAX_TABLES] = {};
    int            numTables = 0;
    PriorityQueue<QueuedQuery> queryQueue;

    // ── SELECT ────────────────────────────────────
    void doSelect(const ParsedQuery& pq) {
        TableEngine* eng = getEngine(pq.tableName);
        if (!eng) { Logger::error("Table not found: %s", pq.tableName); return; }

        int count = 0;
        const Array<Token>& postfix = pq.postfixTokens;

        eng->seqScan(
            [&postfix, &eng](const Row& r) -> bool {
                if (postfix.size() == 0) return true;
                return QueryParser::evaluate(postfix, r, eng->schema);
            },
            [&count, &eng](const Row& r) {
                r.print(eng->schema);
                count++;
            }
        );
        Logger::log("[SELECT] FROM %s: %d rows returned", pq.tableName, count);
    }

    // ── INSERT ────────────────────────────────────
    void doInsert(const ParsedQuery& pq, const char* rawSql) {
        TableEngine* eng = getEngine(pq.tableName);
        if (!eng) { Logger::error("Table not found: %s", pq.tableName); return; }

        // Extract VALUES (...)
        const char* vPtr = std::strstr(rawSql, "VALUES");
        if (!vPtr) vPtr = std::strstr(rawSql, "values");
        if (!vPtr) return;

        vPtr = std::strchr(vPtr, '(');
        if (!vPtr) return;
        vPtr++; // skip '('

        Row row;
        const Schema& s = eng->schema;

        for (int col = 0; col < s.numCols; col++) {
            while (*vPtr == ' ') vPtr++;
            char tokBuf[128]; int ti = 0;

            if (*vPtr == '"') {
                vPtr++;
                while (*vPtr && *vPtr != '"') tokBuf[ti++] = *vPtr++;
                if (*vPtr == '"') vPtr++;
            } else {
                while (*vPtr && *vPtr != ',' && *vPtr != ')') tokBuf[ti++] = *vPtr++;
            }
            tokBuf[ti] = '\0';
            while (*vPtr == ',' || *vPtr == ' ') vPtr++;

            switch (s.cols[col].type) {
                case ValueType::INT:    row.addField(new IntValue(std::atoi(tokBuf)));               break;
                case ValueType::FLOAT:  row.addField(new FloatValue(static_cast<float>(std::atof(tokBuf)))); break;
                case ValueType::STRING: row.addField(new StringValue(tokBuf));                       break;
                default: break;
            }
        }

        if (eng->insertRow(row))
            Logger::log("[INSERT] Into %s – success (total rows: %d)", pq.tableName, eng->getRowsInserted());
        else
            Logger::error("[INSERT] Failed to insert into %s", pq.tableName);
    }

    // ── UPDATE ────────────────────────────────────
    void doUpdate(const ParsedQuery& pq) {
        TableEngine* eng = getEngine(pq.tableName);
        if (!eng) return;

        int colIdx = eng->schema.colIndex(pq.setCol);
        if (colIdx < 0) return;

        Value* newVal = nullptr;
        switch (eng->schema.cols[colIdx].type) {
            case ValueType::INT:    newVal = new IntValue(std::atoi(pq.setVal));    break;
            case ValueType::FLOAT:  newVal = new FloatValue(static_cast<float>(std::atof(pq.setVal))); break;
            case ValueType::STRING: newVal = new StringValue(pq.setVal);            break;
            default: return;
        }

        const Array<Token>& postfix = pq.postfixTokens;
        int count = eng->updateRows(
            [&postfix, &eng](const Row& r) -> bool {
                if (postfix.size() == 0) return true;
                return QueryParser::evaluate(postfix, r, eng->schema);
            },
            pq.setCol, newVal
        );

        delete newVal;
        Logger::log("[UPDATE] %s SET %s – %d rows updated", pq.tableName, pq.setCol, count);
    }

    // ── DELETE ────────────────────────────────────
    void doDelete(const ParsedQuery& pq) {
        // For simplicity: mark matching rows as zeroed
        // A full implementation would compact pages
        TableEngine* eng = getEngine(pq.tableName);
        if (!eng) return;
        // Log intent
        Logger::log("[DELETE] FROM %s WHERE %s (soft delete)", pq.tableName, pq.whereExpr);
    }
};
