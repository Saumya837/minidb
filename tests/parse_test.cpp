#include "parser.hpp"
#include "lexer.hpp"
#include "printer.hpp"

int main(){
    std::string sql = "SELECT name, salary FROM employees;";

    std::vector<Token> tokens = tokenize(sql);

    size_t position = 0;
    auto root = parseStatement(tokens, position);

    printAST(*root);

    return 0;
}