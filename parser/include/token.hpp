#include<string>

enum class TokenType {
    KEYWORD, IDENTIFIER, NUMBER, STRING, OPERATOR,
    COMMA, SEMICOLON, LPAREN, RPAREN, END_OF_INPUT, UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
};