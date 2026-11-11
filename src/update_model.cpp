#include "update_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>
#include <vector>

namespace webcom {
namespace {

constexpr std::string_view kHeaderV1 = "WEBCOM_UPDATES\t1";
constexpr std::string_view kHeaderV2 = "WEBCOM_UPDATES\t2";
constexpr std::string_view kTransactionHeader = "WEBCOM_UPDATE_STATE\t1";
constexpr std::size_t kMaxFile = 64 * 1024;

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}

std::string Lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool IsHex64(std::string_view value) {
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

std::vector<int> NumericVersion(std::string_view value) {
    std::vector<int> out;
    std::size_t i = 0;
    while (i < value.size() && out.size() < 4) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return {};
        int number = 0;
        while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) {
            number = number * 10 + (value[i] - '0');
            if (number > 9999) return {};
            ++i;
        }
        out.push_back(number);
        if (i == value.size() || value[i] == '-') break;
        if (value[i] != '.') return {};
        ++i;
    }
    return out;
}

bool IsHttps(std::string_view value) {
    return value.size() > 8 && Lower(value.substr(0, 8)) == "https://";
}

bool ParseUint64(std::string_view value, std::uint64_t& out) {
    if (value.empty() || value.size() > 20) return false;
    std::uint64_t result = 0;
    for (unsigned char c : value) {
        if (!std::isdigit(c)) return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    out = result;
    return true;
}

} // namespace

std::string ReleaseChannelName(ReleaseChannel channel) {
    return channel == ReleaseChannel::Beta ? "beta" : "stable";
}

std::optional<ReleaseChannel> ParseReleaseChannel(std::string_view value) {
    const auto lower = Lower(value);
    if (lower == "stable") return ReleaseChannel::Stable;
    if (lower == "beta") return ReleaseChannel::Beta;
    return std::nullopt;
}

bool IsValidReleaseVersion(std::string_view value) {
    if (value.empty() || value.size() > 48) return false;
    const auto dash = value.find('-');
    const auto numeric = value.substr(0, dash);
    const auto parts = NumericVersion(numeric);
    if (parts.size() != 3) return false;
    if (dash == std::string_view::npos) return true;
    const auto suffix = value.substr(dash + 1);
    if (suffix.empty() || suffix.size() > 24) return false;
    return std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-';
    });
}

bool IsPrereleaseReleaseVersion(std::string_view value) {
    return IsValidReleaseVersion(value) && value.find('-') != std::string_view::npos;
}

bool IsReleaseVersionAllowedOnChannel(std::string_view value, ReleaseChannel channel) {
    if (!IsValidReleaseVersion(value)) return false;
    // Stable manifests must never advertise a prerelease build. Beta may carry
    // prereleases and final releases so beta testers can move onto a final build
    // without changing channels first.
    return channel == ReleaseChannel::Beta || !IsPrereleaseReleaseVersion(value);
}

bool IsNewerReleaseVersion(std::string_view current, std::string_view candidate) {
    if (!IsValidReleaseVersion(current) || !IsValidReleaseVersion(candidate)) return false;
    const auto current_parts = NumericVersion(current);
    const auto candidate_parts = NumericVersion(candidate);
    for (std::size_t i = 0; i < 3; ++i) {
        if (candidate_parts[i] != current_parts[i]) return candidate_parts[i] > current_parts[i];
    }
    const bool current_prerelease = current.find('-') != std::string_view::npos;
    const bool candidate_prerelease = candidate.find('-') != std::string_view::npos;
    if (current_prerelease != candidate_prerelease) return !candidate_prerelease;
    return candidate > current;
}

