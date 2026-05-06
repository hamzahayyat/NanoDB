#pragma once
#include "../utils/Stack.h"
#include "../utils/Array.h"
#include "../utils/Logger.h"
#include "../schema/Row.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

// ─────────────────────────────────────────────
//  Token types for the expression parser
// ─────────────────────────────────────────────
enum class TokenType {
    OPERAND,     // column name or literal
    OP_AND, OP_OR, OP_NOT,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_MUL, OP_DIV, OP_MOD, OP_ADD, OP_SUB,
    LPAREN, RPAREN,
    END
};

struct Token {
    TokenType type;
    char      text[128];

    Token() : type(TokenType::END) { text[0] = '\0'; }
    Token(TokenType t, const char* s) : type(t) {
        std::strncpy(text, s, 127); text[127] = '\0';
    }
};

// ─────────────────────────────────────────────
//  Parsed query structure
// ─────────────────────────────────────────────
enum class QueryType { SELECT, INSERT, UPDATE, DELETE, CREATE, UNKNOWN };

struct ParsedQuery {
    QueryType type;
    char      tableName[64];
    char      whereExpr[512];        // raw WHERE string
    Array<Token> postfixTokens;      // Shunting-yard output
    char      setCol[64];            // for UPDATE
    char      setVal[128];           // for UPDATE
    Array<char*> insertValues;       // raw value strings (caller frees)
    char      insertCols[MAX_COLS][64];
    int       insertColCount;
    bool      isAdmin;

    ParsedQuery() : type(QueryType::UNKNOWN), isAdmin(false), insertColCount(0) {
        tableName[0] = whereExpr[0] = setCol[0] = setVal[0] = '\0';
    }
};

// ─────────────────────────────────────────────
//  QueryParser – tokenizes + Shunting-Yard
// ─────────────────────────────────────────────
class QueryParser {
public:
    // Parse a full SQL-like string
    static ParsedQuery parse(const char* sql);

    // Evaluate postfix token array against a row + schema
    // Returns true if the row satisfies the condition
    static bool evaluate(const Array<Token>& postfix, const Row& row, const Schema& schema);

private:
    // Tokenize the WHERE expression
    static Array<Token> tokenize(const char* expr);

    // Shunting-Yard algorithm → postfix
    static Array<Token> infixToPostfix(const Array<Token>& infix);

    // Operator precedence
    static int precedence(TokenType t);
    static bool isOperator(TokenType t);
    static bool isLogical(TokenType t);
    static bool isComparison(TokenType t);
    static bool isArithmetic(TokenType t);

    // Get a Value* from a token (column lookup or literal)
    static Value* resolveOperand(const char* text, const Row& row, const Schema& schema);
    static void   freeValue(Value* v, bool isLiteral);
};
