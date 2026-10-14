#include "case_loader.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// 1. Remove a trailing '\r' (Windows line endings), nothing else.
std::string stripCR(const std::string& line);

// 2. Join lines with "\n", dropping blank lines at the END only.
//    Blank lines in the middle, and every line's leading spaces, stay.
std::string joinTrimmed(const std::vector<std::string>& lines);

// 3. Does the line start with "### "?  Return the name after it.
bool isCaseStart(const std::string& line, std::string& nameOut);

}  // namespace

std::vector<TestCase> loadCases(const std::string& path) {
    // open the file; throw std::runtime_error if it fails to open

    // state while reading: which section are we in?
    //   Outside (before the first ###)  |  Sql  |  Expected
    // plus the pieces of the case being built:
    //   name, sqlLines, expectedLines, kind

    // for each line (after stripCR):
    //   - "### name"           -> finish the previous case (if any), start a new one
    //   - "--- expect"/"--- error" (only while in Sql) -> set kind, switch to Expected
    //   - anything else        -> append to sqlLines or expectedLines by state,
    //                             or ignore it while Outside

    // after the loop: finish the last case

    // "finish a case": a case that never saw a marker is an error -> throw
    //   (name it in the message so you know which one is broken);
    //   otherwise sql = joinTrimmed(sqlLines), expected = joinTrimmed(expectedLines),
    //   number = cases.size() + 1, push it
}