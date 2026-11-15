#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

constexpr std::size_t kMaxAdBlockerSiteExceptions = 64;

std::string NormalizeAdBlockerHost(std::string_view host);
std::string AdBlockerHostFromUrl(std::string_view url);
std::vector<std::string> LoadAdBlockerSiteExceptions(const std::filesystem::path& root,
                                                      std::string_view profile);
bool SaveAdBlockerSiteExceptions(const std::filesystem::path& root, std::string_view profile,
                                 const std::vector<std::string>& hosts,
                                 std::string* error = nullptr);
bool ContainsAdBlockerSiteException(const std::vector<std::string>& hosts, std::string_view host);

// The bundled rule template carries an empty excludedInitiatorDomains array in
// every rule. Rendering replaces each marker with the same bounded allowlist.
std::optional<std::string> RenderAdBlockerNetworkRules(std::string_view template_json,
                                                        const std::vector<std::string>& hosts);
std::string BuildAdBlockerSiteConfigScript(const std::vector<std::string>& hosts);

} // namespace webcom
