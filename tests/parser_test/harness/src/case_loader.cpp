#include "case_loader.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <algorithm>
#include <cctype>
#include <fstream>


namespace {

    bool isBlank(const std::string& line){
        return std::all_of(line.begin(), line.end(), [] (unsigned char c) { return std::isspace(c); });
    }

    std::string trimEnds(std::string in_string){
        size_t first = in_string.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = in_string.find_last_not_of(" \t\r\n");
        return in_string.substr(first, last - first + 1);
    }
 

    // 1. Remove a trailing '\r' (Windows line endings), nothing else.
    std::string stripCR(const std::string& line){
        if(!(line.empty()) && ((line.back() == '\r'))){
            return line.substr(0, line.size() - 1);
        }
        return line;
    }

    // 2. Join lines with "\n", dropping blank lines at the END only.
    //    Blank lines in the middle, and every line's leading spaces, stay.
    std::string joinTrimmed(const std::vector<std::string>& lines){
        size_t end = lines.size();

        while((end > 0) && isBlank(lines[end - 1])){
            end--;
        }

        std::string result  = "";
        for(size_t i = 0; i < end; ++i){
           if(i > 0) result += '\n';
           result += lines[i];
        }
        return result;
    }

    // 3. Does the line start with "### "?  Return the name after it.
    bool isCaseStart(const std::string& line, std::string& nameOut){
        if(line.rfind("### ", 0) == 0){
            nameOut = line.substr(4);
            nameOut = trimEnds(nameOut);
            return true;
        }
        return false;
    }

    enum class State { Outside, Sql, Expected };
}


std::vector<TestCase> loadCases(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("cannot open case file: " + path);
    }

    std::vector<TestCase> cases;

    State state = State::Outside;
    bool haveCase = false;    // a "###" has been seen
    bool sawMarker = false;   // "--- expect" / "--- error" seen for this case
    std::string name;
    std::vector<std::string> sqlLines, expectedLines;
    CaseKind kind = CaseKind::Tree;

    // Close the case being built: validate it, then push it.
    auto finishCase = [&]() {
        if (!haveCase) return;
        if (name.empty()) {
            throw std::runtime_error(path + ": case #" +
                std::to_string(cases.size() + 1) + " has an empty name");
        }
        if (!sawMarker) {
            throw std::runtime_error(path + ": case '" + name +
                "' has no '--- expect' or '--- error' marker");
        }
        TestCase c;
        c.number = static_cast<int>(cases.size()) + 1;
        c.name = name;
        c.sql = joinTrimmed(sqlLines);
        c.kind = kind;
        c.expected = joinTrimmed(expectedLines);
        if (c.sql.empty()) {
            throw std::runtime_error(path + ": case '" + name + "' has no SQL");
        }
        cases.push_back(std::move(c));
    };

    std::string line;
    while (std::getline(in, line)) {
        line = stripCR(line);

        std::string newName;
        if (isCaseStart(line, newName)) {
            finishCase();          // closes the PREVIOUS case, still under its own name
            name = newName;
            sqlLines.clear();
            expectedLines.clear();
            sawMarker = false;
            haveCase = true;
            state = State::Sql;
        } else if (state == State::Sql &&
                   (line == "--- expect" || line == "--- error")) {
            kind = (line == "--- expect") ? CaseKind::Tree : CaseKind::Error;
            sawMarker = true;
            state = State::Expected;
        } else if (state == State::Sql) {
            sqlLines.push_back(line);
        } else if (state == State::Expected) {
            expectedLines.push_back(line);
        }
        // State::Outside: text before the first "###" is ignored (comments).
    }
    finishCase();

    return cases;
}