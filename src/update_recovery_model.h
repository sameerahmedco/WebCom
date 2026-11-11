#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "update_model.h"

namespace webcom {

struct RecoveryPackage {
    std::string version;
    std::string sha256;
    std::uint64_t size_bytes = 0;
    std::string publisher_sha256;
    bool operator==(const RecoveryPackage&) const = default;
};

enum class RecoveryReadiness {
    Ready,
    RollbackNotRecommended,
    RunningVersionMismatch,
    PackageMissing,
    PackageVersionMismatch,
    PublisherMismatch,
    MetadataInvalid
};

struct RecoveryEvaluation {
    RecoveryReadiness status = RecoveryReadiness::RollbackNotRecommended;
    std::string message;
    bool ready() const { return status == RecoveryReadiness::Ready; }
};

std::string RecoveryReadinessName(RecoveryReadiness status);
RecoveryEvaluation EvaluateRecoveryReadiness(const UpdateTransaction& transaction,
                                             const std::optional<RecoveryPackage>& package,
                                             std::string_view running_version,
                                             std::string_view pinned_publisher_sha256);

std::filesystem::path RecoveryPackageDirectory(const std::filesystem::path& root,
                                               std::string_view version);
std::filesystem::path RecoveryPackagePath(const std::filesystem::path& root,
                                          std::string_view version);
std::optional<RecoveryPackage> LoadRecoveryPackage(const std::filesystem::path& root);
bool PreserveRecoveryPackage(const std::filesystem::path& root, const RecoveryPackage& metadata,
                             const std::filesystem::path& verified_installer,
                             std::string* error = nullptr);
bool ClearRecoveryPackage(const std::filesystem::path& root, std::string* error = nullptr);

} // namespace webcom
