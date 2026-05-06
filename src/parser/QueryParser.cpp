#include "../../include/parser/QueryParser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>

// ──────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────
static void strTrimCopy(char* dst, const char* src, int maxLen) {
    while (*src == ' ') src++;
    int n = static_cast<int>(std::strlen(src));
    while (n > 0 && src[n-1] == ' ') --n;
    if (n >= maxLen) n = maxLen - 1;
    std::strncpy(dst, src, n);
    dst[n] = '\0';
}

static bool strEqI(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower(*a) != std::tolower(*b)) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

// ──────────────────────────────────────────────
//  Precedence (higher = tighter binding)
// ──────────────────────────────────────────────
int QueryParser::precedence(TokenType t) {
    switch (t) {
        case TokenType::OP_NOT: return 7;
        case TokenType::OP_MUL:
        case TokenType::OP_DIV:
        case TokenType::OP_MOD: return 6;
        case TokenType::OP_ADD:
        case TokenType::OP_SUB: return 5;
        case TokenType::OP_EQ:
        case TokenType::OP_NEQ:
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LE:
        case TokenType::OP_GE:  return 4;
        case TokenType::OP_AND: return 3;
        case TokenType::OP_OR:  return 2;
        default: return 0;
    }
}

bool QueryParser::isOperator(TokenType t) {
    return isLogical(t) || isComparison(t) || isArithmetic(t);
}

bool QueryParser::isLogical(TokenType t) {
    return t == TokenType::OP_AND || t == TokenType::OP_OR || t == TokenType::OP_NOT;
}

bool QueryParser::isComparison(TokenType t) {
    return t == TokenType::OP_EQ  || t == TokenType::OP_NEQ ||
           t == TokenType::OP_LT  || t == TokenType::OP_GT  ||
           t == TokenType::OP_LE  || t == TokenType::OP_GE;
}

bool QueryParser::isArithmetic(TokenType t) {
    return t == TokenType::OP_ADD || t == TokenType::OP_SUB ||
           t == TokenType::OP_MUL || t == TokenType::OP_DIV ||
           t == TokenType::OP_MOD;
}

// ──────────────────────────────────────────────
//  Tokenizer
// ──────────────────────────────────────────────
Array<Token> QueryParser::tokenize(const char* expr) {
    Array<Token> tokens;
    int i = 0;
    int len = static_cast<int>(std::strlen(expr));

    while (i < len) {
        // Skip whitespace
        if (expr[i] == ' ' || expr[i] == '\t') { ++i; continue; }

        if (expr[i] == '(') { tokens.push(Token(TokenType::LPAREN,  "(")); ++i; continue; }
        if (expr[i] == ')') { tokens.push(Token(TokenType::RPAREN,  ")")); ++i; continue; }

        // Multi-char operators
        if (expr[i] == '!' && expr[i+1] == '=') { tokens.push(Token(TokenType::OP_NEQ, "!=")); i+=2; continue; }
        if (expr[i] == '<' && expr[i+1] == '=') { tokens.push(Token(TokenType::OP_LE,  "<=")); i+=2; continue; }
        if (expr[i] == '>' && expr[i+1] == '=') { tokens.push(Token(TokenType::OP_GE,  ">=")); i+=2; continue; }
        if (expr[i] == '=' && expr[i+1] == '=') { tokens.push(Token(TokenType::OP_EQ,  "==")); i+=2; continue; }
        if (expr[i] == '<')  { tokens.push(Token(TokenType::OP_LT, "<")); ++i; continue; }
        if (expr[i] == '>')  { tokens.push(Token(TokenType::OP_GT, ">")); ++i; continue; }
        if (expr[i] == '=')  { tokens.push(Token(TokenType::OP_EQ, "=")); ++i; continue; }
        if (expr[i] == '*')  { tokens.push(Token(TokenType::OP_MUL,"*")); ++i; continue; }
        if (expr[i] == '/')  { tokens.push(Token(TokenType::OP_DIV,"/")); ++i; continue; }
        if (expr[i] == '%')  { tokens.push(Token(TokenType::OP_MOD,"%")); ++i; continue; }
        if (expr[i] == '+')  { tokens.push(Token(TokenType::OP_ADD,"+")); ++i; continue; }
        if (expr[i] == '-')  { tokens.push(Token(TokenType::OP_SUB,"-")); ++i; continue; }

        // String literal "..."
        if (expr[i] == '"') {
            char buf[128]; int bi = 0; ++i;
            while (i < len && expr[i] != '"') buf[bi++] = expr[i++];
            buf[bi] = '\0'; ++i;
            tokens.push(Token(TokenType::OPERAND, buf));
            continue;
        }

        // Word (keyword or identifier or number)
        if (std::isalnum(expr[i]) || expr[i] == '_' || expr[i] == '.') {
            char buf[128]; int bi = 0;
            while (i < len && (std::isalnum(expr[i]) || expr[i]=='_' || expr[i]=='.'))
                buf[bi++] = expr[i++];
            buf[bi] = '\0';

            if (strEqI(buf, "AND"))  tokens.push(Token(TokenType::OP_AND, "AND"));
            else if (strEqI(buf, "OR"))   tokens.push(Token(TokenType::OP_OR,  "OR"));
            else if (strEqI(buf, "NOT"))  tokens.push(Token(TokenType::OP_NOT, "NOT"));
            else                          tokens.push(Token(TokenType::OPERAND, buf));
            continue;
        }
        ++i; // skip unknown char
    }
    return tokens;
}

