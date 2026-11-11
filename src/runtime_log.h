#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace webcom {

enum class RuntimeLogLevel { Info, Warning, Error };

struct RuntimeSessionState {
    bool running = false;
    std::string version;
    unsigned long long sequence = 0;
};

bool AppendRuntimeLog(const std::filesystem::path& root, RuntimeLogLevel level,
                      std::string_view event, std::string_view detail = {},
                      std::string* error = nullptr);
std::string ReadRuntimeLogBundleText(const std::filesystem::path& root,
                                     std::size_t maximum_bytes = 768u * 1024u);
bool BeginRuntimeSession(const std::filesystem::path& root, std::string_view version,
                         bool* previous_unclean = nullptr, std::string* error = nullptr);
bool EndRuntimeSession(const std::filesystem::path& root, std::string_view version,
                       std::string* error = nullptr);
RuntimeSessionState LoadRuntimeSessionState(const std::filesystem::path& root);
std::string SanitizeRuntimeLogDetail(std::string_view detail);

} // namespace webcom
