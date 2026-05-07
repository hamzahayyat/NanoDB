#pragma once
#include "../utils/Array.h"
#include "../utils/Logger.h"
#include <cstring>
#include <cstdio>
#include <climits>

// ─────────────────────────────────────────────
//  JoinGraph – models tables as nodes and
//  join costs as edges. Uses Kruskal's MST
//  to find the cheapest multi-table join path.
// ─────────────────────────────────────────────

static constexpr int MAX_TABLES = 16;

struct JoinEdge {
    int   u, v;      // table indices
    float cost;      // estimated row product / cardinality
};

class JoinGraph {
public:
    int   numNodes;
    char  nodeNames[MAX_TABLES][64];

    JoinGraph() : numNodes(0) {}

    void addTable(const char* name) {
        if (numNodes < MAX_TABLES) {
            std::strncpy(nodeNames[numNodes++], name, 63);
        }
    }

    // Add a directed edge (undirected join cost)
    void addEdge(int u, int v, float cost) {
        JoinEdge e; e.u = u; e.v = v; e.cost = cost;
        edges.push(e);
    }

    void addEdgeByName(const char* a, const char* b, float cost) {
        int u = findNode(a), v = findNode(b);
        if (u >= 0 && v >= 0) addEdge(u, v, cost);
    }

    // Kruskal's MST – returns ordered join path
    Array<JoinEdge> computeMST() {
        // Sort edges by cost (insertion sort – no STL)
        for (int i = 1; i < edges.size(); i++) {
            JoinEdge key = edges[i];
            int j = i - 1;
            while (j >= 0 && edges[j].cost > key.cost) {
                edges[j + 1] = edges[j]; --j;
            }
            edges[j + 1] = key;
        }

        // Union-Find
        int parent[MAX_TABLES];
        for (int i = 0; i < MAX_TABLES; i++) parent[i] = i;

        Array<JoinEdge> mst;
        for (int i = 0; i < edges.size(); i++) {
            int pu = find(parent, edges[i].u);
            int pv = find(parent, edges[i].v);
            if (pu != pv) {
                mst.push(edges[i]);
                parent[pu] = pv;
            }
        }

        // Log MST path
        char pathBuf[256] = "";
        for (int i = 0; i < mst.size(); i++) {
            std::strncat(pathBuf, nodeNames[mst[i].u], 255 - std::strlen(pathBuf));
            std::strncat(pathBuf, " -> ",              255 - std::strlen(pathBuf));
            std::strncat(pathBuf, nodeNames[mst[i].v], 255 - std::strlen(pathBuf));
            if (i < mst.size() - 1) std::strncat(pathBuf, " | ", 255 - std::strlen(pathBuf));
        }
        Logger::log("[OPTIMIZER] Multi-table join routed via MST: %s", pathBuf);
        return mst;
    }

private:
    Array<JoinEdge> edges;

    int findNode(const char* name) const {
        for (int i = 0; i < numNodes; i++)
            if (std::strcmp(nodeNames[i], name) == 0) return i;
        return -1;
    }

    int find(int* parent, int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
};
