#pragma once

#include <vector>
#include <memory>
#include "ast.hpp"
#include "lexer.hpp"

std::unique_ptr<ASTNode> parseStatement(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseSelectStatement(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseInsertStatement(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseCreateStatement(const std::vector<Token>& tokens, size_t& pos);

std::vector<std::unique_ptr<ASTNode>> parseColumnList(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseRelation(const std::vector<Token>& tokens, size_t& pos);


std::unique_ptr<ASTNode> parseDistinctClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseFromClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseJoinClause(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseWhereClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseOrExpr(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseAndExpr(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseComparison(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseOperand(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseAlias(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseHavingClause(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseGroupByClause(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseOrderByClause(const std::vector<Token>& tokens, size_t& pos);
std::unique_ptr<ASTNode> parseOrderItem(const std::vector<Token>& tokens, size_t& pos);

std::unique_ptr<ASTNode> parseLimitClause(const std::vector<Token>& tokens, size_t& pos);




