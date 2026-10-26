#include "AcpiUtils.h"
#include "Thermals.h"
#include "AlienFan-SDK.h"
#include "AlienFX_SDK.h"
#include "DirectAcpi.h"
#include "contracts.h"
#include "inventory.h"
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using json = nlohmann::json;
namespace fs = std::filesystem;
constexpr auto configPath = "/etc/omarchy-alienware/backend.json";
constexpr auto databasePath = "/usr/share/omarchy-alienware/awcc/database.json";
constexpr auto ownedPath = "/run/omarchy-alienware/fan-owner.json";

std::string text(const fs::path& path) {
    std::ifstream in(path);
    std::string value;
    std::getline(in, value);
    return value;
}
json config() {
    struct stat info{};
    if (lstat(configPath, &info) < 0) return json::object();
    if (!S_ISREG(info.st_mode) || info.st_uid != 0 || (info.st_mode & 0022) || info.st_size > 65536)
        throw std::runtime_error("backend.json must be a root-owned, non-group/world-writable regular file under 64 KiB");
    std::ifstream file(configPath);
    auto result = json::parse(file);
    if (!result.is_object()) throw std::runtime_error("backend.json must be an object");
    return result;
}
std::string role(const std::string& label, int index) {
    std::string lowered = label;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    if (lowered.find("cpu") != std::string::npos) return "cpu";
    if (lowered.find("gpu") != std::string::npos) return "gpu";
    return "fan" + std::to_string(index);
}
std::string fanID(const AlienFan_SDK::ALIENFAN_FAN& fan, const std::vector<AlienFan_SDK::ALIENFAN_FAN>& fans) {
    auto label = role(fan.name, fan.id);
    auto count = std::count_if(fans.begin(), fans.end(), [&](const auto& other) { return role(other.name, other.id) == label; });
    return count == 1 ? label : "fan" + std::to_string(fan.id);
}
struct Mode { const char* name; ThermalModes value; };
constexpr Mode modes[] = {
    {"custom", ThermalModes::Manual}, {"balanced", ThermalModes::Balanced},
    {"performance", ThermalModes::Performance}, {"cool", ThermalModes::Cool},
    {"quiet", ThermalModes::Quiet}, {"full-speed", ThermalModes::FullSpeed},
    {"low-power", ThermalModes::BatterySaver}, {"g-mode", ThermalModes::Gmode}
};

void ownBoost(const std::string& model, const std::string& backend, const std::string& id, int value) {
    if (value == 0) return;
    json owner = {{"model", model}, {"backend", backend}, {"fans", json::array()}};
    if (fs::exists(ownedPath)) {
        owner = json::parse(std::ifstream(ownedPath));
        if (owner.value("model", "") != model || owner.value("backend", "") != backend)
            throw std::runtime_error("Stop previous fan control before changing backends");
    }
    if (std::find(owner["fans"].begin(), owner["fans"].end(), id) == owner["fans"].end()) owner["fans"].push_back(id);
    std::ofstream out(std::string(ownedPath) + ".tmp");
    if (!(out << owner.dump()) || !out.flush()) throw std::runtime_error("Cannot record fan ownership for safe cleanup");
    out.close();
    fs::rename(std::string(ownedPath) + ".tmp", ownedPath);
}

