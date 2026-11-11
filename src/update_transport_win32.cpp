#ifdef _WIN32

#include "update_transport_win32.h"
#include "win32_com_compat.h"

#include <bcrypt.h>
#include <softpub.h>
#include <winhttp.h>
#include <wintrust.h>
#include <winver.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace webcom {
namespace {

struct InternetHandle {
    HINTERNET value = nullptr;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    InternetHandle(InternetHandle&& other) noexcept : value(other.value) { other.value = nullptr; }
    InternetHandle& operator=(InternetHandle&& other) noexcept {
        if (this != &other) {
            if (value) WinHttpCloseHandle(value);
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }
};

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::wstring Wide(std::string_view value) {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), required) != required) return {};
    return result;
}

std::string Lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string HexLower(const std::uint8_t* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '0');
    for (std::size_t i = 0; i < size; ++i) {
        result[i * 2] = digits[data[i] >> 4];
        result[i * 2 + 1] = digits[data[i] & 0x0f];
    }
    return result;
}

struct ParsedHttpsUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

std::optional<ParsedHttpsUrl> ParseHttpsUrl(std::string_view utf8, std::string* error) {
    if (utf8.size() < 9 || utf8.size() > 4096 || Lower(utf8.substr(0, 8)) != "https://") {
        Fail(error, "Update URL must use HTTPS.");
        return std::nullopt;
    }
    const auto wide = Wide(utf8);
    if (wide.empty()) { Fail(error, "Update URL is not valid UTF-8."); return std::nullopt; }
    URL_COMPONENTSW parts{}; parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    parts.dwUserNameLength = static_cast<DWORD>(-1);
    parts.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || !parts.lpszHostName || parts.dwHostNameLength == 0) {
        Fail(error, "Update HTTPS URL could not be parsed.");
        return std::nullopt;
    }
    if (parts.dwUserNameLength || parts.dwPasswordLength) {
        Fail(error, "Update URLs may not contain usernames or passwords.");
        return std::nullopt;
    }
    ParsedHttpsUrl result;
    result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    if (parts.lpszUrlPath && parts.dwUrlPathLength) result.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    else result.path = L"/";
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength) result.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    result.port = parts.nPort ? parts.nPort : INTERNET_DEFAULT_HTTPS_PORT;
    return result;
}

bool ConfigureRequest(HINTERNET request, std::string* error) {
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
        return Fail(error, "Could not disable update-request cookies/authentication.");
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)))
        return Fail(error, "Could not enforce HTTPS redirect policy.");
    DWORD maximum_redirects = 3;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &maximum_redirects, sizeof(maximum_redirects)))
        return Fail(error, "Could not bound update redirects.");
#ifdef WINHTTP_OPTION_REJECT_USERPWD_IN_URL
    BOOL reject_userinfo = TRUE;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_REJECT_USERPWD_IN_URL, &reject_userinfo, sizeof(reject_userinfo)))
        return Fail(error, "Could not reject credential-bearing update URLs.");
#endif
    DWORD secure_feature = WINHTTP_ENABLE_SSL_REVOCATION;
    WinHttpSetOption(request, WINHTTP_OPTION_ENABLE_FEATURE, &secure_feature, sizeof(secure_feature));
    return true;
}

bool OpenGetRequest(std::string_view url, InternetHandle& session, InternetHandle& connection,
                    InternetHandle& request, std::string* error) {
    const auto parsed = ParseHttpsUrl(url, error);
    if (!parsed) return false;
    session = InternetHandle(WinHttpOpen(L"WebCom/2.0.0 updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value) return Fail(error, "WinHTTP session could not be created.");
    WinHttpSetTimeouts(session.value, 10000, 10000, 15000, 30000);
    connection = InternetHandle(WinHttpConnect(session.value, parsed->host.c_str(), parsed->port, 0));
    if (!connection.value) return Fail(error, "Update server connection could not be created.");
    request = InternetHandle(WinHttpOpenRequest(connection.value, L"GET", parsed->path.c_str(), nullptr,
                                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE));
    if (!request.value || !ConfigureRequest(request.value, error)) return false;
    if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.value, nullptr))
        return Fail(error, "The update server could not be reached over HTTPS.");
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status != HTTP_STATUS_OK)
        return Fail(error, "The update server did not return HTTP 200.");
    return true;
}

bool Sha256Bytes(const std::uint8_t* data, std::size_t size, std::array<std::uint8_t, 32>& output) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, copied = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    const auto close_algorithm = [&] { BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size),
                          sizeof(object_size), &copied, 0) < 0 || !object_size) { close_algorithm(); return false; }
    std::vector<std::uint8_t> object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) { close_algorithm(); return false; }
    bool ok = true;
    if (size > std::numeric_limits<ULONG>::max()) ok = false;
    else if (size && BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0) < 0) ok = false;
    if (ok && BCryptFinishHash(hash, output.data(), static_cast<ULONG>(output.size()), 0) < 0) ok = false;
    BCryptDestroyHash(hash); close_algorithm(); return ok;
}

