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
#include <tuple>
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
    const auto update_runtime_model = ReadAll(root / "src" / "update_runtime_model.cpp");
    const auto update_model_source = ReadAll(root / "src" / "update_model.cpp");
    const auto update_automation_source = ReadAll(root / "src" / "update_automation_model.cpp");
    const auto update_runtime_tests = ReadAll(root / "tests" / "update_runtime_model_tests.cpp");
    const auto update_model_tests = ReadAll(root / "tests" / "update_model_tests.cpp");
    const auto update_automation_tests = ReadAll(root / "tests" / "update_automation_model_tests.cpp");
    if (!update_transport.empty()) {
        const std::array<std::pair<std::string_view,std::string_view>,11> update_guards{{
            {"WINHTTP_DISABLE_COOKIES", "update transport must disable WinHTTP cookies"},
            {"WINHTTP_DISABLE_AUTHENTICATION", "update transport must disable automatic WinHTTP authentication"},
            {"WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP", "update transport must block HTTPS-to-HTTP redirects"},
            {"BCRYPT_SHA256_ALGORITHM", "update package verification must use SHA-256"},
            {"WinVerifyTrust", "update package verification must use Windows Authenticode trust"},
            {"WTHelperGetProvSignerFromChain", "update package verification must inspect the trusted signer certificate"},
            {"GetFileVersionInfoW", "update package verification must confirm the signed executable product identity"},
            {"DetectCurrentUpdateInstallScope", "update transport must detect the current installer scope before launching setup"},
            {"HKEY_CURRENT_USER", "update scope detection must inspect the per-user registration"},
            {"HKEY_LOCAL_MACHINE", "update scope detection must inspect the per-machine registration"},
            {"RegistryCommandTargetsCurrentExecutable", "update scope detection must bind registry registration to the running executable"}
        }};
        for (const auto& [token, message] : update_guards) {
            ++checks;
            if (update_transport.find(token) == std::string::npos)
                findings.push_back({"src/update_transport_win32.cpp", 0, std::string(message)});
        }
    }


    for (const auto& [source, token, message] : std::array<std::tuple<std::string_view,std::string_view,std::string_view>,9>{{
        {update_runtime_model, "UpdateInstallScope::PerUser", "update runtime must model per-user installer scope"},
        {update_runtime_model, "UpdateInstallScope::PerMachine", "update runtime must model per-machine installer scope"},
        {update_runtime_model, "\"/CURRENTUSER\"", "per-user update launch must preserve per-user installer scope"},
        {update_runtime_model, "\"/ALLUSERS\"", "per-machine update launch must preserve per-machine installer scope"},
        {update_runtime_model, "return std::nullopt", "unknown installer scope must fail closed"},
        {update_model_source, "RevertIncompleteScheduledInstall", "update model must recover a cancelled/incomplete scheduled install"},
        {update_runtime_tests, "unknown install scope cannot launch an installer automatically", "install-scope policy must have portable fail-closed regression coverage"},
        {update_model_tests, "cancelled or incomplete install returns to verified staged state", "incomplete installer reconciliation must have portable regression coverage"},
        {update_model_tests, "incomplete install reconciliation preserves verified package metadata", "reconciliation must preserve the verified package identity"}
    }}) {
        ++checks;
        if (source.find(token) == std::string::npos)
            findings.push_back({"src/update_runtime_model.cpp", 0, std::string(message)});
    }

    for (const auto& [source, token, message] : std::array<std::tuple<std::string_view,std::string_view,std::string_view>,11>{{
        {update_model_source, "WEBCOM_UPDATES\\t3", "update settings must use the v3 schema for independent automatic-download consent"},
        {update_model_source, "automatic_downloads", "update settings must persist automatic verified-download consent"},
        {update_model_tests, "v2 settings retain check consent but do not silently enable automatic downloads", "settings migration must not silently opt existing users into background downloads"},
        {update_automation_source, "kBackgroundUpdateCheckIntervalSeconds", "background update checks must use a bounded persisted cadence"},
        {update_automation_source, "last_background_check", "background update cadence must survive process restarts"},
        {update_automation_tests, "six-hour cadence becomes due exactly at boundary", "background update cadence must have portable regression coverage"},
        {update_automation_tests, "clock moving backwards fails toward one safe recheck", "background scheduling must handle backwards clock changes safely"},
        {update_automation_tests, "background retry delay is capped at five minutes", "background retry scheduling must avoid unbounded or tight retry loops"},
        {update_automation_source, "BackgroundUpdateQuickRetryAllowed", "background update policy must explicitly bound quick retries"},
        {update_automation_tests, "first two failures allow bounded quick retries", "background update quick-retry allowance must have regression coverage"},
        {update_automation_tests, "third and later failures fall back to normal six-hour cadence", "repeated background failures must fall back to the normal cadence"}
    }}) {
        ++checks;
        if (source.find(token) == std::string::npos)
            findings.push_back({"src/update_automation_model.cpp", 0, std::string(message)});
    }

    const auto app_source = ReadAll(root / "src" / "app.cpp");
    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,17>{{
            {"ScheduleAutomaticUpdateCheck(true);", "browser startup must schedule consented background update checks"},
            {"kAutomaticUpdateCheck", "background update checks must use a dedicated Win32 timer"},
            {"BackgroundUpdateCheckDelaySeconds", "background timer scheduling must use the portable cadence policy"},
            {"LoadUpdateAutomationState(root_)", "each background attempt must refresh shared persisted cadence state across profile windows"},
            {"LoadUpdateRetryState(root_)", "background attempts must refresh shared retry/backoff state across profile windows"},
            {"g_update_network_busy.compare_exchange_strong", "multi-profile windows must serialize update check/download network operations"},
            {"RecordBackgroundUpdateCheck", "background endpoint contact must be persisted before asynchronous fetch"},
            {"completion->background", "update completions must preserve manual/background origin"},
            {"update_settings_.automatic_downloads", "automatic staging must require a separate saved user preference"},
            {"StageAvailableUpdateNow(true)", "background checks may stage only when verified automatic downloads are enabled"},
            {"Installer launch always remains manual", "Updates UI must keep installer launch explicitly user-controlled"},
            {"ToggleAutomaticUpdateDownloads", "Updates UI must expose independent automatic-download consent"},
            {"g_update_network_busy.store(false)", "asynchronous update workers must release the process-wide network gate"},
            {"origin=background", "runtime logs must distinguish background update activity from manual actions"},
            {"update.rollback.completed", "rollback completion must retain explicit update lifecycle logging"},
            {"ScheduleAutomaticUpdateCheck(false);\n        Note(L\"Updated WebCom passed its startup health check.", "successful update health completion must resume automatic update cadence"},
            {"ScheduleAutomaticUpdateCheck(false);\n        QueueUI(); Note(L\"Staged update state cleared.", "manual staged-state cleanup must resume automatic update cadence"}
        }}) {
            ++checks;
            const auto ui_token = token == std::string_view("Installer launch always remains manual");
            const auto ui_source = ui_token ? ReadAll(root / "src" / "native_ui.inl") : std::string{};
            const auto& source = ui_token ? ui_source : app_source;
            if (source.find(token) == std::string::npos)
                findings.push_back({ui_token ? "src/native_ui.inl" : "src/app.cpp", 0, std::string(message)});
        }

        const auto rollback = app_source.find("void BrowserApp::Impl::RollbackPreviousVersionNow()");
        ++checks;
        if (rollback == std::string::npos) {
            findings.push_back({"src/app.cpp", 0, "explicit rollback handler is missing"});
        } else {
            const auto context = app_source.substr(rollback, std::min<std::size_t>(7000, app_source.size() - rollback));
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,7>{{
                {"EvaluateRecoveryReadiness", "rollback must pass the centralized recovery-readiness policy"},
                {"VerifyStagedUpdatePackage", "rollback must re-verify the preserved installer immediately before launch"},
                {"DetectCurrentUpdateInstallScope", "rollback must detect the existing installation scope"},
                {"InstallerLaunchPolicyForScope", "rollback must preserve per-user/per-machine installer scope"},
                {"launch_policy->elevate?L\"runas\":L\"open\"", "rollback elevation must depend on the verified installation scope"},
                {"launch_policy->install_mode_argument", "rollback must pass an explicit CURRENTUSER/ALLUSERS installer mode"},
                {"RecoveryReadinessName", "rollback policy failures must retain a bounded log-safe status"}
            }}) {
                ++checks;
                if (context.find(token) == std::string::npos) findings.push_back({"src/app.cpp", LineFor(app_source, rollback), std::string(message)});
            }
        }
        const auto install = app_source.find("void BrowserApp::Impl::InstallStagedUpdateNow()");
        ++checks;
        if (install == std::string::npos) findings.push_back({"src/app.cpp", 0, "verified update install handler is missing"});
        else {
            const auto context = app_source.substr(install, std::min<std::size_t>(7000, app_source.size() - install));
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
                {"VerifyStagedUpdatePackage", "installer must be re-verified immediately before user-triggered launch"},
                {"DetectCurrentUpdateInstallScope", "update installer must detect the existing installation scope"},
                {"InstallerLaunchPolicyForScope", "update installer must preserve the existing install scope"},
                {"launch_policy->elevate ? L\"runas\" : L\"open\"", "update installer elevation must be scope-aware rather than unconditional"},
                {"launch_policy->install_mode_argument", "update installer must pass CURRENTUSER/ALLUSERS explicitly"},
                {"update.install.scope_unknown", "unknown/portable install scope must be logged and blocked fail-closed"}
            }}) {
                ++checks;
                if (context.find(token) == std::string::npos) findings.push_back({"src/app.cpp", LineFor(app_source, install), std::string(message)});
            }
        }
        const auto prepare_health = app_source.find("void BrowserApp::Impl::PrepareUpdateHealthState()");
        ++checks;
        if (prepare_health == std::string::npos) findings.push_back({"src/app.cpp", 0, "update startup reconciliation handler is missing"});
        else {
            const auto context = app_source.substr(prepare_health, std::min<std::size_t>(7000, app_source.size() - prepare_health));
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
                {"RevertIncompleteScheduledInstall", "cancelled/incomplete installer attempts must return to verified staged state"},
                {"previous_version == kWebComVersion", "incomplete-install reconciliation must require the pre-update version to be running"},
                {"update.install.incomplete", "incomplete install reconciliation must be logged"},
                {"signed installer remains staged", "users must be told the verified staged package remains available after cancellation/failure"}
            }}) {
                ++checks;
                if (context.find(token) == std::string::npos) findings.push_back({"src/app.cpp", LineFor(app_source, prepare_health), std::string(message)});
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

    const auto onboarding_model = ReadAll(root / "src" / "onboarding_model.cpp");
    if (!onboarding_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"BuildOnboardingProgress", "Welcome setup progress must be computed by the portable onboarding model"},
            {"bookmark_count > 0", "Welcome progress must recognize bookmark setup"},
            {"workspace_count > 1", "Welcome progress must recognize multi-workspace setup"},
            {"update_checks_enabled", "Welcome progress must recognize update-check setup"}
        }}) {
            ++checks;
            if (onboarding_model.find(token) == std::string::npos)
                findings.push_back({"src/onboarding_model.cpp", 0, std::string(message)});
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
    ++checks;
    if (!profile_host.empty() && profile_host.find("root / \"AdBlocker\" / (lower+\".v1\")") == std::string::npos)
        findings.push_back({"src/profile_host_impl.inl", 0, "profile deletion must remove per-profile Ad Blocker site exceptions"});
    ++checks;
    if (!profile_host.empty() && profile_host.find("ad_blocker_extension_.Reset()") == std::string::npos)
        findings.push_back({"src/profile_host_impl.inl", 0, "browser-engine recovery must release the bundled Ad Blocker extension handle"});

    const auto ad_blocker_preferences = ReadAll(root / "src" / "ad_blocker_preferences.cpp");
    if (!ad_blocker_preferences.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,8>{{
            {"kMaxAdBlockerSiteExceptions", "Ad Blocker site exceptions must remain bounded"},
            {"WEBCOM_AD_BLOCKER\\t1", "Ad Blocker site exceptions must use a versioned profile-scoped format"},
            {"IsValidProfileName(profile)", "Ad Blocker site exception persistence must reject unsafe profile names"},
            {"AtomicWrite", "Ad Blocker site exceptions must be persisted atomically"},
            {"NormalizeAdBlockerHost", "Ad Blocker site exceptions must normalize and validate DNS-style hosts"},
            {"RenderAdBlockerNetworkRules", "Ad Blocker v2 must generate profile-specific DNR exclusions from the saved site list"},
            {"excludedInitiatorDomains", "Ad Blocker v2 must render DNR initiator exclusions rather than faking a UI-only site toggle"},
            {"BuildAdBlockerSiteConfigScript", "Ad Blocker v2 must generate the matching page-level cosmetic/YouTube bypass config"}
        }}) {
            ++checks;
            if (ad_blocker_preferences.find(token) == std::string::npos)
                findings.push_back({"src/ad_blocker_preferences.cpp", 0, std::string(message)});
        }
    }

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

    const auto extension_view_model = ReadAll(root / "src" / "extension_view_model.cpp");
    if (!extension_view_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"ExtensionNeedsAttention", "Extensions v2 must classify source-health attention through the portable view model"},
            {"ExtensionMatchesFilter", "Extensions v2 must keep All/Enabled/Disabled/Attention filtering portable"},
            {"ExtensionMatchesText", "Extensions v2 search must use the portable case-insensitive matcher"},
            {"state.built_in", "built-in extensions must not require an unpacked source mapping"},
            {"state.source_changed", "changed unpacked sources must surface as needing attention"},
            {"AddExtensionViewCount", "Extensions v2 filter chips must use shared counts"}
        }}) {
            ++checks;
            if (extension_view_model.find(token) == std::string::npos)
                findings.push_back({"src/extension_view_model.cpp", 0, std::string(message)});
        }
    }

    const auto extension_sources = ReadAll(root / "src" / "extension_sources.cpp");
    if (!extension_sources.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
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
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"std::clamp(point.x", "untyped std::clamp on POINT.x recreates the MSVC LONG/int ambiguity seen in the Windows build log"},
            {"std::clamp(point.y", "untyped std::clamp on POINT.y recreates the MSVC LONG/int ambiguity seen in the Windows build log"},
            {"DrawLine(&pen,a.x", "raw POINT LONG coordinates recreate the ambiguous GDI+ DrawLine overload under MSVC"},
            {"DrawLine(&pen, a.x", "raw POINT LONG coordinates recreate the ambiguous GDI+ DrawLine overload under MSVC"}
        }}) {
            ++checks;
            if (annotation_editor.find(token) != std::string::npos)
                findings.push_back({"src/annotation_editor.cpp", 0, std::string(message)});
        }
    }

    const auto windows_ui_tests = ReadAll(root / "tests" / "windows_ui_tests.cpp");
    if (!windows_ui_tests.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"#include \"storage.h\"", "Windows UI tests must include the declaration that owns StartupMode and SavedProfile"},
            {"webcom::SavedProfile SeedProfile", "Windows UI seed profiles must use field-explicit construction instead of a brittle positional aggregate"},
            {"profile.startup_mode = webcom::StartupMode::Continue", "Windows UI seed startup mode must stay explicitly namespace-qualified"},
            {"SeedProfile(\"Default\"", "Default UI-test profile must use the field-explicit seed helper"},
            {"SeedProfile(\"Work\"", "Work UI-test profile must use the field-explicit seed helper"}
        }}) {
            ++checks;
            if (windows_ui_tests.find(token) == std::string::npos)
                findings.push_back({"tests/windows_ui_tests.cpp", 0, std::string(message)});
        }
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
            {"false,false,StartupMode::Continue", "unqualified StartupMode::Continue is an MSVC compile regression outside namespace webcom"},
            {"false,false,webcom::StartupMode::Continue", "positional SavedProfile startup-mode aggregates are intentionally forbidden in the Windows UI harness"},
            {"seed.profiles = {\n            {\"Default\"", "Windows UI seed profiles must not return to field-order-sensitive aggregate initialization"}
        }}) {
            ++checks;
            if (windows_ui_tests.find(token) != std::string::npos)
                findings.push_back({"tests/windows_ui_tests.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
            {"void BrowserApp::Impl::RefreshPerformanceTimer()", "performance polling must be centrally rescheduled when the sleep policy changes"},
            {"PerformanceTickIntervalMs(sleep_timeout_)", "performance polling must use the portable adaptive interval policy"},
            {"if (interval) SetTimer(window_, kTick, interval, nullptr)", "Never-sleep mode must be able to leave the automatic performance timer disabled"},
            {"void BrowserApp::Impl::EnsureDownloadsLoaded()", "download history must support first-use loading"},
            {"if (!downloads_loaded_) return;", "shutdown must not overwrite an untouched lazy download-history store"},
            {"void BrowserApp::Impl::EnsureSitePermissionsLoaded()", "site-permission rules must support first-use loading"},
            {"if (!current->private_mode) app->EnsureSitePermissionsLoaded();", "permission decisions must load persisted rules before answering normal-profile requests"},
            {"void BrowserApp::Impl::EnsureNamedSessionsLoaded()", "saved sessions must support first-use loading"},
            {"void BrowserApp::Impl::EnsureExtensionSourcesLoaded()", "unpacked-extension source metadata must support first-use loading"},
            {"EnsureExtensionSourcesLoaded();", "unpacked installs must load existing source mappings before mutation"},
            {"EnsureInternalPageData(InternalPage page)", "native internal pages must load only the bounded datasets they render"},
            {"RefreshPerformanceTimer();", "sleep-policy changes must immediately reschedule automatic polling"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
        ++checks;
        if (app_source.find("SetTimer(window_, kTick, 10000") != std::string::npos)
            findings.push_back({"src/app.cpp", 0, "fixed 10-second performance polling must not return after adaptive scheduling"});

        const auto ctor_start = app_source.find("Impl(HINSTANCE instance,const std::shared_ptr<Session>& session,std::string profile)");
        ++checks;
        if (ctor_start == std::string::npos) {
            findings.push_back({"src/app.cpp", 0, "browser constructor could not be located for cold-start I/O checks"});
        } else {
            const auto ctor_end = app_source.find("~Impl();", ctor_start);
            const auto ctor = app_source.substr(ctor_start, ctor_end == std::string::npos ? std::min<std::size_t>(6000, app_source.size()-ctor_start) : ctor_end-ctor_start);
            for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
                {"LoadDownloads();", "persisted download history must remain off the ordinary constructor path"},
                {"LoadSitePermissions(root_, profile_name_)", "site-permission file I/O must remain off the ordinary constructor path"},
                {"LoadNamedSessions(root_, profile_name_)", "saved-session file I/O must remain off the ordinary constructor path"},
                {"LoadExtensionSources(root_, profile_name_)", "unpacked-extension source metadata must remain off the ordinary constructor path"},
                {"RefreshExtensionSourceHealth();", "unpacked-extension folder scans must remain off the ordinary constructor path"}
            }}) {
                ++checks;
                if (ctor.find(token) != std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
            }
        }
        ++checks;
        if (app_source.find("PWSTR raw = nullptr;\n    PWSTR raw = nullptr;") != std::string::npos)
            findings.push_back({"src/app.cpp", 0, "LoadUnpackedExtension contains a duplicate PWSTR declaration that fails MSVC compilation"});
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
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,24>{{
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
            {"ForgetExtensionSourceMapping", "known unpacked sources must have a non-destructive forget action"},
            {"ExtensionsFilterAll", "Extensions v2 must expose an All filter command"},
            {"ExtensionsFilterEnabled", "Extensions v2 must expose an Enabled filter command"},
            {"ExtensionsFilterDisabled", "Extensions v2 must expose a Disabled filter command"},
            {"ExtensionsFilterAttention", "Extensions v2 must expose a Needs-attention filter command"},
            {"PrepareAdBlockerProfileAssets", "Ad Blocker v2 must generate profile-specific assets before loading the bundled extension"},
            {"RenderAdBlockerNetworkRules", "bundled blocker loading must render saved site exceptions into DNR rules"},
            {"BuildAdBlockerSiteConfigScript", "bundled blocker loading must render the same site exceptions into page helpers"},
            {"ReloadBundledAdBlocker", "site exception changes must reload the bundled extension instead of changing only UI state"},
            {"ToggleAdBlockerForTab", "site information must expose a real per-site blocker toggle"},
            {"AdBlockerHostFromUrl", "site blocker controls must derive a validated HTTP(S) host"},
            {"ad_blocker_extension_ = extension", "WebCom must retain the bundled extension handle for safe site-setting reloads"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,8>{{
            {"HomeRecentBase = 22400", "New Tab recent-site commands must use a dedicated bounded command range"},
            {"HomeShortcutBase = 22500", "custom New Tab shortcuts must use a dedicated bounded command range"},
            {"HomeRecentPinBase = 22700", "recent-site pin actions must use a dedicated bounded command range"},
            {"VisibleHomeRecentSites(6)", "recent-site dispatch must rebuild the visible bounded model before navigation"},
            {"Navigate(recent[index].url)", "recent-site cards must navigate the current New Tab rather than spawning hidden renderers"},
            {"AddHomeShortcutUi", "New Tab must expose a user-driven custom shortcut creation path"},
            {"HideHomeRecentSite", "New Tab must let users hide a recent site without deleting history"},
            {"SaveHomePreferencesNow", "New Tab shortcut/recent-site preferences must persist per profile"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
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

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,15>{{
            {"class BrowserApp::Impl::NativeClientAccessible final : public IAccessible", "painted native pages must expose a client accessibility provider"},
            {"CreateStdAccessibleObject(window_, OBJID_CLIENT", "native-page accessibility must wrap the standard Win32 client tree rather than replace real child controls"},
            {"ROLE_SYSTEM_PUSHBUTTON", "painted native-page actions must expose push-button semantics"},
            {"NativeActionAccessibleName", "painted native-page actions must expose meaningful accessible names"},
            {"EVENT_OBJECT_FOCUS", "keyboard focus changes on painted actions must notify accessibility clients"},
            {"client_accessible_.Get()", "the main client WM_GETOBJECT path must return the native-page accessibility wrapper"},
            {"NativeAccessibleActionIndex", "the Windows provider must use the portable virtual-child resolver"},
            {"NativeAccessibleChildId", "the Windows provider must use bounded virtual child IDs"},
            {"NativeAccessibleNextAction", "MSAA action navigation must use the tested portable navigation model"},
            {"std::wstring name;", "native painted actions must retain an accessibility label alongside command and bounds"},
            {"tab->private_mode && IsHome(tab->url)", "private New Tab must not expose stale painted action children"},
            {"if (!tab || tab->web || tab->creating) return 0;", "web/loading tabs must not expose stale painted action children"},
            {"command >= PrivacyPermissionBase && command < PrivacyPermissionBase + 512", "accessibility naming must not materialize site-permission origins for unrelated painted actions"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto native_ui_accessibility = ReadAll(root / "src" / "native_ui.inl");
    if (!app_source.empty() && !native_ui_accessibility.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
            {"SystemClientAnimationsEnabled()", "browser chrome must observe the Windows client-animation policy"},
            {"RefreshSystemAccessibility();", "system accessibility state must be cached before appearance is applied"},
            {"case WM_SETTINGCHANGE:", "system accessibility changes must refresh without restarting WebCom"},
            {"case WM_THEMECHANGED:", "Windows theme/high-contrast changes must refresh browser chrome live"},
            {"dpi_ = std::clamp<UINT>(HIWORD(wparam),72u,768u);", "DPI transitions must validate the new DPI before scaling native chrome"},
            {"focused_native_command_", "painted native-page actions must retain explicit keyboard focus state"},
            {"message.wParam==VK_RETURN || message.wParam==VK_SPACE", "focused native-page actions must activate from keyboard"},
            {"FocusOutline(dc,ring", "native-page keyboard actions must expose a visible focus ring"},
            {"page!=InternalPage::None && page!=InternalPage::NewTab && !native_actions_.empty()", "F6 focus cycling must enter actionable native internal pages"}
        }}) {
            ++checks;
            const bool native_token = token == std::string_view("FocusOutline(dc,ring");
            const auto& source = native_token ? native_ui_accessibility : app_source;
            if (source.find(token) == std::string::npos) findings.push_back({native_token ? fs::path("src/native_ui.inl") : fs::path("src/app.cpp"), 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,7>{{
            {"KeepDownloadHistoryRecord", "download loading must apply the bounded completed-history retention policy"},
            {"in.close();", "download history must release the source file before persisting pruned entries on Windows"},
            {"RetryFailedDownloadIndices(records, 5)", "bulk retry must use the portable bounded candidate policy"},
            {"At most five sources are handled at once", "bulk retry confirmation must disclose its hard source-page bound"},
            {"OpenDownloadSource", "downloads must expose a validated original-source action"},
            {"ResolveAddress(Utf8(item.uri))", "download source reopening must pass through the normal safe address resolver"},
            {"Add(Wide(resolved.url), false, false)", "bulk retry fallback must open validated sources in background tabs rather than repeatedly stealing focus"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"CloseDuplicateTabsInWorkspace", "tab productivity must expose workspace-scoped duplicate cleanup"},
            {"DuplicateTabCloseCandidates", "duplicate cleanup must delegate candidate selection to the portable model"},
            {"Pinned tabs, the active tab and tabs with downloads are protected", "duplicate cleanup confirmation must state its safety boundary"},
            {"SleepOtherTabsInWorkspace", "tab productivity must expose workspace-scoped sleep-other-tabs"},
            {"Suspend(*item,true)", "sleep-other-tabs must reuse WebCom's normal suspension safety path"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,14>{{
            {"LoadSleepTimeoutPreference(root_, profile_name_)", "browser startup must load the separate per-profile sleep-timeout preference"},
            {"LoadNeverSleepHostsPreference(root_, profile_name_)", "browser startup must load bounded per-profile Never sleep site exceptions"},
            {"SleepTimeout sleep_timeout_ = SleepTimeout::FiveMinutes", "browser must keep an explicit compatibility-default sleep timeout"},
            {"ShouldSuspend(tab->activity, now, sleep_timeout_)", "automatic tab sleeping must use the configured timeout instead of only the legacy MemoryMode"},
            {"ContainsNeverSleepHost(never_sleep_hosts_, automatic_sleep_host)", "automatic sleeping must honor Never sleep site exceptions before suspension"},
            {"SaveSleepTimeoutPreference(root_, profile_name_, sleep_timeout_", "sleep-timeout changes must persist through the dedicated preference store"},
            {"SaveNeverSleepHostsPreference(root_, profile_name_, sleep_timeout_, never_sleep_hosts_", "Never sleep site changes must preserve the active timeout while persisting exceptions"},
            {"ToggleNeverSleepSiteForTab", "tab context actions must expose per-site automatic-sleep exceptions"},
            {"SleepAfter1m", "Performance menu must retain the 1-minute timeout command"},
            {"SleepAfter5m", "Performance menu must retain the 5-minute timeout command"},
            {"SleepAfter15m", "Performance menu must retain the 15-minute timeout command"},
            {"SleepAfter30m", "Performance menu must retain the 30-minute timeout command"},
            {"SleepNever", "Performance menu must retain the Never timeout command"},
            {"timeout == SleepTimeout::OneMinute ? MemoryMode::Saver : MemoryMode::Balanced", "only the one-minute setting may opt into the aggressive Memory Saver target"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto extension_ui_source = ReadAll(root / "src" / "native_ui.inl");
    if (!extension_ui_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,8>{{
            {"L\"Search extensions\"", "Extensions v2 must expose native search"},
            {"ExtensionsFilterEnabled", "Extensions v2 must render the Enabled filter chip"},
            {"ExtensionsFilterDisabled", "Extensions v2 must render the Disabled filter chip"},
            {"ExtensionsFilterAttention", "Extensions v2 must render the Needs-attention filter chip"},
            {"ExtensionMatchesText", "Extensions v2 must filter installed and saved-source rows by search text"},
            {"does not claim Chrome Web Store compatibility", "Extensions UI must keep the WebView2/Chrome Web Store compatibility boundary explicit"},
            {"ClearAdBlockerSiteExceptions", "Extensions must expose cleanup for saved bundled-blocker site exceptions"},
            {"ad_blocker_site_exceptions_.size()", "Extensions must show the bundled blocker's saved site-exception count"}
        }}) {
            ++checks;
            if (extension_ui_source.find(token) == std::string::npos)
                findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
    }

    const auto blocker_manifest = ReadAll(root / "extensions" / "webcom-ad-blocker" / "manifest.json");
    const auto blocker_rules = ReadAll(root / "extensions" / "webcom-ad-blocker" / "network-rules.json");
    const auto blocker_css = ReadAll(root / "extensions" / "webcom-ad-blocker" / "web-ads.css");
    const auto blocker_youtube_css = ReadAll(root / "extensions" / "webcom-ad-blocker" / "youtube.css");
    const auto blocker_youtube_js = ReadAll(root / "extensions" / "webcom-ad-blocker" / "youtube-player.js");
    ++checks;
    if (blocker_manifest.find("site-config.js") == std::string::npos || blocker_manifest.find("youtube-player.js") == std::string::npos ||
        blocker_manifest.find("site-config.js") > blocker_manifest.find("youtube-player.js"))
        findings.push_back({"extensions/webcom-ad-blocker/manifest.json", 0, "Ad Blocker v2 must load the synchronous site config before YouTube filtering helpers"});
    ++checks;
    if (blocker_rules.find("\"excludedInitiatorDomains\": [\"webcom.invalid\"]") == std::string::npos)
        findings.push_back({"extensions/webcom-ad-blocker/network-rules.json", 0, "bundled DNR rules must expose a valid reserved initiator sentinel for profile-specific rendering"});
    ++checks;
    if (blocker_css.find("data-webcom-adblock-off") == std::string::npos || blocker_youtube_css.find("data-webcom-adblock-off") == std::string::npos)
        findings.push_back({"extensions/webcom-ad-blocker", 0, "cosmetic filtering must honor the same per-site pause marker"});
    ++checks;
    if (blocker_youtube_js.find("window.__webcomAdBlockerDisabled === true") == std::string::npos)
        findings.push_back({"extensions/webcom-ad-blocker/youtube-player.js", 0, "YouTube filtering must exit immediately on a paused site"});

    const auto welcome_ui_source = ReadAll(root / "src" / "native_ui.inl");
    if (!welcome_ui_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"BuildOnboardingProgress", "Welcome page must render live setup progress"},
            {"L\"Default browser\"", "Welcome checklist must expose default-browser setup"},
            {"ImportBookmarks", "Welcome checklist must link to bookmark import"},
            {"WorkspaceMenu", "Welcome checklist must link to workspace setup"},
            {"UpdatesPage", "Welcome checklist must link to update choices"},
            {"never silently changes Windows defaults", "Welcome page must keep Windows default-app consent explicit"}
        }}) {
            ++checks;
            if (welcome_ui_source.find(token) == std::string::npos)
                findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
    }

    const auto native_ui_source = ReadAll(root / "src" / "native_ui.inl");
    ++checks;
    if (native_ui_source.find("SleepTimeoutSettingsDetail(sleep_timeout_)") == std::string::npos ||
        native_ui_source.find("Never sleep site") == std::string::npos ||
        native_ui_source.find("PerformanceMenu") == std::string::npos)
        findings.push_back({"src/native_ui.inl", 0, "Settings must expose the Performance timeout menu and Never sleep site count"});

    ++checks;
    if (native_ui_source.find("L\"Retry failed\", RetryFailedDownloads") == std::string::npos ||
        native_ui_source.find("OpenDownloadSourceBase + static_cast<UINT>(i)") == std::string::npos)
        findings.push_back({"src/native_ui.inl", 0, "Downloads v2 must expose bounded bulk retry and per-entry source actions"});

    const auto memory_script = ReadAll(root / "scripts" / "Measure-WebComMemory.ps1");
    if (!memory_script.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,10>{{
            {"PrivateCommitBudgetMiB", "memory QA must support an explicit private-commit budget"},
            {"FailOnBudgetExceeded", "memory QA must support a CI-style regression gate"},
            {"WebViewPrivateCommitMiB", "memory QA must separate host and WebView2 totals"},
            {"No command lines, page titles, URLs, credentials, or browsing data are collected", "memory QA must disclose its privacy boundary"},
            {"'one-tab'", "memory acceptance script must retain the one-tab scenario label"},
            {"'multi-profile'", "memory acceptance script must retain the multi-profile scenario label"},
            {"[string]$JsonPath", "memory acceptance script must support a machine-readable JSON artifact"},
            {"webcom-memory-report-v1", "memory acceptance JSON must use an explicit schema identifier"},
            {"Select-Object * -ExcludeProperty Records", "memory acceptance JSON/CSV must exclude per-process record details from summary samples"},
            {"Win32_Process -Property @('ProcessId', 'ParentProcessId', 'Name', 'CreationDate')", "memory sampling must not read process command lines or browsing arguments"}
        }}) {
            ++checks;
            if (memory_script.find(token) == std::string::npos) findings.push_back({"scripts/Measure-WebComMemory.ps1", 0, std::string(message)});
        }
    }

    const auto acceptance_script = ReadAll(root / "scripts" / "Test-WindowsAcceptance.ps1");
    if (!acceptance_script.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,15>{{
            {"webcom-windows-acceptance-v1", "Windows acceptance must emit a versioned machine-readable summary"},
            {"AllowCompilerWarnings", "Windows acceptance must fail project MSVC warnings by default with an explicit override"},
            {"WebCom-ui-tests.exe", "Windows acceptance must replay the packaged native UI harness"},
            {"WEBCOM_QA_MODE", "Windows acceptance must use the gated isolated QA launch path"},
            {"WebCom-Acceptance-", "Windows acceptance data roots must use the reserved temp-folder prefix"},
            {"msedgewebview2.exe", "Windows acceptance must require an observed WebView2 runtime process"},
            {"ProductVersion", "Windows acceptance must verify packaged ProductVersion"},
            {"FileVersion", "Windows acceptance must verify packaged FileVersion"},
            {"manifest_version", "Windows acceptance must verify the packaged blocker manifest"},
            {"Measure-WebComMemory.ps1", "Windows acceptance must optionally integrate the one-tab memory gate"},
            {"compilerWarningCount", "Windows acceptance summary must record the MSVC warning count"},
            {"build-last.log", "Windows acceptance must preserve the exact Windows build log"},
            {"repositoryPattern", "Windows acceptance warning policy must scope MSVC warning failures to repository sources"},
            {"$buildArgs += '-AllowCompilerWarnings'", "Windows acceptance diagnostic warning override must be forwarded to build.cmd"},
            {"$compilerWarnings.Count -gt 0 -and -not $AllowCompilerWarnings", "Windows acceptance must keep its post-build repository-warning gate even with native /WX"}
        }}) {
            ++checks;
            if (acceptance_script.find(token) == std::string::npos)
                findings.push_back({"scripts/Test-WindowsAcceptance.ps1", 0, std::string(message)});
        }
    }

    const auto main_source = ReadAll(root / "src" / "main.cpp");
    if (!main_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"WEBCOM_QA_MODE", "QA data-root launch must remain environment-gated"},
            {"--qa-data-root", "native Windows acceptance must retain the isolated data-root switch"},
            {"IsAcceptanceDataRootAllowed", "QA launch must validate the isolated data root before use"},
            {"WebCom.Windows.Acceptance.v01", "QA launch must use a lock isolated from normal WebCom sessions"},
            {"std::make_unique<webcom::BrowserApp>(instance, qa_data_root)", "QA launch must construct the browser with the isolated root"},
            {"if (!qa_mode) MessageBoxW", "QA startup failures must avoid modal dialogs that can hang automation"}
        }}) {
            ++checks;
            if (main_source.find(token) == std::string::npos) findings.push_back({"src/main.cpp", 0, std::string(message)});
        }
    }

    const auto startup_source = ReadAll(root / "src" / "startup_model.cpp");
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
        {"IsAcceptanceDataRootAllowed", "acceptance-root validation must remain in the portable startup model"},
        {"WebCom-Acceptance-", "acceptance-root validation must require the reserved leaf prefix"},
        {"normalized_candidate.parent_path()", "acceptance-root validation must restrict launches to direct temp children"}
    }}) {
        ++checks;
        if (startup_source.find(token) == std::string::npos) findings.push_back({"src/startup_model.cpp", 0, std::string(message)});
    }

    const auto manifest_script = ReadAll(root / "scripts" / "New-ReleaseManifest.ps1");
    const auto verify_script = ReadAll(root / "scripts" / "Verify-UpdatePackage.ps1");
    const auto matrix_script = ReadAll(root / "scripts" / "Test-ReleaseMatrix.ps1");
    const auto fresh_install_script = ReadAll(root / "scripts" / "Test-FreshInstall.ps1");
    const auto upgrade_install_script = ReadAll(root / "scripts" / "Test-UpgradeInstall.ps1");
    const auto release_candidate_script = ReadAll(root / "scripts" / "Test-ReleaseCandidate.ps1");
    const auto build_package_script = ReadAll(root / "scripts" / "Verify-BuildPackage.ps1");
    const auto signed_installer_script = ReadAll(root / "scripts" / "Build-SignedInstaller.ps1");
    const auto release_candidate_build_script = ReadAll(root / "scripts" / "Build-ReleaseCandidate.ps1");
    const auto public_release_approval_script = ReadAll(root / "scripts" / "Approve-PublicRelease.ps1");
    const auto public_release_invoke_script = ReadAll(root / "scripts" / "Invoke-PublicRelease.ps1");
    const auto release_evidence_script = ReadAll(root / "scripts" / "New-ReleaseEvidenceBundle.ps1");
    const auto published_release_script = ReadAll(root / "scripts" / "Test-PublishedRelease.ps1");
    const auto rollback_candidate_script = ReadAll(root / "scripts" / "Test-RollbackCandidate.ps1");
    const auto release_withdrawal_script = ReadAll(root / "scripts" / "Prepare-ReleaseWithdrawal.ps1");
    const auto restore_webview_script = ReadAll(root / "scripts" / "Restore-WebView2.ps1");
    const auto provenance_script = ReadAll(root / "scripts" / "New-ReleaseProvenance.ps1");
    const auto dependency_lock = ReadAll(root / "dependencies.lock.json");
    ++checks;
    if (manifest_script.find("stable channel cannot publish a prerelease version") == std::string::npos)
        findings.push_back({"scripts/New-ReleaseManifest.ps1", 0, "release manifest generation must reject Stable prerelease versions"});
    ++checks;
    if (verify_script.find("ProductVersion") == std::string::npos || verify_script.find("stable channel cannot advertise a prerelease version") == std::string::npos)
        findings.push_back({"scripts/Verify-UpdatePackage.ps1", 0, "release verification must enforce ProductVersion and Stable prerelease policy"});
    ++checks;
    if (matrix_script.find("Release-matrix policy passed") == std::string::npos || matrix_script.find("ExpectedPublisherSha256") == std::string::npos)
        findings.push_back({"scripts/Test-ReleaseMatrix.ps1", 0, "Stable/Beta release-matrix QA script is missing its signer/channel gate"});
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,2>{{
        {"webcom-release-matrix-v2", "release-matrix QA must emit a versioned machine-readable report"},
        {"OutputJson", "release-matrix QA must support a report path for Release Candidate aggregation"}
    }}) {
        ++checks;
        if (matrix_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-ReleaseMatrix.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,10>{{
        {"webcom-fresh-install-v2", "fresh-install QA must emit a versioned report"},
        {"ExpectedVersion", "fresh-install QA must verify the intended product version"},
        {"ProductVersion", "fresh-install QA must verify installed ProductVersion"},
        {"FileVersion", "fresh-install QA must verify installed FileVersion"},
        {"WEBCOM_QA_MODE", "fresh-install QA must use the isolated runtime launch path"},
        {"msedgewebview2.exe", "fresh-install QA must require an observed WebView2 runtime"},
        {"registryCleanupVerified", "fresh-install QA must record uninstall registry cleanup"},
        {"WebCom-Acceptance-FreshInstall-", "fresh-install QA must reserve an isolated temporary data root"},
        {"installed uninstaller signature status", "fresh-install QA must require a valid signed uninstaller"},
        {"uninstallerPublisherSha256", "fresh-install QA must bind the installed uninstaller to the release publisher"}
    }}) {
        ++checks;
        if (fresh_install_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-FreshInstall.ps1", 0, std::string(message)});
    }

    ++checks;
    if (fresh_install_script.find("installedExecutableSha256") == std::string::npos)
        findings.push_back({"scripts/Test-FreshInstall.ps1", 0, "fresh-install QA must record the exact installed WebCom.exe SHA-256 for signed-package identity binding"});

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"webcom-upgrade-install-v1", "upgrade QA must emit a versioned machine-readable report"},
        {"PreviousInstaller", "upgrade QA must require an explicit previous signed package"},
        {"CurrentInstaller", "upgrade QA must require the current signed package"},
        {"state.v1", "upgrade QA must verify persisted browser state across installer replacement"},
        {"profileStatePreservedByInstaller", "upgrade QA must report installer-level profile preservation"},
        {"Invoke-RuntimeSmoke", "upgrade QA must smoke-test both previous and current installed browsers"},
        {"msedgewebview2.exe", "upgrade QA must observe the WebView2 runtime on both versions"},
        {"registryCleanupVerified", "upgrade QA must record uninstall registration cleanup"},
        {"byte-identical", "upgrade QA must reject a no-op identical installer pair"}
    }}) {
        ++checks;
        if (upgrade_install_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-UpgradeInstall.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,21>{{
        {"webcom-release-candidate-v1", "Release Candidate gate must emit a versioned summary"},
        {"Test-WindowsAcceptance.ps1", "Release Candidate gate must run clean Windows/MSVC/WebView2 acceptance"},
        {"Test-ReleaseMatrix.ps1", "Release Candidate gate must verify signed channel manifests/packages"},
        {"Test-FreshInstall.ps1", "Release Candidate gate must run signed fresh-install/runtime/uninstall QA"},
        {"Test-UpgradeInstall.ps1", "Release Candidate gate must support previous-to-current upgrade QA"},
        {"WorkingSetBudgetMiB = 300", "Release Candidate memory gate must retain the 300 MiB one-tab target"},
        {"compilerWarningCount", "Release Candidate gate must fail any project MSVC warnings reported by Windows acceptance"},
        {"registryCleanupVerified", "Release Candidate gate must require installer registry cleanup"},
        {"profileStatePreservedByInstaller", "Release Candidate upgrade gate must require persisted state preservation"},
        {"readyForPublicBeta", "Release Candidate summary must retain backwards-compatible readiness metadata"},
        {"readyForPublicReleaseReview", "Release Candidate summary must explicitly authorize final public-release review"},
        {"NoPreviousSignedBuild", "Release Candidate gate must require an explicit waiver when no previous signed release exists"},
        {"signed-dist-verification.json", "Release Candidate gate must require the signed staged-package verification report"},
        {"installedExecutableSha256", "Release Candidate gate must bind installed WebCom.exe to the signed staged executable hash"},
        {"differs from the signed dist executable", "Release Candidate gate must fail installer/executable identity mismatches"},
        {"release-provenance.json", "Release Candidate gate must require release provenance beside the signed installer"},
        {"webcom.spdx.json", "Release Candidate gate must require the SPDX SBOM beside the signed installer"},
        {"webcom-release-provenance-v1", "Release Candidate gate must validate the provenance schema"},
        {"source-manifest hash does not match this source tree", "Release Candidate gate must bind provenance to the current source manifest"},
        {"dependency-lock identity is inconsistent", "Release Candidate gate must bind provenance to the current dependency lock"},
        {"SBOM identity is inconsistent", "Release Candidate gate must hash-bind the SPDX SBOM"}
    }}) {
        ++checks;
        if (release_candidate_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-ReleaseCandidate.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,29>{{
        {"webcom-build-package-v1", "build-package verification must emit a versioned machine-readable report"},
        {"Get-PeMachine", "build-package verification must inspect PE machine architecture instead of trusting filenames"},
        {"0xAA64", "build-package verification must understand ARM64 PE machine values"},
        {"0x8664", "build-package verification must understand x64 PE machine values"},
        {"ProductVersion", "build-package verification must check Windows ProductVersion"},
        {"FileVersion", "build-package verification must check Windows FileVersion"},
        {"source-hashes.sha256", "build-package verification must validate the source integrity manifest"},
        {"source file is not covered by docs/source-hashes.sha256", "source integrity verification must reject unmanifested source files"},
        {"compiler/debug artifact leaked into package", "build-package verification must reject compiler/debug artifacts"},
        {"manifest_version must be 3", "build-package verification must require the bundled Manifest V3 blocker"},
        {"RequireTestsPassed", "build-package verification must support a release test-pass requirement"},
        {"RequireUiTests", "build-package verification must support a native UI harness requirement"},
        {"RequireReleaseReady", "build-package verification must support a stricter release-ready policy"},
        {"Warnings as errors: ON", "release-ready verification must require native warnings-as-errors"},
        {"Update stable manifest: https://", "release-ready verification must require a compiled HTTPS Stable manifest endpoint"},
        {"compiled publisher pin does not match", "release-ready verification must bind compiled update trust to the expected publisher pin"},
        {"RequireSignedExecutable", "post-sign verification must support Authenticode identity checks"},
        {"dependencies.lock.json", "build-package verification must bind the staged package to the source dependency lock"},
        {"DEPENDENCY-VERIFICATION.json", "build-package verification must require packaged dependency verification evidence"},
        {"webcom-dependency-verification-v1", "dependency verification evidence must use a versioned schema"},
        {"officialHashVerified", "release-ready package verification must require official dependency hash evidence"},
        {"packageSha512Base64", "release-ready package verification must retain the verified WebView2 package SHA512"},
        {"Verified release dependencies: REQUIRED", "release-ready package verification must prove the build required verified dependencies"},
        {"Get-PeSecurityFlags", "build-package verification must inspect actual PE mitigation flags"},
        {"highEntropyVa", "release-ready package verification must require HIGHENTROPYVA in the built PE"},
        {"dynamicBase", "release-ready package verification must require DYNAMICBASE in the built PE"},
        {"nxCompat", "release-ready package verification must require NXCOMPAT in the built PE"},
        {"guardCf", "release-ready package verification must require Guard CF in the built PE"},
        {"public release document $publicDocument", "release-ready staging must include privacy/security/third-party public documents"}
    }}) {
        ++checks;
        if (build_package_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Verify-BuildPackage.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"Verify-BuildPackage.ps1", "signed-installer creation must run the strict staged-package verifier"},
        {"pre-sign package integrity verification", "signed-installer creation must fail package integrity before signing"},
        {"post-sign package integrity verification", "signed-installer creation must reverify the package after executable signing"},
        {"signed-dist-verification.json", "signed-installer creation must archive a post-sign package verification report"},
        {"-RequireSignedExecutable", "post-sign package verification must require Authenticode identity"},
        {"innoSignCommand", "signed-installer creation must provide Inno a sign tool for Setup/self-copies/uninstaller"},
        {"--signtool=$innoSignCommand", "Inno compilation must receive the release sign-tool definition"},
        {"Inno Setup 7", "signed-installer creation must auto-detect the current Inno Setup 7 release line"},
        {"Inno Setup 6", "signed-installer creation must retain Inno Setup 6 fallback compatibility"}
    }}) {
        ++checks;
        if (signed_installer_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Build-SignedInstaller.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
        {"Build-PublicBeta.ps1", "canonical Release Candidate build wrapper must retain the backwards-compatible implementation"},
        {"@PSBoundParameters", "Release Candidate wrapper must forward the exact caller-supplied release parameters"},
        {"Build-ReleaseCandidate.ps1", "Release Candidate source must expose the canonical signed-candidate workflow"}
    }}) {
        ++checks;
        if (release_candidate_build_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Build-ReleaseCandidate.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,15>{{
        {"webcom-public-release-v1", "public-release promotion must emit versioned approval evidence"},
        {"readyForPublicReleaseReview", "public-release promotion must require an RC explicitly ready for final review"},
        {"memoryGate.required", "public-release promotion must reject a skipped memory gate"},
        {"workingSetBudgetMiB", "public-release promotion must enforce the approved one-tab memory budget"},
        {"machineInstall", "public-release promotion must require elevated machine-install QA as well as per-user QA"},
        {"waived-no-previous-signed-build", "public-release promotion may waive upgrade QA only for a first signed release"},
        {"Get-AuthenticodeSignature", "public-release promotion must revalidate the final installer signature"},
        {"ExpectedPublisherSha256", "public-release promotion must bind approval to the publisher certificate pin"},
        {"Require-PublicHttps", "public-release promotion must require real public HTTPS operational URLs"},
        {"LicenseMode", "public-release promotion must require an explicit distribution-license decision"},
        {"OpenSource publication requires an explicit LicenseFile", "open-source publication must require an explicit non-empty license"},
        {"PUBLIC-RELEASE.json", "public-release promotion must create an unambiguous approval artifact"},
        {"PUBLIC-RELEASE-SHA256SUMS.txt", "public-release promotion must emit user-verifiable release checksums"},
        {"approvedForPublicRelease = $true", "approval evidence must explicitly mark the exact Stable artifacts approved"},
        {"does not upload or publish release artifacts", "public-release approval must remain a local evidence gate rather than silently publishing"}
    }}) {
        ++checks;
        if (public_release_approval_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Approve-PublicRelease.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,11>{{
        {"webcom-public-release-run-v1", "one-command release orchestration must emit a versioned local run report"},
        {"Build-ReleaseCandidate.ps1", "one-command release orchestration must build the exact signed candidate"},
        {"Test-ReleaseCandidate.ps1", "one-command release orchestration must run the real Windows RC gates"},
        {"RunMachineInstall=$true", "one-command public release must require elevated machine-install acceptance"},
        {"WorkingSetBudgetMiB=$WorkingSetBudgetMiB", "one-command public release must keep the mandatory memory budget in the RC call"},
        {"Approve-PublicRelease.ps1", "one-command release orchestration must run the final promotion gate"},
        {"New-ReleaseEvidenceBundle.ps1", "one-command release orchestration must archive immutable evidence"},
        {"publicationPerformed=$false", "one-command release orchestration must record that it never published automatically"},
        {"NoPreviousSignedBuild", "one-command release orchestration must require explicit previous-build or first-release policy"},
        {"WindowsBuiltInRole]::Administrator", "one-command public release must require elevation for machine-install QA"},
        {"Test-PublishedRelease.ps1", "one-command release orchestration must direct operators to post-publication verification"}
    }}) {
        ++checks;
        if (public_release_invoke_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Invoke-PublicRelease.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,10>{{
        {"webcom-release-evidence-bundle-v1", "release evidence archive must have a versioned manifest"},
        {"PUBLIC-RELEASE.json.sha256", "release evidence archive must validate the public-release approval checksum"},
        {"PUBLIC-RELEASE-SHA256SUMS.txt", "release evidence archive must preserve the user-facing release checksum list"},
        {"approval.artifacts.installer.path", "release evidence archive must include the exact approved installer"},
        {"approval.artifacts.stableManifest.path", "release evidence archive must include the exact approved Stable manifest"},
        {"source-hashes.sha256", "release evidence archive must include the frozen source-manifest identity"},
        {"dependencies.lock.json", "release evidence archive must include the dependency lock"},
        {"EVIDENCE-MANIFEST.json", "release evidence archive must emit an internal hash manifest"},
        {"Compress-Archive", "release evidence archive must create a portable immutable evidence ZIP"},
        {"copied evidence hash changed", "release evidence archive must rehash every copied item before compression"}
    }}) {
        ++checks;
        if (release_evidence_script.find(token) == std::string::npos)
            findings.push_back({"scripts/New-ReleaseEvidenceBundle.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
        {"webcom-published-release-health-v1", "post-publication health verification must emit a versioned report"},
        {"PublicHttps", "post-publication verification must reject non-public/non-HTTPS endpoints"},
        {"published Stable manifest differs from PUBLIC-RELEASE approval", "live Stable manifest must hash-match final approval"},
        {"published installer differs from PUBLIC-RELEASE approval", "live installer must hash-match final approval"},
        {"published installer hash differs from published manifest", "live installer must also match the live manifest hash"},
        {"Get-AuthenticodeSignature", "post-publication verification must revalidate Authenticode"},
        {"published installer publisher differs from PUBLIC-RELEASE approval", "post-publication verification must revalidate publisher identity"},
        {"publicUrls.releaseNotes", "post-publication verification must probe the public release-notes endpoint"},
        {"request redirected away from HTTPS", "post-publication verification must fail HTTPS downgrade redirects"},
        {"PassThru=$true", "installer downloads must preserve response metadata while writing to disk"},
        {"published-release-health.json", "post-publication verification must persist a local health report"},
        {"$output + '.sha256'", "post-publication health evidence must receive its own checksum"}
    }}) {
        ++checks;
        if (published_release_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-PublishedRelease.ps1", 0, std::string(message)});
    }


    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
        {"webcom-rollback-candidate-v1", "rollback validation must emit a versioned evidence report"},
        {"approved Stable WebCom public release", "rollback validation must require prior PUBLIC-RELEASE approval"},
        {"rollback target $previousVersion must be older", "rollback target must be strictly older than the current release"},
        {"different publisher certificate", "rollback validation must reject publisher-certificate changes rather than weakening the pin"},
        {"previous installer SHA-256 differs", "rollback validation must hash-bind the previous installer to its approval"},
        {"previous Stable manifest SHA-256 differs", "rollback validation must hash-bind the previous manifest to its approval"},
        {"Get-AuthenticodeSignature", "rollback validation must revalidate Authenticode on the previous installer"},
        {"previous manifest package URL", "rollback validation must require a public HTTPS package URL"},
        {"canDowngradeAlreadyInstalledCurrentVersion=$false", "rollback evidence must state that a lower manifest cannot downgrade already-installed current clients"},
        {"higher-version hotfix", "rollback evidence must direct affected current clients to verified rollback or a higher-version hotfix"},
        {"This validation does not publish", "rollback validation must remain evidence-only and non-destructive"},
        {"samePublisherRequired=$true", "rollback report must record the same-publisher policy"}
    }}) {
        ++checks;
        if (rollback_candidate_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Test-RollbackCandidate.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,13>{{
        {"webcom-release-withdrawal-v1", "release withdrawal must emit a versioned plan"},
        {"webcom-incident-evidence-v1", "release withdrawal must emit a versioned incident evidence manifest"},
        {"WithdrawOnly","release withdrawal must support a withdraw-only path"},
        {"RestoreLastKnownGood","release withdrawal must support restoring a previously approved release"},
        {"Test-RollbackCandidate.ps1","restore preparation must invoke the strict rollback-candidate validator"},
        {"publicationPerformed=$false","incident preparation must record that it never modifies public hosting automatically"},
        {"published health report refers to a different PUBLIC-RELEASE approval","incident evidence must reject unrelated live-health reports"},
        {"stable-restore.v2.txt","restore preparation must stage the exact prior approved Stable manifest"},
        {"lowerVersionManifestCannotDowngradeAlreadyInstalledCurrentRelease=$true","withdrawal evidence must disclose lower-version downgrade limitations"},
        {"RELEASE-WITHDRAWAL.json.sha256","withdrawal plan must receive its own checksum"},
        {"INCIDENT-EVIDENCE.json","incident bundle must carry a rehashed evidence manifest"},
        {"Compress-Archive","incident response must create a portable immutable evidence bundle"},
        {"No website, CDN, manifest endpoint, installer, or client was changed automatically","incident tooling must remain manual/fail-safe at the publication boundary"}
    }}) {
        ++checks;
        if (release_withdrawal_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Prepare-ReleaseWithdrawal.ps1", 0, std::string(message)});
    }

    const auto cmake_source = ReadAll(root / "CMakeLists.txt");
    ++checks;
    if (cmake_source.find("webcom_performance_preferences_tests") == std::string::npos ||
        cmake_source.find("src/performance_preferences.cpp") == std::string::npos)
        findings.push_back({"CMakeLists.txt", 0, "Performance preferences must stay covered by portable tests and the native browser target"});
    ++checks;
    if (cmake_source.find("webcom_download_history_model_tests") == std::string::npos ||
        cmake_source.find("src/download_history_model.cpp") == std::string::npos)
        findings.push_back({"CMakeLists.txt", 0, "Downloads v2 retention/retry policy must stay covered by portable tests and the native browser target"});
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"option(WEBCOM_WARNINGS_AS_ERRORS", "native warning-as-error policy must be an explicit CMake option"},
        {"if(MSVC AND WEBCOM_WARNINGS_AS_ERRORS)", "native MSVC warning failures must be enabled by default but diagnostically switchable"},
        {"$<$<COMPILE_LANGUAGE:CXX>:/WX>", "WebCom native targets must compile project warnings as errors by default"},
        {"/external:anglebrackets;/external:W0", "third-party/system angle-bracket headers must not turn external warnings into WebCom /WX failures"},
        {";/GS;/guard:cf;", "native C++ targets must explicitly retain stack-cookie and Control Flow Guard compilation"},
        {"/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf /CETCOMPAT", "native link targets must retain ASLR, DEP, CFG and CET compatibility flags"},
        {"src/security_baseline_model.cpp", "native browser targets must compile the release security-baseline policy"},
        {"src/security_bootstrap_win32.cpp", "native browser targets must compile the Windows security bootstrap"},
        {"webcom_security_baseline_model_tests", "release security-baseline policy must retain portable regression coverage"}
    }}) {
        ++checks;
        if (cmake_source.find(token) == std::string::npos) findings.push_back({"CMakeLists.txt", 0, std::string(message)});
    }

    const auto readme_source = ReadAll(root / "README.md");
    const auto build_script = ReadAll(root / "scripts" / "Build.ps1");
    ++checks;
    if (build_script.find("Test-WindowsAcceptance.ps1") == std::string::npos)
        findings.push_back({"scripts/Build.ps1", 0, "Windows packages must include the acceptance runner used for release sign-off"});
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,13>{{
        {"[switch]$AllowCompilerWarnings", "normal Windows builds must expose an explicit diagnostic-only warning override"},
        {"-DWEBCOM_WARNINGS_AS_ERRORS=$(if ($AllowCompilerWarnings) { 'OFF' } else { 'ON' })", "Build.ps1 must keep /WX enabled unless the diagnostic override is explicit"},
        {"Diagnostic mode: WebCom native /WX is disabled", "warning override must visibly disclose that release warning policy is disabled"},
        {"Do not use this switch for Release Candidate or public-release sign-off", "warning override must not be presented as release-safe"},
        {"Verifying staged Windows package integrity", "Build.ps1 must verify the staged package before ZIP creation"},
        {"Verify-BuildPackage.ps1", "Build.ps1 must use the strict package integrity verifier"},
        {"PACKAGE-VERIFICATION.json", "Build.ps1 must archive package integrity evidence in the portable package"},
        {"[switch]$RequireVerifiedDependencies", "release builds must support an explicit strict dependency-verification policy"},
        {"DEPENDENCY-VERIFICATION.json", "Build.ps1 must carry dependency verification into the staged package"},
        {"Verified release dependencies:", "BUILD-INFO must record whether official dependency verification was required"},
        {"SECURITY.md", "staged Windows packages must carry the public vulnerability-reporting policy"},
        {"PRIVACY.md", "staged Windows packages must carry the public privacy statement"},
        {"Native security hardening:", "BUILD-INFO must record the native process/PE security baseline"}
    }}) {
        ++checks;
        if (build_script.find(token) == std::string::npos) findings.push_back({"scripts/Build.ps1", 0, std::string(message)});
    }
    const auto security_model_source = ReadAll(root / "src" / "security_baseline_model.cpp");
    const auto security_model_header = ReadAll(root / "src" / "security_baseline_model.h");
    const auto security_bootstrap_source = ReadAll(root / "src" / "security_bootstrap_win32.cpp");
    const auto security_policy_doc = ReadAll(root / "SECURITY.md");
    const auto privacy_doc = ReadAll(root / "PRIVACY.md");
    const auto contributing_doc = ReadAll(root / "CONTRIBUTING.md");
    const auto support_doc = ReadAll(root / "SUPPORT.md");
    const auto conduct_doc = ReadAll(root / "CODE_OF_CONDUCT.md");
    const auto pull_request_template = ReadAll(root / ".github" / "PULL_REQUEST_TEMPLATE.md");
    const auto public_release_doc = ReadAll(root / "docs" / "PUBLIC-RELEASE.md");
    const auto post_release_doc = ReadAll(root / "docs" / "POST-RELEASE.md");
    const auto incident_response_doc = ReadAll(root / "docs" / "INCIDENT-RESPONSE.md");
    const auto background_updates_doc = ReadAll(root / "docs" / "BACKGROUND-UPDATES.md");
    const auto github_workflow = ReadAll(root / ".github" / "workflows" / "windows.yml");
    const auto dependabot_config = ReadAll(root / ".github" / "dependabot.yml");
    const auto gitignore_source = ReadAll(root / ".gitignore");
    const auto gitattributes_source = ReadAll(root / ".gitattributes");
    for (const auto& [source, token, file, message] : std::array<std::tuple<std::string_view,std::string_view,std::string_view,std::string_view>,35>{{
        {security_model_header, "safe_dll_search = true", "src/security_baseline_model.h", "security policy must require safe DLL search"},
        {security_bootstrap_source, "SetDefaultDllDirectories", "src/security_bootstrap_win32.cpp", "Windows startup must remove unsafe default DLL-search locations"},
        {security_bootstrap_source, "HeapEnableTerminationOnCorruption", "src/security_bootstrap_win32.cpp", "Windows startup must request fail-closed heap-corruption handling"},
        {security_bootstrap_source, "ProcessImageLoadPolicy", "src/security_bootstrap_win32.cpp", "Windows startup must set image-load mitigations"},
        {security_bootstrap_source, "NoRemoteImages", "src/security_bootstrap_win32.cpp", "process image policy must reject remote images"},
        {security_bootstrap_source, "NoLowMandatoryLabelImages", "src/security_bootstrap_win32.cpp", "process image policy must reject low-integrity images"},
        {main_source, "ApplyProcessSecurityBaseline", "src/main.cpp", "process security baseline must execute before browser initialization"},
        {security_policy_doc, "GitHub Private Vulnerability Reporting", "SECURITY.md", "public source must document private vulnerability reporting"},
        {privacy_doc, "does **not** include a WebCom analytics or telemetry upload service", "PRIVACY.md", "public privacy statement must disclose the no-WebCom-telemetry behavior"},
        {privacy_doc, "Microsoft Edge WebView2 Runtime", "PRIVACY.md", "privacy statement must identify the WebView2 privacy boundary"},
        {privacy_doc, "WebCom/3.0.0 updater", "PRIVACY.md", "privacy statement must disclose background update network metadata"},
        {background_updates_doc, "six-hour cadence", "docs/BACKGROUND-UPDATES.md", "background update documentation must disclose the normal check cadence"},
        {background_updates_doc, "Installing an update is never automatic.", "docs/BACKGROUND-UPDATES.md", "background update documentation must preserve explicit installer launch"},
        {background_updates_doc, "first two failures receive quick retries", "docs/BACKGROUND-UPDATES.md", "background update documentation must disclose bounded retry behavior"},
        {contributing_doc, "Do not weaken", "CONTRIBUTING.md", "contributor policy must protect release/security gates"},
        {support_doc, "Security issues", "SUPPORT.md", "public source must route security reports away from ordinary support issues"},
        {conduct_doc, "constructive technical collaboration", "CODE_OF_CONDUCT.md", "public repository must document baseline community conduct expectations"},
        {pull_request_template, "Security/privacy/release implications were reviewed", ".github/PULL_REQUEST_TEMPLATE.md", "public pull requests must prompt contributors to review release/security/privacy impact"},
        {public_release_doc, "Canonical three-phase release flow", "docs/PUBLIC-RELEASE.md", "public release guide must document build, Windows RC, and final promotion as separate phases"},
        {public_release_doc, "Human sign-off that code cannot automate", "docs/PUBLIC-RELEASE.md", "public release guide must identify ownership/legal/operational sign-off"},
        {post_release_doc, "One-command local release validation", "docs/POST-RELEASE.md", "post-release guide must document the fail-closed one-command local ceremony"},
        {post_release_doc, "After manual publication", "docs/POST-RELEASE.md", "post-release guide must require live manifest/installer verification after manual publication"},
        {incident_response_doc, "Prepare-ReleaseWithdrawal.ps1", "docs/INCIDENT-RESPONSE.md", "incident runbook must document the non-destructive withdrawal workflow"},
        {incident_response_doc, "Test-RollbackCandidate.ps1", "docs/INCIDENT-RESPONSE.md", "incident runbook must document last-known-good rollback validation"},
        {incident_response_doc, "does **not** automatically downgrade", "docs/INCIDENT-RESPONSE.md", "incident runbook must disclose the lower-version manifest downgrade limitation"},
        {incident_response_doc, "Do not weaken version checks or publisher verification", "docs/INCIDENT-RESPONSE.md", "incident runbook must forbid bypassing release security to force rollback"},
        {github_workflow, "portable-sanitizers", ".github/workflows/windows.yml", "CI must retain ASan/UBSan portable regression coverage"},
        {github_workflow, "persist-credentials: false", ".github/workflows/windows.yml", "CI checkout must not retain unnecessary repository credentials"},
        {dependabot_config, "package-ecosystem: github-actions", ".github/dependabot.yml", "GitHub Actions dependencies must have update monitoring"},
        {github_workflow, "node --test tests/ad_blocker_tests.cjs", ".github/workflows/windows.yml", "portable CI must run bundled blocker script tests"},
        {gitignore_source, "/build-*/", ".gitignore", "public repository must ignore the actual portable/sanitizer build directories"},
        {gitignore_source, "/release/", ".gitignore", "public repository must not accidentally commit signed release output"},
        {gitignore_source, "/acceptance/", ".gitignore", "public repository must not accidentally commit local acceptance evidence"},
        {gitattributes_source, "*.cmd text eol=lf", ".gitattributes", "text files must keep deterministic LF bytes so source-manifest hashes survive cross-platform Git checkouts"},
        {gitattributes_source, "*.png binary", ".gitattributes", "binary assets must be protected from line-ending conversion"}
    }}) {
        ++checks;
        if (source.find(token) == std::string::npos) findings.push_back({std::string(file), 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,7>{{
        {"webcom-dependencies-v1", "dependency lock must use a versioned schema"},
        {"Microsoft.Web.WebView2", "dependency lock must name the WebView2 NuGet package"},
        {"1.0.4314.35", "dependency lock must pin the current WebView2 SDK version"},
        {"api.nuget.org/v3-flatcontainer", "dependency lock must pin the official HTTPS NuGet package source"},
        {"registration5-gz-semver2", "dependency lock must identify an official NuGet registration hash source"},
        {"SHA512", "dependency lock must declare the official package hash algorithm"},
        {"releaseRequiresOfficialCatalogHash", "dependency lock must state that public releases require official catalog hash evidence"}
    }}) {
        ++checks;
        if (dependency_lock.find(token) == std::string::npos)
            findings.push_back({"dependencies.lock.json", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"dependencies.lock.json", "WebView2 restore must consume the source dependency lock"},
        {"[switch]$RequireOfficialHash", "WebView2 restore must support strict official-hash mode"},
        {"Release dependency verification cannot use -SdkDir", "strict release dependency verification must reject unverifiable offline SDK directories"},
        {"packageHashAlgorithm", "WebView2 restore must read official NuGet catalog hash metadata"},
        {"package SHA512 does not match NuGet catalog metadata", "WebView2 restore must fail package/catalog SHA512 mismatches"},
        {"requires the official NuGet catalog SHA512", "strict release restore must fail if official hash metadata is unavailable"},
        {"webcom-dependency-verification-v1", "WebView2 restore must emit versioned dependency evidence"},
        {"packageSha512Base64", "dependency evidence must record the actual WebView2 package SHA512"},
        {"lockSha256", "dependency evidence must bind back to dependencies.lock.json"}
    }}) {
        ++checks;
        if (restore_webview_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Restore-WebView2.ps1", 0, std::string(message)});
    }

    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,10>{{
        {"webcom-release-provenance-v1", "release provenance must use a versioned schema"},
        {"SPDX-2.3", "release generation must emit an SPDX 2.3 SBOM"},
        {"docs/source-hashes.sha256", "provenance must bind the release to the source integrity manifest"},
        {"dependencies.lock.json", "provenance must bind the release to the dependency lock"},
        {"packageSha512Base64", "provenance must include verified dependency package identity"},
        {"signedDist", "provenance must bind the signed staged executable report"},
        {"installerSha256", "provenance must bind the signed installer hash"},
        {"manifestSha256", "provenance must bind the release-manifest hash"},
        {"sbomSha256", "provenance must hash-bind the generated SBOM"},
        {"DEPENDS_ON", "SPDX output must express WebCom's WebView2 dependency relationship"}
    }}) {
        ++checks;
        if (provenance_script.find(token) == std::string::npos)
            findings.push_back({"scripts/New-ReleaseProvenance.ps1", 0, std::string(message)});
    }

    const auto public_beta_script = ReadAll(root / "scripts" / "Build-PublicBeta.ps1");
    ++checks;
    if (public_beta_script.find("Test-ReleaseCandidate.ps1") == std::string::npos ||
        public_beta_script.find("Do not publish yet") == std::string::npos)
        findings.push_back({"scripts/Build-PublicBeta.ps1", 0, "signed release generation must explicitly defer publishing until the Release Candidate gate passes"});
    ++checks;
    if (public_beta_script.find("signed-dist-verification.json") == std::string::npos ||
        public_beta_script.find("Build verification:") == std::string::npos)
        findings.push_back({"scripts/Build-PublicBeta.ps1", 0, "legacy-compatible Release Candidate builder must retain and surface the signed staged-package verification report"});
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
        {"-RequireVerifiedDependencies", "Release Candidate builds must require official dependency verification"},
        {"New-ReleaseProvenance.ps1", "Release Candidate generation must emit provenance and SBOM evidence"},
        {"release-provenance.json", "Release Candidate generation must surface the provenance artifact"},
        {"webcom.spdx.json", "Release Candidate generation must surface the SPDX SBOM"}
    }}) {
        ++checks;
        if (public_beta_script.find(token) == std::string::npos)
            findings.push_back({"scripts/Build-PublicBeta.ps1", 0, std::string(message)});
    }
    const auto resource_script = ReadAll(root / "resources" / "webcom.rc");
    const auto installer_script = ReadAll(root / "installer" / "WebCom.iss");
    for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,9>{{
        {"PrivilegesRequired=lowest", "public installer must default to least-privilege per-user installation"},
        {"PrivilegesRequiredOverridesAllowed=dialog commandline", "installer must retain explicit elevated machine-install override"},
        {"MinVersion=10.0", "public installer must reject unsupported pre-Windows-10 systems"},
        {"SetupIconFile=..\\resources\\webcom.ico", "public installer must use the WebCom brand icon"},
        {"PRIVACY.md", "installed application must carry the public privacy statement"},
        {"SECURITY.md", "installed application must carry the vulnerability-reporting policy"},
        {"THIRD-PARTY-NOTICES.md", "installed application must carry third-party notices"},
        {"SignTool=webcom", "Inno Setup must sign Setup/self-copies/uninstaller during compilation"},
        {"SignedUninstaller=yes", "public installer must deploy a signed uninstaller"}
    }}) {
        ++checks;
        if (installer_script.find(token) == std::string::npos) findings.push_back({"installer/WebCom.iss", 0, std::string(message)});
    }
    ++checks;
    if (installer_script.find("[UninstallDelete]") != std::string::npos || installer_script.find("filesandordirs; Name: \"{app}\"") != std::string::npos)
        findings.push_back({"installer/WebCom.iss", 0, "installer must not blanket-delete a custom install directory during uninstall"});
    for (const auto& [text, token, file, message] : std::array<std::tuple<std::string_view,std::string_view,std::string_view,std::string_view>,7>{{
        {cmake_source, "project(WebCom VERSION 3.0.0", "CMakeLists.txt", "current WebCom product version must remain 3.0.0"},
        {app_source, "kWebComVersion = \"3.0.0\"", "src/app.cpp", "runtime product version must remain 3.0.0"},
        {readme_source, "WebCom v3", "README.md", "public README must identify the current product generation as WebCom v3"},
        {readme_source, "machine-readable semantic versioning uses **3.0.0**", "README.md", "README must document v3/3.0.0 versioning policy"},
        {build_script, "WebCom-v3-Windows-", "scripts/Build.ps1", "portable package names must identify WebCom v3"},
        {resource_script, "VALUE \"ProductVersion\", \"3.0.0", "resources/webcom.rc", "Windows version resources must identify WebCom 3.0.0"},
        {installer_script, "#define AppVersion \"3.0.0\"", "installer/WebCom.iss", "installer default product version must identify WebCom 3.0.0"}
    }}) {
        ++checks;
        if (text.find(token) == std::string::npos) findings.push_back({std::string(file), 0, std::string(message)});
    }

    const auto core_source = ReadAll(root / "src" / "core.cpp");
    if (!core_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,8>{{
            {"SleepTimeout::OneMinute:return 60'000", "portable sleep model must preserve the 1-minute threshold"},
            {"SleepTimeout::FifteenMinutes:return 900'000", "portable sleep model must preserve the 15-minute threshold"},
            {"SleepTimeout::ThirtyMinutes:return 1'800'000", "portable sleep model must preserve the 30-minute threshold"},
            {"SleepTimeout::Never:return std::numeric_limits<std::uint64_t>::max()", "portable sleep model must represent Never without a finite timeout"},
            {"if(timeout==SleepTimeout::Never)return false", "Never must fail closed before elapsed-time arithmetic"},
            {"PerformanceTickIntervalMs(SleepTimeout timeout)", "adaptive performance polling must live in the portable core model"},
            {"case SleepTimeout::FiveMinutes:return 30'000", "balanced five-minute sleeping should reduce idle polling to 30 seconds"},
            {"case SleepTimeout::Never:return 0", "Never-sleep mode must disable automatic performance polling"}
        }}) {
            ++checks;
            if (core_source.find(token) == std::string::npos)
                findings.push_back({"src/core.cpp", 0, std::string(message)});
        }
    }

    const auto performance_preferences = ReadAll(root / "src" / "performance_preferences.cpp");
    if (!performance_preferences.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
            {R"(WEBCOM_PERFORMANCE\t1)", "Performance preferences must use an explicit versioned local format"},
            {"IsValidProfileName(profile)", "Performance preferences must reuse the hardened profile-name policy"},
            {"AtomicWrite", "Performance preferences must persist atomically"},
            {"MoveFileExW", "Windows Performance preference replacement must be atomic and write-through"},
            {"0600", "POSIX Performance preference temporary files must stay owner-only"},
            {"SleepTimeout::ThirtyMinutes", "Performance preferences must preserve the 30-minute option"},
            {"SleepTimeout::Never", "Performance preferences must preserve the Never option"},
            {"kMaxNeverSleepHosts", "Never sleep site preferences must retain a hard upper bound"},
            {"NormalizeHostInternal", "Never sleep site persistence must normalize and validate host-only values"},
            {R"(line.starts_with("never\t"))", "Performance preference loading must preserve Never sleep site records"},
            {R"(content += "never\t" + host)", "Performance preference serialization must preserve Never sleep site records"},
            {"std::find(emitted.begin(), emitted.end(), host)", "Never sleep site serialization must deduplicate normalized hosts"}
        }}) {
            ++checks;
            if (performance_preferences.find(token) == std::string::npos)
                findings.push_back({"src/performance_preferences.cpp", 0, std::string(message)});
        }
    }

    const auto download_history_model = ReadAll(root / "src" / "download_history_model.cpp");
    if (!download_history_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"record.state != DownloadHistoryState::Completed", "automatic download-history cleanup must never remove failed or active records"},
            {"now_ms - record.started_ms <= retention_ms", "completed download-history retention must use bounded elapsed-time logic"},
            {"maximum = std::min<std::size_t>(maximum, 8)", "bulk failed-download retry must retain a hard upper bound"},
            {"record.state == DownloadHistoryState::Interrupted", "bulk retry candidates must be failed/interrupted records"},
            {"!record.paused && record.has_source", "bulk retry must exclude paused records and records without a source"}
        }}) {
            ++checks;
            if (download_history_model.find(token) == std::string::npos)
                findings.push_back({"src/download_history_model.cpp", 0, std::string(message)});
        }
    }

    const auto tab_batch_model = ReadAll(root / "src" / "tab_batch_model.cpp");
    if (!tab_batch_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"DuplicateTabCloseCandidates", "duplicate-tab cleanup must use the portable bounded selection policy"},
            {"item.workspace != workspace", "duplicate-tab cleanup must stay workspace-scoped"},
            {"item.active || item.pinned || item.has_download", "duplicate-tab cleanup must protect active, pinned and downloading tabs"},
            {"item.private_mode", "duplicate matching must keep private and normal tabs distinct"},
            {"return item.url +", "duplicate matching must preserve URL path/query case"}
        }}) {
            ++checks;
            if (tab_batch_model.find(token) == std::string::npos) findings.push_back({"src/tab_batch_model.cpp", 0, std::string(message)});
        }
    }

    const auto home_model = ReadAll(root / "src" / "home_model.cpp");
    if (!home_model.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"BuildHomeRecentSites", "native New Tab recent sites must use the bounded portable model"},
            {"std::unordered_set<std::wstring> seen", "recent sites must deduplicate repeated hosts"},
            {"maximum = std::min<std::size_t>(maximum, 12)", "recent-site model must keep a hard upper bound"},
            {"HomeDisplayHost", "recent-site cards must derive a display host without exposing full URLs"}
        }}) {
            ++checks;
            if (home_model.find(token) == std::string::npos) findings.push_back({"src/home_model.cpp", 0, std::string(message)});
        }
    }

    const auto home_preferences = ReadAll(root / "src" / "home_preferences.cpp");
    if (!home_preferences.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"kMaxHomeShortcuts", "custom New Tab shortcuts must retain a hard upper bound"},
            {"WEBCOM_HOME_PREFERENCES", "New Tab preferences must use an explicit versioned local format"},
            {"AtomicWrite", "New Tab preferences must persist atomically"},
            {"ResolveAddress", "custom shortcut persistence must validate browser-safe addresses"},
            {"kMaxHiddenRecentHosts", "hidden recent-site preferences must stay bounded"},
            {"IsHomeRecentHostHidden", "recent-site hiding must remain a non-destructive preference rather than deleting history"}
        }}) {
            ++checks;
            if (home_preferences.find(token) == std::string::npos) findings.push_back({"src/home_preferences.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,3>{{
            {"value == L\"webcom:passwords\"", "webcom:passwords must remain routable as a native internal page"},
            {"case InternalPage::Passwords: return L\"Passwords & Autofill\"", "password manager internal page must retain its user-facing title"},
            {"case PasswordMenu: OpenInternalPage(L\"webcom:passwords\")", "Password settings entry must open the native manager page"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto history_model_source = ReadAll(root / "src" / "history_model.cpp");
    if (!history_model_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"HistoryHost", "History v2 must retain a normalized host extractor"},
            {"BuildHistoryHostSummaries", "History v2 must retain bounded top-site aggregation"},
            {"maximum = std::min<std::size_t>(maximum, 16)", "History top-site aggregation must stay bounded"},
            {"DeleteHistoryHost", "History v2 must retain whole-site cleanup without touching other sites"}
        }}) {
            ++checks;
            if (history_model_source.find(token) == std::string::npos) findings.push_back({"src/history_model.cpp", 0, std::string(message)});
        }
    }

    const auto memory_model_source = ReadAll(root / "src" / "memory_snapshot_model.cpp");
    if (!memory_model_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"Total working set", "memory snapshot must report total working set"},
            {"WebCom host:", "memory snapshot must split host memory from WebView2"},
            {"WebView2 group:", "memory snapshot must report WebView2 process-group memory"},
            {"PARTIAL RESULT", "memory snapshot must fail visibly when the runtime process list is unavailable"},
            {"Automatic sleep:", "memory snapshot must include the active automatic-sleep policy"},
            {"Never-sleep sites:", "memory snapshot must include the per-profile never-sleep exception count"}
        }}) {
            ++checks;
            if (memory_model_source.find(token) == std::string::npos) findings.push_back({"src/memory_snapshot_model.cpp", 0, std::string(message)});
        }
    }
    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"value == L\"webcom:about\"", "webcom:about must remain routable as a native internal page"},
            {"case InternalPage::About: return L\"About WebCom\"", "About page must retain its native user-facing title"},
            {"case About: OpenInternalPage(L\"webcom:about\")", "About command must open the native About page instead of a legacy message box"},
            {"MemorySnapshot snapshot", "Windows memory collection must populate the portable memory snapshot model"},
            {"BuildMemorySnapshotText(snapshot)", "Windows memory report must format through the tested portable model"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto password_ui_source = ReadAll(root / "src" / "native_ui.inl");
    if (!password_ui_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"InternalPage::Passwords", "Passwords & Autofill must remain a first-class native internal page"},
            {"Credential values stay in the WebView2 profile store", "credential manager must disclose WebView2 ownership of saved values"},
            {"Clear saved passwords...", "credential manager must expose confirmation-gated password clearing"},
            {"Clear autofill form data...", "credential manager must expose confirmation-gated autofill clearing"},
            {"WebCom does not read, list, export, or display saved password values", "credential manager must not imply password enumeration/decryption support"}
        }}) {
            ++checks;
            if (password_ui_source.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
    }

    const auto failure_ui_source = ReadAll(root / "src" / "native_ui.inl");
    if (!failure_ui_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"L\"This page couldn’t be opened\"", "failed tabs must retain a clear native recovery heading"},
            {"Reload,L\"Try again\"", "failed tabs must expose a native retry action"},
            {"FocusAddress,L\"Edit address\"", "failed tabs must expose a native address-edit recovery action"},
            {"NewTab,L\"New Tab\"", "failed tabs must expose a native escape-to-new-tab action"},
            {"a broken site cannot replace the browser recovery controls", "failed-page copy must retain the native browser-control trust boundary"}
        }}) {
            ++checks;
            if (failure_ui_source.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"for(auto& item:tabs_)if(IsSplitVisible(*item))SetVisible(*item,true);", "tab handoff must reveal destination WebView controllers before hiding outgoing controllers"},
            {"ChromeTabSelectionDuration(changed,ReducedMotionActive())", "tab selection animation must honor effective profile/Windows reduced motion and skip same-tab transitions"},
            {"ChromeShouldAnimateReflow(!reflow_origins.empty(),ReducedMotionActive())", "tab reflow animation must be gated by geometry changes and effective reduced motion"},
            {"RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW", "omnibox suggestion reveal must paint its owner-drawn buffer at final bounds without an intermediate erase"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos) findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    const auto dark_menu_source = ReadAll(root / "src" / "dark_menu.cpp");
    const auto dark_menu_header = ReadAll(root / "src" / "dark_menu.h");
    if (!dark_menu_source.empty() && !dark_menu_header.empty()) {
        ++checks;
        if (dark_menu_header.find("const Palette* palette = nullptr, bool reduce_motion = false") == std::string::npos)
            findings.push_back({"src/dark_menu.h", 0, "native menu API must expose a reduced-motion switch without breaking existing callers"});
        ++checks;
        if (dark_menu_source.find("const bool allow_animation = !reduce_motion && system_animate && !GetSystemMetrics(SM_REMOTESESSION);") == std::string::npos)
            findings.push_back({"src/dark_menu.cpp", 0, "dark menu blend animation must honor WebCom reduced motion and Windows animation policy"});
        ++checks;
        if (dark_menu_source.find("case WM_SETTINGCHANGE: case WM_THEMECHANGED: case WM_SYSCOLORCHANGE:") == std::string::npos)
            findings.push_back({"src/dark_menu.cpp", 0, "native dark menus must dismiss when Windows accessibility/theme metrics change"});
    }

    const auto native_ui = ReadAll(root / "src" / "native_ui.inl");
    if (!native_ui.empty()) {
        ++checks;
        if (native_ui.find("Create chrome controls hidden. Layout() positions them before first reveal") == std::string::npos)
            findings.push_back({"src/native_ui.inl", 0, "startup chrome controls must stay hidden until their first final layout"});
        ++checks;
        if (native_ui.find("WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY") != std::string::npos ||
            native_ui.find("WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL") != std::string::npos)
            findings.push_back({"src/native_ui.inl", 0, "tab strip/address edit must not be created visible at the 1x1 client origin"});
        ++checks;
        if (native_ui.find("EnsureInternalPageData(page);") == std::string::npos)
            findings.push_back({"src/native_ui.inl", 0, "native internal pages must request their lazy datasets before rendering"});
        ++checks;
        if (native_ui.find("!downloads_loaded_ || !download_entries_.empty()") == std::string::npos)
            findings.push_back({"src/native_ui.inl", 0, "lazy download history must not remove the downloads affordance before first use"});
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,11>{{
            {"L\"Your shortcuts\"", "New Tab must expose user-managed shortcut tiles"},
            {"HomeShortcutBase+static_cast<UINT>(i)", "custom shortcut cards must route through bounded shortcut commands"},
            {"HomeShortcutRemoveBase+static_cast<UINT>(i)", "custom shortcut cards must expose a native remove affordance"},
            {"L\"Recent\"", "New Tab must expose a Recent section"},
            {"VisibleHomeRecentSites", "New Tab Recent cards must respect hidden-host preferences"},
            {"HomeRecentPinBase+static_cast<UINT>(i)", "recent-site cards must expose a pin-to-shortcuts action"},
            {"HomeRecentHideBase+static_cast<UINT>(i)", "recent-site cards must expose a non-destructive hide action"},
            {"L\"Workspaces\"", "New Tab must expose workspace cards"},
            {"WorkspaceBase+static_cast<UINT>(index)", "workspace cards must route through the existing workspace switch commands"},
            {"{L\"History\",History},{L\"Downloads\",Downloads},{L\"Bookmarks\",BookmarkManager},{L\"Sessions\",SessionsManager}", "New Tab quick actions must retain core browser destinations"},
            {"add_home_action", "New Tab cards must use native hit targets rather than another renderer"}
        }}) {
            ++checks;
            if (native_ui.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"L\"Top sites\"", "History v2 must expose top-site chips"},
            {"HistoryHostFilterBase+static_cast<UINT>(i)", "History top-site chips must route through bounded filter commands"},
            {"DeleteHistoryHostBase+static_cast<UINT>(i)", "History top-site chips must expose whole-site cleanup"},
            {"HistoryDateLabel(item.started_ms)", "Downloads must group rows by local date"},
            {"std::wstring download_group", "Downloads date grouping must avoid repeating the same date header"}
        }}) {
            ++checks;
            if (native_ui.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
        ++checks;
        if (native_ui.find("add(CloseDuplicateTabs,L\"Close duplicate tabs\"") == std::string::npos ||
            native_ui.find("add(SleepOtherTabs,L\"Sleep other tabs\"") == std::string::npos)
            findings.push_back({"src/native_ui.inl", 0, "Quick Commands must expose workspace-safe duplicate cleanup and sleep-other-tabs actions"});
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,4>{{
            {"internal_page==InternalPage::Privacy", "Privacy Manager must remain a searchable native page"},
            {"L\"Search cookies and permissions\"", "Privacy Manager search must label its cookie/permission scope"},
            {"const std::wstring privacy_summary", "Privacy Manager must retain its bounded profile summary counts"},
            {"visible_permission_rows", "Privacy Manager permission filtering must use compact filtered row indexing"}
        }}) {
            ++checks;
            if (native_ui.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,5>{{
            {"InternalPage::About", "About WebCom must remain a first-class native internal page"},
            {"L\"WebCom version\"", "About page must show the browser version"},
            {"L\"WebView2 Runtime\"", "About page must show the installed WebView2 runtime version"},
            {"L\"Performance snapshot\"", "About page must link to the runtime memory snapshot"},
            {"L\"Compatibility and privacy\"", "About page must disclose the WebView2 compatibility/privacy boundary"}
        }}) {
            ++checks;
            if (native_ui.find(token) == std::string::npos) findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
        static const std::regex internal_decl(R"((?:const\s+)?(?:auto|InternalPage)\s+internal_page\s*=)");
        const auto count = static_cast<std::size_t>(std::distance(
            std::sregex_iterator(native_ui.begin(), native_ui.end(), internal_decl), std::sregex_iterator()));
        ++checks;
        if (count > 1) findings.push_back({"src/native_ui.inl", 0, "internal_page is declared more than once; this previously caused an MSVC redefinition failure"});
    }

    const auto recently_closed_source = ReadAll(root / "src" / "recently_closed_model.cpp");
    if (!recently_closed_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,6>{{
            {"WEBCOM_RECENTLY_CLOSED", "recently-closed recovery must use an explicit versioned profile-local format"},
            {"kMaxRecentlyClosedEntries", "recently-closed recovery must retain a hard entry bound"},
            {"ResolveAddress", "recently-closed recovery must validate addresses through WebCom's normal resolver"},
            {"AtomicWrite", "recently-closed recovery must use atomic replacement"},
            {"0600", "portable recently-closed temporary files must be owner-only on POSIX"},
            {"MoveFileExW", "Windows recently-closed persistence must replace the flushed file atomically"}
        }}) {
            ++checks;
            if (recently_closed_source.find(token) == std::string::npos)
                findings.push_back({"src/recently_closed_model.cpp", 0, std::string(message)});
        }
    }

    if (!app_source.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,12>{{
            {"EnsureRecentlyClosedLoaded", "recently-closed recovery must be lazy-loaded before read/write use"},
            {"SaveRecentlyClosedNow", "recently-closed recovery must persist after mutations"},
            {"ReopenClosedAt", "History and Ctrl+Shift+T must share a bounded restore path"},
            {"tab->activity.protected_from_sleep = saved.keep_awake", "reopened tabs must restore keep-awake state"},
            {"tab->activity.pinned = saved.pinned", "reopened tabs must restore pinned state"},
            {"value == L\"webcom:feedback\"", "Report a Problem must remain a native internal page"},
            {"CopyProblemTemplateNow", "Report a Problem must expose a privacy-safe copyable template"},
            {"NoteAction", "actionable browser-owned notifications must retain a dedicated path"},
            {"OpenToastDownload", "download completion notification must be able to reopen the completed file"},
            {"available. Click to review.", "available updates must expose an actionable browser-owned notification"},
            {"app->history_entries_.clear()", "full browsing-data cleanup must clear WebCom-owned history after WebView2 succeeds"},
            {"app->closed_tabs_.clear()", "full browsing-data cleanup must clear recently-closed recovery after WebView2 succeeds"}
        }}) {
            ++checks;
            if (app_source.find(token) == std::string::npos)
                findings.push_back({"src/app.cpp", 0, std::string(message)});
        }
    }

    if (!native_ui.empty()) {
        for (const auto& [token, message] : std::array<std::pair<std::string_view,std::string_view>,8>{{
            {"L\"Recently closed\"", "History must expose persistent recently-closed recovery"},
            {"ReopenClosedBase+static_cast<UINT>(i)", "recently-closed rows must use a bounded restore command range"},
            {"L\"Clear recovered\"", "History must expose explicit recently-closed cleanup"},
            {"InternalPage::Feedback", "Report a Problem must render as a first-class native page"},
            {"L\"Copy report template\"", "Report a Problem must expose the privacy-safe template action"},
            {"WebCom never uploads them automatically", "Report a Problem must disclose the no-automatic-upload boundary"},
            {"L\"Ad blocker protection\"", "Privacy Manager must expose honest blocker protection state"},
            {"L\"Clear paused sites\"", "Privacy Manager must expose cleanup for blocker site exceptions"}
        }}) {
            ++checks;
            if (native_ui.find(token) == std::string::npos)
                findings.push_back({"src/native_ui.inl", 0, std::string(message)});
        }
    }

    checks += files.size();

    const auto accessibility_model = ReadAll(root / "src" / "accessibility_model.cpp");
    const auto accessibility_tests = ReadAll(root / "tests" / "accessibility_model_tests.cpp");
    const auto native_accessibility_model = ReadAll(root / "src" / "native_accessibility_model.cpp");
    const auto native_accessibility_tests = ReadAll(root / "tests" / "native_accessibility_model_tests.cpp");
    ++checks;
    if (accessibility_model.find("inputs.profile_reduce_motion || !inputs.system_animations_enabled || inputs.remote_session") == std::string::npos)
        findings.push_back({"src/accessibility_model.cpp", 0, "effective reduced motion must compose profile, Windows animation and Remote Desktop policy"});
    ++checks;
    if (accessibility_model.find("inputs.profile_high_contrast || inputs.system_high_contrast") == std::string::npos)
        findings.push_back({"src/accessibility_model.cpp", 0, "effective high contrast must compose profile and Windows High Contrast policy"});
    ++checks;
    if (accessibility_tests.find("Windows animations-off forces reduced browser chrome motion") == std::string::npos)
        findings.push_back({"tests/accessibility_model_tests.cpp", 0, "accessibility policy must have portable Windows-policy regression coverage"});

    ++checks;
    if (native_accessibility_model.find("NativeAccessibleActionIndex") == std::string::npos ||
        native_accessibility_model.find("NativeAccessibleChildId") == std::string::npos)
        findings.push_back({"src/native_accessibility_model.cpp", 0, "native accessibility virtual-child arithmetic must stay in the portable tested model"});
    ++checks;
    if (native_accessibility_tests.find("standard Win32 child IDs are not treated as virtual actions") == std::string::npos ||
        native_accessibility_tests.find("forward navigation stops at the last action") == std::string::npos)
        findings.push_back({"tests/native_accessibility_model_tests.cpp", 0, "native accessibility model must guard standard-child boundaries and navigation limits"});
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
