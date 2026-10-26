#include "DirectAcpi.h"
#include <fcntl.h>
#include <unistd.h>
#include <format>
#include <fstream>
#include <regex>

int directAcpi(const char* prefix, int a, int b, int c, int d) {
    if (!std::regex_match(prefix, std::regex("AMW[13W]"))) throw std::runtime_error("Invalid ACPI prefix");
    bool read = a == 0x14 && (b == 0x0b || b == 0x0c);
    bool write = (a == 0x15 && (b == 1 || b == 2)) || (a == 0x25 && (b == 0 || b == 1));
    if ((!read && !write) || (!read && !acpiWriteAllowed) || c < 0 || c > 0x1ff || d < 0 || d > 255)
        throw std::runtime_error("ACPI operation not allowed");
    std::string command = std::format("\\_SB.{}.WMAX 0 {:#x} {{{:#x},{:#x},{:#x},0x00}}", prefix, a, b, c, d);
    int fd = open("/proc/acpi/call", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw std::runtime_error("acpi_call unavailable; install acpi_call-dkms and the matching kernel headers, then load acpi_call explicitly");
    auto n = ::write(fd, command.data(), command.size());
    close(fd);
    if (n != static_cast<ssize_t>(command.size())) throw std::runtime_error("ACPI write failed");
    std::ifstream response("/proc/acpi/call");
    std::string value;
    response >> value;
    if (!std::regex_match(value, std::regex("(0x[0-9a-fA-F]+|[0-9]+)")))
        throw std::runtime_error("ACPI firmware rejected the operation: " + value);
    return static_cast<int>(std::stoul(value, nullptr, 0));
}
