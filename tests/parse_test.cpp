#include "parser.hpp"
#include "lexer.hpp"
#include "printer.hpp"

int main(){
    // Query 1 - std::string sql = "SELECT name, salary FROM employees;";
    // Query 2 - std::string sql = "SELECT name, salary FROM employees WHERE salary > 5000;";
    // Query 3 - std::string sql = "SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';";
    // Query 4 - std::string sql = "SELECT name, salary FROM employees WHERE salary < 3000 OR salary >= 10000;";
    std::string sql = "SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary order by salary desc, department desc limit 10;";
    std::vector<Token> tokens = tokenize(sql);

    size_t position = 0;
    auto root = parseStatement(tokens, position);

    printAST(*root);

    return 0;
}