json thermal(const json& request, const json& cfg) {
    std::string action = request.at("action");
    AlienFan_SDK::Control fan;
    fan.Probe();
    auto model = text("/sys/class/dmi/id/product_name");
    std::ifstream dbFile(databasePath);
    if (!dbFile) throw std::runtime_error("Packaged AWCC database is missing");
    auto db = json::parse(dbFile);
    bool known = db.contains(model);
    auto selected = cfg.value("thermal", "auto");
    if (selected != "auto" && selected != "kernel" && selected != "awcc") throw std::runtime_error("Unknown thermal backend");
    bool kernel = selected == "kernel" || (selected == "auto" && (!fan.fans.empty() || !fan.profiles.empty() || fs::exists("/sys/firmware/acpi/platform_profile_choices")));
    bool described = known && legacyCapabilities(db.at(model));
    bool legacy = !kernel && selected != "kernel" && described && fs::exists("/proc/acpi/call");
    if (action == "reset") {
        if (!fs::exists(ownedPath)) return {{"ok", true}, {"reset", false}};
        auto owner = json::parse(std::ifstream(ownedPath));
        if (owner.value("model", "") != model || owner.value("backend", "") != (kernel ? "kernel" : legacy ? "awcc" : "none"))
            throw std::runtime_error("Fan owner no longer matches hardware; refusing to switch backends during cleanup");
        for (const auto& id : owner.at("fans")) {
            auto reset = request;
            reset["action"] = "boost";
            reset["fan"] = id;
            reset["value"] = 0;
            thermal(reset, cfg);
        }
        fs::remove(ownedPath);
        return {{"ok", true}, {"reset", true}};
    }
    json result = {{"ok", true}, {"model", model}, {"supportedModel", known},
        {"backend", kernel ? "kernel" : legacy ? "awcc" : "none"},
        {"fans", json::array()}, {"temps", json::object()},
        {"profile", {{"current", ""}, {"choices", json::array()}, {"writable", false}}},
        {"curveSupported", false}, {"warnings", json::array()}};
    if (kernel) {
        bool globalProfile = fan.profiles.empty() && fs::exists("/sys/firmware/acpi/platform_profile_choices");
        std::vector<std::string> globalChoices;
        if (globalProfile) {
            std::istringstream names(text("/sys/firmware/acpi/platform_profile_choices"));
            for (std::string name; names >> name;) globalChoices.push_back(name);
        }
        for (const auto& item : fan.fans) {
            int rpm = fan.GetFanRPM(item), max = fan.GetMaxRPM(item), boost = fan.GetFanBoost(item);
            result["fans"].push_back({{"id", fanID(item, fan.fans)}, {"index", item.id}, {"label", item.name},
                {"rpm", std::max(rpm, 0)}, {"max", std::max(max, 0)}, {"boost", std::max(boost, 0)},
                {"percent", std::max(fan.GetFanPercent(item), 0)}, {"readable", rpm >= 0}, {"boostWritable", boost >= 0 && boost <= 255}});
        }
        for (const auto& sensor : fan.sensors) {
            auto key = role(sensor.name, sensor.index);
            int value = fan.GetTempValue(sensor);
            if ((key == "cpu" || key == "gpu") && value >= 0 && value <= 150)
                result["temps"][key] = std::max(value, result["temps"].value(key, 0));
        }
        for (const auto& profile : fan.profiles) result["profile"]["choices"].push_back(profile.name);
        result["profile"]["current"] = fan.GetPowerProfile().name;
        result["profile"]["writable"] = !fan.profiles.empty();
        if (globalProfile) {
            result["profile"]["choices"] = globalChoices;
            result["profile"]["current"] = text("/sys/firmware/acpi/platform_profile");
            result["profile"]["writable"] = !globalChoices.empty();
        }
        result["profile"]["gmodeForced"] = text("/sys/module/alienware_wmi/parameters/force_gmode") == "Y";
        std::set<std::string> boostRoles;
        for (const auto& item : result["fans"]) if (item["boostWritable"] == true) boostRoles.insert(item["id"]);
        result["curveSupported"] = boostRoles.contains("cpu") && boostRoles.contains("gpu") &&
            result["temps"].contains("cpu") && result["temps"].contains("gpu");
        bool custom = std::any_of(fan.profiles.begin(), fan.profiles.end(), [](const auto& p) { return p.type == AlienFan_SDK::CUSTOM; });
        custom = custom || std::find(globalChoices.begin(), globalChoices.end(), "custom") != globalChoices.end();
        result["curveSupported"] = result["curveSupported"].get<bool>() && custom;
        if (action == "profile") {
            std::string name = request.at("name");
            if (globalProfile) {
                if (std::find(globalChoices.begin(), globalChoices.end(), name) == globalChoices.end()) throw std::runtime_error("Profile not advertised by kernel");
                std::ofstream out("/sys/firmware/acpi/platform_profile");
                if (!(out << name) || !out.flush()) throw std::runtime_error("Kernel profile write failed");
                if (text("/sys/firmware/acpi/platform_profile") != name) throw std::runtime_error("Kernel profile readback failed");
            } else {
                auto it = std::find_if(fan.profiles.begin(), fan.profiles.end(), [&](const auto& p) { return p.name == name; });
                if (it == fan.profiles.end() || !fan.SetPowerProfile(it->type) || fan.GetPowerProfile().name != name)
                    throw std::runtime_error("Kernel profile unavailable or write failed");
            }
        } else if (action == "boost") {
            std::string id = request.at("fan");
            auto it = std::find_if(fan.fans.begin(), fan.fans.end(), [&](const auto& f) { return fanID(f, fan.fans) == id; });
            if (it == fan.fans.end()) throw std::runtime_error("Fan boost unavailable");
            int previous = fan.GetFanBoost(*it);
            if (previous < 0 || previous > 255) throw std::runtime_error("Fan boost readback unavailable");
            int value = bounded(request.at("value"), 0, 255);
            ownBoost(model, "kernel", id, value);
            if (!fan.SetFanBoost(*it, value) || fan.GetFanBoost(*it) != value) throw std::runtime_error("Kernel fan write failed");
        } else if (action != "status") throw std::runtime_error("Unknown thermal action");
    } else if (legacy) {
        Daemon unused;
        AcpiUtils acpi(unused);
        Thermals thermals(acpi);
        auto current = thermals.getCurrentMode();
        for (auto mode : modes) {
            if (thermals.supportsThemeralMode(mode.value) && (mode.value != ThermalModes::Gmode || acpi.hasFeature(FeatureSet::GModeToggle)))
                result["profile"]["choices"].push_back(mode.name);
            if (mode.value == current) result["profile"]["current"] = mode.name;
        }
        result["profile"]["writable"] = acpi.hasFeature(FeatureSet::ThermalModes);
        result["warnings"].push_back("AWCC ACPI provides thermal profiles and boost, not additive curve or RPM telemetry");
        if (acpi.hasFeature(FeatureSet::FanBoost)) {
            int cpuBoost = thermals.getCpuBoost();
            result["fans"].push_back({{"id", "cpu"}, {"label", "CPU Fan"}, {"boost", std::max(cpuBoost, 0)}, {"readable", false}, {"boostWritable", cpuBoost >= 0 && cpuBoost <= 100}});
            if (model != "Alienware Aurora R11")
            {
                int gpuBoost = thermals.getGpuBoost();
                result["fans"].push_back({{"id", "gpu"}, {"label", "GPU Fan"}, {"boost", std::max(gpuBoost, 0)}, {"readable", false}, {"boostWritable", gpuBoost >= 0 && gpuBoost <= 100}});
            }
            else result["warnings"].push_back("Aurora R11 legacy GPU boost disabled: upstream read/write fan addresses differ");
        }
        if (action == "profile") {
            std::string name = request.at("name");
            auto it = std::find_if(std::begin(modes), std::end(modes), [&](auto mode) { return name == mode.name; });
            if (!acpi.hasFeature(FeatureSet::ThermalModes) || it == std::end(modes) || !thermals.supportsThemeralMode(it->value))
                throw std::runtime_error("Profile is not supported by the AWCC device database");
            if ((it->value == ThermalModes::Gmode || current == ThermalModes::Gmode) && !acpi.hasFeature(FeatureSet::GModeToggle))
                throw std::runtime_error("G-mode switching is not advertised by the AWCC device database");
            if (current != it->value) { acpiWriteAllowed = true; thermals.setThermalMode(it->value); }
            if (thermals.getCurrentMode() != it->value) throw std::runtime_error("Firmware did not accept the thermal profile");
        } else if (action == "boost") {
            if (!acpi.hasFeature(FeatureSet::FanBoost)) throw std::runtime_error("AWCC fan boost unsupported");
            int value = bounded(request.at("value"), 0, 100);
            std::string id = request.at("fan");
            if (id != "cpu" && id != "gpu") throw std::runtime_error("Unknown AWCC fan");
            if (id == "gpu" && model == "Alienware Aurora R11") throw std::runtime_error("Aurora R11 legacy GPU fan addresses are ambiguous; use kernel control");
            int previous = id == "cpu" ? thermals.getCpuBoost() : thermals.getGpuBoost();
            if (previous < 0 || previous > 100) throw std::runtime_error("AWCC fan boost readback unavailable");
            ownBoost(model, "awcc", id, value);
            acpiWriteAllowed = true;
            if (id == "cpu") thermals.setCpuBoost(value); else thermals.setGpuBoost(value);
            if ((id == "cpu" ? thermals.getCpuBoost() : thermals.getGpuBoost()) != value)
                throw std::runtime_error("Firmware did not accept fan boost");
        } else if (action != "status") throw std::runtime_error("Unknown thermal action");
    } else {
        result["warnings"].push_back(known && !described ? "AWCC entry lacks explicit thermal capabilities; legacy ACPI disabled" :
            known ? "No kernel controls or loaded acpi_call; no firmware probing was attempted" : "Model absent from AWCC database; kernel controls only");
        if (action != "status") throw std::runtime_error("No supported thermal backend is available");
    }
    return result;
}