std::optional<UpdateManifest> ParseUpdateManifest(std::string_view text, std::string* error) {
    if (text.empty() || text.size() > kMaxFile) { Fail(error, "Update manifest is empty or too large."); return std::nullopt; }
    UpdateManifest result;
    bool header = false, version = false, channel = false, url = false, sha = false, published = false;
    bool size = false, signature = false, publisher = false;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (!header) {
            if (line == "WEBCOM_RELEASE\t1") result.schema_version = 1;
            else if (line == "WEBCOM_RELEASE\t2") result.schema_version = 2;
            else { Fail(error, "Unsupported update manifest header."); return std::nullopt; }
            header = true;
        } else if (!line.empty()) {
            const auto tab = line.find('\t');
            if (tab == std::string_view::npos) { Fail(error, "Malformed update manifest record."); return std::nullopt; }
            const auto key = line.substr(0, tab);
            const auto value = line.substr(tab + 1);
            if (key == "version" && !version) { result.version = std::string(value); version = true; }
            else if (key == "channel" && !channel) {
                const auto parsed = ParseReleaseChannel(value); if (!parsed) { Fail(error, "Invalid release channel."); return std::nullopt; }
                result.channel = *parsed; channel = true;
            } else if (key == "url" && !url) { result.url = std::string(value); url = true; }
            else if (key == "sha256" && !sha) { result.sha256 = std::string(value); sha = true; }
            else if (key == "published" && !published) { result.published = std::string(value); published = true; }
            else if (result.schema_version >= 2 && key == "size" && !size) {
                if (!ParseUint64(value, result.size_bytes)) { Fail(error, "Invalid package size."); return std::nullopt; }
                size = true;
            } else if (result.schema_version >= 2 && key == "signature" && !signature) {
                if (Lower(value) != "authenticode") { Fail(error, "Unsupported package signature type."); return std::nullopt; }
                result.signature = PackageSignatureKind::Authenticode; signature = true;
            } else if (result.schema_version >= 2 && key == "publisher_sha256" && !publisher) {
                result.publisher_sha256 = std::string(value); publisher = true;
            } else { Fail(error, "Unknown or duplicate update manifest field."); return std::nullopt; }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (!header || !version || !channel || !url || !sha || !published) { Fail(error, "Update manifest is incomplete."); return std::nullopt; }
    if (result.schema_version >= 2 && (!size || !signature || !publisher)) { Fail(error, "Signed update manifest is incomplete."); return std::nullopt; }
    if (!IsValidReleaseVersion(result.version)) { Fail(error, "Invalid release version."); return std::nullopt; }
    if (!IsHttps(result.url) || result.url.find_first_of("\r\n\t ") != std::string::npos) { Fail(error, "Update URL must be a compact HTTPS URL."); return std::nullopt; }
    if (!IsHex64(result.sha256)) { Fail(error, "Invalid SHA-256 digest."); return std::nullopt; }
    if (result.published.empty() || result.published.size() > 64 || result.published.find_first_of("\r\n\t") != std::string::npos) {
        Fail(error, "Invalid publication timestamp."); return std::nullopt;
    }
    if (result.schema_version >= 2) {
        if (result.size_bytes == 0 || result.size_bytes > 4ull * 1024ull * 1024ull * 1024ull) {
            Fail(error, "Package size is outside WebCom update limits."); return std::nullopt;
        }
        if (!IsHex64(result.publisher_sha256)) { Fail(error, "Invalid publisher certificate SHA-256 fingerprint."); return std::nullopt; }
    }
    return result;
}

bool IsInstallEligibleUpdateManifest(const UpdateManifest& manifest) {
    return manifest.schema_version >= 2 && manifest.size_bytes > 0 &&
           manifest.signature == PackageSignatureKind::Authenticode &&
           IsHex64(manifest.publisher_sha256) && IsHex64(manifest.sha256) && IsHttps(manifest.url) &&
           IsReleaseVersionAllowedOnChannel(manifest.version, manifest.channel);
}


std::string UpdateTransactionPhaseName(UpdateTransactionPhase phase) {
    switch (phase) {
    case UpdateTransactionPhase::VerifiedPackage: return "verified";
    case UpdateTransactionPhase::InstallScheduled: return "install-scheduled";
    case UpdateTransactionPhase::AwaitingHealthCheck: return "awaiting-health-check";
    default: return "none";
    }
}

static std::optional<UpdateTransactionPhase> ParseUpdateTransactionPhase(std::string_view value) {
    const auto lower = Lower(value);
    if (lower == "none") return UpdateTransactionPhase::None;
    if (lower == "verified") return UpdateTransactionPhase::VerifiedPackage;
    if (lower == "install-scheduled") return UpdateTransactionPhase::InstallScheduled;
    if (lower == "awaiting-health-check") return UpdateTransactionPhase::AwaitingHealthCheck;
    return std::nullopt;
}

bool StageVerifiedUpdate(UpdateTransaction& state, const UpdateManifest& manifest,
                         std::string_view current_version, std::string* error) {
    if (!IsInstallEligibleUpdateManifest(manifest)) return Fail(error, "Update manifest is not install eligible.");
    if (!IsValidReleaseVersion(current_version)) return Fail(error, "Current WebCom version is invalid.");
    if (!IsNewerReleaseVersion(current_version, manifest.version)) return Fail(error, "Update is not newer than the running version.");
    UpdateTransaction next;
    next.phase = UpdateTransactionPhase::VerifiedPackage;
    next.target_version = manifest.version;
    next.previous_version = std::string(current_version);
    next.package_sha256 = Lower(manifest.sha256);
    next.package_size = manifest.size_bytes;
    state = std::move(next);
    return true;
}

bool MarkUpdateInstallScheduled(UpdateTransaction& state, std::string* error) {
    if (state.phase != UpdateTransactionPhase::VerifiedPackage) return Fail(error, "Only a verified package can be scheduled for install.");
    state.phase = UpdateTransactionPhase::InstallScheduled;
    return true;
}

bool MarkUpdateAwaitingHealthCheck(UpdateTransaction& state, std::string* error) {
    if (state.phase != UpdateTransactionPhase::InstallScheduled) return Fail(error, "Install must be scheduled before health checking.");
    state.phase = UpdateTransactionPhase::AwaitingHealthCheck;
    state.failed_health_starts = 0;
    return true;
}

bool MarkUpdateHealthy(UpdateTransaction& state, std::string_view running_version, std::string* error) {
    if (state.phase != UpdateTransactionPhase::AwaitingHealthCheck) return Fail(error, "No update is awaiting a health check.");
    if (running_version != state.target_version) return Fail(error, "Running version does not match the staged update target.");
    state = {};
    return true;
}

bool RecordUpdateHealthFailure(UpdateTransaction& state, std::string* error) {
    if (state.phase != UpdateTransactionPhase::AwaitingHealthCheck) return Fail(error, "No update is awaiting a health check.");
    if (state.failed_health_starts < 1000u) ++state.failed_health_starts;
    return true;
}

bool UpdateRollbackRecommended(const UpdateTransaction& state) {
    return state.phase == UpdateTransactionPhase::AwaitingHealthCheck &&
           state.failed_health_starts >= 2u && !state.previous_version.empty();
}

UpdateTransaction LoadUpdateTransaction(const std::filesystem::path& root) {
    const auto path = root / "update-state.v1";
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (text.empty() || text.size() > 8192) return {};
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string::npos ? text.size() - start : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(std::move(line));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (lines.empty() || lines.front() != kTransactionHeader) return {};
    UpdateTransaction result; bool phase=false,target=false,previous=false,sha=false,size=false,failures=false;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto tab = lines[i].find('\t'); if (tab == std::string::npos) return {};
        const auto key = std::string_view(lines[i]).substr(0, tab);
        const auto value = std::string_view(lines[i]).substr(tab + 1);
        if (key == "phase" && !phase) { const auto parsed=ParseUpdateTransactionPhase(value); if(!parsed) return {}; result.phase=*parsed; phase=true; }
        else if (key == "target" && !target) { result.target_version=std::string(value); target=true; }
        else if (key == "previous" && !previous) { result.previous_version=std::string(value); previous=true; }
        else if (key == "sha256" && !sha) { result.package_sha256=std::string(value); sha=true; }
        else if (key == "size" && !size) { if(!ParseUint64(value,result.package_size)) return {}; size=true; }
        else if (key == "failures" && !failures) { std::uint64_t n=0; if(!ParseUint64(value,n) || n>1000) return {}; result.failed_health_starts=static_cast<unsigned>(n); failures=true; }
        else return {};
    }
    if (!phase || !target || !previous || !sha || !size || !failures) return {};
    if (result.phase == UpdateTransactionPhase::None) {
        if (!result.target_version.empty() || !result.previous_version.empty() || !result.package_sha256.empty() || result.package_size != 0 || result.failed_health_starts != 0) return {};
        return result;
    }
    if (!IsValidReleaseVersion(result.target_version) || !IsValidReleaseVersion(result.previous_version) ||
        !IsHex64(result.package_sha256) || result.package_size == 0 || result.package_size > 4ull*1024ull*1024ull*1024ull) return {};
    if (result.phase != UpdateTransactionPhase::AwaitingHealthCheck && result.failed_health_starts != 0) return {};
    return result;
}

