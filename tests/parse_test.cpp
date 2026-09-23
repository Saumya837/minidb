#include "parser.hpp"
#include "lexer.hpp"
#include "printer.hpp"
#include <iostream>

int main(){
    std::string sql;
    while (true){
        std::cout << "SQL> ";

        std::getline(std::cin, sql); 

        if (sql == "exit" || sql == "quit"){
            break;
        }

        try{
            std::vector<Token> tokens = tokenize(sql);
            size_t position = 0;
            auto root = parseStatement(tokens, position);

            printAST(*root);
        }
        catch (const std::runtime_error& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }
    return 0;
}