// ──────────────────────────────────────────────
//  Shunting-Yard → Postfix
// ──────────────────────────────────────────────
Array<Token> QueryParser::infixToPostfix(const Array<Token>& infix) {
    Array<Token> output;
    Stack<Token> opStack;

    // Build display string for logging
    char infixStr[512] = "";
    for (int i = 0; i < infix.size(); i++) {
        std::strncat(infixStr, infix[i].text, 510 - std::strlen(infixStr));
        std::strncat(infixStr, " ",  510 - std::strlen(infixStr));
    }

    for (int i = 0; i < infix.size(); i++) {
        const Token& tok = infix[i];

        if (tok.type == TokenType::OPERAND) {
            output.push(tok);
        } else if (tok.type == TokenType::LPAREN) {
            opStack.push(tok);
        } else if (tok.type == TokenType::RPAREN) {
            while (!opStack.isEmpty() && opStack.peek().type != TokenType::LPAREN)
                output.push(opStack.pop());
            if (!opStack.isEmpty()) opStack.pop(); // discard LPAREN
        } else if (isOperator(tok.type)) {
            while (!opStack.isEmpty() &&
                   opStack.peek().type != TokenType::LPAREN &&
                   precedence(opStack.peek().type) >= precedence(tok.type))
            {
                output.push(opStack.pop());
            }
            opStack.push(tok);
        }
    }
    while (!opStack.isEmpty()) output.push(opStack.pop());

    // Log postfix
    char postfixStr[512] = "";
    for (int i = 0; i < output.size(); i++) {
        std::strncat(postfixStr, output[i].text, 510 - std::strlen(postfixStr));
        std::strncat(postfixStr, " ",             510 - std::strlen(postfixStr));
    }
    Logger::log("[PARSER] Infix  \"%s\" -> Postfix \"%s\"", infixStr, postfixStr);
    return output;
}

// ──────────────────────────────────────────────
//  Resolve operand to a Value*
// ──────────────────────────────────────────────
Value* QueryParser::resolveOperand(const char* text, const Row& row, const Schema& schema) {
    // Try column lookup
    int ci = schema.colIndex(text);
    if (ci >= 0 && row.fields[ci]) return row.fields[ci]->clone();

    // Float literal
    bool isFloat = false;
    for (int k = 0; text[k]; k++) if (text[k] == '.') { isFloat = true; break; }
    bool isNum = (text[0] == '-' || std::isdigit(text[0]));
    for (int k = 1; text[k]; k++) if (!std::isdigit(text[k]) && text[k] != '.') { isNum = false; break; }

    if (isNum && isFloat)  return new FloatValue(static_cast<float>(std::atof(text)));
    if (isNum)             return new IntValue(std::atoi(text));

    // String literal
    return new StringValue(text);
}