std::vector<uint8_t> ids(const json& value, int api) {
    if (!value.is_array() || value.empty() || value.size() > 255) throw std::runtime_error("Light mapping must contain 1-255 IDs");
    std::vector<uint8_t> result;
    for (const auto& id : value) result.push_back(bounded(id, 0, lightLimit(api)));
    return result;
}
AlienFX_SDK::Afx_action color(std::string hex) {
    if (!std::regex_match(hex, std::regex("[0-9a-fA-F]{6}"))) throw std::runtime_error("Expected RRGGBB color");
    auto v = std::stoul(hex, nullptr, 16);
    return {AlienFX_SDK::AlienFX_A_Color, 5, 5, static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
}
json lighting(const json& request, const json& cfg) {
    std::string action = request.at("action"), section = request.at("section");
    auto configured = cfg.value("devices", json::object());
    SDKInventory mapping(configured);
    json devices = json::array(), regions = json::array();
    std::map<std::string, std::vector<std::pair<AlienFX_SDK::Afx_device*, std::vector<uint8_t>>>> targets;
    bool keyboard = false;
    bool canBrightness = true;
    for (size_t deviceIndex = 0; deviceIndex < mapping.fxdevs.size(); deviceIndex++) {
        auto& device = mapping.fxdevs[deviceIndex];
        if (!device.present || !device.dev) continue;
        auto id = std::format("{:04x}:{:04x}", device.vid, device.pid);
        auto entry = configured.value(id, json::object());
        if (!entry.is_object()) throw std::runtime_error("Device configuration must be an object");
        auto regionMap = entry.value("regions", json::object());
        auto keyMap = entry.value("keys", json::object());
        if (!regionMap.is_object() || !keyMap.is_object()) throw std::runtime_error("Light maps must be objects");
        devices.push_back({{"id", id}, {"name", device.name}, {"api", device.version},
            {"interface", mapping.interfaces[deviceIndex]}, {"ambiguous", mapping.matches[id] != 1},
            {"brightnessSupported", brightnessSupported(device.version)},
            {"regions", regionMap}, {"keys", keyMap.size()}, {"mapped", mapping.matches[id] == 1 && (!regionMap.empty() || !keyMap.empty())}});
        if (mapping.matches[id] != 1) continue;
        auto selected = section == "kbd" ? keyMap : regionMap;
        if (section != "kbd") for (auto it = keyMap.begin(); it != keyMap.end(); ++it) selected["key:" + it.key()] = it.value();
        if (section == "kbd" && (entry.value("keyLayout", "") != "x15-r2" || !x15LayoutComplete(keyMap) ||
            text("/sys/class/dmi/id/product_name") != "Alienware x15 R2")) selected = json::object();
        keyboard = keyboard || (section == "kbd" && !selected.empty());
        for (auto it = selected.begin(); it != selected.end(); ++it) {
            auto lights = ids(it.value(), device.version);
            if (section == "device" && request.value("device", "") != id) continue;
            targets[it.key()].push_back({&device, lights});
            canBrightness = canBrightness && brightnessSupported(device.version);
            regions.push_back({{"id", it.key()}, {"name", it.key()}, {"ledCount", lights.size()}});
        }
    }
    if (action == "status") {
        if (section == "kbd") return {{"ok", true}, {"present", keyboard}, {"keyCount", targets.size()}};
        return {{"ok", true}, {"backend", "AlienFX-SDK"}, {"devices", devices}, {"regions", regions},
            {"device", {{"ready", !targets.empty()}, {"brightnessSupported", canBrightness && !targets.empty()}, {"path", "AlienFX-SDK"}}}};
    }
    if (targets.empty()) throw std::runtime_error("No mapped lighting for this device; configure root-owned backend.json using verified light IDs");
    if (action == "brightness" && !canBrightness) throw std::runtime_error("Standalone brightness is not supported by every mapped controller");
    std::map<std::string, AlienFX_SDK::Afx_action> colors;
    if (action == "set-map") {
        auto values = request.at("colors");
        if (!values.is_object() || values.empty()) throw std::runtime_error("Empty color map");
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (!targets.contains(it.key())) throw std::runtime_error("Unmapped light: " + it.key());
            colors[it.key()] = color(it.value());
        }
    } else if (action == "set-all" || action == "off") {
        auto value = color(action == "off" ? "000000" : request.at("color").get<std::string>());
        for (const auto& [id, unused] : targets) colors[id] = value;
    } else if (action != "brightness") throw std::runtime_error("Unsupported lighting action");
    std::set<AlienFX_SDK::Afx_device*> changed;
    for (const auto& [id, value] : colors) for (auto [device, lights] : targets.at(id)) {
        if (!device->dev->SetMultiColor(&lights, value)) throw std::runtime_error("AlienFX refused light color");
        changed.insert(device);
    }
    if (action == "brightness") {
        int level = bounded(request.at("value"), 0, 100) * 255 / 100;
        std::map<AlienFX_SDK::Afx_device*, std::set<uint8_t>> byDevice;
        for (const auto& [id, group] : targets) for (const auto& [device, lights] : group)
            byDevice[device].insert(lights.begin(), lights.end());
        for (const auto& [device, lights] : byDevice) {
            std::vector<AlienFX_SDK::Afx_light> names;
            for (auto light : lights) { AlienFX_SDK::Afx_light l{}; l.lightid = light; names.push_back(l); }
            device->dev->SetBrightness(level, 255, &names, true);
            changed.insert(device);
        }
    }
    for (auto* device : changed) if (!device->dev->UpdateColors()) throw std::runtime_error("AlienFX update failed");
    return {{"ok", true}};
}

