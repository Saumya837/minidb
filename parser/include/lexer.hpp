#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class TokenType {
    // keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP, DISTINCT,

    FROM, WHERE, GROUP, BY, ORDER, LIMIT, AND, OR, INTO, VALUES,
    // identifiers & literals
    IDENTIFIER, NUMBER, STRING,
    // operators
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL,
    // punctuation
    COMMA, SEMICOLON,
    //DDL keywords
    TABLE, INDEX, FUNCTION,

    // ORDER keywords
    ASC, DESC,

    // JOIN
    LEFT, RIGHT, JOIN,
    
    ON, AS, HAVING, LPAREN, RPAREN, STAR,
    
    // control
    END_OF_INPUT, UNKNOWN,
};

struct Token {
    TokenType type;
    std::string lexeme;
};

const std::unordered_map<std::string, TokenType> keywordTable = {
    {"SELECT", TokenType::SELECT},
    {"DISTINCT", TokenType::DISTINCT},
    {"INSERT", TokenType::INSERT},
    {"INTO", TokenType::INTO},
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
    {"INDEX", TokenType::INDEX},
    {"FUNCTION", TokenType::FUNCTION},
    {"JOIN",   TokenType::JOIN},
    {"ASC",    TokenType::ASC},
    {"DESC",   TokenType::DESC},
    {"LEFT",   TokenType::LEFT},
    {"RIGHT",  TokenType::RIGHT},
    {"ON",     TokenType::ON},
    {"AS",     TokenType::AS},
    {"HAVING", TokenType::HAVING},
    {"VALUES", TokenType::VALUES}
};

std::vector<Token> tokenize(const std::string& sql);