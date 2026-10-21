#include "case_loader.hpp"
#include "case_runner.hpp"
#include "failure_log.hpp"
#include "statistics.hpp"

#include <iostream>

#ifndef TEST_DIR
#error "TEST_DIR must be defined by CMake (see target_compile_definitions)"
#endif

int main(int argc, char** argv) {
    std::string base   = TEST_DIR;
    std::string path   = argc > 1 ? argv[1] : base + "/cases/cases.txt";
    std::string logDir = argc > 2 ? argv[2] : base + "/logs";

    try {
        std::vector<TestCase> cases = loadCases(path);
        if (cases.empty()) {
            throw std::runtime_error(path + ": no cases found");
        }

        FailureLog flog(logDir, path);
        int failed = 0;
        for (const TestCase& tc : cases) {
            CaseResult r = runCase(tc);
            if (r.passed) {
                std::cout << "PASS  " << tc.number << " " << tc.name << "\n";
            } else {
                ++failed;
                std::cout << "FAIL  " << tc.number << " " << tc.name
                          << "\n      " << r.reason << "\n";
                flog.record(r);
            }
        }

        int total = static_cast<int>(cases.size());
        std::cout << "\n" << (total - failed) << "/" << total << " passed\n";
        if (flog.created()) std::cout << "details: " << flog.path() << "\n";

        appendStatistics(logDir, path, total, failed, flog.path());
        return failed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "test run aborted: " << e.what() << "\n";
        return 2;
    }
}