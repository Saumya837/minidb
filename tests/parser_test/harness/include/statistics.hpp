#pragma once
#include <string>

// Append one summary line for a finished run to <logDir>/test_statistics.log.
// Call it once, after every case has run (not for an aborted run).
// failureLogPath is the failure log of this run, or "" if nothing failed.
// Throws std::runtime_error if the file cannot be written.
void appendStatistics(const std::string& logDir,const std::string& caseFile, int total, int failed, const std::string& failureLogPath);