#pragma once

#include "update_model.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace webcom {

struct ReleaseRuntimeConfig {
    std::string stable_manifest_url;
    std::string beta_manifest_url;
    std::string publisher_sha256;
};

enum class UpdateInstallScope { Unknown, PerUser, PerMachine };

struct UpdateInstallerLaunchPolicy {
    bool elevate = false;
    std::string install_mode_argument;
    bool operator==(const UpdateInstallerLaunchPolicy&) const = default;
};

enum class UpdateCheckStatus {
    EndpointNotConfigured,
    InvalidConfiguration,
    ManifestRejected,
    WrongChannel,
    VersionPolicyRejected,
    PublisherMismatch,
    NoUpdate,
    UpdateAvailable
};

struct UpdateCheckEvaluation {
    UpdateCheckStatus status = UpdateCheckStatus::InvalidConfiguration;
    std::string message;
    std::optional<UpdateManifest> manifest;
};

bool IsTrustedPublisherFingerprint(std::string_view value);
std::string_view UpdateInstallScopeName(UpdateInstallScope scope);
std::optional<UpdateInstallerLaunchPolicy> InstallerLaunchPolicyForScope(UpdateInstallScope scope);
std::optional<std::string> ManifestUrlForChannel(const ReleaseRuntimeConfig& config,
                                                  ReleaseChannel channel);
UpdateCheckEvaluation EvaluateUpdateManifest(std::string_view current_version,
                                             ReleaseChannel selected_channel,
                                             const ReleaseRuntimeConfig& config,
                                             std::string_view manifest_text);
std::filesystem::path StagedUpdateDirectory(const std::filesystem::path& root,
                                            std::string_view version);
std::filesystem::path StagedUpdatePackagePath(const std::filesystem::path& root,
                                              std::string_view version);

} // namespace webcom
