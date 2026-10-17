#include "failure_log.hpp"

#include <ctime>
#include <filesystem>
#include <stdexcept>

namespace {
    std::string timestamp(const char* format) {
        std::time_t now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), format, std::localtime(&now));
        return buf;
    }
}  // namespace

FailureLog::FailureLog(std::string dir, std::string caseFile)
    : dir_(std::move(dir)), caseFile_(std::move(caseFile)) {}

void FailureLog::open() {
    std::filesystem::create_directories(dir_);
    path_ = dir_ + "/failures_" + timestamp("%Y%m%d_%H%M%S") + ".log";

    out_.open(path_);
    if (!out_.is_open()) {
        std::string failedPath = path_;
        path_.clear();
        throw std::runtime_error("cannot write failure log: " + failedPath);
    }
    out_ << "Failures from run at " << timestamp("%Y-%m-%d %H:%M:%S") << "\n"
         << "Case file: " << caseFile_ << "\n\n";
}

void FailureLog::record(const CaseResult& r) {
    if (!out_.is_open()) open();

    out_ << "=== FAIL #" << r.testCase.number << " " << r.testCase.name << " ===\n"
         << "reason: " << r.reason << "\n"
         << "--- sql\n" << r.testCase.sql << "\n"
         << "--- expected" << (r.testCase.kind == CaseKind::Error ? " (error)" : " (tree)") << "\n"
         << r.testCase.expected << "\n"
         << "--- actual\n" << r.actual << "\n\n";
         
    out_.flush();
}