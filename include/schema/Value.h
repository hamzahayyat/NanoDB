#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>

enum class ValueType { INT, FLOAT, STRING, NUL };

class Value {
public:
    ValueType type;
    explicit Value(ValueType t) : type(t) {}
    virtual ~Value() = default;
    virtual Value* clone()                const = 0;
    virtual bool   equals(const Value* o) const = 0;
    virtual bool   lessThan(const Value* o) const = 0;
    virtual void   print()                const = 0;
    virtual void   serialize(char* buf, int& sz) const = 0;
    virtual void   deserialize(const char* buf) = 0;
    bool operator==(const Value& o) const { return equals(&o); }
    bool operator!=(const Value& o) const { return !equals(&o); }
    bool operator< (const Value& o) const { return lessThan(&o); }
    bool operator> (const Value& o) const { return o.lessThan(this); }
    bool operator<=(const Value& o) const { return !o.lessThan(this); }
    bool operator>=(const Value& o) const { return !lessThan(&o); }
};

class IntValue;
class FloatValue;
class StringValue;

class FloatValue : public Value {
public:
    float val;
    FloatValue(float v = 0.0f) : Value(ValueType::FLOAT), val(v) {}
    Value* clone() const override { return new FloatValue(val); }
    bool   equals(const Value* o) const override;
    bool   lessThan(const Value* o) const override;
    void   print() const override { std::printf("%.4f", val); }
    void   serialize(char* buf, int& sz) const override {
        std::memcpy(buf, &val, sizeof(float)); sz = sizeof(float);
    }
    void deserialize(const char* buf) override { std::memcpy(&val, buf, sizeof(float)); }
};

class IntValue : public Value {
public:
    int val;
    IntValue(int v = 0) : Value(ValueType::INT), val(v) {}
    Value* clone() const override { return new IntValue(val); }
    bool   equals(const Value* o) const override {
        if (o->type == ValueType::INT)   return val == static_cast<const IntValue*>(o)->val;
        if (o->type == ValueType::FLOAT) return static_cast<float>(val) == static_cast<const FloatValue*>(o)->val;
        return false;
    }
    bool   lessThan(const Value* o) const override {
        if (o->type == ValueType::INT)   return val < static_cast<const IntValue*>(o)->val;
        if (o->type == ValueType::FLOAT) return static_cast<float>(val) < static_cast<const FloatValue*>(o)->val;
        return false;
    }
    void print() const override { std::printf("%d", val); }
    void serialize(char* buf, int& sz) const override {
        std::memcpy(buf, &val, sizeof(int)); sz = sizeof(int);
    }
    void deserialize(const char* buf) override { std::memcpy(&val, buf, sizeof(int)); }
};

inline bool FloatValue::equals(const Value* o) const {
    if (o->type == ValueType::FLOAT) return val == static_cast<const FloatValue*>(o)->val;
    if (o->type == ValueType::INT)   return val == static_cast<float>(static_cast<const IntValue*>(o)->val);
    return false;
}
inline bool FloatValue::lessThan(const Value* o) const {
    if (o->type == ValueType::FLOAT) return val < static_cast<const FloatValue*>(o)->val;
    if (o->type == ValueType::INT)   return val < static_cast<float>(static_cast<const IntValue*>(o)->val);
    return false;
}

static constexpr int MAX_STR = 128;

class StringValue : public Value {
public:
    char val[MAX_STR];
    StringValue() : Value(ValueType::STRING) { val[0] = '\0'; }
    explicit StringValue(const char* s) : Value(ValueType::STRING) {
        std::strncpy(val, s, MAX_STR - 1); val[MAX_STR - 1] = '\0';
    }
    Value* clone() const override { return new StringValue(val); }
    bool   equals(const Value* o) const override {
        if (o->type != ValueType::STRING) return false;
        return std::strcmp(val, static_cast<const StringValue*>(o)->val) == 0;
    }
    bool   lessThan(const Value* o) const override {
        if (o->type != ValueType::STRING) return false;
        return std::strcmp(val, static_cast<const StringValue*>(o)->val) < 0;
    }
    void print() const override { std::printf("\"%s\"", val); }
    void serialize(char* buf, int& sz) const override { std::memcpy(buf, val, MAX_STR); sz = MAX_STR; }
    void deserialize(const char* buf) override { std::memcpy(val, buf, MAX_STR); }
};

inline Value* makeIntValue(int v)            { return new IntValue(v); }
inline Value* makeFloatValue(float v)        { return new FloatValue(v); }
inline Value* makeStringValue(const char* s) { return new StringValue(s); }