int main(int argc, char** argv) {
    loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
    try {
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << json({{"ok", true}, {"backend", "0.4.0"}, {"awcc", "1.19.0"},
                {"awccRevision", AWCC_REVISION}, {"alienfxRevision", ALIENFX_REVISION}}).dump() << '\n'; return 0;
        }
        if (geteuid() != 0) throw std::runtime_error("Use alienwarectl; the SDK bridge is daemon-only");
        std::string raw;
        char c;
        while (std::cin.get(c)) { if (raw.size() >= 65536) throw std::runtime_error("Request too large"); raw += c; }
        auto request = json::parse(raw);
        std::string section = request.at("section"), action = request.at("action");
        fs::create_directories("/run/omarchy-alienware");
        int fd = open("/run/omarchy-alienware/hardware.lock", O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_RDWR, 0600);
        if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) < 0) throw std::runtime_error("Hardware backend is busy");
        auto cfg = config();
        json result = section == "thermal" ? thermal(request, cfg) :
            (section == "rgb" || section == "kbd" || section == "device") ? lighting(request, cfg) :
            throw std::runtime_error("Unknown request section");
        std::cout << result.dump() << '\n';
        close(fd);
        return 0;
    } catch (const std::exception& e) {
        auto code = std::string(e.what()).find("backend is busy") != std::string::npos ? "device-busy" : "not-supported";
        std::cout << json({{"ok", false}, {"code", code}, {"error", e.what()}}).dump() << '\n';
        return 1;
    }
}
