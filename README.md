# NanoDB – Architecture & Query Optimizer

> A mini database engine built from scratch in C++17.  
> Graduate project for CS-4002 Applied Programming, FAST-NUCES Islamabad, Spring 2026.

---

## Project Structure

```
NanoDB/
├── include/
│   ├── memory/
│   │   ├── Page.h              # Fixed-size page structure (4 KB)
│   │   ├── DoublyLinkedList.h  # Custom DLL for LRU cache
│   │   └── BufferPool.h        # Buffer pool manager header
│   ├── schema/
│   │   ├── Value.h             # Polymorphic value types (INT/FLOAT/STRING)
│   │   └── Row.h               # Row & Schema definitions
│   ├── parser/
│   │   └── QueryParser.h       # Tokenizer + Shunting-Yard postfix parser
│   ├── catalog/
│   │   └── SystemCatalog.h     # Hash-map based system catalog
│   ├── executor/
│   │   ├── AVLTree.h           # Self-balancing AVL tree index
│   │   ├── JoinGraph.h         # Graph + Kruskal's MST join optimizer
│   │   └── TableEngine.h       # Storage engine (page + index management)
│   ├── utils/
│   │   ├── Logger.h            # File + stdout logger
│   │   ├── Stack.h             # Custom stack (no STL)
│   │   ├── Array.h             # Custom dynamic array (no STL)
│   │   ├── PriorityQueue.h     # Binary min-heap priority queue
│   │   └── TPCHGenerator.h     # TPC-H synthetic data generator
│   └── NanoDB.h                # Top-level engine orchestrator
├── src/
│   ├── memory/
│   │   └── BufferPool.cpp
│   ├── parser/
│   │   └── QueryParser.cpp
│   └── main.cpp                # Test runner entry point
├── data/                        # Generated at runtime
├── CMakeLists.txt
├── Makefile
└── README.md
```

---

## Prerequisites

| Tool    | Version |
|---------|---------|
| g++     | ≥ 9     |
| make    | any     |
| cmake   | ≥ 3.14 (optional) |
| valgrind| any (for memory check) |

---

## Build & Run

### Using Make (recommended)

```bash
# Build
make

# Run full TPC-H workload (20K customers, 30K orders, 50K lineitems)
make run

# Run with 50-page buffer pool (Test Case D stress test)
make stress

# Clean all artifacts
make clean
```

### Using CMake

```bash
mkdir build && cd build
cmake ..
make
./test_runner
```

---

## Execution Output

After running, two outputs are produced:

1. **Console** – live results for all 7 demo test cases
2. **`nanodb_execution.log`** – detailed internal trace including:
   - `[LRU]` page eviction events
   - `[PARSER]` infix → postfix conversions
   - `[OPTIMIZER]` MST join routing decisions
   - `[BENCH]` index vs sequential scan timing
   - `[CACHE HIT/MISS]` buffer pool activity

---

## Demo Test Cases

| Case | Feature | What to Look For |
|------|---------|-----------------|
| A | Parser + Postfix | `[PARSER] Infix "..." -> Postfix "..."` in log |
| B | AVL Index Speedup | Console: sequential vs indexed timing |
| C | MST Join Optimizer | `[OPTIMIZER] MST: customer -> orders -> lineitem` |
| D | LRU Memory Stress | `[STRESS] Total LRU evictions: N` |
| E | Priority Queue | Admin UPDATE executes before 10 SELECT queries |
| F | Deep Expression | Complex nested WHERE evaluated correctly |
| G | Durability | Records survive flush + simulated reboot |

---

## Architecture Notes

### No STL Policy

Every data structure is custom-built:

| STL Equivalent | Custom Implementation |
|---|---|
| `std::vector` | `Array<T>` (include/utils/Array.h) |
| `std::stack` | `Stack<T>` (include/utils/Stack.h) |
| `std::map` | `SystemCatalog` open-addressing hash map |
| `std::list` | `DoublyLinkedList` (include/memory/DoublyLinkedList.h) |
| `std::priority_queue` | `PriorityQueue<T>` binary min-heap |

### Complexity Summary

| Module | Operation | Complexity |
|--------|-----------|------------|
| BufferPool (LRU) | Fetch/Evict | O(1) |
| SystemCatalog | Lookup | O(1) amortized |
| AVL Index | Search/Insert/Delete | O(log N) |
| QueryParser | Tokenize + Postfix | O(N) |
| Join Optimizer (MST) | Kruskal's | O(E log E) |
| Sequential Scan | Full table scan | O(N) |

---

## Memory Check

```bash
valgrind --leak-check=full --error-exitcode=1 ./build/test_runner --no-gen
```

---

## GitHub

> [https://github.com/YOUR_USERNAME/NanoDB](https://github.com/YOUR_USERNAME/NanoDB)

Replace `YOUR_USERNAME` with your actual GitHub username before submission.