// ──────────────────────────────────────────────
//  Evaluate postfix against a row
// ──────────────────────────────────────────────
bool QueryParser::evaluate(const Array<Token>& postfix, const Row& row, const Schema& schema) {
    // We use a stack of Values*
    Stack<Value*> valStack;

    auto applyArith = [](Value* a, Value* b, TokenType op) -> Value* {
        if (a->type == ValueType::INT && b->type == ValueType::INT) {
            int av = static_cast<IntValue*>(a)->val;
            int bv = static_cast<IntValue*>(b)->val;
            int res = 0;
            if (op == TokenType::OP_ADD) res = av + bv;
            else if (op == TokenType::OP_SUB) res = av - bv;
            else if (op == TokenType::OP_MUL) res = av * bv;
            else if (op == TokenType::OP_DIV) res = (bv != 0) ? av / bv : 0;
            else if (op == TokenType::OP_MOD) res = (bv != 0) ? av % bv : 0;
            return new IntValue(res);
        }
        float av = (a->type == ValueType::FLOAT) ? static_cast<FloatValue*>(a)->val
                                                  : static_cast<float>(static_cast<IntValue*>(a)->val);
        float bv = (b->type == ValueType::FLOAT) ? static_cast<FloatValue*>(b)->val
                                                  : static_cast<float>(static_cast<IntValue*>(b)->val);
        float res = 0;
        if (op == TokenType::OP_ADD) res = av + bv;
        else if (op == TokenType::OP_SUB) res = av - bv;
        else if (op == TokenType::OP_MUL) res = av * bv;
        else if (op == TokenType::OP_DIV) res = (bv != 0.0f) ? av / bv : 0.0f;
        return new FloatValue(res);
    };

    for (int i = 0; i < postfix.size(); i++) {
        const Token& tok = postfix[i];

        if (tok.type == TokenType::OPERAND) {
            valStack.push(resolveOperand(tok.text, row, schema));
            continue;
        }

        if (isArithmetic(tok.type)) {
            Value* b = valStack.pop();
            Value* a = valStack.pop();
            Value* res = applyArith(a, b, tok.type);
            delete a; delete b;
            valStack.push(res);
            continue;
        }

        if (isComparison(tok.type)) {
            Value* b = valStack.pop();
            Value* a = valStack.pop();
            bool result = false;
            switch (tok.type) {
                case TokenType::OP_EQ:  result = a->equals(b);       break;
                case TokenType::OP_NEQ: result = !a->equals(b);      break;
                case TokenType::OP_LT:  result = a->lessThan(b);     break;
                case TokenType::OP_GT:  result = b->lessThan(a);     break;
                case TokenType::OP_LE:  result = !b->lessThan(a);    break;
                case TokenType::OP_GE:  result = !a->lessThan(b);    break;
                default: break;
            }
            delete a; delete b;
            valStack.push(new IntValue(result ? 1 : 0));
            continue;
        }

        if (tok.type == TokenType::OP_AND) {
            Value* b = valStack.pop();
            Value* a = valStack.pop();
            int res = (static_cast<IntValue*>(a)->val != 0 && static_cast<IntValue*>(b)->val != 0) ? 1 : 0;
            delete a; delete b;
            valStack.push(new IntValue(res));
            continue;
        }

        if (tok.type == TokenType::OP_OR) {
            Value* b = valStack.pop();
            Value* a = valStack.pop();
            int res = (static_cast<IntValue*>(a)->val != 0 || static_cast<IntValue*>(b)->val != 0) ? 1 : 0;
            delete a; delete b;
            valStack.push(new IntValue(res));
            continue;
        }

        if (tok.type == TokenType::OP_NOT) {
            Value* a = valStack.pop();
            int res = (static_cast<IntValue*>(a)->val == 0) ? 1 : 0;
            delete a;
            valStack.push(new IntValue(res));
            continue;
        }
    }

    bool result = false;
    if (!valStack.isEmpty()) {
        Value* top = valStack.pop();
        result = (top->type == ValueType::INT && static_cast<IntValue*>(top)->val != 0);
        delete top;
    }

    // Clean remaining stack
    while (!valStack.isEmpty()) { delete valStack.pop(); }
    return result;
}

