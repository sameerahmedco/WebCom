#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

enum class SitePermissionKind { Camera, Microphone, Geolocation, Notifications };
enum class SitePermissionDecision { Ask, Allow, Block };

struct SitePermissionRule {
    std::string origin;
    SitePermissionKind kind = SitePermissionKind::Camera;
    SitePermissionDecision decision = SitePermissionDecision::Ask;
};

std::string SiteOrigin(std::string_view url);
std::string SitePermissionKindName(SitePermissionKind kind);
std::string SitePermissionDecisionName(SitePermissionDecision decision);
SitePermissionDecision NextSitePermissionDecision(SitePermissionDecision decision);
SitePermissionDecision SitePermissionFor(const std::vector<SitePermissionRule>& rules,
                                         std::string_view origin,
                                         SitePermissionKind kind);
void SetSitePermission(std::vector<SitePermissionRule>& rules,
                       std::string origin,
                       SitePermissionKind kind,
                       SitePermissionDecision decision);
void ResetSitePermissions(std::vector<SitePermissionRule>& rules, std::string_view origin);
std::vector<SitePermissionRule> LoadSitePermissions(const std::filesystem::path& root,
                                                    std::string_view profile);
bool SaveSitePermissions(const std::filesystem::path& root,
                         std::string_view profile,
                         const std::vector<SitePermissionRule>& rules,
                         std::string* error = nullptr);

} // namespace webcom
