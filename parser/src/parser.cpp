#include "parser.hpp"
#include <iostream>

namespace {
    std::string tokenTypeToString(TokenType type) {
        switch (type) {
            case TokenType::SELECT:         return "SELECT";
            case TokenType::INSERT:         return "INSERT";
            case TokenType::UPDATE:         return "UPDATE";
            case TokenType::DELETE:         return "DELETE";
            case TokenType::CREATE:         return "CREATE";
            case TokenType::ALTER:          return "ALTER";
            case TokenType::DROP:           return "DROP";
            case TokenType::FROM:           return "FROM";
            case TokenType::SET:            return "SET";
            case TokenType::WHERE:          return "WHERE";
            case TokenType::GROUP:          return "GROUP";
            case TokenType::BY:             return "BY";
            case TokenType::ORDER:          return "ORDER";
            case TokenType::LIMIT:          return "LIMIT";
            case TokenType::AND:            return "AND";
            case TokenType::OR:             return "OR";
            case TokenType::TABLE:          return "TABLE";
            case TokenType::INDEX:          return "INDEX";
            case TokenType::JOIN:           return "JOIN";
            case TokenType::HAVING:         return "HAVING";
            case TokenType::AS:             return "AS";
            case TokenType::ON:             return "ON";
            case TokenType::LEFT:           return "LEFT";
            case TokenType::RIGHT:          return "RIGHT";
            case TokenType::INTO:           return "INTO";
            case TokenType::VALUES:         return "VALUES";
            case TokenType::DISTINCT:       return "DISTINCT";
            case TokenType::IDENTIFIER:     return "an identifier";
            case TokenType::NUMBER:         return "a number";
            case TokenType::STRING:         return "a string";
            case TokenType::EQUALS:         return "'='";
            case TokenType::GREATER:        return "'>'";
            case TokenType::SMALLER:        return "'<'";
            case TokenType::GREATER_EQUAL:  return "'>='";
            case TokenType::LESSER_EQUAL:   return "'<='";
            case TokenType::COMMA:          return "','";
            case TokenType::SEMICOLON:      return "';'";
            case TokenType::LPAREN:         return "'('";
            case TokenType::RPAREN:         return "')'";
            case TokenType::STAR:           return "'*'";
            case TokenType::ASC:            return "ASC";
            case TokenType::DESC:           return "DESC";
            case TokenType::IF:             return "IF";
            case TokenType::EXISTS:         return "EXISTS";
            case TokenType::NOT:            return "NOT";
            case TokenType::IS:             return "IS";
            case TokenType::FUNCTION:       return "FUNCTION";
            case TokenType::NULL_KW:        return "NULL";
            case TokenType::PRIMARY:        return "PRIMARY";
            case TokenType::KEY:            return "KEY";
            case TokenType::UNIQUE:         return "UNIQUE";
            case TokenType::DEFAULT:        return "DEFAULT";
            case TokenType::CONSTRAINT:     return "CONSTRAINT";
            case TokenType::FOREIGN:        return "FOREIGN";
            case TokenType::REFERENCES:     return "REFERENCES";
            case TokenType::CHECK:          return "CHECK";
            case TokenType::CASCADE:        return "CASCADE";
            case TokenType::RESTRICT:       return "RESTRICT";
            case TokenType::AUTO_INCREMENT:   return "AUTO_INCREMENT";
            case TokenType::INT:            return "INT";
            case TokenType::INTEGER:        return "INTEGER";
            case TokenType::BIGINT:         return "BIGINT";
            case TokenType::SMALLINT:       return "SMALLINT";
            case TokenType::FLOAT:          return "FLOAT";
            case TokenType::DOUBLE:         return "DOUBLE";
            case TokenType::DECIMAL:        return "DECIMAL";
            case TokenType::CHAR:           return "CHAR";
            case TokenType::VARCHAR:        return "VARCHAR";
            case TokenType::TEXT:           return "TEXT";
            case TokenType::BOOLEAN:        return "BOOLEAN";
            case TokenType::DATE:           return "DATE";
            case TokenType::TIMESTAMP:      return "TIMESTAMP";
            case TokenType::END_OF_INPUT:   return "end of input";
            case TokenType::UNKNOWN:        return "an unknown token";
            default:                        return "unknown token type";
        }
    }

    std::string syntaxErrorLocation(const std::vector<Token>& tokens, size_t pos) {
        if (pos >= tokens.size() || tokens[pos].type == TokenType::END_OF_INPUT) {
            return "unexpected end of input";
        }
        return "at or near \"" + tokens[pos].lexeme + "\"";
    }

    [[noreturn]] 
    void parseError(const std::vector<Token>& tokens, size_t idx, const std::string& detail) {
        throw std::runtime_error("Syntax error: " + syntaxErrorLocation(tokens, idx) + ", " + detail);
    }

    bool check(const std::vector<Token>& tokens, size_t pos, TokenType type) {
        if (pos >= tokens.size()) return false;
        return tokens[pos].type == type;
    }

