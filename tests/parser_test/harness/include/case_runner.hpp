#pragma once
#include "case_loader.hpp"
#include <string>

struct CaseResult {
    TestCase testCase;
    bool passed = false;
    std::string reason;   // empty when passed
    std::string actual;   // tree text or error text, for the failure log
};

// "" if equal, otherwise a one-line description of the first difference.
std::string firstDifference(const std::string& expected, const std::string& actual);

// Tokenize + parse one case and compare with what the case expects.
// Never throws: any failure becomes a failed CaseResult.
CaseResult runCase(const TestCase& tc);