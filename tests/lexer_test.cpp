#include "ast.hpp"
#include "lexer.hpp"
#include <iostream>

int main() {
    // Query 1 - std::string sql = "SELECT name, salary FROM employees WHERE salary > 5000;";
    // Query 2 - std::string sql = "SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';";
    // Query 3 - 
    std::string sql = "SELECT name, salary FROM employees WHERE salary < 3000 OR salary >= 10000;";
    std::vector<Token> tokens = tokenize(sql);
    for (const auto& t : tokens) {
        std::cout << static_cast<int>(t.type) << " : \"" << t.lexeme << "\"\n";
    }
    return 0;
}