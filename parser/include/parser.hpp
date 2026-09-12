#pragma once

#include <vector>
#include <memory>
#include "ast.hpp"
#include "token.hpp"

std::unique_ptr<ASTNode> parseStatement(const std::vector<Token>& tokens, size_t& pos);
std::vector<std::unique_ptr<ASTNode>> parseColumnList(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseFromClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseWhereClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseOrExpr(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseAndExpr(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseComparison(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseOperand(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseGroupByClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseLimitClause(const std::vector<Token>& tokens, size_t& pos);

