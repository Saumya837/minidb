#pragma once
#include <ctime>
#include <string>

// Local time formatted with strftime, e.g. nowString("%Y-%m-%d %H:%M:%S").
inline std::string nowString(const char* format) {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), format, std::localtime(&now));
    return buf;
}