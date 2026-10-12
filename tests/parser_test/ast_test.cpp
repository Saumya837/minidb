#include <memory>
#include "ast.hpp"
#include "printer.hpp"

int main() {
    auto root = std::make_unique<ASTNode>();
    root->type = StatementType::SELECT;

    auto fromNode = std::make_unique<ASTNode>();
    fromNode->type = Clauses::FROM;
    auto tableNode = std::make_unique<ASTNode>();
    tableNode->type = Relations::TABLE;
    tableNode->value = "employees";
    fromNode->children.push_back(std::move(tableNode));

    auto col1 = std::make_unique<ASTNode>();
    col1->type = ValueType::COLUMN;
    col1->value = "name";

    auto col2 = std::make_unique<ASTNode>();
    col2->type = ValueType::COLUMN;
    col2->value = "salary";

    auto whereNode = std::make_unique<ASTNode>();
    whereNode->type = Clauses::WHERE;
    
    auto greaterNode = std::make_unique<ASTNode>();
    greaterNode->type = ExpressionType::GREATER;

    auto lhs = std::make_unique<ASTNode>();
    lhs->type = ValueType::COLUMN;
    lhs->value = "salary";

    auto rhs = std::make_unique<ASTNode>();
    rhs->type = ValueType::LITERAL;
    rhs->value = "5000";

    greaterNode->children.push_back(std::move(lhs));
    greaterNode->children.push_back(std::move(rhs));
    whereNode->children.push_back(std::move(greaterNode));

    root->children.push_back(std::move(fromNode));
    root->children.push_back(std::move(col1));
    root->children.push_back(std::move(col2));
    root->children.push_back(std::move(whereNode));

    printAST(*root);
    return 0;
}