bool Sha256File(const std::filesystem::path& path, std::string& output, std::string* error) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0, copied = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return Fail(error, "Windows SHA-256 provider could not be opened.");
    const auto close_algorithm = [&] { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &copied, 0) < 0 || !object_size) {
        close_algorithm(); return Fail(error, "Windows SHA-256 provider did not return an object size.");
    }
    std::vector<std::uint8_t> object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        close_algorithm(); return Fail(error, "Windows SHA-256 hash could not be created.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { BCryptDestroyHash(hash); close_algorithm(); return Fail(error, "Staged update package could not be opened for hashing."); }
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash); close_algorithm(); return Fail(error, "SHA-256 hashing failed while reading the update package.");
        }
    }
    if (!input.eof()) { BCryptDestroyHash(hash); close_algorithm(); return Fail(error, "The staged update package could not be read completely."); }
    std::array<std::uint8_t, 32> digest{};
    const bool ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    BCryptDestroyHash(hash); close_algorithm();
    if (!ok) return Fail(error, "SHA-256 hashing could not be finished.");
    output = HexLower(digest.data(), digest.size());
    return true;
}

bool VerifyAuthenticodePublisher(const std::filesystem::path& path, std::string_view expected,
                                 std::string* error) {
    WINTRUST_FILE_INFO file{}; file.cbStruct = sizeof(file); file.pcwszFilePath = path.c_str();
    WINTRUST_DATA trust{}; trust.cbStruct = sizeof(trust); trust.dwUIChoice = WTD_UI_NONE;
    trust.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN; trust.dwUnionChoice = WTD_CHOICE_FILE;
    trust.pFile = &file; trust.dwStateAction = WTD_STATEACTION_VERIFY;
    trust.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN;
    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = WinVerifyTrust(nullptr, &policy, &trust);
    if (status != ERROR_SUCCESS) {
        trust.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr, &policy, &trust);
        return Fail(error, "The staged installer does not have a trusted Authenticode signature.");
    }
    bool verified = false;
    if (auto* provider = WTHelperProvDataFromStateData(trust.hWVTStateData)) {
        if (auto* signer = WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0); signer && signer->csCertChain > 0) {
            if (const PCCERT_CONTEXT certificate = signer->pasCertChain[0].pCert; certificate) {
                std::array<std::uint8_t, 32> fingerprint{};
                if (Sha256Bytes(certificate->pbCertEncoded, certificate->cbCertEncoded, fingerprint))
                    verified = Lower(HexLower(fingerprint.data(), fingerprint.size())) == Lower(expected);
            }
        }
    }
    trust.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr, &policy, &trust);
    if (!verified) return Fail(error, "The staged installer signer does not match WebCom's pinned publisher certificate.");
    return true;
}

bool VerifyWebComPackageIdentity(const std::filesystem::path& path, std::string_view version,
                                   std::string* error) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 16u * 1024u * 1024u) return Fail(error, "The staged installer has no usable Windows version information.");
    std::vector<std::uint8_t> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return Fail(error, "The staged installer version information could not be read.");
    VS_FIXEDFILEINFO* fixed = nullptr; UINT fixed_size = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixed_size) ||
        !fixed || fixed_size < sizeof(VS_FIXEDFILEINFO) || fixed->dwSignature != 0xFEEF04BD)
        return Fail(error, "The staged installer has invalid fixed version information.");
    unsigned expected[3]{}; std::size_t offset = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (offset >= version.size() || !std::isdigit(static_cast<unsigned char>(version[offset])))
            return Fail(error, "The release version could not be compared with the installer identity.");
        unsigned value = 0;
        while (offset < version.size() && std::isdigit(static_cast<unsigned char>(version[offset]))) {
            value = value * 10u + static_cast<unsigned>(version[offset] - '0'); ++offset;
        }
        expected[i] = value;
        if (i < 2) { if (offset >= version.size() || version[offset] != '.') return Fail(error, "The release version is malformed."); ++offset; }
    }
    const unsigned actual_major = HIWORD(fixed->dwProductVersionMS);
    const unsigned actual_minor = LOWORD(fixed->dwProductVersionMS);
    const unsigned actual_patch = HIWORD(fixed->dwProductVersionLS);
    if (actual_major != expected[0] || actual_minor != expected[1] || actual_patch != expected[2])
        return Fail(error, "The staged installer product version does not match the release manifest.");

    struct Translation { WORD language; WORD codepage; };
    Translation* translations = nullptr; UINT translations_size = 0;
    std::wstring product_name;
    if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &translations_size) &&
        translations && translations_size >= sizeof(Translation)) {
        const std::size_t count = translations_size / sizeof(Translation);
        for (std::size_t i = 0; i < count && product_name.empty(); ++i) {
            wchar_t key[96]{};
            swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\ProductName", translations[i].language, translations[i].codepage);
            wchar_t* value = nullptr; UINT chars = 0;
            if (VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &chars) && value && chars)
                product_name.assign(value, chars > 0 && value[chars - 1] == L'\0' ? chars - 1 : chars);
        }
    }
    if (product_name.empty()) {
        wchar_t* value = nullptr; UINT chars = 0;
        if (VerQueryValueW(data.data(), L"\\StringFileInfo\\040904B0\\ProductName", reinterpret_cast<void**>(&value), &chars) && value && chars)
            product_name.assign(value, chars > 0 && value[chars - 1] == L'\0' ? chars - 1 : chars);
    }
    if (_wcsicmp(product_name.c_str(), L"WebCom") != 0)
        return Fail(error, "The staged signed executable is not identified as the WebCom product.");
    return true;
}

