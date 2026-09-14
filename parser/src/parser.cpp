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
        return parseSelectStatement(tokens, pos);
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

std::unique_ptr<ASTNode> parseSelectStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::SELECT);

    auto root = std::make_unique<ASTNode>();
    root -> type = StatementType::SELECT;

    std::vector<std::unique_ptr<ASTNode>> columnList = parseColumnList(tokens, pos);

    auto fromNode = parseFromClause(tokens, pos);
    
    // auto whereNode = std::make_unique<ASTNode>();
    // if (check(tokens, pos,TokenType::WHERE)){
    //     whereNode = parseWhereClause(tokens, pos);
    // }

    

    root->children.push_back(std::move(fromNode));

    for(auto& col : columnList) {
        root->children.push_back(std::move(col));
    }

    // if (whereNode -> type) {
    //      root->children.push_back(std::move(whereNode));
    // }

    expect(tokens, pos, TokenType::SEMICOLON);

    return root;
}

std::vector<std::unique_ptr<ASTNode>> parseColumnList(const std::vector<Token>& tokens, size_t& pos){
    std::vector<std::unique_ptr<ASTNode>> columnList;

    // TODO: future iteration — support SELECT * (star) and column
    // aliasing (e.g. "salary AS s") here. For now, only bare
    // comma-separated IDENTIFIERs are handled.

    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto first_column = std::make_unique<ASTNode>();
    first_column->type = ValueType::COLUMN;
    first_column->value = idToken.lexeme;
    columnList.push_back(std::move(first_column));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto nextId = expect(tokens, pos, TokenType::IDENTIFIER);
        auto next_col = std::make_unique<ASTNode>();
        next_col->type = ValueType::COLUMN;
        next_col->value = nextId.lexeme;
        columnList.push_back(std::move(next_col));
    }
    return columnList;
}


std::unique_ptr<ASTNode> parseFromClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::FROM);

    auto from_node = std::make_unique<ASTNode>();
    from_node -> type = Clauses::FROM;

    auto baseTable = parseRelation(tokens, pos);
    from_node->children.push_back(std::move(baseTable));

    return from_node;
}


std::unique_ptr<ASTNode> parseRelation(const std::vector<Token>& tokens, size_t& pos){
    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto relation = std::make_unique<ASTNode>();
    relation->type = Relations::TABLE;
    relation->value = idToken.lexeme;
    return relation;
}

// std::unique_ptr<ASTNode> parseWhereClause(const std::vector<Token>& tokens, size_t& pos){
//     expect(tokens, pos, TokenType::WHERE);

//     auto wh_node = std::make_unique<ASTNode>();
//     wh_node->type = Clauses::WHERE;
// }


