#pragma once

#include <windows.h>
#include <unknwn.h>
#include <wrl/client.h>

#include <atomic>
#include <new>
#include <type_traits>
#include <utility>

namespace webcom {
namespace detail {

template <typename Interface, typename Callable, typename InvokeSignature>
class ComCallback;

// WebView2 handler interfaces each expose one HRESULT-returning Invoke method.
// Deducing its parameter list avoids duplicating a class for every event type
// and works with the official Windows SDK as well as Clang's MS extensions.
template <typename Interface, typename Callable, typename... Args>
class ComCallback<Interface, Callable,
                  HRESULT(STDMETHODCALLTYPE Interface::*)(Args...)> final
    : public Interface {
    static_assert(std::is_base_of_v<IUnknown, Interface>,
                  "A callback handler must be a COM interface.");
    static_assert(std::is_invocable_r_v<HRESULT, Callable&, Args...>,
                  "The callback must match the handler's Invoke signature.");

public:
    template <typename Function>
    explicit ComCallback(Function&& function)
        : callback_(std::forward<Function>(function)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                              void** result) noexcept override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) ||
            IsEqualIID(iid, __uuidof(Interface))) {
            *result = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return references_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const ULONG remaining =
            references_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(Args... arguments) noexcept override {
        // Exceptions must never propagate across a COM ABI boundary.
        try {
            return callback_(std::forward<Args>(arguments)...);
        } catch (const std::bad_alloc&) {
            return E_OUTOFMEMORY;
        } catch (...) {
            return E_FAIL;
        }
    }

private:
    std::atomic<ULONG> references_{1};
    Callable callback_;
};

}  // namespace detail

// The returned smart pointer owns the handler's initial reference. WebView2
// retains its own reference when registration succeeds. No WRL event.h or
// runtime-class implementation is required.
template <typename Interface, typename Function>
Microsoft::WRL::ComPtr<Interface> MakeCallback(Function&& function) {
    using Callable = std::decay_t<Function>;
    using Handler = detail::ComCallback<Interface, Callable,
                                       decltype(&Interface::Invoke)>;
    Microsoft::WRL::ComPtr<Interface> handler;
    handler.Attach(new (std::nothrow) Handler(std::forward<Function>(function)));
    return handler;
}

}  // namespace webcom
