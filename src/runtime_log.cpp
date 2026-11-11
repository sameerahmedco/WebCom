#include "runtime_log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <system_error>
#include <utility>

namespace webcom {
namespace {

constexpr std::size_t kMaxCurrentLog = 256u * 1024u;
constexpr std::size_t kMaxDetail = 768;
constexpr std::string_view kSessionHeader = "WEBCOM_RUNTIME_SESSION\t1";
std::mutex g_log_mutex;

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}

std::filesystem::path LogDir(const std::filesystem::path& root) { return root / "Logs"; }
std::filesystem::path CurrentLog(const std::filesystem::path& root) { return LogDir(root) / "runtime.log"; }
std::filesystem::path PreviousLog(const std::filesystem::path& root) { return LogDir(root) / "runtime.previous.log"; }
std::filesystem::path SessionStatePath(const std::filesystem::path& root) { return LogDir(root) / "session-state.v1"; }

bool ValidEvent(std::string_view value) {
    return !value.empty() && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '.' || c == '_' || c == '-';
           });
}

std::string Lower(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

const char* LevelName(RuntimeLogLevel level) {
    switch (level) {
    case RuntimeLogLevel::Warning: return "WARN";
    case RuntimeLogLevel::Error: return "ERROR";
    default: return "INFO";
    }
}

bool RotateIfNeeded(const std::filesystem::path& root, std::size_t incoming, std::string* error) {
    std::error_code ec;
    const auto current = CurrentLog(root);
    const auto size = std::filesystem::exists(current, ec) ? std::filesystem::file_size(current, ec) : 0;
    if (ec) return Fail(error, "Runtime log size could not be read.");
    if (size + incoming <= kMaxCurrentLog) return true;
    const auto previous = PreviousLog(root);
    std::filesystem::remove(previous, ec); ec.clear();
    std::filesystem::rename(current, previous, ec);
    if (ec) return Fail(error, "Runtime log could not be rotated.");
    return true;
}

std::string ReadBounded(const std::filesystem::path& path, std::size_t maximum) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::string result;
    result.resize(maximum);
    input.read(result.data(), static_cast<std::streamsize>(maximum));
    result.resize(static_cast<std::size_t>(input.gcount()));
    return result;
}

bool SaveSessionState(const std::filesystem::path& root, const RuntimeSessionState& state, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(LogDir(root), ec);
    if (ec) return Fail(error, "Runtime log directory could not be created.");
    const auto path = SessionStatePath(root);
    auto temp = path; temp += ".tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return Fail(error, "Runtime session state could not be written.");
        output << kSessionHeader << "\nrunning\t" << (state.running ? '1' : '0')
               << "\nversion\t" << state.version << "\nsequence\t" << state.sequence << "\n";
        if (!output) return Fail(error, "Runtime session state could not be completed.");
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec); ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) { std::filesystem::remove(temp, ec); return Fail(error, "Runtime session state could not be replaced."); }
    return true;
}

} // namespace

std::string SanitizeRuntimeLogDetail(std::string_view detail) {
    std::string out;
    out.reserve(std::min<std::size_t>(detail.size(), kMaxDetail));
    bool previous_space = false;
    for (unsigned char c : detail) {
        if (out.size() >= kMaxDetail) break;
        if (c < 0x20 || c == 0x7f || c == '\t') {
            if (!previous_space) out.push_back(' ');
            previous_space = true;
            continue;
        }
        out.push_back(static_cast<char>(c));
        previous_space = c == ' ';
    }
    auto redact = [&](std::string needle) {
        for (;;) {
            auto lower = Lower(out);
            const auto pos = lower.find(needle);
            if (pos == std::string::npos) break;
            auto end = out.find(' ', pos);
            if (end == std::string::npos) end = out.size();
            out.replace(pos, end - pos, "[redacted-url]");
        }
    };
    redact("https://"); redact("http://"); redact("file:///");
    return out;
}