    Token expect(const std::vector<Token> &tokens, size_t& pos, TokenType type){
        if (pos >= tokens.size())
            parseError(tokens, pos, "Unexpected End of input, expected token type " + std::to_string(static_cast<int>(type)));
        else if (tokens[pos].type == type){
            return tokens[pos++];
        }
        else{
            parseError(tokens, pos, "expected "+ tokenTypeToString(type));
        }
    }

    Token expectLiteral(const std::vector<Token> &tokens, size_t &pos){
        if(check(tokens, pos, TokenType::NUMBER)) return expect(tokens, pos, TokenType::NUMBER);
        if(check(tokens, pos, TokenType::STRING)) return expect(tokens, pos, TokenType::STRING); 
        parseError(tokens, pos, "expected a number or string");
    }
}

std::unique_ptr<ASTNode> parseStatement(const std::vector<Token>& tokens, size_t& pos){
    if(check(tokens, pos, TokenType::SELECT)){
        return parseSelectStatement(tokens, pos);
    } 
    else if(check(tokens, pos, TokenType::INSERT)){
        return parseInsertStatement(tokens, pos);;
    } 
    else if(check(tokens, pos, TokenType::DELETE)){
        return parseDeleteStatement(tokens, pos);
    }
    else if(check(tokens, pos, TokenType::DROP)){
        return parseDropStatement(tokens, pos); 
    }
     else if(check(tokens, pos, TokenType::CREATE)){
        return parseCreateStatement(tokens, pos); 
    }
    else if(check(tokens, pos, TokenType::UPDATE)){
        return parseUpdateStatement(tokens, pos);
    }
    else if(check(tokens, pos, TokenType::ALTER)){
        parseError(tokens, pos, "ALTER is not supported yet");
    }
    else {
        parseError(tokens, pos, "expected SELECT, INSERT, UPDATE, DELETE or DROP"); 
    }   
}

// One column: name, type, constraints. Does not touch the parentheses or commas.
std::unique_ptr<ASTNode> parseColumnDef(const std::vector<Token>& tokens, size_t& pos) {
    if (!check(tokens, pos, TokenType::IDENTIFIER)) {
        parseError(tokens, pos, "expected a column name");
    }
    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto col = std::make_unique<ASTNode>();
    col->type = ValueType::COLUMN;                 // adapt: new tag + label "COLUMN_DEF"
    col->value = idToken.lexeme;                  // adapt: same field parseRelation reads

    // type
    auto type = std::make_unique<ASTNode>();
    type->type = Clauses::TYPE;     // adapt: new tag + label "TYPE"

    if (check(tokens, pos, TokenType::INT)) {
        type->value = "INT";
        pos++;
    } else if (check(tokens, pos, TokenType::TEXT)) {
        type->value = "TEXT";
        pos++;
    } else if (check(tokens, pos, TokenType::VARCHAR)) {
        type->value = "VARCHAR";
        pos++;
        expect(tokens, pos, TokenType::LPAREN);
        if (!check(tokens, pos, TokenType::NUMBER)) {
            parseError(tokens, pos, "expected a length");
        }
        type->children.push_back(parseLiteral(tokens, pos));   // adapt: your literal builder
        expect(tokens, pos, TokenType::RPAREN);
    } else {
        parseError(tokens, pos, "expected a type (INT, TEXT or VARCHAR)");
    }
    col->children.push_back(std::move(type));

    // constraints
    while (true) {
        if (check(tokens, pos, TokenType::PRIMARY)) {
            expect(tokens, pos, TokenType::PRIMARY);
            expect(tokens, pos, TokenType::KEY);
            auto pk = std::make_unique<ASTNode>();
            pk->type = Constraints::PRIMARY_KEY;         // adapt: new tag + label "PRIMARY_KEY"
            col->children.push_back(std::move(pk));
        }
         else if (check(tokens, pos, TokenType::NOT)) {
            expect(tokens, pos, TokenType::NOT);
            expect(tokens, pos, TokenType::NULL_KW); // adapt: your NULL token name
            auto nn = std::make_unique<ASTNode>();
            nn->type = ExpressionType::NOT_NULL;            // adapt: new tag + label "NOT_NULL"
            col->children.push_back(std::move(nn));
        } 
        else if (check(tokens, pos, TokenType::AUTO_INCREMENT)) {
            expect(tokens, pos, TokenType::AUTO_INCREMENT);   // adapt: your NULL token name
            auto inc = std::make_unique<ASTNode>();
            inc->type = Constraints::AUTO_INCREMENT;    // adapt: new tag + label "NOT_NULL"
            col->children.push_back(std::move(inc));
        } 
        else {
            break;
        }
    }
    return col;
}

std::unique_ptr<ASTNode> parseCreateTable(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::TABLE);
    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::CREATE;

    if (check(tokens, pos, TokenType::IF)) {
        expect(tokens, pos, TokenType::IF);
        expect(tokens, pos, TokenType::NOT);
        expect(tokens, pos, TokenType::EXISTS);
        auto ine = std::make_unique<ASTNode>();
        ine->type = Constraints::IF_NOT_EXISTS;
        root->children.push_back(std::move(ine));
    }

    auto relation =  parseRelation(tokens, pos);
    root->children.push_back(std::move(relation));

    expect(tokens, pos, TokenType::LPAREN);

    root->children.push_back(parseColumnDef(tokens, pos));
    while (check(tokens, pos, TokenType::COMMA)) {
        expect(tokens, pos, TokenType::COMMA);
        root->children.push_back(parseColumnDef(tokens, pos));
    }
    expect(tokens, pos, TokenType::RPAREN);
    return root;
}

