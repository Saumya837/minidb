#include "case_loader.hpp"
#include "case_runner.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "tests/parser_test/tests.txt";

    std::vector<TestCase> cases;
    try {
        cases = loadCases(path);
    } catch (const std::exception& e) {
        std::cerr << "cannot load cases: " << e.what() << "\n";
        return 2;
    }

    int failed = 0;
    for (const TestCase& tc : cases) {
        CaseResult r = runCase(tc);
        if (r.passed) {
            std::cout << "PASS  " << tc.number << " " << tc.name << "\n";
        } else {
            ++failed;
            std::cout << "FAIL  " << tc.number << " " << tc.name
                      << "\n      " << r.reason << "\n";
        }
    }
    std::cout << "\n" << (cases.size() - failed) << "/" << cases.size() << " passed\n";
    return failed == 0 ? 0 : 1;
}