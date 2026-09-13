#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class TokenType {
    // keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP,
    FROM, WHERE, GROUP, BY, ORDER, LIMIT, AND, OR, TABLE, JOIN,
    // identifiers & literals
    IDENTIFIER, NUMBER, STRING,
    // operators
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL,
    // punctuation
    COMMA, SEMICOLON,
    // control
    END_OF_INPUT, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
};

const std::unordered_map<std::string, TokenType> keywordTable = {
    {"SELECT", TokenType::SELECT},
    {"INSERT", TokenType::INSERT},
    {"UPDATE", TokenType::UPDATE},
    {"DELETE", TokenType::DELETE},
    {"CREATE", TokenType::CREATE},
    {"ALTER",  TokenType::ALTER},
    {"DROP",   TokenType::DROP},
    {"FROM",   TokenType::FROM},
    {"WHERE",  TokenType::WHERE},
    {"GROUP",  TokenType::GROUP},
    {"BY",     TokenType::BY},
    {"ORDER",  TokenType::ORDER},
    {"LIMIT",  TokenType::LIMIT},
    {"AND",    TokenType::AND},
    {"OR",     TokenType::OR},
    {"TABLE",  TokenType::TABLE},
    {"JOIN",   TokenType::JOIN}
};

std::vector<Token> tokenize(const std::string& sql);