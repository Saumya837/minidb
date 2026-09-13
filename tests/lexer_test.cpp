#include "ast.hpp"
#include "lexer.hpp"
#include <iostream>

int main() {
    // std::string sql = "SELECT name, salary FROM employees WHERE salary > 5000;";
    std::string sql = "SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';";
    std::vector<Token> tokens = tokenize(sql);
    for (const auto& t : tokens) {
        std::cout << static_cast<int>(t.type) << " : \"" << t.lexeme << "\"\n";
    }
    return 0;
}