std::unique_ptr<ASTNode> parseCreateStatement(const std::vector<Token> &tokens, size_t& pos){
    expect(tokens, pos, TokenType::CREATE);
    std::unique_ptr<ASTNode> root = nullptr;
    
    if (check(tokens, pos, TokenType::TABLE)) {
        root = parseCreateTable(tokens, pos);
    } else {
        parseError(tokens, pos, "expected TABLE, MATERIALIZED VIEW or FUNCTION");
    }
    //parsing value with its type
    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseAssignment(const std::vector<Token>& tokens, size_t& pos){
    auto operand1 = std::make_unique<ASTNode>();
    if(tokens[pos].lexeme.find('.') != std::string::npos){
        if(check(tokens, pos, TokenType::IDENTIFIER)){
            operand1 = parseQualifier(tokens, pos, false);
        }
    }
    else{
        auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);
        operand1->type = ValueType::COLUMN;
        operand1->value = idToken.lexeme;
    }

    expect(tokens, pos, TokenType::EQUALS);
    auto op1 = std::make_unique<ASTNode>();
    op1->type = ExpressionType::EQUALS;
    op1->children.push_back(std::move(operand1));

    if(tokens[pos].lexeme.find('.') != std::string::npos){
        if(check(tokens, pos, TokenType::IDENTIFIER)){
            auto qualifier = parseQualifier(tokens, pos, false);
            op1->children.push_back(std::move(qualifier));
        }
        else{
            auto operand2 = parseLiteral(tokens, pos);
            op1->children.push_back(std::move(operand2));
        }
    }
    else if(check(tokens, pos, TokenType::IDENTIFIER) && check(tokens, pos+1, TokenType::LPAREN)){
        auto function = parseFunctionCall(tokens, pos);
        op1->children.push_back(std::move(function));
    }
    else if(check(tokens, pos, TokenType::IDENTIFIER)){
        auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);

        auto operand1 = std::make_unique<ASTNode>();
        operand1->type = ValueType::COLUMN;
        operand1->value = idToken.lexeme;
        op1->children.push_back(std::move(operand1));
    }
    else{
        auto operand2 = parseLiteral(tokens, pos);
        op1->children.push_back(std::move(operand2));
    }
    return op1;
}

std::vector<std::unique_ptr<ASTNode>> parseAssignmentList(const std::vector<Token>& tokens, size_t& pos){
    std::vector<std::unique_ptr<ASTNode>> assignmentList;

    auto assign = parseAssignment(tokens, pos);
    assignmentList.push_back(std::move(assign));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto assign = parseAssignment(tokens, pos);
        assignmentList.push_back(std::move(assign));
    }
    return assignmentList;
}

std::unique_ptr<ASTNode> parseUpdateStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::UPDATE);

    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::UPDATE;

    auto relation = parseRelationWithAlias(tokens, pos);
    root->children.push_back(std::move(relation));

    expect(tokens, pos, TokenType::SET);
    auto set = std::make_unique<ASTNode>();
    set->type = Clauses::SET;

    auto assignList = parseAssignmentList(tokens, pos);
    for(auto& assign : assignList) {
        set->children.push_back(std::move(assign));
    }

    root->children.push_back(std::move(set));

    if(check(tokens, pos, TokenType::WHERE)){
        auto where = parseWhereClause(tokens, pos);
        root->children.push_back(std::move(where));
    }

    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseDeleteStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::DELETE);
    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::DELETE;

    expect(tokens, pos, TokenType::FROM);
    auto from = std::make_unique<ASTNode>();
    from->type = Clauses::FROM;                       // same tag parseFromClause uses

    auto relation = parseRelationWithAlias(tokens, pos);
    from->children.push_back(std::move(relation));
    root->children.push_back(std::move(from));

    if(check(tokens, pos, TokenType::WHERE)){
        auto whereNode = parseWhereClause(tokens, pos);
        root->children.push_back(std::move(whereNode));
    }

    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseIndex(const std::vector<Token> &tokens, size_t &pos){
    auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);

    if(idToken.lexeme.find('.') != std::string::npos){
        parseError(tokens, pos - 1, "index name cannot contain '.'");
    }

    auto index = std::make_unique<ASTNode>();
    index->type = ValueType::INDEX;
    index->value = idToken.lexeme;
    return index;
}

std::unique_ptr<ASTNode> parseIndexStatement(const std::vector<Token> &tokens, size_t &pos){
    auto indexList = std::make_unique<ASTNode>();
    indexList->type = InternalNode::INDEX_LIST;

    auto index = parseIndex(tokens, pos);
    indexList->children.push_back(std::move(index));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto next_index = parseIndex(tokens, pos);
        indexList->children.push_back(std::move(next_index));
    }
    return indexList;
}