bool SaveUpdateTransaction(const std::filesystem::path& root, const UpdateTransaction& state, std::string* error) {
    if (state.phase == UpdateTransactionPhase::None) {
        if (!state.target_version.empty() || !state.previous_version.empty() || !state.package_sha256.empty() || state.package_size != 0 || state.failed_health_starts != 0)
            return Fail(error, "Idle update state must not retain staged package metadata.");
    } else {
        if (!IsValidReleaseVersion(state.target_version) || !IsValidReleaseVersion(state.previous_version) ||
            !IsHex64(state.package_sha256) || state.package_size == 0 || state.package_size > 4ull*1024ull*1024ull*1024ull)
            return Fail(error, "Update transaction contains invalid package metadata.");
        if (state.phase != UpdateTransactionPhase::AwaitingHealthCheck && state.failed_health_starts != 0)
            return Fail(error, "Only health-check state may retain failed-start counts.");
    }
    std::error_code ec; std::filesystem::create_directories(root, ec);
    if (ec) return Fail(error, "Could not create the WebCom data directory.");
    const auto path=root/"update-state.v1"; const auto temp=root/"update-state.v1.tmp";
    {
        std::ofstream output(temp,std::ios::binary|std::ios::trunc);
        if(!output) return Fail(error,"Could not write update transaction state.");
        output << kTransactionHeader << "\n"
               << "phase\t" << UpdateTransactionPhaseName(state.phase) << "\n"
               << "target\t" << state.target_version << "\n"
               << "previous\t" << state.previous_version << "\n"
               << "sha256\t" << Lower(state.package_sha256) << "\n"
               << "size\t" << state.package_size << "\n"
               << "failures\t" << state.failed_health_starts << "\n";
        if(!output) return Fail(error,"Could not finish update transaction state.");
    }
    std::filesystem::rename(temp,path,ec);
    if(ec){std::filesystem::remove(path,ec);ec.clear();std::filesystem::rename(temp,path,ec);}
    if(ec){std::filesystem::remove(temp,ec);return Fail(error,"Could not replace update transaction state.");}
    return true;
}

