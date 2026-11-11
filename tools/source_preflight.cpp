#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
struct Finding {
    fs::path file;
    std::size_t line = 0;
    std::string message;
};

std::string ReadAll(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::size_t LineFor(std::string_view text, std::size_t offset) {
    return 1 + static_cast<std::size_t>(std::count(text.begin(), text.begin() + std::min(offset, text.size()), '\n'));
}

bool SourceFile(const fs::path& path) {
    const auto extension = path.extension().string();
    return extension == ".cpp" || extension == ".h" || extension == ".inl";
}

bool HasMixedAutoDeclarator(std::string_view line) {
    static const std::regex prefix(R"(^\s*(?:const\s+)?auto\s+[A-Za-z_][A-Za-z0-9_]*\s*=)");
    std::cmatch match;
    const std::string copy(line);
    if (!std::regex_search(copy.c_str(), match, prefix)) return false;
    std::size_t i = static_cast<std::size_t>(match.position() + match.length());
    int paren = 0, bracket = 0, brace = 0;
    bool string = false, character = false, escape = false;
    for (; i < copy.size(); ++i) {
        const char c = copy[i];
        if (escape) { escape = false; continue; }
        if ((string || character) && c == '\\') { escape = true; continue; }
        if (string) { if (c == '"') string = false; continue; }
        if (character) { if (c == '\'') character = false; continue; }
        if (c == '"') { string = true; continue; }
        if (c == '\'') { character = true; continue; }
        if (c == '/' && i + 1 < copy.size() && copy[i + 1] == '/') break;
        if (c == '(') ++paren;
        else if (c == ')') paren = std::max(0, paren - 1);
        else if (c == '[') ++bracket;
        else if (c == ']') bracket = std::max(0, bracket - 1);
        else if (c == '{') ++brace;
        else if (c == '}') brace = std::max(0, brace - 1);
        else if (c == ';' && paren == 0 && bracket == 0 && brace == 0) break;
        else if (c == ',' && paren == 0 && bracket == 0 && brace == 0) {
            std::size_t j = i + 1;
            while (j < copy.size() && std::isspace(static_cast<unsigned char>(copy[j]))) ++j;
            if (j >= copy.size() || !(std::isalpha(static_cast<unsigned char>(copy[j])) || copy[j] == '_')) continue;
            ++j;
            while (j < copy.size() && (std::isalnum(static_cast<unsigned char>(copy[j])) || copy[j] == '_')) ++j;
            while (j < copy.size() && std::isspace(static_cast<unsigned char>(copy[j]))) ++j;
            if (j < copy.size() && copy[j] == '=') return true;
        }
    }
    return false;
}

void AddLineFindings(const fs::path& file, std::string_view text, std::vector<Finding>& findings) {
    static const std::regex invalid_auto(R"(\bauto\s+(?:char|wchar_t|int|long|short|float|double|bool|unsigned|signed|const|volatile)\b)");
    std::istringstream lines{std::string(text)};
    std::string line;
    std::size_t number = 0;
    while (std::getline(lines, line)) {
        ++number;
        if (std::regex_search(line, invalid_auto))
            findings.push_back({file, number, "invalid 'auto' combined with another built-in type specifier"});
        if (HasMixedAutoDeclarator(line))
            findings.push_back({file, number, "multiple declarators in one auto statement; split them so MSVC never has to deduce different types"});
        for (const std::string_view name : {std::string_view("std::min("), std::string_view("std::max(")}) {
            std::size_t position = 0;
            while ((position = line.find(name, position)) != std::string::npos) {
                const auto open = position + name.size() - 1;
                int depth = 0;
                std::size_t close = std::string::npos;
                for (std::size_t i = open; i < line.size(); ++i) {
                    if (line[i] == '(') ++depth;
                    else if (line[i] == ')' && --depth == 0) { close = i; break; }
                }
                if (close == std::string::npos) break;
                const std::string call = line.substr(position, close - position + 1);
                const bool geometry = call.find(".left") != std::string::npos || call.find(".right") != std::string::npos ||
                                      call.find(".top") != std::string::npos || call.find(".bottom") != std::string::npos;
                const bool dip_math = call.find("D(") != std::string::npos;
                const bool typed = call.find("static_cast<int>") != std::string::npos || call.find("static_cast<LONG>") != std::string::npos ||
                                   call.find("std::min<LONG>") != std::string::npos || call.find("std::max<LONG>") != std::string::npos;
                if (geometry && dip_math && !typed)
                    findings.push_back({file, number, "std::min/max mixes Win32 RECT geometry with int/DPI math without an explicit common type"});
                position = close + 1;
            }
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::current_path();
    const fs::path source_root = root / "src";
    if (!fs::is_directory(source_root)) {
        std::cerr << "WebCom source preflight: src directory not found under " << root << "\n";
        return 2;
    }

    std::vector<Finding> findings;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(source_root))
        if (entry.is_regular_file() && SourceFile(entry.path())) files.push_back(entry.path());
    std::sort(files.begin(), files.end());

    std::size_t checks = 0;
    std::size_t cookie_calls = 0;
    fs::path cookie_file;
    std::size_t cookie_offset = 0;
    for (const auto& file : files) {
        const auto text = ReadAll(file);
        if (text.empty()) continue;
        const auto relative = fs::relative(file, root);

        const auto compat = text.find("win32_com_compat.h");
        for (const std::string_view header : {std::string_view("<gdiplus.h>"), std::string_view("<WebView2.h>"), std::string_view("<WebView2EnvironmentOptions.h>")}) {
            const auto position = text.find(header);
            if (position != std::string::npos) {
                ++checks;
                if (compat == std::string::npos || compat > position)
                    findings.push_back({relative, LineFor(text, position), "include win32_com_compat.h before " + std::string(header)});
            }
        }

        std::size_t search = 0;
        while ((search = text.find("get_CookieManager", search)) != std::string::npos) {
            ++cookie_calls; cookie_file = relative; cookie_offset = search; ++search;
        }
        AddLineFindings(relative, text, findings);
    }

    ++checks;
    if (cookie_calls != 1) {
        findings.push_back({"src/app.cpp", 0, "expected exactly one get_CookieManager call through the versioned WebView2 helper"});
    } else {
        const auto text = ReadAll(root / cookie_file);
        const auto begin = cookie_offset > 700 ? cookie_offset - 700 : 0;
        const auto context = text.substr(begin, std::min<std::size_t>(1400, text.size() - begin));
        ++checks;
        if (context.find("ComPtr<ICoreWebView2_2> web2") == std::string::npos ||
            context.find("web2->get_CookieManager") == std::string::npos)
            findings.push_back({cookie_file, LineFor(text, cookie_offset), "CookieManager must be acquired through ICoreWebView2_2, never ICoreWebView2"});
    }

    const auto update_transport = ReadAll(root / "src" / "update_transport_win32.cpp");
    if (!update_transport.empty()) {
        const std::array<std::pair<std::string_view,std::string_view>,7> update_guards{{
            {"WINHTTP_DISABLE_COOKIES", "update transport must disable WinHTTP cookies"},
            {"WINHTTP_DISABLE_AUTHENTICATION", "update transport must disable automatic WinHTTP authentication"},
            {"WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP", "update transport must block HTTPS-to-HTTP redirects"},
            {"BCRYPT_SHA256_ALGORITHM", "update package verification must use SHA-256"},
            {"WinVerifyTrust", "update package verification must use Windows Authenticode trust"},
            {"WTHelperGetProvSignerFromChain", "update package verification must inspect the trusted signer certificate"},
            {"GetFileVersionInfoW", "update package verification must confirm the signed executable product identity"}
        }};
        for (const auto& [token, message] : update_guards) {
            ++checks;
            if (update_transport.find(token) == std::string::npos)
                findings.push_back({"src/update_transport_win32.cpp", 0, std::string(message)});
        }
    }


    const auto app_source = ReadAll(root / "src" / "app.cpp");
    if (!app_source.empty()) {
        const auto rollback = app_source.find("void BrowserApp::Impl::RollbackPreviousVersionNow()");
        ++checks;
        if (rollback == std::string::npos) {
            findings.push_back({"src/app.cpp", 0, "explicit rollback handler is missing"});
        } else {
            const auto context = app_source.substr(rollback, std::min<std::size_t>(7000, app_source.size() - rollback));
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
                {"EvaluateRecoveryReadiness", "rollback must pass the centralized recovery-readiness policy"},
                {"VerifyStagedUpdatePackage", "rollback must re-verify the preserved installer immediately before launch"},
                {"L\"runas\"", "rollback installer launch must use Windows elevation"},
                {"RecoveryReadinessName", "rollback policy failures must retain a bounded log-safe status"}
            }}) {
                ++checks;
                if (context.find(token) == std::string::npos) findings.push_back({"src/app.cpp", LineFor(app_source, rollback), std::string(message)});
            }
        }
        const auto health = app_source.find("void BrowserApp::Impl::ConfirmUpdateHealth()");
        ++checks;
        if (health == std::string::npos) findings.push_back({"src/app.cpp", 0, "update health confirmation handler is missing"});
        else {
            const auto context = app_source.substr(health, std::min<std::size_t>(6000, app_source.size() - health));
            ++checks;
            if (context.find("VerifyStagedUpdatePackage") == std::string::npos || context.find("PreserveRecoveryPackage") == std::string::npos)
                findings.push_back({"src/app.cpp", LineFor(app_source, health), "known-good rollback preservation must re-verify the current installer before archiving it"});
        }
    }

    if (!app_source.empty()) {
        const auto recovery_export = app_source.find("void BrowserApp::Impl::ExportRecoveryReportNow()");
        ++checks;
        if (recovery_export == std::string::npos) findings.push_back({"src/app.cpp", 0, "explicit recovery report export handler is missing"});
        else {
            const auto context = app_source.substr(recovery_export, std::min<std::size_t>(5000, app_source.size() - recovery_export));
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
                {"ReadRuntimeLogBundleText", "recovery report must use the bounded runtime log reader"},
                {"BuildDiagnosticsZip", "recovery report must use the bounded diagnostics ZIP builder"},
                {"ReliabilityReportPrivacyText", "recovery report must include the explicit local-only privacy notice"}
            }}) {
                ++checks;
                if (context.find(token) == std::string::npos) findings.push_back({"src/app.cpp", LineFor(app_source, recovery_export), std::string(message)});
            }
            ++checks;
            if (context.find("FetchUpdateManifestHttps") != std::string::npos || context.find("DownloadAndVerifyUpdatePackage") != std::string::npos)
                findings.push_back({"src/app.cpp", LineFor(app_source, recovery_export), "recovery report export must remain a local-only action with no network fetch/download path"});
        }
    }

    const auto update_model = ReadAll(root / "src" / "update_model.cpp");
    if (!update_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
            {"IsPrereleaseReleaseVersion", "release policy must classify prerelease versions explicitly"},
            {"IsReleaseVersionAllowedOnChannel", "release policy must bind version kind to Stable/Beta channels"},
            {"channel == ReleaseChannel::Beta || !IsPrereleaseReleaseVersion", "Stable must fail closed for prerelease versions"}
        }}) {
            ++checks;
            if (update_model.find(token) == std::string::npos) findings.push_back({"src/update_model.cpp", 0, std::string(message)});
        }
    }

    const auto update_recovery = ReadAll(root / "src" / "update_recovery_model.cpp");
    if (!update_recovery.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"EvaluateRecoveryReadiness", "rollback recovery must use one centralized readiness evaluation"},
            {"RunningVersionMismatch", "rollback recovery must reject an unexpected running version"},
            {"PackageVersionMismatch", "rollback recovery must require the recorded previous version"},
            {"PublisherMismatch", "rollback recovery must bind the preserved installer to the compiled publisher pin"}
        }}) {
            ++checks;
            if (update_recovery.find(token) == std::string::npos) findings.push_back({"src/update_recovery_model.cpp", 0, std::string(message)});
        }
    }

    const auto update_retry = ReadAll(root / "src" / "update_retry_model.cpp");
    if (!update_retry.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"constexpr std::uint64_t delays[]{15, 30, 60, 120, 300}", "update retry delays must remain bounded and deterministic"},
            {"consecutive_failures < 32u", "update retry failure count must remain bounded"},
            {R"(temp += ".tmp")", "update retry state must be written atomically through a temporary file"},
            {"retry_after_unix_seconds", "update retry state must persist an explicit retry deadline"}
        }}) {
            ++checks;
            if (update_retry.find(token) == std::string::npos) findings.push_back({"src/update_retry_model.cpp", 0, std::string(message)});
        }
    }

    const auto reliability_report = ReadAll(root / "src" / "reliability_report_model.cpp");
    if (!reliability_report.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
            {"not uploaded automatically", "recovery report privacy text must explicitly state that no automatic upload occurs"},
            {"redacts HTTP, HTTPS and file URLs", "recovery report privacy text must describe runtime-log URL redaction"},
            {"Review every file before sharing", "recovery report privacy text must tell users to review report contents"}
        }}) {
            ++checks;
            if (reliability_report.find(token) == std::string::npos) findings.push_back({"src/reliability_report_model.cpp", 0, std::string(message)});
        }
    }

    const auto runtime_log = ReadAll(root / "src" / "runtime_log.cpp");
    if (!runtime_log.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"kMaxCurrentLog", "runtime log must stay size-bounded"},
            {"runtime.previous.log", "runtime log must rotate instead of growing unbounded"},
            {"https://", "runtime log sanitizer must recognize web URLs for redaction"},
            {"file:///", "runtime log sanitizer must recognize local-file URLs for redaction"}
        }}) {
            ++checks;
            if (runtime_log.find(token) == std::string::npos) findings.push_back({"src/runtime_log.cpp", 0, std::string(message)});
        }
    }

    const auto profile_host = ReadAll(root / "src" / "profile_host_impl.inl");
    if (!profile_host.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
            {"CoreWebView2EnvironmentOptions", "extension-capable environment must use CoreWebView2EnvironmentOptions"},
            {"ICoreWebView2EnvironmentOptions6", "browser extensions must be enabled through the versioned environment-options interface"},
            {"put_AreBrowserExtensionsEnabled(TRUE)", "browser extensions must be explicitly enabled before environment creation"}
        }}) {
            ++checks;
            if (profile_host.find(token) == std::string::npos)
                findings.push_back({"src/profile_host_impl.inl", 0, std::string(message)});
        }
    }

    ++checks;
    if (!profile_host.empty() && profile_host.find("ExtensionSourcesPath(root,deleted)") == std::string::npos)
        findings.push_back({"src/profile_host_impl.inl", 0, "profile deletion must remove per-profile extension source metadata"});

    const auto extension_model = ReadAll(root / "src" / "extension_model.cpp");
    if (!extension_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,7>{{
            {"kMaxManifestBytes", "extension manifest preview must stay size-bounded"},
            {"manifest.json", "extension preview must require manifest.json in the selected folder"},
            {"declares_toolbar_ui", "extension preview must retain the toolbar-UI compatibility warning signal"},
            {"host_permissions", "extension details must inspect host permissions"},
            {"content_scripts", "extension details must identify content-script use"},
            {"service_worker", "extension details must identify background/service-worker use"},
            {"ExtensionCompatibilitySummary", "extension details must keep a bounded compatibility summary"}
        }}) {
            ++checks;
            if (extension_model.find(token) == std::string::npos)
                findings.push_back({"src/extension_model.cpp", 0, std::string(message)});
        }
    }

    const auto extension_sources = ReadAll(root / "src" / "extension_sources.cpp");
    if (!extension_sources.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,10>{{
            {"kMaxRecords = 64", "extension source metadata must remain record-bounded"},
            {"kMaxBytes = 128u * 1024u", "extension source metadata must remain size-bounded"},
            {"kMaxSnapshotFiles = 4096", "extension source change detection must remain file-count bounded"},
            {"kHeaderV1", "extension source metadata must retain v1 migration support"},
            {"kHeaderV2", "extension source metadata must persist the v2 change-stamp schema"},
            {"AtomicWrite", "extension source metadata must be persisted atomically"},
            {"IsValidExtensionId", "persisted extension IDs must be validated"},
            {"IsValidExtensionSourceStamp", "persisted extension source stamps must be validated"},
            {"SnapshotExtensionSource", "extension source metadata must expose bounded change detection"},
            {"ExtensionSourcesPath", "extension source metadata must stay isolated per profile"}
        }}) {
            ++checks;
            if (extension_sources.find(token) == std::string::npos)
                findings.push_back({"src/extension_sources.cpp", 0, std::string(message)});
        }
    }

    const auto annotation_editor = ReadAll(root / "src" / "annotation_editor.cpp");
    if (!annotation_editor.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"std::clamp<LONG>(point.x", "annotation coordinates must clamp Win32 LONG values with an explicit LONG type"},
            {"std::clamp<LONG>(point.y", "annotation coordinates must clamp Win32 LONG values with an explicit LONG type"},
            {"const int image_x=", "annotation image-space conversion must normalize Win32 geometry to int before integer clamp"},
            {"static_cast<INT>(a.x)", "GDI+ DrawLine coordinates must select the integer overload explicitly under MSVC"},
            {"static_cast<INT>(b.y)", "GDI+ DrawLine coordinates must select the integer overload explicitly under MSVC"}
        }}) {
            ++checks;
            if (annotation_editor.find(token) == std::string::npos)
                findings.push_back({"src/annotation_editor.cpp", 0, std::string(message)});
        }
    }

    const auto windows_ui_tests = ReadAll(root / "tests" / "windows_ui_tests.cpp");
    if (!windows_ui_tests.empty()) {
        ++checks;
        if (windows_ui_tests.find("webcom::StartupMode::Continue") == std::string::npos)
            findings.push_back({"tests/windows_ui_tests.cpp", 0, "Windows UI tests must qualify StartupMode outside namespace webcom"});
        ++checks;
        if (windows_ui_tests.find("false,false,StartupMode::Continue") != std::string::npos)
            findings.push_back({"tests/windows_ui_tests.cpp", 0, "unqualified StartupMode::Continue is an MSVC compile regression outside namespace webcom"});
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"RenameWorkspaceCmd", "workspace rename command id must not collide with BrowserApp::Impl::RenameWorkspace"},
            {"DeleteWorkspaceCmd", "workspace delete command id must not collide with BrowserApp::Impl::DeleteWorkspace"},
            {"items.push_back({RenameWorkspaceCmd", "workspace menu must use the distinct rename command id"},
            {"items.push_back({DeleteWorkspaceCmd", "workspace menu must use the distinct delete command id"},
            {"case RenameWorkspaceCmd: RenameWorkspace();", "workspace dispatch must route the distinct rename command id to the rename method"},
            {"case DeleteWorkspaceCmd: DeleteWorkspace();", "workspace dispatch must route the distinct delete command id to the delete method"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
        ++checks;
        if (app_source.find("WorkspaceMenu, AddWorkspace, RenameWorkspace, DeleteWorkspace") != std::string::npos)
            findings.push_back({"src/app.cpp", 0, "workspace command ids collide with member-function names under MSVC"});
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,13>{{
            {"ICoreWebView2Profile7", "extension manager must use the versioned WebView2 profile interface"},
            {"GetBrowserExtensions", "extension manager must enumerate installed extensions through WebView2"},
            {"AddBrowserExtension", "extension manager must install only through WebView2's extension API"},
            {"ICoreWebView2BrowserExtensionEnableCompletedHandler", "extension enable/disable must use the WebView2 extension API"},
            {"ICoreWebView2BrowserExtensionRemoveCompletedHandler", "extension removal must use the WebView2 extension API"},
            {"RememberExtensionSource", "successful unpacked installs must persist their source mapping"},
            {"ForgetExtensionSource", "explicit extension removal must delete its source mapping"},
            {"RefreshExtensionSourceHealth", "extension manager must refresh source change diagnostics"},
            {"ReloadExtensionSource", "extension manager must retain an explicit source reload path"},
            {"OpenExtensionSourceFolder", "extension manager must expose the recorded source folder"},
            {"ShowExtensionDetails", "extension manager must expose compatibility/permission details"},
            {"ShowExtensionSourceDetails", "known unpacked sources must expose developer diagnostics"},
            {"ForgetExtensionSourceMapping", "known unpacked sources must have a non-destructive forget action"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"WM_GETOBJECT", "custom tab strip must expose an accessibility object"},
            {"LresultFromObject", "custom tab strip must return its MSAA provider through WM_GETOBJECT"},
            {"ROLE_SYSTEM_PAGETABLIST", "tab accessibility provider must expose page-tab semantics"},
            {"EVENT_OBJECT_SELECTION", "tab changes must notify accessibility clients of selection changes"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto memory_script = ReadAll(root / "scripts" / "Measure-WebComMemory.ps1");
    if (!memory_script.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"PrivateCommitBudgetMiB", "memory QA must support an explicit private-commit budget"},
            {"FailOnBudgetExceeded", "memory QA must support a CI-style regression gate"},
            {"WebViewPrivateCommitMiB", "memory QA must separate host and WebView2 totals"},
            {"No command lines, page titles, URLs, credentials, or browsing data are collected", "memory QA must disclose its privacy boundary"}
        }}) {
            ++checks;
            if (memory_script.find(token) == std::string::npos) findings.push_back({"scripts/Measure-WebComMemory.ps1", 0, std::string(message)});
        }
    }

    const auto manifest_script = ReadAll(root / "scripts" / "New-ReleaseManifest.ps1");
    const auto verify_script = ReadAll(root / "scripts" / "Verify-UpdatePackage.ps1");
    const auto matrix_script = ReadAll(root / "scripts" / "Test-ReleaseMatrix.ps1");
    ++checks;
    if (manifest_script.find("stable channel cannot publish a prerelease version") == std::string::npos)
        findings.push_back({"scripts/New-ReleaseManifest.ps1", 0, "release manifest generation must reject Stable prerelease versions"});
    ++checks;
    if (verify_script.find("ProductVersion") == std::string::npos || verify_script.find("stable channel cannot advertise a prerelease version") == std::string::npos)
        findings.push_back({"scripts/Verify-UpdatePackage.ps1", 0, "release verification must enforce ProductVersion and Stable prerelease policy"});
    ++checks;
    if (matrix_script.find("Release-matrix policy passed") == std::string::npos || matrix_script.find("ExpectedPublisherSha256") == std::string::npos)
        findings.push_back({"scripts/Test-ReleaseMatrix.ps1", 0, "Stable/Beta release-matrix QA script is missing its signer/channel gate"});

    const auto native_ui = ReadAll(root / "src" / "native_ui.inl");
    if (!native_ui.empty()) {
        static const std::regex internal_decl(R"((?:const\s+)?(?:auto|InternalPage)\s+internal_page\s*=)");
        const auto count = static_cast<std::size_t>(std::distance(
            std::sregex_iterator(native_ui.begin(), native_ui.end(), internal_decl), std::sregex_iterator()));
        ++checks;
        if (count > 1) findings.push_back({"src/native_ui.inl", 0, "internal_page is declared more than once; this previously caused an MSVC redefinition failure"});
    }

    checks += files.size();
    if (!findings.empty()) {
        std::cerr << "WebCom MSVC source preflight failed with " << findings.size() << " finding(s):\n";
        for (const auto& finding : findings) {
            std::cerr << "  " << finding.file.generic_string();
            if (finding.line) std::cerr << ':' << finding.line;
            std::cerr << ": " << finding.message << '\n';
        }
        return 1;
    }
    std::cout << checks << " MSVC source-preflight checks, 0 failures\n";
    return 0;
}
