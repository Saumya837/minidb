#pragma once
#include <concepts>
#include <string>
#include <vector>
#include <variant>
#include <memory>

enum class StatementType {
    SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP
};

enum class Clauses{
    DISTINCT, FROM, WHERE, GROUP_BY, ORDER_BY, HAVING, LIMIT, SET, TYPE
};

enum class Relations{
    TABLE, JOIN, LEFT_JOIN, RIGHT_JOIN
};

enum class ExpressionType {
    EQUALS, GREATER, SMALLER, GREATER_EQUAL, LESSER_EQUAL, AND, OR, NOT_NULL, _NULL
};

enum class InternalNode {
   VALUE_TUPLE, ORDER_ITEM, ARG_LIST, QUALIFIER, INDEX_LIST, SCHEMA
};

enum class ValueType {
    COLUMN, LITERAL, POSITION, ALIAS, STAR, FUNCTION, INDEX
};

enum class OrderDirection {
    ASC, DESC
};

enum class Constraints{
    PRIMARY_KEY, FOREIGN_KEY, UNIQUE, IF_EXISTS, IF_NOT_EXISTS
};

using ASTTag = std::variant<StatementType, Clauses, Relations, ExpressionType, ValueType, OrderDirection, InternalNode, Constraints >;
struct ASTNode {
    ASTTag type;
    std::string value;
    std::vector<std::unique_ptr<ASTNode>> children;
};