std::unique_ptr<ASTNode> parseDropStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::DROP);
    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::DROP;

    if(check(tokens, pos, TokenType::INDEX)){
        expect(tokens, pos, TokenType::INDEX);

        if(check(tokens, pos, TokenType::IF) && check(tokens, pos + 1, TokenType::EXISTS)){
            expect(tokens, pos, TokenType::IF);
            expect(tokens, pos, TokenType::EXISTS);
            auto if_exists_node = std::make_unique<ASTNode>();
            if_exists_node->type = Constraints::IF_EXISTS;
            root->children.push_back(std::move(if_exists_node));
        }

        auto index = parseIndexStatement(tokens, pos);
        root->children.push_back(std::move(index));
        expect(tokens, pos, TokenType::ON);

        auto relation = parseRelation(tokens, pos);
        root->children.push_back(std::move(relation));
    } 

    else if(check(tokens, pos, TokenType::TABLE)){
        expect(tokens, pos, TokenType::TABLE);

        if(check(tokens, pos, TokenType::IF) && check(tokens, pos+1, TokenType::EXISTS)){
            expect(tokens, pos, TokenType::IF);
            expect(tokens, pos, TokenType::EXISTS);
            auto if_exists_node = std::make_unique<ASTNode>();
            if_exists_node->type = Constraints::IF_EXISTS;
            root->children.push_back(std::move(if_exists_node));
        }

        auto relation = parseRelation(tokens, pos);
        root->children.push_back(std::move(relation));
    }
    else {
        parseError(tokens, pos, "expected TABLE or INDEX");  
    }
    // if(check(tokens, pos, TokenType::FUNCTION)){
    //     expect(tokens, pos, TokenType::FUNCTION);
    //     auto function = parseFunctionDef(tokens, pos, false);
    //     root->children.push_back(std::move(function));
    // }

    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseInsertStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::INSERT);
    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::INSERT;

    expect(tokens, pos, TokenType::INTO);
    
    auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto relation = std::make_unique<ASTNode>();
    relation->type = Relations::TABLE;
    relation->value = idToken.lexeme;
    root->children.push_back(std::move(relation));

    if (check(tokens, pos, TokenType::LPAREN)){
        expect(tokens, pos, TokenType::LPAREN);
        auto columnList = parseColumnList(tokens, pos);
        for(auto& col : columnList) {
            root->children.push_back(std::move(col));
        }
        expect(tokens, pos, TokenType::RPAREN);
    }

    expect(tokens, pos, TokenType::VALUES);

    auto valueTuple = parseValueTuple(tokens, pos);
    root->children.push_back(std::move(valueTuple));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto valueTuple = parseValueTuple(tokens, pos);
        root->children.push_back(std::move(valueTuple));
    }

    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseValueTuple(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::LPAREN);

    auto value_list = std::make_unique<ASTNode>();
    value_list->type = InternalNode::VALUE_TUPLE;
    auto value_item = parseLiteral(tokens, pos);
    value_list->children.push_back(std::move(value_item));
    
    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto next_value_item = parseLiteral(tokens, pos);
        value_list->children.push_back(std::move(next_value_item));
    }
    expect(tokens, pos, TokenType::RPAREN);
    return value_list;
}

std::unique_ptr<ASTNode> parseSelectStatement(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::SELECT);

    auto root = std::make_unique<ASTNode>();
    root -> type = StatementType::SELECT;

    if(check(tokens, pos, TokenType::DISTINCT)){
        auto distinctNode = parseDistinctClause(tokens, pos);
        root->children.push_back(std::move(distinctNode));
    }

    std::vector<std::unique_ptr<ASTNode>> projList = parseProjectionList(tokens, pos);
    if(check(tokens, pos, TokenType::FROM)){
        auto fromNode = parseFromClause(tokens, pos);
        root->children.push_back(std::move(fromNode));
    }
   
    for(auto& projection : projList) {
        root->children.push_back(std::move(projection));
    }

    if (check(tokens, pos, TokenType::WHERE)){
        auto whereNode = parseWhereClause(tokens, pos);
        root->children.push_back(std::move(whereNode));
    }

    bool hasGroupBy = false;
    if (check(tokens, pos, TokenType::GROUP)){ 
        auto group_by = parseGroupByClause(tokens, pos);
        root->children.push_back(std::move(group_by));
        hasGroupBy = true;
    }

    if(check(tokens, pos, TokenType::HAVING)){
        if(hasGroupBy){
            auto havingNode = parseHavingClause(tokens, pos);
            root->children.push_back(std::move(havingNode));
        }
        else{
            parseError(tokens, pos, "HAVING requires GROUP BY");   
        }
    }

    if(check(tokens, pos, TokenType::ORDER)){
        auto order_by = parseOrderByClause(tokens, pos);
        root->children.push_back(std::move(order_by));
    }

    if (check(tokens, pos, TokenType::LIMIT)){
        auto limit_clause = parseLimitClause(tokens, pos);
        root->children.push_back(std::move(limit_clause));
    }
   
    expect(tokens, pos, TokenType::SEMICOLON);
    return root;
}

