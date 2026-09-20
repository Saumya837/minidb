#include "lexer.hpp"
#include <string>
#include <vector>
#include <stdexcept>

std::vector<Token> tokenize(const std::string &sql){
    std::vector<Token> tokens;
    int pos = 0;
    int length = sql.length();

    while (pos < length) {
        char c = sql[pos];

        if (c == ' '){
            pos++;
            continue;
        }

        else if (std::isdigit(static_cast<unsigned char>(c))){
            std::string lexeme = "";
            while ((pos < length) && (std::isdigit(static_cast<unsigned char>(sql[pos])))){
                lexeme += sql[pos];
                pos++;
            }

            if (pos < length && sql[pos] == '.' && pos + 1 < length && std::isdigit(static_cast<unsigned char>(sql[pos + 1]))) {
                lexeme += sql[pos];
                pos++;
                while ((pos < length) && std::isdigit(static_cast<unsigned char>(sql[pos]))){
                    lexeme += sql[pos];
                    pos++;
                }
            }

            tokens.push_back({TokenType::NUMBER, lexeme});
            continue;
        }

        else if (std::isalpha(static_cast<unsigned char>(c)) or c == '_') {
            std::string lexeme = "";
            while((pos < length) && (std::isdigit(static_cast<unsigned char>(sql[pos])) || std::isalpha(static_cast<unsigned char>(sql[pos])) || sql[pos] == '_' || sql[pos] == '.')){
                lexeme += sql[pos];
                pos++;
            }

            std::string upperLexeme = lexeme;
            for (char& ch :upperLexeme){
                ch = std::toupper(static_cast<unsigned char>(ch));
            }
            auto it = keywordTable.find(upperLexeme);
            if (it != keywordTable.end()){
                tokens.push_back(Token{it->second, lexeme});
            } else{
                tokens.push_back(Token{TokenType::IDENTIFIER, lexeme});
            }
            continue;
        }

        else if (c == '\'') {
            pos++;
            std::string lexeme = "";
            while ((pos < length) && (sql[pos] != '\'')){
                lexeme += sql[pos];
                pos++;
            }
            pos++;
            tokens.push_back(Token{TokenType::STRING, lexeme});
            continue;
        }

        else if (c == '>' || c == '<'){
            pos++;
            if(pos < length && sql[pos] == '='){
                tokens.push_back(
                                    Token{ c == '>' ? TokenType::GREATER_EQUAL : TokenType::LESSER_EQUAL, 
                                    c == '>' ? ">=" : "<="}
                                );
                pos++;
            }
            else{
                    tokens.push_back(Token{ c == '>' ? TokenType::GREATER : TokenType::SMALLER, 
                                                c == '>' ? ">" : "<"});
            }
            continue;
        }

        else if (c == '='){
            tokens.push_back(Token{TokenType::EQUALS, "="});
            pos++;
            continue;
        }

        else if (c == ','){
            tokens.push_back(Token{TokenType::COMMA, ","});
            pos++;
            continue;
        }

        else if (c == ';'){
            tokens.push_back(Token{TokenType::SEMICOLON, ";"});
            pos++;
            continue;
        }

        else{
            throw std::runtime_error("Unexpected symbol '" + std::string(1, c) +
                    "' at position " + std::to_string(pos));
        }
    }
    
    tokens.push_back(Token{TokenType::END_OF_INPUT, ""});
    return tokens;
}

    