bool AppendRuntimeLog(const std::filesystem::path& root, RuntimeLogLevel level,
                      std::string_view event, std::string_view detail, std::string* error) {
    if (!ValidEvent(event)) return Fail(error, "Runtime log event name is invalid.");
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::error_code ec;
    std::filesystem::create_directories(LogDir(root), ec);
    if (ec) return Fail(error, "Runtime log directory could not be created.");
    const auto safe = SanitizeRuntimeLogDetail(detail);
    const std::string line = UtcNow() + "\t" + LevelName(level) + "\t" + std::string(event) + "\t" + safe + "\n";
    if (!RotateIfNeeded(root, line.size(), error)) return false;
    std::ofstream output(CurrentLog(root), std::ios::binary | std::ios::app);
    if (!output) return Fail(error, "Runtime log could not be opened.");
    output.write(line.data(), static_cast<std::streamsize>(line.size()));
    return static_cast<bool>(output) || Fail(error, "Runtime log could not be appended.");
}

RuntimeSessionState LoadRuntimeSessionState(const std::filesystem::path& root) {
    std::ifstream input(SessionStatePath(root), std::ios::binary);
    if (!input) return {};
    std::string header, running, version, sequence, extra;
    if (!std::getline(input, header) || header != kSessionHeader ||
        !std::getline(input, running) || !std::getline(input, version) || !std::getline(input, sequence)) return {};
    if (!running.empty() && running.back() == '\r') running.pop_back();
    if (!version.empty() && version.back() == '\r') version.pop_back();
    if (!sequence.empty() && sequence.back() == '\r') sequence.pop_back();
    if (running != "running\t0" && running != "running\t1") return {};
    if (!version.starts_with("version\t") || !sequence.starts_with("sequence\t")) return {};
    RuntimeSessionState state;
    state.running = running.back() == '1';
    state.version = version.substr(8);
    const auto number = sequence.substr(9);
    if (number.empty() || number.size() > 20) return {};
    unsigned long long parsed = 0;
    for (unsigned char c : number) {
        if (!std::isdigit(c)) return {};
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (parsed > (std::numeric_limits<unsigned long long>::max() - digit) / 10ull) return {};
        parsed = parsed * 10ull + digit;
    }
    state.sequence = parsed;
    if (std::getline(input, extra) && !extra.empty() && extra != "\r") return {};
    return state;
}

bool BeginRuntimeSession(const std::filesystem::path& root, std::string_view version,
                         bool* previous_unclean, std::string* error) {
    if (version.empty() || version.size() > 48) return Fail(error, "Runtime version is invalid.");
    const auto previous = LoadRuntimeSessionState(root);
    if (previous_unclean) *previous_unclean = previous.running;
    RuntimeSessionState next{true, std::string(version), previous.sequence + 1};
    if (!SaveSessionState(root, next, error)) return false;
    const std::string detail = "version=" + std::string(version) + "; sequence=" + std::to_string(next.sequence) +
                               "; previous_unclean=" + (previous.running ? "yes" : "no");
    if (!AppendRuntimeLog(root, RuntimeLogLevel::Info, "session.start", detail, error)) return false;
    if (previous.running)
        AppendRuntimeLog(root, RuntimeLogLevel::Warning, "session.previous_unclean",
                         "previous_version=" + previous.version + "; previous_sequence=" + std::to_string(previous.sequence), nullptr);
    return true;
}

bool EndRuntimeSession(const std::filesystem::path& root, std::string_view version, std::string* error) {
    auto state = LoadRuntimeSessionState(root);
    if (!state.running) return true;
    state.running = false;
    state.version = std::string(version);
    if (!SaveSessionState(root, state, error)) return false;
    return AppendRuntimeLog(root, RuntimeLogLevel::Info, "session.clean_shutdown",
                            "version=" + std::string(version) + "; sequence=" + std::to_string(state.sequence), error);
}

std::string ReadRuntimeLogBundleText(const std::filesystem::path& root, std::size_t maximum_bytes) {
    if (maximum_bytes < 1024 || maximum_bytes > 2u * 1024u * 1024u) return {};
    const std::size_t half = maximum_bytes / 2;
    const auto previous = ReadBounded(PreviousLog(root), half);
    const auto current = ReadBounded(CurrentLog(root), maximum_bytes - previous.size());
    std::string out;
    if (!previous.empty()) out += "--- previous runtime log ---\n" + previous + "\n";
    if (!current.empty()) out += "--- current runtime log ---\n" + current;
    if (out.size() > maximum_bytes) out.resize(maximum_bytes);
    return out;
}

} // namespace webcom
