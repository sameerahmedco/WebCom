#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct NamedSessionTab {
    std::string url;
    std::string title;
    bool pinned = false;
    std::string workspace = "Main";
};

struct NamedSession {
    std::string name;
    std::uint64_t created_ms = 0;
    std::vector<NamedSessionTab> tabs;
};

std::vector<NamedSession> LoadNamedSessions(const std::filesystem::path& root,
                                            std::string_view profile);
bool SaveNamedSessions(const std::filesystem::path& root,
                       std::string_view profile,
                       const std::vector<NamedSession>& sessions,
                       std::string* error = nullptr);
bool UpsertNamedSession(std::vector<NamedSession>& sessions,
                        NamedSession session,
                        std::string* error = nullptr);
bool DeleteNamedSession(std::vector<NamedSession>& sessions,
                        std::string_view name);

} // namespace webcom
