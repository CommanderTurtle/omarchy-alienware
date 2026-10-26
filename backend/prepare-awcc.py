"""Build the upstream thermal framework without its GUI, socket server or shell executor."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:3])
target.mkdir(parents=True, exist_ok=True)
header = (source / "include/AcpiUtils.h").read_text(encoding="utf-8")
assert header.count('#include "Daemon.h"') == 1
header = header.replace('#include "Daemon.h"', '#include "DirectAcpi.h"')
assert header.count('m_featureSetBits{"1111111"}') == 1
header = header.replace('m_featureSetBits{"1111111"}', 'm_featureSetBits{"0000000"}')
(target / "AcpiUtils.h").write_text('#include <vector>\n' + header)
thermal = (source / "include/Thermals.h").read_text(encoding="utf-8")
(target / "Thermals.h").write_text(thermal)
code = (source / "src/AcpiUtils.cpp").read_text(encoding="utf-8")
start = code.index("int AcpiUtils::executeAcpiCommand(")
end = code.index("void AcpiUtils::deviceInfo(", start)
code = code[:start] + '''int AcpiUtils::executeAcpiCommand(int a, int b, int c, int d) {
    return directAcpi(m_acpiPrefix, a, b, c, d);
}
\n''' + code[end:]
assert code.count('"/etc/awcc/database.json"') == 1
code = code.replace('"/etc/awcc/database.json"', '"/usr/share/omarchy-alienware/awcc/database.json"')
(target / "AcpiUtils.cpp").write_text('#include <array>\n#include <cstring>\n' + code)
thermal_code = (source / "src/Thermals.cpp").read_text(encoding="utf-8")
assert thermal_code.count("return ThermalModes::Manual;") == 1
thermal_code = thermal_code.replace("return ThermalModes::Manual;", 'throw std::runtime_error("Unknown firmware thermal mode");')
(target / "Thermals.cpp").write_text('#include <stdexcept>\n' + thermal_code)

sdk = pathlib.Path(sys.argv[3])
fx = (sdk / "AlienFX-SDK/src/AlienFX_SDK.cpp").read_text(encoding="utf-8")
guard = '    if (reportIDList[version] == 0) {'
assert fx.count(guard) == 1
fx = fx.replace(guard, '    if (version == API_UNKNOWN || length < 8 || length >= MAX_BUFFERSIZE) return false;\n' + guard)
packet = '    std::uint8_t buffer[MAX_BUFFERSIZE];'
assert fx.count(packet) == 2
fx = fx.replace(packet, '    std::uint8_t buffer[MAX_BUFFERSIZE]{};')
status = 'std::uint8_t Functions::GetDeviceStatus() {\n    std::uint8_t buffer[MAX_BUFFERSIZE]{};'
assert fx.count(status) == 1
fx = fx.replace(status, status + '\n    buffer[0] = reportIDList[version];')
anchor = '    memset(buffer, version == API_V6 ? 0xff : 0x00, length);'
assert fx.count(anchor) == 1
fx = fx.replace(anchor, '''    if (length <= 0 || length > MAX_BUFFERSIZE || command[0] + 1 > length)
        throw std::runtime_error("Invalid HID packet length");
''' + anchor)
anchor = '            memcpy(buffer + m.i, m.vval.data(), m.vval.size());'
assert fx.count(anchor) == 1
fx = fx.replace(anchor, '''            if (m.i < 0 || static_cast<size_t>(m.i) + m.vval.size() > static_cast<size_t>(length))
                throw std::runtime_error("HID command exceeds report length");
''' + anchor)
anchor = 'needV8Feature = mods->front().vval.size() == 1;'
assert fx.count(anchor) == 1
fx = fx.replace(anchor, 'needV8Feature = !mods->empty() && mods->front().vval.size() == 1;')
(target / "AlienFX_SDK.cpp").write_text('#include <stdexcept>\n' + fx)

transport = (sdk / "AlienFX-SDK/src/libusb_helper.cpp").read_text(encoding="utf-8")
for function in ("hid_send_output_report", "hid_send_feature_report", "hid_write"):
    call = f"    return {function}(dev, buffer, length);"
    assert transport.count(call) == 1
    transport = transport.replace(call, f'''    if ({function}(dev, buffer, length) != static_cast<int>(length))
        throw std::runtime_error("HID write failed or was incomplete");
    return true;''')
for function in ("hid_read", "hid_get_feature_report", "hid_get_input_report"):
    call = f"    return {function}(dev, buffer, length);"
    assert transport.count(call) == 1
    transport = transport.replace(call, f'''    int count = {function}(dev, buffer, length);
    if (count <= 0) throw std::runtime_error("HID read failed or timed out");
    return true;''')
(target / "libusb_helper.cpp").write_text('#include <stdexcept>\n' + transport)