std::unique_ptr<ASTNode> parseDistinctClause(const std::vector<Token> &tokens, size_t& pos){
    expect(tokens,pos, TokenType::DISTINCT);

    auto distinctNode = std::make_unique<ASTNode>();
    distinctNode->type = Clauses::DISTINCT;

    return distinctNode;
}

std::unique_ptr<ASTNode> parseStar(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::STAR);
    auto starNode = std::make_unique<ASTNode>();
    starNode->type = ValueType::STAR;
    return starNode;
}

std::unique_ptr<ASTNode> parseLiteral(const std::vector<Token>& tokens, size_t& pos){
    auto literalToken = expectLiteral(tokens, pos);
    auto literalNode = std::make_unique<ASTNode>();
    literalNode->type = ValueType::LITERAL;
    literalNode->value = literalToken.lexeme;
    return literalNode;
}

std::unique_ptr<ASTNode> parseQualifier(const std::vector<Token> &tokens, size_t& pos, bool allow_star ){
    const std::string& current_token = tokens[pos].lexeme;
    size_t idx = pos;
    pos++;
    size_t len = current_token.find('.');

    if (current_token.find('.', len + 1) != std::string::npos ||
        len == 0 || len == current_token.size() - 1)
        parseError(tokens, idx, "malformed qualified name");                   
    
    auto qualifier = std::make_unique<ASTNode>();
    qualifier->type = InternalNode::QUALIFIER;
    qualifier->value = current_token.substr(0, len);

    if (current_token.substr(len + 1) == "*") {
        if(allow_star){
            auto starNode = std::make_unique<ASTNode>();
            starNode->type = ValueType::STAR;
            starNode->children.push_back(std::move(qualifier));
            return starNode;
        }
        else{
            parseError(tokens, idx, "star is only allowed in the select list"); 
        }
    }
    else {
        auto columnNode = std::make_unique<ASTNode>();
        columnNode->type = ValueType::COLUMN;
        columnNode->value = current_token.substr(len + 1);
        columnNode->children.push_back(std::move(qualifier));
        return columnNode;
    }
}

std::vector<std::unique_ptr<ASTNode>> parseProjectionList(const std::vector<Token>& tokens, size_t& pos){
    std::vector<std::unique_ptr<ASTNode>> projList;

    if(check(tokens, pos, TokenType::STAR)){
        auto starNode = parseStar(tokens, pos);
        projList.push_back(std::move(starNode));
    }
    else if(check(tokens, pos, TokenType::IDENTIFIER) && check(tokens, pos+1, TokenType::LPAREN)){
        auto function = parseFunctionCall(tokens, pos);
        if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
            auto alias = parseAlias(tokens, pos); // Check for alias after column
            function->children.push_back(std::move(alias));
        }
        projList.push_back(std::move(function));
    }
    else if(tokens[pos].lexeme.find('.') != std::string::npos){
        if(check(tokens, pos, TokenType::IDENTIFIER)) {
            auto qualifierStarColumn = parseQualifier(tokens, pos);
            if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
                auto alias = parseAlias(tokens, pos); // Check for alias after column
                qualifierStarColumn->children.push_back(std::move(alias));
            }
            projList.push_back(std::move(qualifierStarColumn));
        }
        else{
            auto literalNode = parseLiteral(tokens, pos);
            if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
                auto alias = parseAlias(tokens, pos); // Check for alias after column
                literalNode->children.push_back(std::move(alias));
            }
            projList.push_back(std::move(literalNode));
        }
    }
    else if(check(tokens, pos, TokenType::IDENTIFIER)){
        auto column = parseColumn(tokens, pos);
        if(check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER)){
            auto alias = parseAlias(tokens, pos);
            column->children.push_back(std::move(alias));
        }
        projList.push_back(std::move(column));
    }
    else if (check(tokens, pos, TokenType::NUMBER) || check(tokens, pos, TokenType::STRING)){
        auto literalNode = parseLiteral(tokens, pos);
        if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
            auto alias = parseAlias(tokens, pos); // Check for alias after column
            literalNode->children.push_back(std::move(alias));
        }
        projList.push_back(std::move(literalNode));
    }
    else{
        parseError(tokens, pos, "expected a value (column, literal or function call) ");
    }

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);

        if(check(tokens, pos, TokenType::STAR)){
            auto starNode = parseStar(tokens, pos);
            projList.push_back(std::move(starNode));
        }
        else if(check(tokens, pos+1, TokenType::LPAREN)){
            auto function = parseFunctionCall(tokens, pos);
            if(check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER)){
                auto alias = parseAlias(tokens, pos);
                function->children.push_back(std::move(alias));
            }
            projList.push_back(std::move(function));
        }
        else if(tokens[pos].lexeme.find('.') != std::string::npos){
            if(check(tokens, pos, TokenType::IDENTIFIER)) {
                auto qualifierStarColumn = parseQualifier(tokens, pos);
                if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
                    auto alias = parseAlias(tokens, pos); // Check for alias after column
                    qualifierStarColumn->children.push_back(std::move(alias));
                }
                projList.push_back(std::move(qualifierStarColumn));
            }
        } 
        else if(check(tokens, pos, TokenType::IDENTIFIER)){
            auto column = parseColumn(tokens, pos);
            if(check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER)){
                auto alias = parseAlias(tokens, pos);
                column->children.push_back(std::move(alias));
            }
            projList.push_back(std::move(column));
        }
        else if(check(tokens, pos, TokenType::NUMBER) || check(tokens, pos, TokenType::STRING)){
            auto literalNode = parseLiteral(tokens, pos);
            if((check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER))){
                auto alias = parseAlias(tokens, pos); // Check for alias after column
                literalNode->children.push_back(std::move(alias));
            }
            projList.push_back(std::move(literalNode));
        }
        else{
            parseError(tokens, pos, "expected a value (column, literal or function call) after ',' ");
        }
    }
    return projList;
}

