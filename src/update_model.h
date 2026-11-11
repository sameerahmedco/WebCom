#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace webcom {

enum class ReleaseChannel { Stable, Beta };
enum class PackageSignatureKind { None, Authenticode };

struct UpdateSettings {
    ReleaseChannel channel = ReleaseChannel::Stable;
    // Update checks are opt-in. WebCom never downloads or installs merely
    // because a release channel is selected.
    bool checks_enabled = false;
    bool operator==(const UpdateSettings&) const = default;
};

enum class UpdateTransactionPhase { None, VerifiedPackage, InstallScheduled, AwaitingHealthCheck };

struct UpdateTransaction {
    UpdateTransactionPhase phase = UpdateTransactionPhase::None;
    std::string target_version;
    std::string previous_version;
    std::string package_sha256;
    std::uint64_t package_size = 0;
    unsigned failed_health_starts = 0;
    bool operator==(const UpdateTransaction&) const = default;
};

struct UpdateManifest {
    unsigned schema_version = 1;
    std::string version;
    ReleaseChannel channel = ReleaseChannel::Stable;
    std::string url;
    std::string sha256;
    std::string published;
    std::uint64_t size_bytes = 0;
    PackageSignatureKind signature = PackageSignatureKind::None;
    std::string publisher_sha256;
};

std::string ReleaseChannelName(ReleaseChannel channel);
std::optional<ReleaseChannel> ParseReleaseChannel(std::string_view value);
bool IsValidReleaseVersion(std::string_view value);
bool IsPrereleaseReleaseVersion(std::string_view value);
bool IsReleaseVersionAllowedOnChannel(std::string_view value, ReleaseChannel channel);
bool IsNewerReleaseVersion(std::string_view current, std::string_view candidate);
std::optional<UpdateManifest> ParseUpdateManifest(std::string_view text, std::string* error = nullptr);
bool IsInstallEligibleUpdateManifest(const UpdateManifest& manifest);
std::string UpdateTransactionPhaseName(UpdateTransactionPhase phase);
bool StageVerifiedUpdate(UpdateTransaction& state, const UpdateManifest& manifest,
                         std::string_view current_version, std::string* error = nullptr);
bool MarkUpdateInstallScheduled(UpdateTransaction& state, std::string* error = nullptr);
bool MarkUpdateAwaitingHealthCheck(UpdateTransaction& state, std::string* error = nullptr);
bool MarkUpdateHealthy(UpdateTransaction& state, std::string_view running_version,
                       std::string* error = nullptr);
bool RecordUpdateHealthFailure(UpdateTransaction& state, std::string* error = nullptr);
bool UpdateRollbackRecommended(const UpdateTransaction& state);
UpdateTransaction LoadUpdateTransaction(const std::filesystem::path& root);
bool SaveUpdateTransaction(const std::filesystem::path& root, const UpdateTransaction& state,
                           std::string* error = nullptr);
UpdateSettings LoadUpdateSettings(const std::filesystem::path& root);
bool SaveUpdateSettings(const std::filesystem::path& root, const UpdateSettings& settings,
                        std::string* error = nullptr);

} // namespace webcom
