#pragma once

#include "core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

constexpr std::size_t kMaxNeverSleepHosts = 64;

std::optional<SleepTimeout> LoadSleepTimeoutPreference(const std::filesystem::path& root,
                                                       std::string_view profile);
bool SaveSleepTimeoutPreference(const std::filesystem::path& root, std::string_view profile,
                                SleepTimeout timeout, std::string* error = nullptr);

std::string NormalizeNeverSleepHost(std::string_view host);
std::vector<std::string> LoadNeverSleepHostsPreference(const std::filesystem::path& root,
                                                       std::string_view profile);
bool SaveNeverSleepHostsPreference(const std::filesystem::path& root, std::string_view profile,
                                   SleepTimeout timeout, const std::vector<std::string>& hosts,
                                   std::string* error = nullptr);
bool ContainsNeverSleepHost(const std::vector<std::string>& hosts, std::string_view host);

} // namespace webcom
