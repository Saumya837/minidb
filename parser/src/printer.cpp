#include "ast.hpp"
#include "printer.hpp"

#include <iostream>
#include <string>
#include <variant>
#include <type_traits>

std::string tagToString(const ASTTag& tag){
    return std::visit([](auto&& t)-> std::string { 
        using T = std::decay_t<decltype(t)>;

        if constexpr(std::is_same_v<T, StatementType>) {
            switch(t){
                case StatementType::SELECT: return "Select";  
                case StatementType::INSERT: return "Insert";
                case StatementType::UPDATE: return "UPDATE";
                case StatementType::DELETE: return "DELETE"; 
                case StatementType::CREATE: return "CREATE"; 
                case StatementType::ALTER:  return "ALTER";
                case StatementType::DROP:   return "DROP";  
            }
        }
        else if constexpr(std::is_same_v<T, Clauses>){
            switch(t){
                case Clauses::WHERE: return "WHERE";
                case Clauses::FROM: return "FROM";
                case Clauses::GROUP_BY: return "GROUP BY";
                case Clauses::ORDER_BY: return "ORDER BY";
                case Clauses::LIMIT: return "LIMIT";
            }
        }
        else if constexpr(std::is_same_v<T, Relations>){
            switch(t){
                case Relations::TABLE: return "TABLE";
                case Relations::JOIN: return "JOIN";
            }
        }
        else if constexpr(std::is_same_v<T, ExpressionType>){
            switch(t){
                case ExpressionType::EQUALS: return "EQUALS";
                case ExpressionType::GREATER: return "GREATER";
                case ExpressionType::SMALLER: return "SMALLER";
                case ExpressionType::GREATER_EQUAL: return "GREATER EQUAL";
                case ExpressionType::LESSER_EQUAL: return "SMALLER EQUAL";
                case ExpressionType::AND: return "AND";
                case ExpressionType::OR: return "OR";
            }
        }
        else if constexpr(std::is_same_v<T, ValueType>){
            switch(t){
                case ValueType::COLUMN: return "COLUMN";
                case ValueType::LITERAL: return "LITERAL";
                case ValueType::POSITION: return "POSITION";
            }
        }
        else if constexpr(std::is_same_v<T, OrderDirection>){
            switch(t){
                case OrderDirection::ASC: return "ASC";
                case OrderDirection::DESC: return "DESC";
            }
        }
        else if constexpr(std::is_same_v<T, InternalNode>){
            switch(t){
                // To Make it future Proof and modular kept it switch even though only One option of InternalNode is present
                case InternalNode::ORDER_ITEM: return "ORDER_ITEM";
            }
        }
    }, tag);
}

void printTree(const ASTNode& node, const std::string& prefix, bool isLast){
    std::string label = tagToString(node.type);

    if (!node.value.empty()) label += ": " + node.value;

    std::cout<< prefix << (isLast ? "└─> " : "├─> ") << label << "\n";

    std::string childPrefix = prefix + (isLast ? "    " : "│   ");

    for(size_t i = 0; i<node.children.size(); ++i){
        printTree(*node.children[i], childPrefix, i == node.children.size() -1);
    }
}

void printAST(const ASTNode& root) {
    std::string label = tagToString(root.type);
    if(!root.value.empty())
        label += ": " + root.value;
    std::cout<< label <<"\n";

    for (size_t i = 0; i<root.children.size(); ++i){
        printTree(*root.children[i], "", i == root.children.size() - 1);
    }
}