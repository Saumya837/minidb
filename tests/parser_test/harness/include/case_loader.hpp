#pragma once

#include <string>
 #include <vector>

enum class CaseKind { 
    Tree, 
    Error 
};

struct TestCase {
    int number;            // 1-based position in the file
    std::string name;      // text after "### "
    std::string sql;       // lines between the name and the marker, joined with "\n"
    CaseKind kind;         // "--- expect" -> Tree, "--- error" -> Error
    std::string expected;  // lines after the marker, joined with "\n"
};

 std::vector<TestCase> loadCases(const std::string& path);
 