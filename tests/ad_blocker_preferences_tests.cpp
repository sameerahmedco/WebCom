#include "ad_blocker_preferences.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace webcom;

namespace {
int checks = 0;
int failures = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::filesystem::path TempRoot() {
    const auto root = std::filesystem::temp_directory_path() / "webcom-ad-blocker-pref-tests";
    std::error_code ec; std::filesystem::remove_all(root, ec); std::filesystem::create_directories(root, ec);
    return root;
}
}

int main() {
    Check(NormalizeAdBlockerHost("WWW.Example.COM") == "www.example.com", "host normalization preserves exact www host");
    Check(NormalizeAdBlockerHost("example.com") == "example.com", "host normalization lowercases DNS host");
    Check(NormalizeAdBlockerHost("bad_host.example").empty(), "underscore host rejected");
    Check(NormalizeAdBlockerHost("-bad.example").empty(), "leading hyphen rejected");
    Check(NormalizeAdBlockerHost("bad-.example").empty(), "trailing hyphen rejected");
    Check(AdBlockerHostFromUrl("https://www.Example.com/path?q=1") == "www.example.com", "URL host extraction");
    Check(AdBlockerHostFromUrl("http://user@example.com:8080/path") == "example.com", "URL strips credentials and port");
    Check(AdBlockerHostFromUrl("file:///tmp/test").empty(), "non-web URL rejected");
    Check(AdBlockerHostFromUrl("https://[::1]/").empty(), "IPv6 host not persisted");

    const auto root = TempRoot();
    std::vector<std::string> hosts{"Example.com", "www.example.com", "example.com", "bad_host"};
    std::string error;
    Check(SaveAdBlockerSiteExceptions(root, "Default", hosts, &error), "save site exceptions");
    const auto loaded = LoadAdBlockerSiteExceptions(root, "Default");
    Check(loaded.size() == 2, "save/load deduplicates and rejects invalid hosts");
    Check(ContainsAdBlockerSiteException(loaded, "EXAMPLE.COM"), "contains normalizes host");
    Check(!ContainsAdBlockerSiteException(loaded, "other.example.com"), "exceptions remain host-specific in preference model");
    Check(!SaveAdBlockerSiteExceptions(root, "../bad", hosts, &error), "invalid profile rejected");

    std::vector<std::string> many;
    for (int i = 0; i < 80; ++i) many.push_back("site" + std::to_string(i) + ".example.com");
    Check(SaveAdBlockerSiteExceptions(root, "Default", many, &error), "bounded exception save");
    Check(LoadAdBlockerSiteExceptions(root, "Default").size() == kMaxAdBlockerSiteExceptions, "exception list bounded at 64");

    const std::string rule_template = R"([{"condition":{"excludedInitiatorDomains": ["webcom.invalid"]}},{"condition":{"excludedInitiatorDomains": ["webcom.invalid"]}}])";
    const auto rendered = RenderAdBlockerNetworkRules(rule_template, {"www.example.com", "news.example.org"});
    Check(rendered.has_value(), "network rule template rendered");
    Check(rendered && rendered->find("\"excludedInitiatorDomains\": [\"www.example.com\", \"news.example.org\"]") != std::string::npos,
          "network rules receive allowlist domains");
    Check(rendered && rendered->find("webcom.invalid") == std::string::npos, "all rule sentinels replaced when exceptions exist");
    Check(!RenderAdBlockerNetworkRules("[]", {"example.com"}).has_value(), "missing DNR marker fails closed");
    const auto empty_render = RenderAdBlockerNetworkRules(rule_template, {});
    Check(empty_render && empty_render->find("webcom.invalid") != std::string::npos, "zero-exception rules retain harmless reserved sentinel");

    std::ifstream shipped_rules("extensions/webcom-ad-blocker/network-rules.json", std::ios::binary);
    if (!shipped_rules) shipped_rules.open("../extensions/webcom-ad-blocker/network-rules.json", std::ios::binary);
    const std::string shipped_template((std::istreambuf_iterator<char>(shipped_rules)), std::istreambuf_iterator<char>());
    const auto shipped_render = RenderAdBlockerNetworkRules(shipped_template, {"www.example.com"});
    Check(shipped_render && shipped_render->find("www.example.com") != std::string::npos, "shipped DNR template accepts profile rendering");

    const auto script = BuildAdBlockerSiteConfigScript({"www.example.com", "news.example.org"});
    Check(script.find("www.example.com") != std::string::npos && script.find("news.example.org") != std::string::npos,
          "site config embeds allowlist");
    Check(script.find("__webcomAdBlockerDisabled") != std::string::npos, "site config exports MAIN-world pause flag");
    Check(script.find("data-webcom-adblock-off") != std::string::npos, "site config marks document for cosmetic bypass");

    std::error_code ec; std::filesystem::remove_all(root, ec);
    std::cout << (checks - failures) << " ad blocker preference checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
