#include "contracts.h"
#include <iostream>
#include <limits>

using nlohmann::json;
void require(bool condition) {
    if (!condition) throw std::runtime_error("contract regression");
}
int main() {
    try {
        require(legacyCapabilities({{"featureSet", "0000010"}, {"thermalModes", "11000000"}}));
        for (auto entry : {json::object(), json{{"thermalModes", "11000000"}},
            json{{"featureSet", "1111111"}}, json{{"featureSet", "11111111"}, {"thermalModes", "11000000"}},
            json{{"featureSet", "0000001"}, {"thermalModes", "11000000"}},
            json{{"featureSet", 127}, {"thermalModes", "11000000"}}}) require(!legacyCapabilities(entry));
        require(bounded(0, 0, 255) == 0 && bounded(255, 0, 255) == 255 && bounded(100, 0, 100) == 100);
        require(lightLimit(2) == 23 && lightLimit(3) == 23 && lightLimit(6) == 7 && lightLimit(5) == 255);
        require(brightnessSupported(2) && brightnessSupported(8) && !brightnessSupported(6) && !brightnessSupported(7));
        json keys = json::object();
        for (int index : x15KeyIndices) keys[std::to_string(index)] = {index};
        require(x15LayoutComplete(keys));
        keys.erase("16");
        require(!x15LayoutComplete(keys));
        for (auto value : {json(-1), json(256), json(1.5), json("1"), json(true), json(nullptr), json(std::numeric_limits<uint64_t>::max())}) {
            bool rejected = false;
            try { bounded(value, 0, 255); } catch (const std::exception&) { rejected = true; }
            require(rejected);
        }
        std::cout << "Capability and range contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
