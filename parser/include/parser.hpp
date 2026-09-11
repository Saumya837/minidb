#pragma once

#include <vector>
#include <memory>
#include "ast.hpp"
#include "token.hpp"

std::unique_ptr<ASTNode> parseFromClause(const std::vector<Token> &token, size_t &pos);
std::unique_ptr<ASTNode> parseColumnList(const std::vector<Token> &token, size_t &pos);
std::unique_ptr<ASTNode> parseComparison(const std::vector<Token> &token, size_t &pos);

