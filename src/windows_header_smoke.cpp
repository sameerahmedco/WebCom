#ifdef _WIN32
#include "win32_com_compat.h"
#include <wrl/client.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <gdiplus.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <winver.h>
#include "update_transport_win32.h"
#include "favicon_image.h"

namespace webcom {

// This translation unit deliberately compiles the COM/WebView2/GDI+ header
// chain together. Older WebCom Windows builds exposed SDK/include-order
// regressions only under MSVC; keeping this as a native source makes those
// failures small and local instead of surfacing deep inside app.cpp.
void WebComWindowsHeaderSmoke() {
    Microsoft::WRL::ComPtr<ICoreWebView2_2> webview2;
    auto environmentOptions = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions6> extensionOptions;
    if (environmentOptions) environmentOptions.As(&extensionOptions);
    Microsoft::WRL::ComPtr<IStream> stream;
    Gdiplus::Bitmap* bitmap = nullptr;
    HINTERNET internet = nullptr;
    WINTRUST_DATA trust{};
    VS_FIXEDFILEINFO version{};
    (void)webview2;
    (void)environmentOptions;
    (void)extensionOptions;
    (void)stream;
    (void)bitmap;
    (void)internet;
    (void)trust;
    (void)version;
}

} // namespace webcom
#endif
