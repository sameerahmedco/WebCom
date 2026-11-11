#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct DiagnosticsBundleEntry {
    std::string name;
    std::string content;
};

// Builds a small ZIP archive using the "store" method (no compression).
// Entry names must be simple relative filenames. Contents and total archive
// size are bounded so diagnostics export cannot allocate unbounded memory.
std::optional<std::vector<std::uint8_t>> BuildDiagnosticsZip(
    const std::vector<DiagnosticsBundleEntry>& entries,
    std::string* error = nullptr,
    std::size_t maximum_bytes = 4u * 1024u * 1024u);

bool DiagnosticsBundleNameSafe(std::string_view name);

} // namespace webcom
