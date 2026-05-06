#pragma once
#include "Value.h"
#include <cstring>
#include <cstdio>

static constexpr int MAX_COLS = 16;

// ─────────────────────────────────────────────
//  ColumnDef – one column's metadata
// ─────────────────────────────────────────────
struct ColumnDef {
    char      name[64];
    ValueType type;
    int       byteSize;   // serialized size

    ColumnDef() : type(ValueType::NUL), byteSize(0) { name[0] = '\0'; }
    ColumnDef(const char* n, ValueType t)
        : type(t) {
        std::strncpy(name, n, 63); name[63] = '\0';
        byteSize = (t == ValueType::INT)   ? sizeof(int)
                 : (t == ValueType::FLOAT) ? sizeof(float)
                 :                           MAX_STR;
    }
};

// ─────────────────────────────────────────────
//  Schema – ordered list of columns
// ─────────────────────────────────────────────
struct Schema {
    ColumnDef cols[MAX_COLS];
    int       numCols;
    char      tableName[64];

    Schema() : numCols(0) { tableName[0] = '\0'; }

    void addColumn(const char* name, ValueType t) {
        if (numCols < MAX_COLS) cols[numCols++] = ColumnDef(name, t);
    }

    int colIndex(const char* name) const {
        for (int i = 0; i < numCols; i++)
            if (std::strcmp(cols[i].name, name) == 0) return i;
        return -1;
    }

    int rowByteSize() const {
        int sz = 0;
        for (int i = 0; i < numCols; i++) sz += cols[i].byteSize;
        return sz;
    }

    void print() const {
        std::printf("Schema [%s] (", tableName);
        for (int i = 0; i < numCols; i++) {
            const char* t = (cols[i].type == ValueType::INT)    ? "INT"
                          : (cols[i].type == ValueType::FLOAT)  ? "FLOAT"
                          :                                        "STRING";
            std::printf("%s:%s", cols[i].name, t);
            if (i < numCols - 1) std::printf(", ");
        }
        std::printf(")\n");
    }
};

// ─────────────────────────────────────────────
//  Row – holds an array of Value* pointers
// ─────────────────────────────────────────────
struct Row {
    Value* fields[MAX_COLS];
    int    numFields;

    Row() : numFields(0) {
        for (int i = 0; i < MAX_COLS; i++) fields[i] = nullptr;
    }

    Row(const Row& o) : numFields(o.numFields) {
        for (int i = 0; i < numFields; i++)
            fields[i] = o.fields[i] ? o.fields[i]->clone() : nullptr;
    }

    ~Row() { clear(); }

    void clear() {
        for (int i = 0; i < numFields; i++) {
            delete fields[i];
            fields[i] = nullptr;
        }
        numFields = 0;
    }

    void addField(Value* v) {
        if (numFields < MAX_COLS) fields[numFields++] = v;
    }

    void print(const Schema& s) const {
        for (int i = 0; i < numFields; i++) {
            std::printf("%s=", s.cols[i].name);
            if (fields[i]) fields[i]->print();
            else std::printf("NULL");
            if (i < numFields - 1) std::printf(", ");
        }
        std::printf("\n");
    }

    // Serialize row into buf; returns bytes written
    int serialize(char* buf, const Schema& s) const {
        int off = 0;
        for (int i = 0; i < numFields && i < s.numCols; i++) {
            int sz = 0;
            if (fields[i]) fields[i]->serialize(buf + off, sz);
            off += s.cols[i].byteSize;   // always advance fixed size
        }
        return off;
    }

    // Deserialize row from buf
    void deserialize(const char* buf, const Schema& s) {
        clear();
        int off = 0;
        for (int i = 0; i < s.numCols; i++) {
            Value* v = nullptr;
            switch (s.cols[i].type) {
                case ValueType::INT:    v = new IntValue();    break;
                case ValueType::FLOAT:  v = new FloatValue();  break;
                case ValueType::STRING: v = new StringValue(); break;
                default: break;
            }
            if (v) { v->deserialize(buf + off); }
            fields[i] = v;
            off += s.cols[i].byteSize;
        }
        numFields = s.numCols;
    }
};