// ──────────────────────────────────────────────
//  Main parse() entry
// ──────────────────────────────────────────────
ParsedQuery QueryParser::parse(const char* sql) {
    ParsedQuery q;
    char buf[1024];
    std::strncpy(buf, sql, 1023); buf[1023] = '\0';

    // Detect ADMIN prefix
    if (std::strncmp(buf, "ADMIN ", 6) == 0) {
        q.isAdmin = true;
        std::memmove(buf, buf + 6, std::strlen(buf + 6) + 1);
    }

    // Classify query type
    char* ptr = buf;
    while (*ptr == ' ') ptr++;

    if (std::strncmp(ptr, "SELECT", 6) == 0)      q.type = QueryType::SELECT;
    else if (std::strncmp(ptr, "INSERT", 6) == 0) q.type = QueryType::INSERT;
    else if (std::strncmp(ptr, "UPDATE", 6) == 0) q.type = QueryType::UPDATE;
    else if (std::strncmp(ptr, "DELETE", 6) == 0) q.type = QueryType::DELETE;
    else if (std::strncmp(ptr, "CREATE", 6) == 0) q.type = QueryType::CREATE;

    // Extract table name after FROM / INTO / TABLE
    auto extractAfter = [](const char* src, const char* keyword, char* dst, int maxLen) {
        const char* p = std::strstr(src, keyword);
        if (!p) return;
        p += std::strlen(keyword);
        while (*p == ' ') p++;
        int i = 0;
        while (*p && *p != ' ' && *p != '(' && i < maxLen - 1) dst[i++] = *p++;
        dst[i] = '\0';
    };

    if (q.type == QueryType::SELECT || q.type == QueryType::DELETE)
        extractAfter(ptr, "FROM ", q.tableName, 64);
    else if (q.type == QueryType::INSERT)
        extractAfter(ptr, "INTO ", q.tableName, 64);
    else if (q.type == QueryType::UPDATE)
        extractAfter(ptr, "UPDATE ", q.tableName, 64);
    else if (q.type == QueryType::CREATE)
        extractAfter(ptr, "TABLE ", q.tableName, 64);

    // Extract WHERE clause
    const char* wherePtr = std::strstr(ptr, "WHERE ");
    if (!wherePtr) wherePtr = std::strstr(ptr, "where ");
    if (wherePtr) {
        wherePtr += 6;
        const char* setPtr = std::strstr(wherePtr, " SET ");
        int wLen = setPtr ? static_cast<int>(setPtr - wherePtr) : static_cast<int>(std::strlen(wherePtr));
        if (wLen >= 511) wLen = 511;
        std::strncpy(q.whereExpr, wherePtr, wLen);
        q.whereExpr[wLen] = '\0';

        // Tokenize + convert to postfix
        Array<Token> infix = tokenize(q.whereExpr);
        q.postfixTokens = infixToPostfix(infix);
    }

    // Extract SET col = val for UPDATE
    if (q.type == QueryType::UPDATE) {
        const char* setPtr = std::strstr(ptr, " SET ");
        if (setPtr) {
            setPtr += 5;
            char setbuf[128];
            int i = 0;
            while (*setPtr && *setPtr != ' ' && *setPtr != '\n' && i < 127)
                setbuf[i++] = *setPtr++;
            setbuf[i] = '\0';
            // col = val
            char* eq = std::strchr(setbuf, '=');
            if (eq) {
                *eq = '\0';
                std::strncpy(q.setCol, setbuf, 63);
                std::strncpy(q.setVal, eq + 1, 127);
            }
        }
    }

    Logger::log("[PARSER] Query type=%d table=%s admin=%d",
                static_cast<int>(q.type), q.tableName, q.isAdmin ? 1 : 0);
    return q;
}
