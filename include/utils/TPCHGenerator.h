#pragma once
#include "../schema/Row.h"
#include "../utils/Logger.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ─────────────────────────────────────────────
//  TPC-H Data Generator
//  Generates synthetic customer, orders, lineitem
//  records and writes them as SQL INSERT statements
//  into a queries.txt workload file
// ─────────────────────────────────────────────

static const char* SEGMENTS[] = {"AUTOMOBILE","BUILDING","FURNITURE","HOUSEHOLD","MACHINERY"};
static const char* STATUS[]   = {"O","F","P"};
static const char* PRIORITY[] = {"1-URGENT","2-HIGH","3-MEDIUM","4-NOT SPECIFIED","5-LOW"};

class TPCHGenerator {
public:
    static void generate(const char* outputFile,
                         int numCustomers,
                         int numOrders,
                         int numLineItems)
    {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        FILE* f = std::fopen(outputFile, "w");
        if (!f) { Logger::error("Cannot open %s", outputFile); return; }

        // Customers
        for (int i = 1; i <= numCustomers; i++) {
            char name[64];
            std::snprintf(name, sizeof(name), "Customer#%06d", i);
            int    nation  = (std::rand() % 25) + 1;
            float  acctbal = static_cast<float>(std::rand() % 10000) + static_cast<float>(std::rand() % 100) / 100.0f;
            const char* seg = SEGMENTS[std::rand() % 5];
            std::fprintf(f,
                "INSERT INTO customer VALUES (%d, \"%s\", %d, %.2f, \"%s\")\n",
                i, name, nation, acctbal, seg);
        }

        // Orders
        for (int i = 1; i <= numOrders; i++) {
            int    custkey   = (std::rand() % numCustomers) + 1;
            float  totalprice = 1000.0f + static_cast<float>(std::rand() % 500000);
            const char* stat = STATUS[std::rand() % 3];
            const char* pri  = PRIORITY[std::rand() % 5];
            std::fprintf(f,
                "INSERT INTO orders VALUES (%d, %d, \"%.2f\", \"%s\", \"%s\")\n",
                i, custkey, totalprice, stat, pri);
        }

        // Lineitems
        for (int i = 1; i <= numLineItems; i++) {
            int   orderkey  = (std::rand() % numOrders) + 1;
            float quantity  = static_cast<float>((std::rand() % 50) + 1);
            float extprice  = quantity * (1.0f + static_cast<float>(std::rand() % 100));
            float discount  = static_cast<float>(std::rand() % 10) / 100.0f;
            std::fprintf(f,
                "INSERT INTO lineitem VALUES (%d, %d, %.2f, %.2f, %.2f)\n",
                i, orderkey, quantity, extprice, discount);
        }

        std::fclose(f);
        Logger::log("[GENERATOR] Wrote %d customers, %d orders, %d lineitems to %s",
                    numCustomers, numOrders, numLineItems, outputFile);
    }

    // Append demo queries for all 7 test cases
    static void appendDemoQueries(const char* outputFile) {
        FILE* f = std::fopen(outputFile, "a");
        if (!f) return;

        std::fprintf(f, "\n# === DEMO TEST CASES ===\n");

        // Test A
        std::fprintf(f,
            "SELECT FROM customer WHERE (c_acctbal > 5000 AND c_mktsegment == \"BUILDING\") OR c_nationkey == 15\n");

        // Test B triggers are done in test_runner code

        // Test C
        std::fprintf(f, "# JOIN customer orders lineitem\n");

        // Various SELECTs for priority queue stress test
        for (int i = 0; i < 50; i++)
            std::fprintf(f, "SELECT FROM orders WHERE o_orderstatus == \"O\"\n");

        // Admin update
        std::fprintf(f, "ADMIN UPDATE customer SET c_acctbal=9999.99 WHERE c_custkey == 1\n");

        // Test F
        std::fprintf(f,
            "SELECT FROM orders WHERE ( (o_totalprice * 1.5) > 100000 AND (o_custkey %% 2 == 0) ) OR (o_orderstatus != \"O\")\n");

        // Test G inserts
        for (int i = 99001; i <= 99005; i++) {
            std::fprintf(f,
                "INSERT INTO customer VALUES (%d, \"NewCustomer#%06d\", 10, 1234.56, \"BUILDING\")\n",
                i, i);
        }

        std::fclose(f);
        Logger::log("[GENERATOR] Appended demo queries to %s", outputFile);
    }
};
