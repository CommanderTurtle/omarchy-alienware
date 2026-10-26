#pragma once
#include <stdexcept>
#include <string>
class Daemon {
public:
    std::string executeFromDaemon(const char*) { throw std::runtime_error("Shell execution is disabled"); }
};
inline bool acpiWriteAllowed = false;
int directAcpi(const char* prefix, int a, int b, int c, int d);
