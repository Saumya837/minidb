#include "ast.hpp"
#include "lexer.hpp"
#include <iostream>

int main() {
    // Query 1 - std::string sql = "SELECT name, salary FROM employees WHERE salary > 5000;";
    // Query 2 - std::string sql = "SELECT name, salary FROM employees WHERE age >= 30 AND department = 'IT';";
    //std::string sql = "SELECT department, salary FROM employees WHERE salary >= 5000 GROUP BY department, salary order by salary, department limit 10;";
    //std::string sql = "SELECT department, salary FROM employees LEFT JOIN departments ON employees.dept_id = departments.id;";
    std::string sql = "INSERT INTO EMPLOYEE (NAME, AGE, DEPARTMENT, DESIGNATION, SALARY) VALUES('SOMYA', 29, 'CSE', 'DB ENGINEER', 100000);";
    std::vector<Token> tokens = tokenize(sql);
    for (const auto& t : tokens) {
        std::cout << static_cast<int>(t.type) << " : \"" << t.lexeme << "\"\n";
    }
    return 0;
}