std::unique_ptr<ASTNode> parseFunctionCall(const std::vector<Token> &tokens, size_t& pos){
    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto function = std::make_unique<ASTNode>();
    function->type = ValueType::FUNCTION;
    function->value = idToken.lexeme;

    expect(tokens, pos, TokenType::LPAREN);
    if(check(tokens, pos, TokenType::STAR)){
        expect(tokens, pos, TokenType::STAR);
    }
    else{
        auto arg_list = std::make_unique<ASTNode>();
        arg_list->type = InternalNode::ARG_LIST;
        auto args = parseArgsList(tokens, pos);
        for (auto &arg :args){
            arg_list->children.push_back(std::move(arg));
        }
        function->children.push_back(std::move(arg_list));   
    }
    expect(tokens, pos, TokenType::RPAREN);
    return function;
}

std::vector<std::unique_ptr<ASTNode>> parseArgsList(const std::vector<Token>& tokens, size_t& pos){
    std::vector<std::unique_ptr<ASTNode>> argsList;
    auto first_arg = parseArg(tokens, pos);
    argsList.push_back(std::move(first_arg));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto next_arg = parseArg(tokens, pos);
        argsList.push_back(std::move(next_arg));
    }
    return argsList;
}

std::unique_ptr<ASTNode> parseArg(const std::vector<Token>& tokens, size_t& pos){
    auto arg = std::make_unique<ASTNode>();

    if(check(tokens, pos, TokenType::IDENTIFIER) && check(tokens, pos+1, TokenType::LPAREN)){
        arg = parseFunctionCall(tokens, pos);
    }

    else if(check(tokens, pos, TokenType::IDENTIFIER)){
        if(tokens[pos].lexeme.find('.') != std::string::npos){
            auto arg = parseQualifier(tokens, pos, false);
            return arg;
        } 
        Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
        arg->type = ValueType::COLUMN;
        arg->value = idToken.lexeme;
    }
    else {
        Token idToken = expectLiteral(tokens, pos);
        arg->type = ValueType::LITERAL;
        arg->value = idToken.lexeme;
    }
    return arg;
}

std::unique_ptr<ASTNode> parseColumn(const std::vector<Token> &tokens, size_t& pos){
    if((tokens[pos].lexeme.find('.') != std::string::npos) && check(tokens, pos, TokenType::IDENTIFIER)){
        auto col = parseQualifier(tokens, pos, false);
        return col;
    } 

    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    auto column = std::make_unique<ASTNode>();
    column->type = ValueType::COLUMN;
    column->value = idToken.lexeme;

    return column;
}

std::vector<std::unique_ptr<ASTNode>> parseColumnList(const std::vector<Token>& tokens, size_t& pos){
    std::vector<std::unique_ptr<ASTNode>> columnList;

    auto first_column = parseColumn(tokens, pos);
    columnList.push_back(std::move(first_column));

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto next_col = parseColumn(tokens, pos);
        columnList.push_back(std::move(next_col));
    }
    return columnList;
}

std::unique_ptr<ASTNode> parseRelationWithAlias(const std::vector<Token>& tokens, size_t& pos) {
    auto rel = parseRelation(tokens, pos);
    if (check(tokens, pos, TokenType::AS) || check(tokens, pos, TokenType::IDENTIFIER)) {
        rel->children.push_back(parseAlias(tokens, pos));
    }
    return rel;
}

static std::unique_ptr<ASTNode> parseSchema(const std::vector<Token>& tokens, size_t idx, std::string& name) {
    size_t dot = name.find('.');
    if (dot == std::string::npos) {
        return nullptr;
    }

    // Allowed shape: exactly one '.', with text on both sides.
    // dot == 0 cannot happen today (the lexer rejects a leading '.'), but it is
    // kept so a future DOT token cannot reintroduce the case.
    if (dot == 0 ||
        dot == name.size() - 1 ||
        name.find('.', dot + 1) != std::string::npos) {
        parseError(tokens, idx, "invalid relation name");
    }

    auto schema = std::make_unique<ASTNode>();
    schema->type  = InternalNode::SCHEMA;
    schema->value = name.substr(0, dot);
    name          = name.substr(dot + 1);
    return schema;
}

