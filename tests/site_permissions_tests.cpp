#include "site_permissions.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
int checks = 0;
void expect(bool value, const char* label) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
}

int main() {
    using namespace webcom;
    expect(SiteOrigin("https://Example.COM/path?q=1") == "https://example.com", "https origin normalized");
    expect(SiteOrigin("http://Example.COM:80/a") == "http://example.com", "default http port removed");
    expect(SiteOrigin("https://Example.COM:443/a") == "https://example.com", "default https port removed");
    expect(SiteOrigin("https://user:pass@Example.com:8443/a") == "https://example.com:8443", "userinfo removed");
    expect(SiteOrigin("file:///tmp/test").empty(), "non-web origin rejected");

    std::vector<SitePermissionRule> rules;
    expect(SitePermissionFor(rules, "https://example.com/a", SitePermissionKind::Camera) == SitePermissionDecision::Ask,
           "missing rule asks");
    SetSitePermission(rules, "https://example.com/page", SitePermissionKind::Camera, SitePermissionDecision::Allow);
    expect(rules.size() == 1, "allow rule stored");
    expect(SitePermissionFor(rules, "https://EXAMPLE.com/else", SitePermissionKind::Camera) == SitePermissionDecision::Allow,
           "rule matches normalized origin");
    SetSitePermission(rules, "https://example.com", SitePermissionKind::Camera, SitePermissionDecision::Block);
    expect(rules.size() == 1 && rules[0].decision == SitePermissionDecision::Block, "rule replaced");
    SetSitePermission(rules, "https://example.com", SitePermissionKind::Camera, SitePermissionDecision::Ask);
    expect(rules.empty(), "ask removes persistent override");

    expect(NextSitePermissionDecision(SitePermissionDecision::Ask) == SitePermissionDecision::Allow, "ask cycles allow");
    expect(NextSitePermissionDecision(SitePermissionDecision::Allow) == SitePermissionDecision::Block, "allow cycles block");
    expect(NextSitePermissionDecision(SitePermissionDecision::Block) == SitePermissionDecision::Ask, "block cycles ask");

    const auto root = std::filesystem::temp_directory_path() / "webcom-site-permissions-test";
    std::error_code ec; std::filesystem::remove_all(root, ec);
    rules = {{"https://example.com", SitePermissionKind::Camera, SitePermissionDecision::Allow},
             {"https://example.com", SitePermissionKind::Notifications, SitePermissionDecision::Block},
             {"https://maps.example", SitePermissionKind::Geolocation, SitePermissionDecision::Allow}};
    std::string error;
    expect(SaveSitePermissions(root, "Work", rules, &error), "rules saved");
    const auto loaded = LoadSitePermissions(root, "Work");
    expect(loaded.size() == 3, "rules loaded");
    expect(SitePermissionFor(loaded, "https://example.com", SitePermissionKind::Notifications) == SitePermissionDecision::Block,
           "block persisted");
    ResetSitePermissions(rules, "https://example.com/path");
    expect(rules.size() == 1 && rules.front().origin == "https://maps.example", "origin reset only");
    std::filesystem::remove_all(root, ec);

    std::cout << "site_permissions_tests: " << checks << " checks passed\n";
    return 0;
}
