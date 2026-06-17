#include <concepts>
#include <string>
#include <vector>
#include <variant>
#include <memory>

enum class StatementType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    CREATE,
    ALTER,
    DROP
};

enum class Clauses{
    FROM,
    WHERE,
    GROUP_BY,
    ORDER_BY,
    LIMIT
};

enum class Relations{
    TABLE,
    JOIN 
};

enum class ExpressionType {
    EQUALS,
    GREATER,
    SMALLER,
    GREATER_EQUAL,
    LESSER_EQUAL,
    AND, 
    OR
};

enum class ValueType{
    COLUMN,
    LITERAL
};

using ASTTag = std::variant<StatementType, Clauses, Relations, ExpressionType, ValueType>;


struct ASTNode {
    ASTTag type;
    std::string value;
    std::vector<std::unique_ptr<ASTNode>> children;
};