std::unique_ptr<ASTNode> parseRelation(const std::vector<Token>& tokens, size_t& pos) {
    if (!check(tokens, pos, TokenType::IDENTIFIER)) {
        parseError(tokens, pos, "expected a table name");
    }

    size_t idx = pos;                                   // offending token, taken before consuming
    Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
    std::string name = idToken.lexeme;

    if (name.find('*') != std::string::npos) {
        parseError(tokens, idx, "relation name cannot contain '*'");
    }

    auto schema = parseSchema(tokens, idx, name);       // may shorten `name`
    auto relation = std::make_unique<ASTNode>();
    relation->type  = Relations::TABLE;
    relation->value = name;
    if (schema) {
        relation->children.push_back(std::move(schema));
    }
    return relation;
}

std::unique_ptr<ASTNode> parseFromClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::FROM);

    auto from_node = std::make_unique<ASTNode>();
    from_node -> type = Clauses::FROM;

    auto baseTable = parseRelationWithAlias(tokens, pos);

    from_node->children.push_back(std::move(baseTable));

    while (check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto nextRel = parseRelationWithAlias(tokens, pos);
        from_node->children.push_back(std::move(nextRel));
    }
    while(check(tokens, pos, TokenType::LEFT) || check(tokens, pos, TokenType::RIGHT) || check(tokens, pos, TokenType::JOIN)){
        auto join = parseJoinClause(tokens, pos);
        from_node->children.push_back(std::move(join));
    }
    return from_node;
}

std::unique_ptr<ASTNode> parseWhereClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::WHERE);

    auto wh_node = std::make_unique<ASTNode>();
    wh_node->type = Clauses::WHERE;

    auto or_node = parseOrExpr(tokens, pos);
    wh_node->children.push_back(std::move(or_node));

    return wh_node;
}

std::unique_ptr<ASTNode> parsePrimary(const std::vector<Token>& tokens, size_t& pos) {
    if (check(tokens, pos, TokenType::LPAREN)) {
        expect(tokens, pos, TokenType::LPAREN);
        auto node = parseOrExpr(tokens, pos);   // back to the top of the precedence chain
        expect(tokens, pos, TokenType::RPAREN);
        return node;
    }
    return parseComparison(tokens, pos);
}

std::unique_ptr<ASTNode> parseOrExpr(const std::vector<Token>& tokens, size_t& pos){
    auto left = parseAndExpr(tokens, pos);

    while(check(tokens, pos, TokenType::OR)){
        expect(tokens, pos, TokenType::OR);
        auto right = parseAndExpr(tokens, pos);

        auto or_node = std::make_unique<ASTNode>();
        or_node->type = ExpressionType::OR;
        or_node->children.push_back(std::move(left));
        or_node->children.push_back(std::move(right));
        left = std::move(or_node);      // the tree so far becomes the new left side
    }
    return left;
}

std::unique_ptr<ASTNode> parseAndExpr(const std::vector<Token>& tokens, size_t& pos){
    auto left = parsePrimary(tokens, pos);

    while(check(tokens, pos, TokenType::AND)){
        expect(tokens, pos, TokenType::AND);
        auto right = parsePrimary(tokens, pos);

        auto and_node = std::make_unique<ASTNode>();
        and_node->type = ExpressionType::AND;
        and_node->children.push_back(std::move(left));
        and_node->children.push_back(std::move(right));
        left = std::move(and_node);
    }
    return left;
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
        parseError(tokens, pos, "<comparator expected>");
    }

    auto operand2 = parseOperand(tokens, pos);
    comperator->children.push_back(std::move(operand1));
    comperator->children.push_back(std::move(operand2));

    return comperator;
}

