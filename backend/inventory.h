#pragma once
#include "AlienFX_SDK.h"
#include "contracts.h"
#include <format>
#include <map>
#include <memory>

struct USBContext {
    libusb_context* value = nullptr;
    USBContext() {
        if (libusb_init(&value) < 0) throw std::runtime_error("Cannot initialize USB descriptor reader");
    }
    ~USBContext() { if (value) libusb_exit(value); }
};

struct SDKInventory {
    USBContext context;
    std::unique_ptr<hid_device_info, decltype(&hid_free_enumeration)> enumeration{hid_enumerate(0, 0), hid_free_enumeration};
    std::vector<std::unique_ptr<AlienFX_SDK::Functions>> handles;
    std::vector<AlienFX_SDK::Afx_device> fxdevs;
    std::map<std::string, int> matches;
    std::vector<int> interfaces;

    explicit SDKInventory(const nlohmann::json& configured) {
        for (auto* item = enumeration.get(); item; item = item->next) {
            auto id = std::format("{:04x}:{:04x}", item->vendor_id, item->product_id);
            auto entry = configured.value(id, nlohmann::json::object());
            if (!entry.is_object()) throw std::runtime_error("Device configuration must be an object");
            if (entry.contains("interface") && bounded(entry["interface"], 0, 255) != item->interface_number) continue;
            auto device = std::make_unique<AlienFX_SDK::Functions>();
            if (!device->AlienFXProbeDevice(context.value, item->vendor_id, item->product_id, item->path)) continue;
            matches[id]++;
            fxdevs.push_back({item->product_id, item->vendor_id, device.get(), device->description, device->version});
            fxdevs.back().present = true;
            interfaces.push_back(item->interface_number);
            handles.push_back(std::move(device));
        }
    }
};
