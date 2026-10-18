#include "statistics.hpp"

#include "time_util.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

void appendStatistics(const std::string& logDir,
                      const std::string& caseFile,
                      int total, int failed,
                      const std::string& failureLogPath) {
    std::filesystem::create_directories(logDir);
    std::string path = logDir + "/test_statistics.log";

    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write statistics log: " + path);
    }

    // File names only: full paths differ between machines and run locations.
    std::string cases = std::filesystem::path(caseFile).filename().string();
    std::string details = failureLogPath.empty()
        ? "-"
        : std::filesystem::path(failureLogPath).filename().string();

    out << nowString("%Y-%m-%d %H:%M:%S")
        << " | " << cases
        << " | total=" << total
        << " | passed=" << (total - failed)
        << " | failed=" << failed
        << " | " << details << "\n";
}