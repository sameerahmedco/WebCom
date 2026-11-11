#pragma once

#ifdef _WIN32

#include "update_model.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace webcom {

std::optional<std::string> FetchUpdateManifestHttps(std::string_view url,
                                                    std::size_t maximum_bytes,
                                                    std::string* error = nullptr);

bool VerifyStagedUpdatePackage(const std::filesystem::path& package,
                               const UpdateManifest& manifest,
                               std::string_view pinned_publisher_sha256,
                               std::string* error = nullptr);

bool DownloadAndVerifyUpdatePackage(const UpdateManifest& manifest,
                                    std::string_view pinned_publisher_sha256,
                                    const std::filesystem::path& destination,
                                    std::string* error = nullptr);

} // namespace webcom

#endif
