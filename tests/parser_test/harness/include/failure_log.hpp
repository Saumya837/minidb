#pragma once
#include "case_runner.hpp"

#include <fstream>
#include <string>

// Writes one log file per test run, and only if something fails.
// The file is created on the first record() call, so a clean run leaves
// nothing behind. Name: <dir>/failures_YYYYMMDD_HHMMSS.log
class FailureLog {
public:
    FailureLog(std::string dir, std::string caseFile);

    // Append one failed case. Opens the file on first use.
    // Throws std::runtime_error if the file cannot be created.
    void record(const CaseResult& result);

    bool created() const { return out_.is_open(); }
    const std::string& path() const { return path_; }   // "" until created

private:
    void open();

    std::string dir_;
    std::string caseFile_;
    std::string path_;
    std::ofstream out_;
};