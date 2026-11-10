// Included inside namespace webcom, immediately before BrowserApp::Impl.
// Session is the sole owner of saved metadata and browser windows. Each window
// holds only a weak Session reference, preventing an ownership cycle.
class BrowserApp::Session : public std::enable_shared_from_this<BrowserApp::Session> {
public:
    explicit Session(HINSTANCE instance, std::filesystem::path data_root = DataDirectory());
    ~Session();
    bool OpenProfile(std::string profile, const std::wstring& address, bool private_mode,
                     int show_command = SW_SHOWNORMAL, bool add_if_open = false);
    bool HandleKeyMessage(const MSG& message);
    bool SaveAll(HWND owner, bool show_error);
    bool IsProfileOpen(std::string_view profile) const;
    void ActivateProfile(std::string_view profile);
    void ApplyPreferences();
    void OpenAllProfiles(Impl* requester);
    void CloseOtherProfileWindows(Impl* keep);
    void FinalizeProfileDeletion(Impl* requester);
    bool MoveTabToProfile(Impl* source, std::uint64_t tab_id, std::string_view target_profile);
    std::string ProfileForWindow(HWND window) const;
    void RequestEnvironment(const std::shared_ptr<Impl>& requester);
    void InvalidateEnvironment(std::uint64_t expected_generation);
    void OnWindowDestroyed(Impl* window);

    const std::filesystem::path root;
    AppState state;

private:
    HINSTANCE instance_ = nullptr;
    std::vector<std::shared_ptr<Impl>> windows_;
    ComPtr<ICoreWebView2Environment> environment_;
    std::uint64_t environment_generation_ = 0;
    bool environment_starting_ = false;
    bool shutting_down_ = false;
    bool registry_dirty_ = false;
    bool saving_ = false;
    bool recovered_session_ = false;

    void DeliverEnvironment(const std::shared_ptr<Impl>& window, HRESULT error,
                            ICoreWebView2Environment* environment);
};