bool DownloadHttpsToFile(std::string_view url, const std::filesystem::path& path,
                         std::uint64_t expected_size, std::string* error) {
    InternetHandle session, connection, request;
    if (!OpenGetRequest(url, session, connection, request, error)) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return Fail(error, "The update staging file could not be created.");
    std::uint64_t total = 0;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            output.close(); return Fail(error, "The update package download was interrupted.");
        }
        if (!read) break;
        total += read;
        if (total > expected_size) { output.close(); return Fail(error, "The update package exceeded the manifest size."); }
        output.write(reinterpret_cast<const char*>(buffer.data()), read);
        if (!output) { output.close(); return Fail(error, "The update staging file could not be written."); }
    }
    output.close();
    if (total != expected_size) return Fail(error, "The downloaded update package size does not match the manifest.");
    return true;
}

} // namespace

std::optional<std::string> FetchUpdateManifestHttps(std::string_view url,
                                                    std::size_t maximum_bytes,
                                                    std::string* error) {
    if (maximum_bytes < 256 || maximum_bytes > 1024 * 1024) {
        Fail(error, "Manifest size limit is invalid."); return std::nullopt;
    }
    InternetHandle session, connection, request;
    if (!OpenGetRequest(url, session, connection, request, error)) return std::nullopt;
    std::string body; body.reserve(std::min<std::size_t>(maximum_bytes, 64 * 1024));
    std::array<char, 8192> buffer{};
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            Fail(error, "The release manifest download was interrupted."); return std::nullopt;
        }
        if (!read) break;
        if (body.size() > maximum_bytes - read) {
            Fail(error, "The release manifest exceeded WebCom's size limit."); return std::nullopt;
        }
        body.append(buffer.data(), read);
    }
    return body;
}

bool VerifyStagedUpdatePackage(const std::filesystem::path& package,
                               const UpdateManifest& manifest,
                               std::string_view pinned_publisher_sha256,
                               std::string* error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(package, ec);
    if (ec || size != manifest.size_bytes) return Fail(error, "The staged installer size does not match the manifest.");
    std::string hash;
    if (!Sha256File(package, hash, error)) return false;
    if (Lower(hash) != Lower(manifest.sha256)) return Fail(error, "The staged installer SHA-256 does not match the manifest.");
    if (Lower(manifest.publisher_sha256) != Lower(pinned_publisher_sha256))
        return Fail(error, "The manifest publisher does not match the WebCom build pin.");
    if (!VerifyWebComPackageIdentity(package, manifest.version, error)) return false;
    return VerifyAuthenticodePublisher(package, pinned_publisher_sha256, error);
}

bool DownloadAndVerifyUpdatePackage(const UpdateManifest& manifest,
                                    std::string_view pinned_publisher_sha256,
                                    const std::filesystem::path& destination,
                                    std::string* error) {
    if (!IsInstallEligibleUpdateManifest(manifest)) return Fail(error, "The release manifest is not install eligible.");
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) return Fail(error, "The update staging directory could not be created.");
    std::filesystem::path partial = destination;
    partial += L".part";
    std::filesystem::remove(partial, ec); ec.clear();
    if (!DownloadHttpsToFile(manifest.url, partial, manifest.size_bytes, error)) {
        std::filesystem::remove(partial, ec); return false;
    }
    if (!VerifyStagedUpdatePackage(partial, manifest, pinned_publisher_sha256, error)) {
        std::filesystem::remove(partial, ec); return false;
    }
    std::filesystem::remove(destination, ec); ec.clear();
    std::filesystem::rename(partial, destination, ec);
    if (ec) { std::filesystem::remove(partial, ec); return Fail(error, "The verified update package could not be committed to staging."); }
    return true;
}

} // namespace webcom

#endif