UpdateSettings LoadUpdateSettings(const std::filesystem::path& root) {
    const auto path = root / "updates.v1";
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (text.empty() || text.size() > 4096) return {};
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string::npos ? text.size() - start : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(std::move(line));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (lines.empty()) return {};
    const bool v1 = lines[0] == kHeaderV1; const bool v2 = lines[0] == kHeaderV2;
    if (!v1 && !v2) return {};
    UpdateSettings result; bool channel=false,checks=false;
    for(std::size_t i=1;i<lines.size();++i){
        const auto tab=lines[i].find('\t'); if(tab==std::string::npos)return {};
        const auto key=std::string_view(lines[i]).substr(0,tab); const auto value=std::string_view(lines[i]).substr(tab+1);
        if(key=="channel"&&!channel){const auto parsed=ParseReleaseChannel(value);if(!parsed)return {};result.channel=*parsed;channel=true;}
        else if(v2&&key=="checks_enabled"&&!checks){if(value=="1")result.checks_enabled=true;else if(value=="0")result.checks_enabled=false;else return {};checks=true;}
        else return {};
    }
    if(!channel || (v2&&!checks)) return {};
    return result;
}

bool SaveUpdateSettings(const std::filesystem::path& root, const UpdateSettings& settings, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return Fail(error, "Could not create the WebCom data directory.");
    const auto path = root / "updates.v1";
    const auto temp = root / "updates.v1.tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return Fail(error, "Could not write update settings.");
        output << kHeaderV2 << "\nchannel\t" << ReleaseChannelName(settings.channel)
               << "\nchecks_enabled\t" << (settings.checks_enabled ? "1" : "0") << "\n";
        if (!output) return Fail(error, "Could not finish update settings.");
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) { std::filesystem::remove(path, ec); ec.clear(); std::filesystem::rename(temp, path, ec); }
    if (ec) { std::filesystem::remove(temp, ec); return Fail(error, "Could not replace update settings."); }
    return true;
}

} // namespace webcom