std::unique_ptr<ASTNode> parseOperand(const std::vector<Token>& tokens, size_t& pos){
    auto op = std::make_unique<ASTNode>();
    if(check(tokens, pos, TokenType::IDENTIFIER) && check(tokens, pos+1, TokenType::LPAREN)){
        op = parseFunctionCall(tokens, pos);
    }
    else if(check(tokens, pos, TokenType::IDENTIFIER)){
        if(tokens[pos].lexeme.find('.') != std::string::npos){
            auto operand = parseQualifier(tokens, pos, false);
            return operand;
        } 
        auto idToken = expect(tokens, pos, TokenType::IDENTIFIER);
        op->type = ValueType::COLUMN;
        op->value = idToken.lexeme;
    }

    else if(check(tokens, pos, TokenType::STRING) || check(tokens, pos, TokenType::NUMBER)){
        auto idToken = expectLiteral(tokens, pos);
        op->type = ValueType::LITERAL;
        op->value = idToken.lexeme;
    }
    else {
        parseError(tokens, pos, "expected a value (column, literal or function call)");
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

std::unique_ptr<ASTNode> parseLimitClause(const std::vector<Token>& tokens, size_t& pos){
    expect(tokens, pos, TokenType::LIMIT);

    auto limitNode = std::make_unique<ASTNode>();
    limitNode->type = Clauses::LIMIT;

    auto idToken = expect(tokens, pos, TokenType::NUMBER);
    auto numberVal = std::make_unique<ASTNode>();
    numberVal->type = ValueType::LITERAL;
    numberVal->value = idToken.lexeme;

    limitNode->children.push_back(std::move(numberVal));

    return limitNode;
}

std::unique_ptr<ASTNode> parseJoinClause(const std::vector<Token> &tokens, size_t& pos){
    auto join = std::make_unique<ASTNode>();
    if(check(tokens, pos, TokenType::LEFT)){
        expect(tokens, pos, TokenType::LEFT);
        expect(tokens, pos, TokenType::JOIN);
        join->type = Relations::LEFT_JOIN;
        auto rel = parseRelationWithAlias(tokens, pos);
        join->children.push_back(std::move(rel));
    }
    else if(check(tokens, pos, TokenType::RIGHT)){
        expect(tokens, pos, TokenType::RIGHT);
        expect(tokens, pos, TokenType::JOIN);
        join->type = Relations::RIGHT_JOIN;
        auto rel = parseRelationWithAlias(tokens, pos);
        join->children.push_back(std::move(rel));
    }

    else{
        expect(tokens, pos, TokenType::JOIN);
        join->type = Relations::JOIN;
        auto rel = parseRelationWithAlias(tokens, pos);
        join->children.push_back(std::move(rel));
    }

    expect(tokens, pos, TokenType::ON);
    auto cmp = parseOrExpr(tokens, pos);
    join->children.push_back(std::move(cmp));
    return join;
}

std::unique_ptr<ASTNode> parseAlias(const std::vector<Token> &tokens, size_t& pos){
    auto alias = std::make_unique<ASTNode>();
    if(check(tokens, pos, TokenType::AS)){
        expect(tokens, pos, TokenType::AS);
    }

    if (tokens[pos].lexeme.find('.') != std::string::npos) {
        parseError(tokens, pos, "alias cannot contain '.'"); 
    }

    auto idToken =  expect(tokens, pos, TokenType::IDENTIFIER);
    alias->type = ValueType::ALIAS;
    alias->value = idToken.lexeme;

    return alias;
}

std::unique_ptr<ASTNode> parseHavingClause(const std::vector<Token> &tokens, size_t& pos){
    expect(tokens,pos, TokenType::HAVING);

    auto havingNode = std::make_unique<ASTNode>();
    havingNode->type = Clauses::HAVING;

    auto comparsion = parseOrExpr(tokens, pos);

    havingNode->children.push_back(std::move(comparsion));
    return havingNode;
}

std::unique_ptr<ASTNode> parseOrderItem(const std::vector<Token> &tokens, size_t& pos){
    auto order_item = std::make_unique<ASTNode>();
    order_item->type = InternalNode::ORDER_ITEM;
    if(check(tokens, pos, TokenType::IDENTIFIER)){
        if(tokens[pos].lexeme.find('.') != std::string::npos){
            auto item = parseQualifier(tokens, pos, false);
            order_item->children.push_back(std::move(item));
        } 
        else{
            Token idToken = expect(tokens, pos, TokenType::IDENTIFIER);
            auto col = std::make_unique<ASTNode>();
            col->type = ValueType::COLUMN;
            col->value = idToken.lexeme;
            order_item->children.push_back(std::move(col));
        }
    }
    else if (check(tokens, pos, TokenType::NUMBER)){
        Token posToken = expect(tokens, pos, TokenType::NUMBER);
        auto posNode = std::make_unique<ASTNode>();
        posNode->type = ValueType::POSITION;
        posNode->value = posToken.lexeme;
        order_item->children.push_back(std::move(posNode));
    }
    else{
        return nullptr;
    }

    auto sortDir = std::make_unique<ASTNode>();
    if (check(tokens, pos, TokenType::DESC)){
        expect(tokens, pos, TokenType::DESC);
        sortDir->type = OrderDirection::DESC;
        order_item->children.push_back(std::move(sortDir));
    }
    else{
        if (check(tokens, pos, TokenType::ASC)){
            expect(tokens, pos, TokenType::ASC);
        }
        sortDir->type = OrderDirection::ASC;
        order_item->children.push_back(std::move(sortDir));
    }
    return order_item;
}

std::unique_ptr<ASTNode> parseOrderByClause(const std::vector<Token> &tokens, size_t& pos){
    expect(tokens, pos, TokenType::ORDER);
    expect(tokens, pos, TokenType::BY);

    auto orderby = std::make_unique<ASTNode>();
    orderby->type = Clauses::ORDER_BY;

    auto order_item = parseOrderItem(tokens, pos);
    if(order_item){
        orderby->children.push_back(std::move(order_item));
    }
    else{
        parseError(tokens, pos, "expected column or position for ORDER BY");
    }

    while(check(tokens, pos, TokenType::COMMA)){
        expect(tokens, pos, TokenType::COMMA);
        auto next_order_item = parseOrderItem(tokens, pos);
        if(next_order_item){
            orderby->children.push_back(std::move(next_order_item));
        }
        else{
            parseError(tokens, pos, "expected column or position after ','");
        }
    }
    return orderby;
}





