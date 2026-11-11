#include "diagnostics_bundle.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>

namespace webcom {
namespace {

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}

void Put16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
}

std::uint32_t Crc32(std::string_view text) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char byte : text) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

struct CentralRecord {
    std::string name;
    std::uint32_t crc = 0;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
};

} // namespace

bool DiagnosticsBundleNameSafe(std::string_view name) {
    if (name.empty() || name.size() > 96 || name == "." || name == "..") return false;
    if (name.find("..") != std::string_view::npos || name.find('/') != std::string_view::npos ||
        name.find('\\') != std::string_view::npos || name.front() == '.') return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.';
    });
}

std::optional<std::vector<std::uint8_t>> BuildDiagnosticsZip(
    const std::vector<DiagnosticsBundleEntry>& entries,
    std::string* error,
    std::size_t maximum_bytes) {
    if (entries.empty() || entries.size() > 16) {
        Fail(error, "Diagnostics bundle must contain between 1 and 16 entries.");
        return std::nullopt;
    }
    if (maximum_bytes < 1024 || maximum_bytes > 64u * 1024u * 1024u) {
        Fail(error, "Diagnostics bundle size limit is invalid.");
        return std::nullopt;
    }

    std::vector<std::string> names;
    names.reserve(entries.size());
    std::size_t content_total = 0;
    for (const auto& entry : entries) {
        if (!DiagnosticsBundleNameSafe(entry.name)) {
            Fail(error, "Diagnostics bundle contains an unsafe filename.");
            return std::nullopt;
        }
        std::string lower = entry.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (std::find(names.begin(), names.end(), lower) != names.end()) {
            Fail(error, "Diagnostics bundle filenames must be unique ignoring case.");
            return std::nullopt;
        }
        names.push_back(std::move(lower));
        if (entry.content.size() > 1024u * 1024u) {
            Fail(error, "A diagnostics bundle entry exceeds 1 MiB.");
            return std::nullopt;
        }
        if (content_total > maximum_bytes - entry.content.size()) {
            Fail(error, "Diagnostics bundle content exceeds the configured limit.");
            return std::nullopt;
        }
        content_total += entry.content.size();
    }

    std::vector<std::uint8_t> output;
    output.reserve(std::min<std::size_t>(maximum_bytes, content_total + entries.size() * 160u + 64u));
    std::vector<CentralRecord> central;
    central.reserve(entries.size());

    for (const auto& entry : entries) {
        if (output.size() > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
        const auto offset = static_cast<std::uint32_t>(output.size());
        const auto crc = Crc32(entry.content);
        const auto size = static_cast<std::uint32_t>(entry.content.size());
        const auto name_size = static_cast<std::uint16_t>(entry.name.size());

        Put32(output, 0x04034b50u); // local header
        Put16(output, 20); Put16(output, 0); Put16(output, 0); // version, flags, store
        Put16(output, 0); Put16(output, 0); // deterministic DOS time/date
        Put32(output, crc); Put32(output, size); Put32(output, size);
        Put16(output, name_size); Put16(output, 0);
        output.insert(output.end(), entry.name.begin(), entry.name.end());
        output.insert(output.end(), entry.content.begin(), entry.content.end());
        if (output.size() > maximum_bytes) {
            Fail(error, "Diagnostics ZIP exceeds the configured size limit.");
            return std::nullopt;
        }
        central.push_back({entry.name, crc, size, offset});
    }

    const auto central_offset = static_cast<std::uint32_t>(output.size());
    for (const auto& record : central) {
        Put32(output, 0x02014b50u); // central directory header
        Put16(output, 20); Put16(output, 20); Put16(output, 0); Put16(output, 0);
        Put16(output, 0); Put16(output, 0);
        Put32(output, record.crc); Put32(output, record.size); Put32(output, record.size);
        Put16(output, static_cast<std::uint16_t>(record.name.size()));
        Put16(output, 0); Put16(output, 0); Put16(output, 0); Put16(output, 0); Put32(output, 0);
        Put32(output, record.offset);
        output.insert(output.end(), record.name.begin(), record.name.end());
        if (output.size() > maximum_bytes) {
            Fail(error, "Diagnostics ZIP exceeds the configured size limit.");
            return std::nullopt;
        }
    }
    const auto central_size = static_cast<std::uint32_t>(output.size()) - central_offset;
    Put32(output, 0x06054b50u); // EOCD
    Put16(output, 0); Put16(output, 0);
    Put16(output, static_cast<std::uint16_t>(central.size()));
    Put16(output, static_cast<std::uint16_t>(central.size()));
    Put32(output, central_size); Put32(output, central_offset); Put16(output, 0);
    if (output.size() > maximum_bytes) {
        Fail(error, "Diagnostics ZIP exceeds the configured size limit.");
        return std::nullopt;
    }
    return output;
}

} // namespace webcom
