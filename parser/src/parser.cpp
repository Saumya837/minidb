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

    root->children.push_back(std::move(fromNode));

    for(auto& col : columnList) {
        root->children.push_back(std::move(col));
    }
    
    if (check(tokens, pos, TokenType::WHERE)){
        auto whereNode = parseWhereClause(tokens, pos);
        root->children.push_back(std::move(whereNode));
    }

    if (check(tokens, pos, TokenType::GROUP)){ 
        auto group_by = parseGroupByClause(tokens, pos);
        root->children.push_back(std::move(group_by));
    }
   
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

std::unique_ptr<ASTNode> parseWhereClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::WHERE);

    auto wh_node = std::make_unique<ASTNode>();
    wh_node->type = Clauses::WHERE;

    auto or_node = parseOrExpr(tokens, pos);
    wh_node->children.push_back(std::move(or_node));

    return wh_node;
}


std::unique_ptr<ASTNode>  parseOrExpr(const std::vector<Token>& tokens, size_t&pos){
    auto and_node1 = parseAndExpr(tokens, pos);

    if(check(tokens, pos, TokenType::OR)){
        expect(tokens, pos, TokenType::OR);
        auto or_node = std::make_unique<ASTNode>();
        or_node->type = ExpressionType::OR;
        auto and_node2 = parseAndExpr(tokens, pos);
        or_node->children.push_back(std::move(and_node1));
        or_node->children.push_back(std::move(and_node2));
        return or_node;
    }
    return and_node1;
}

std::unique_ptr<ASTNode> parseAndExpr(const std::vector<Token>& tokens, size_t& pos){
    auto comp1 = parseComparison(tokens, pos);

    if(check(tokens, pos, TokenType::AND)){
        expect(tokens, pos, TokenType::AND);
        auto and_node = std::make_unique<ASTNode>();
        and_node->type = ExpressionType::AND;
        auto comp2 = parseComparison(tokens, pos);
        and_node->children.push_back(std::move(comp1));
        and_node->children.push_back(std::move(comp2));
        return and_node;
    }

    return comp1;
}

std::unique_ptr<ASTNode> parseComparison(const std::vector<Token>& tokens, size_t& pos){
    auto operand1 = parseOperand(tokens, pos);
    auto comperator = std::make_unique<ASTNode>();
    if (check(tokens, pos, TokenType::EQUALS)) {
        expect(tokens, pos, TokenType::EQUALS);
        comperator->type = ExpressionType::EQUALS;
    }
    else if (check(tokens, pos, TokenType::GREATER)) {
        expect(tokens, pos, TokenType::GREATER);
        comperator->type = ExpressionType::GREATER;
    }
    else if (check(tokens, pos, TokenType::GREATER_EQUAL)) {
        expect(tokens, pos, TokenType::GREATER_EQUAL);
        comperator->type = ExpressionType::GREATER_EQUAL;
    }
    else if (check(tokens, pos, TokenType::SMALLER)) {
        expect(tokens, pos, TokenType::SMALLER);
        comperator->type = ExpressionType::SMALLER;
    }
    else if (check(tokens, pos, TokenType::LESSER_EQUAL)) {
        expect(tokens, pos, TokenType::LESSER_EQUAL);
        comperator->type = ExpressionType::LESSER_EQUAL;
    }
    else {
        throw std::runtime_error("Expected a comparator at position " + std::to_string(pos));
    }

    auto operand2 = parseOperand(tokens, pos);
    comperator->children.push_back(std::move(operand1));
    comperator->children.push_back(std::move(operand2));

    return comperator;
}

std::unique_ptr<ASTNode> parseOperand(const std::vector<Token>& tokens, size_t& pos){
    auto op = std::make_unique<ASTNode>();
    if(check(tokens, pos, TokenType::IDENTIFIER)){
        auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);
        op->type = ValueType::COLUMN;
        op->value = idToken.lexeme;
    }
    else if(check(tokens, pos, TokenType::STRING)){
        auto idToken = expect(tokens, pos, TokenType::STRING);
        op->type = ValueType::LITERAL;
        op->value = idToken.lexeme;
    }
    else{
        auto idToken = expect(tokens, pos, TokenType::NUMBER);
        op->type = ValueType::LITERAL;
        op->value = idToken.lexeme;
    }
    return op;
}

std::unique_ptr<ASTNode> parseGroupByClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::GROUP);
    expect(tokens, pos, TokenType::BY);

    auto groupby = std::make_unique<ASTNode>();
    groupby->type = Clauses::GROUP_BY;

    auto colList =  parseColumnList(tokens, pos);

    for (auto &col: colList){
        groupby->children.push_back(std::move(col));
    }
    return groupby;
}