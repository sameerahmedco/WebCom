#include "update_runtime_model.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace webcom {
namespace {

std::string Lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool IsCompactHttpsUrl(std::string_view value) {
    if (value.size() < 9 || value.size() > 2048) return false;
    if (Lower(value.substr(0, 8)) != "https://") return false;
    return value.find_first_of("\r\n\t ") == std::string_view::npos;
}

} // namespace

bool IsTrustedPublisherFingerprint(std::string_view value) {
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

std::optional<std::string> ManifestUrlForChannel(const ReleaseRuntimeConfig& config,
                                                  ReleaseChannel channel) {
    const std::string& value = channel == ReleaseChannel::Beta
        ? config.beta_manifest_url : config.stable_manifest_url;
    if (value.empty() || !IsCompactHttpsUrl(value)) return std::nullopt;
    return value;
}

UpdateCheckEvaluation EvaluateUpdateManifest(std::string_view current_version,
                                             ReleaseChannel selected_channel,
                                             const ReleaseRuntimeConfig& config,
                                             std::string_view manifest_text) {
    UpdateCheckEvaluation result;
    if (!IsTrustedPublisherFingerprint(config.publisher_sha256)) {
        result.status = UpdateCheckStatus::InvalidConfiguration;
        result.message = "This build does not contain a valid pinned WebCom publisher fingerprint.";
        return result;
    }
    if (!ManifestUrlForChannel(config, selected_channel)) {
        result.status = UpdateCheckStatus::EndpointNotConfigured;
        result.message = "This build does not contain an HTTPS manifest endpoint for the selected release channel.";
        return result;
    }
    std::string parse_error;
    auto manifest = ParseUpdateManifest(manifest_text, &parse_error);
    if (!manifest) {
        result.status = UpdateCheckStatus::ManifestRejected;
        result.message = parse_error.empty() ? "The release manifest could not be parsed." : parse_error;
        return result;
    }
    if (manifest->channel != selected_channel) {
        result.status = UpdateCheckStatus::WrongChannel;
        result.message = "The release manifest channel does not match the selected WebCom channel.";
        return result;
    }
    if (!IsReleaseVersionAllowedOnChannel(manifest->version, selected_channel)) {
        result.status = UpdateCheckStatus::VersionPolicyRejected;
        result.message = "The release version is not permitted on the selected WebCom channel.";
        return result;
    }
    if (!IsInstallEligibleUpdateManifest(*manifest)) {
        result.status = UpdateCheckStatus::ManifestRejected;
        result.message = "The release manifest is not install eligible.";
        return result;
    }
    if (Lower(manifest->publisher_sha256) != Lower(config.publisher_sha256)) {
        result.status = UpdateCheckStatus::PublisherMismatch;
        result.message = "The release manifest publisher fingerprint does not match this WebCom build.";
        return result;
    }
    if (!IsValidReleaseVersion(current_version)) {
        result.status = UpdateCheckStatus::InvalidConfiguration;
        result.message = "The running WebCom version is not valid for update comparison.";
        return result;
    }
    if (!IsNewerReleaseVersion(current_version, manifest->version)) {
        result.status = UpdateCheckStatus::NoUpdate;
        result.message = "WebCom is already on this release or a newer one.";
        result.manifest = std::move(manifest);
        return result;
    }
    result.status = UpdateCheckStatus::UpdateAvailable;
    result.message = "A newer signed WebCom release is available.";
    result.manifest = std::move(manifest);
    return result;
}

std::filesystem::path StagedUpdateDirectory(const std::filesystem::path& root,
                                            std::string_view version) {
    if (!IsValidReleaseVersion(version)) return {};
    return root / "Updates" / "staging" / std::string(version);
}

std::filesystem::path StagedUpdatePackagePath(const std::filesystem::path& root,
                                              std::string_view version) {
    const auto directory = StagedUpdateDirectory(root, version);
    if (directory.empty()) return {};
    return directory / ("WebCom-Setup-" + std::string(version) + "-x64.exe");
}

} // namespace webcom
