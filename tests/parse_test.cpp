#include "parser.hpp"
#include "lexer.hpp"
#include "printer.hpp"

int main(){
    // Query 1 - std::string sql = "SELECT name, salary FROM employees;";
    // Query 2 - std::string sql = "SELECT name, salary FROM employees WHERE salary > 5000;";
    // Query 3 - std::string sql = "SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';";
    // Query 4 - std::string sql = "SELECT name, salary FROM employees WHERE salary < 3000.50 OR salary >= 10000.40;";
    //std::string sql = "SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary ORDER BY salary, department limit 10;";
    //std::string sql = "SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary ORDER BY salary desc, department limit 10;";
    //std::string sql = "SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary ORDER BY salary asc, department limit 10;";
    std::string sql = "SELECT department as dept, salary as liab FROM employees emp LEFT JOIN departments dept ON emp.dept_id = dept.id WHERE salary < 3000.50 OR salary >= 10000.40 limit 10;";

    std::vector<Token> tokens = tokenize(sql);

    size_t position = 0;
    auto root = parseStatement(tokens, position);

    printAST(*root);

    return 0;
}