#include "parser.hpp"
#include "lexer.hpp"
#include "tree_dump.hpp"
#include <iostream>

int main() {
    auto tokens = tokenize("SELECT name, salary FROM employees WHERE salary > 5000;");
    size_t pos = 0;
    auto root = parseStatement(tokens, pos);
    for (const auto& line : dumpTree(*root))
        std::cout << "|" << line << "\n";

    return 0;
}