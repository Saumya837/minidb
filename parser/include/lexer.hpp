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

     // data types
    INT, INTEGER, BIGINT, SMALLINT,
    FLOAT, DOUBLE, DECIMAL,
    CHAR, VARCHAR, TEXT,
    BOOLEAN, DATE, TIMESTAMP,

    // constraint keywords
    CONSTRAINT, FOREIGN, REFERENCES, CHECK,
    CASCADE, RESTRICT, AUTO_INCREMENT, PRIMARY,
    KEY, UNIQUE, DEFAULT,

    // ORDER keywords
    ASC, DESC, NOT,

    // JOIN
    LEFT, RIGHT, JOIN,
    
    ON, AS, HAVING, LPAREN, RPAREN, STAR, SET, 

    IF, ELSE, EXISTS, IS,
    
    NULL_KW, ISNULL, NOTNULL, 

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
    {"VALUES", TokenType::VALUES},
    {"IF",     TokenType::IF},
    {"NOT",    TokenType::NOT},
    {"IS",     TokenType::IS},
    {"NULL",   TokenType::NULL_KW},
    {"NOTNULL", TokenType::NOTNULL},
    {"ISNULL",        TokenType::ISNULL},
    {"EXISTS",        TokenType::EXISTS},
    {"SET",           TokenType::SET},
    {"PRIMARY",       TokenType::PRIMARY},
    {"KEY",           TokenType::KEY},
    {"UNIQUE",        TokenType::UNIQUE},
    {"DEFAULT",       TokenType::DEFAULT},
    {"CONSTRAINT",    TokenType::CONSTRAINT},
    {"FOREIGN",       TokenType::FOREIGN},
    {"REFERENCES",    TokenType::REFERENCES},
    {"CHECK",         TokenType::CHECK},
    {"CASCADE",       TokenType::CASCADE},
    {"RESTRICT",      TokenType::RESTRICT},
    {"AUTO_INCREMENT", TokenType::AUTO_INCREMENT},
    {"INT",       TokenType::INT},
    {"INTEGER",   TokenType::INTEGER},
    {"BIGINT",    TokenType::BIGINT},
    {"SMALLINT",  TokenType::SMALLINT},
    {"FLOAT",     TokenType::FLOAT},
    {"DOUBLE",    TokenType::DOUBLE},
    {"DECIMAL",   TokenType::DECIMAL},
    {"CHAR",      TokenType::CHAR},
    {"VARCHAR",   TokenType::VARCHAR},
    {"TEXT",      TokenType::TEXT},
    {"BOOLEAN",   TokenType::BOOLEAN},
    {"DATE",      TokenType::DATE},
    {"TIMESTAMP", TokenType::TIMESTAMP}
};

std::vector<Token> tokenize(const std::string& sql);