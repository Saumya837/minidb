#include "parser.hpp"
#include "lexer.hpp"
#include "printer.hpp"
#include <iostream>

int main(){
    std::string buffer;
    while (true) {
        std::cout << (buffer.empty() ? "SQL> " : "...> ");
        std::string line;
        std::getline(std::cin, line);

        if (buffer.empty() && (line == "exit" || line == "quit")) break;

        buffer += line + " ";   // space, not nothing — otherwise two words on
                                // adjacent lines (e.g. "...mang\nON...") would
                                // concatenate into one token

        if (line.find(';') == std::string::npos) {
            continue;            // no terminator yet, keep reading lines
        }

        try {
            std::vector<Token> tokens = tokenize(buffer);
            size_t position = 0;
            auto root = parseStatement(tokens, position);
            printAST(*root);
        } catch (const std::runtime_error& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
        buffer.clear();
    }
    return 0;
}