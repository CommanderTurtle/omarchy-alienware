#pragma once
#include <nlohmann/json.hpp>
#include <regex>
#include <stdexcept>

inline int bounded(const nlohmann::json& value, int low, int high) {
    if (!value.is_number_integer()) throw std::runtime_error("Expected an integer");
    auto n = value.get<long long>();
    if (n < low || n > high) throw std::runtime_error("Value outside supported range");
    return static_cast<int>(n);
}

inline bool legacyCapabilities(const nlohmann::json& entry) {
    return entry.is_object() && entry.contains("featureSet") && entry["featureSet"].is_string() &&
        std::regex_match(entry["featureSet"].get<std::string>(), std::regex("[01]{7}")) &&
        entry.contains("thermalModes") && entry["thermalModes"].is_string() &&
        std::regex_match(entry["thermalModes"].get<std::string>(), std::regex("[01]{8}")) &&
        (std::stoi(entry["featureSet"].get<std::string>(), nullptr, 2) & 2) != 0;
}

inline int lightLimit(int api) {
    if (api == 2 || api == 3) return 23;
    if (api == 6) return 7;
    if (api == 4 || api == 5 || api == 7 || api == 8) return 255;
    throw std::runtime_error("Unsupported AlienFX API");
}

inline bool brightnessSupported(int api) {
    return api == 2 || api == 3 || api == 4 || api == 5 || api == 8;
}

inline constexpr int x15KeyIndices[] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,34,35,
    40,42,43,44,45,46,47,48,49,50,51,52,53,55,60,61,62,63,64,65,66,67,68,69,70,71,72,74,80,81,83,84,
    85,86,87,88,89,90,91,92,94,100,101,102,103,104,109,111,112,114,133,134,135
};

inline bool x15LayoutComplete(const nlohmann::json& keys) {
    if (!keys.is_object()) return false;
    for (int index : x15KeyIndices) if (!keys.contains(std::to_string(index))) return false;
    return true;
}
