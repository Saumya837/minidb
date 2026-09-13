#include "parser.hpp"

// bool check(const std::vector<Token>& tokens, size_t pos, TokenType type);
// Token expect(const std::vector<Token>& tokens, size_t& pos, TokenType type);
namespace{
    bool check(const std::vector<Token>& tokens, size_t pos, TokenType type) {
        if (pos >= tokens.size()) return false;
        return tokens[pos].type == type;
    }

    Token expect(const std::vector<Token> &tokens, size_t& pos, TokenType type){
        if (pos >= tokens.size())
            throw std::runtime_error("Unexpexted End of input, expexted token type " + std::to_string(static_cast<int>(type)));
        else if (tokens[pos].type == type){
            return tokens[pos++];
        }
        else{
            throw std::runtime_error("Expected token type " + std::to_string(static_cast<int>(type)) + " but found " + tokens[pos].lexeme + "' at position " + std::to_string(pos));
        }
    }
}

std::unique_ptr<ASTNode> parseStatement(const std::vector<Token>& tokens, size_t& pos){
    if(check(tokens, pos, TokenType::SELECT)){
        throw std::runtime_error("parseSelectStatement not yet implemented");
    } 
    else if(check(tokens, pos, TokenType::INSERT)){
        throw std::runtime_error("parseInsertStatement not yet implemented");
    } 
    else if(check(tokens, pos, TokenType::CREATE)){
        throw std::runtime_error("parseCreateStatement not yet implemented");
    }
    else {
        throw std::runtime_error("Unknown statement type at position " + std::to_string(pos));
    }   
}