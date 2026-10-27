#include "case_runner.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "tree_dump.hpp"
#include <exception>
#include <vector>

namespace {

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) out += '\n';
        out += lines[i];
    }
    return out;
}

}  // namespace

std::string firstDifference(const std::string& expected, const std::string& actual) {
    std::vector<std::string> exp = splitLines(expected);
    std::vector<std::string> act = splitLines(actual);

    size_t common = exp.size() < act.size() ? exp.size() : act.size();
    for (size_t i = 0; i < common; ++i) {
        if (exp[i] != act[i]) {
            return "line " + std::to_string(i + 1) + ": expected '" + exp[i] +
                   "' but got '" + act[i] + "'";
        }
    }
    if (exp.size() > act.size()) {
        return "output ended after " + std::to_string(act.size()) +
               " lines; expected line " + std::to_string(act.size() + 1) +
               " '" + exp[act.size()] + "'";
    }
    if (act.size() > exp.size()) {
        return "expected " + std::to_string(exp.size()) + " lines but got " +
               std::to_string(act.size()) + "; first extra line '" +
               act[exp.size()] + "'";
    }
    return "";
}

CaseResult runCase(const TestCase& tc) {
    CaseResult result;
    result.testCase = tc;

    try {
        auto tokens = tokenize(tc.sql);
        size_t pos = 0;
        auto root = parseStatement(tokens, pos);
        result.actual = joinLines(dumpTree(*root));

        if (tc.kind == CaseKind::Error) {
            result.reason = "expected a syntax error but parsing succeeded";
        } else {
            result.reason = firstDifference(tc.expected, result.actual);
        }
    } catch (const std::exception& e) {
        result.actual = e.what();

        if (tc.kind == CaseKind::Tree) {
            result.reason = "unexpected error: " + result.actual;
        } else {
            result.reason = firstDifference(tc.expected, result.actual);
        }
    }

    result.passed = result.reason.empty();
    return result;
}