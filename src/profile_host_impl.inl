// Included inside namespace webcom after all BrowserApp::Impl definitions.

BrowserApp::Session::Session(HINSTANCE instance, std::filesystem::path data_root)
    : root(std::move(data_root)), state(), instance_(instance) {
    state = LoadState(root, &recovered_session_);
}

BrowserApp::Session::~Session() {
    shutting_down_ = true;
    // During exception/startup cleanup the facade can still own open windows.
    // Move the registry first, so WM_DESTROY cannot mutate a vector in use.
    auto remaining = std::move(windows_);
    for (const auto& window : remaining) {
        window->closing_ = true;
        for (auto& tab : window->tabs_) window->Release(*tab);
        window->environment_.Reset();
        if (window->window_ && IsWindow(window->window_)) DestroyWindow(window->window_);
    }
    environment_.Reset();
}

bool BrowserApp::Session::IsProfileOpen(std::string_view profile) const {
    const auto requested = Wide(profile);
    for (const auto& window : windows_)
        if (!window->closing_ && window->window_ && IsWindow(window->window_) &&
            _wcsicmp(Wide(window->profile_name_).c_str(), requested.c_str()) == 0) return true;
    return false;
}

void BrowserApp::Session::ActivateProfile(std::string_view profile) {
    // Profile names are validated/canonicalized when windows are created.
    if (state.active_profile == profile) return;
    const auto found = std::find_if(state.profiles.begin(), state.profiles.end(),
        [&](const SavedProfile& saved) { return saved.name == profile; });
    if (found == state.profiles.end()) return;
    state.active_profile = found->name;
    registry_dirty_ = true;
}

void BrowserApp::Session::ApplyPreferences() {
    const auto windows = windows_;
    for (const auto& window : windows) {
        if (window->closing_) continue;
        for (auto& tab : window->tabs_) {
            window->ConfigurePasswords(*tab);
            if (tab->web && !window->IsActive(*tab) && !tab->activity.suspended)
                window->LowMemoryTarget(*tab, state.memory_mode == MemoryMode::Saver &&
                    !tab->activity.audible && !tab->activity.pinned &&
                    !tab->activity.protected_from_sleep && !window->HasDownload(*tab));
        }
        window->Tick();
        window->UpdateUI();
    }
    registry_dirty_ = true;
}

void BrowserApp::Session::OpenAllProfiles(Impl* requester) {
    if (shutting_down_) return;
    std::vector<std::string> names;
    names.reserve(state.profiles.size());
    for (const auto& profile : state.profiles) names.push_back(profile.name);
    for (const auto& name : names) {
        if (requester && name == requester->profile_name_) continue;
        OpenProfile(name, L"", false, SW_SHOWNORMAL, false);
    }
}

void BrowserApp::Session::CloseOtherProfileWindows(Impl* keep) {
    if (shutting_down_) return;
    const auto windows = windows_;
    for (const auto& window : windows) {
        if (!window || window.get() == keep || window->closing_ || !window->window_ || !IsWindow(window->window_)) continue;
        PostMessageW(window->window_, WM_CLOSE, 0, 0);
    }
}

void BrowserApp::Session::FinalizeProfileDeletion(Impl* requester) {
    if(shutting_down_ || !requester || requester->closing_ || state.profiles.size()<=1) return;
    const std::string deleted=requester->profile_name_;
    auto found=std::find_if(state.profiles.begin(),state.profiles.end(),[&](const SavedProfile& item){return item.name==deleted;});
    if(found==state.profiles.end()) return;

    // Capture every other live profile before removing this one so the atomic
    // registry write cannot discard newer tabs from a different window.
    const auto windows=windows_;
    for(const auto& window:windows)
        if(window && window.get()!=requester && !window->closing_ && window->window_ && IsWindow(window->window_)) window->CaptureProfile();

    std::string remove_error;
    if(!RemoveProfile(state,deleted,&remove_error)) return;
    const std::string fallback=state.active_profile;
    registry_dirty_=true;

    std::string lower=deleted;
    for(char& c:lower) if(c>='A' && c<='Z') c=static_cast<char>(c+('a'-'A'));
    std::error_code ec;
    for(const auto& file:std::array<std::filesystem::path,5>{
            root / "Appearance" / (lower+".v1"),
            root / "Bookmarks" / (lower+".v1"),
            root / "Permissions" / (lower+".v1"),
            HistoryPathFor(root,deleted), DownloadsPathFor(root,deleted)}) {
        ec.clear(); std::filesystem::remove(file,ec);
    }

    std::string save_error;
    if(SaveStateAfterProfileDeletion(root,state,&save_error)) registry_dirty_=false;

    // Open/focus the fallback before destroying the deleted profile's window.
    OpenProfile(fallback,L"",false,SW_SHOWNORMAL,false);
    for(const auto& candidate:windows_)
        if(candidate && !candidate->closing_ && candidate->profile_name_==fallback)
            candidate->Notice(L"Profile '"+Wide(deleted)+L"' was deleted, including its cookies, passwords and local WebCom data.");

    requester->delete_profile_pending_=false;
    requester->closing_=true;
    requester->CancelTabDrag();
    if(requester->window_){
        KillTimer(requester->window_,kTick);KillTimer(requester->window_,kSave);KillTimer(requester->window_,kUiFrame);
        KillTimer(requester->window_,kToast);KillTimer(requester->window_,kAnimationFrame);KillTimer(requester->window_,kDownloadTick);
    }
    for(auto& tab:requester->tabs_) requester->Release(*tab);
    requester->environment_.Reset();
    if(requester->window_ && IsWindow(requester->window_)) DestroyWindow(requester->window_);
}

bool BrowserApp::Session::OpenProfile(std::string profile, const std::wstring& address,
                                     bool private_mode, int show_command, bool add_if_open) {
    if (shutting_down_) return false;
    if (profile.empty()) profile = state.active_profile;
    if (!IsValidProfileName(profile)) return false;
    const auto resolved = ResolveAddress(Utf8(address));
    if (!resolved.allowed || (!address.empty() && Utf8(address).empty())) return false;
    auto saved = std::find_if(state.profiles.begin(), state.profiles.end(),
        [&](const SavedProfile& item) { return _wcsicmp(Wide(item.name).c_str(), Wide(profile).c_str()) == 0; });
    if (saved == state.profiles.end()) {
        if (state.profiles.size() >= 32) {
            MessageBoxW(nullptr, L"WebCom supports up to 32 profiles. Choose an existing profile.",
                        L"Profiles", MB_OK | MB_ICONINFORMATION);
            return false;
        }
        state.profiles.push_back({profile, {{"webcom:newtab", "New Tab", false, false}}, 0});
        registry_dirty_ = true;
    } else profile = saved->name;
    ActivateProfile(profile);
    // Copy the registry before calling window APIs: activation/focus can send
    // synchronous owner messages, and modal UI can alter the live registry.
    const auto existing = windows_;
    for (const auto& window : existing) {
        if (window->closing_ || window->profile_name_ != profile || !IsWindow(window->window_)) continue;
        if (IsIconic(window->window_)) ShowWindow(window->window_, SW_RESTORE);
        else ShowWindow(window->window_, SW_SHOW);
        SetForegroundWindow(window->window_);
        if (add_if_open || private_mode || !address.empty()) {
            const auto id = window->Add(Wide(resolved.url), true, private_mode);
            if (!id) return false;
            if (IsHome(Wide(resolved.url))) window->FocusOmnibox();
        }
        if (registry_dirty_) window->Dirty();
        return true;
    }
    auto window = std::make_shared<Impl>(instance_, shared_from_this(), profile);
    windows_.push_back(window);
    try {
        if (!window->Create(show_command, address, private_mode)) {
            if (window->window_ && IsWindow(window->window_)) DestroyWindow(window->window_);
            std::erase(windows_, window);
            return false;
        }
    } catch (...) {
        if (window->window_ && IsWindow(window->window_)) DestroyWindow(window->window_);
        std::erase(windows_, window);
        throw;
    }
    if (registry_dirty_) window->Dirty();
    if (recovered_session_) {
        window->Note(L"Recovered your last known-good session after the current session file could not be read.");
        recovered_session_ = false;
    }
    return true;
}

std::string BrowserApp::Session::ProfileForWindow(HWND window) const {
    if(!window)return {};
    const HWND root_window=GetAncestor(window,GA_ROOT);
    for(const auto& candidate:windows_)
        if(!candidate->closing_ && candidate->window_==root_window && IsWindow(root_window)) return candidate->profile_name_;
    return {};
}

bool BrowserApp::Session::MoveTabToProfile(Impl* source,std::uint64_t tab_id,std::string_view target_profile) {
    if(shutting_down_ || !source || source->closing_ || target_profile.empty() || target_profile==source->profile_name_) return false;
    auto* tab=source->Find(tab_id); if(!tab || source->HasDownload(*tab)) return false;
    const auto url=tab->url,title=tab->title,group=tab->group; const bool private_mode=tab->private_mode;
    std::optional<std::uint8_t> group_color;
    if(!group.empty())if(const auto* meta=source->FindTabGroup(group))group_color=meta->color;
    const bool pinned=tab->activity.pinned,awake=tab->activity.protected_from_sleep;
    if(!OpenProfile(std::string(target_profile),L"",false,SW_SHOWNORMAL,false))return false;
    std::shared_ptr<Impl> target;
    for(const auto& candidate:windows_)if(!candidate->closing_ && candidate->profile_name_==target_profile){target=candidate;break;}
    if(!target || target.get()==source)return false;
    const auto new_id=target->Add(url,true,private_mode);auto* created=target->Find(new_id);if(!created)return false;
    created->title=title;created->activity.pinned=pinned;created->activity.protected_from_sleep=awake;created->group=group;
    if(pinned){std::size_t pinned_count=0;for(const auto& item:target->tabs_)if(item->id!=new_id && item->activity.pinned)++pinned_count;target->MoveTabTo(new_id,pinned_count);}
    if(!group.empty()){
        target->AssignTabGroup(new_id,group);
        if(group_color)target->SetGroupColor(group,*group_color);
    }
    target->Dirty();target->UpdateUI(true);

    const auto found=std::find_if(source->tabs_.begin(),source->tabs_.end(),[&](const auto& item){return item->id==tab_id;});
    if(found==source->tabs_.end())return true;
    const std::size_t removed=static_cast<std::size_t>(found-source->tabs_.begin());
    const bool was_active=removed==source->active_; source->Release(**found);source->tabs_.erase(found);
    if(source->tabs_.empty())source->Add(L"webcom:newtab",true,false);
    else {
        if(source->active_>removed)--source->active_;
        else if(source->active_>=source->tabs_.size())source->active_=source->tabs_.size()-1;
        for(auto& item:source->tabs_)item->activity.active=false;
        source->tabs_[source->active_]->activity.active=true;
        if(was_active){source->Ensure(*source->tabs_[source->active_]);source->SetVisible(*source->tabs_[source->active_],true);}
        source->Dirty();source->UpdateUI(true);
    }
    return true;
}

bool BrowserApp::Session::HandleKeyMessage(const MSG& message) {
    if (!message.hwnd) return false;
    const HWND root_window = GetAncestor(message.hwnd, GA_ROOT);
    const auto windows = windows_;
    for (const auto& window : windows)
        if (!window->closing_ && window->window_ == root_window)
            return window->HandleKeyMessage(message);
    return false;
}

bool BrowserApp::Session::SaveAll(HWND owner, bool show_error) {
    if (saving_) return false;
    const auto windows = windows_;
    bool dirty = registry_dirty_;
    for (const auto& window : windows) dirty = dirty || window->dirty_;
    if (!dirty) return true;
    saving_ = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{saving_};
    // Capture every live window before one atomic replacement; a delayed timer
    // in one profile cannot overwrite newer tabs in another profile.
    for (const auto& window : windows)
        if (!window->closing_ && window->window_ && IsWindow(window->window_)) window->CaptureProfile();
    std::string error;
    if (!SaveState(root, state, &error)) {
        if (show_error) MessageBoxW(owner, (L"Your browser session could not be saved.\n\n" + Wide(error)).c_str(),
                                   L"WebCom", MB_OK | MB_ICONERROR);
        else for (const auto& window : windows)
            if (!window->closing_) window->Note(L"Session could not be saved. Check disk space and folder permissions.");
        return false;
    }
    registry_dirty_ = false;
    for (const auto& window : windows) {
        window->dirty_ = false;
        if (window->window_ && IsWindow(window->window_)) KillTimer(window->window_, kSave);
    }
    return true;
}

void BrowserApp::Session::DeliverEnvironment(const std::shared_ptr<Impl>& window, HRESULT error,
                                            ICoreWebView2Environment* environment) {
    if (window->closing_ || !window->window_ || !IsWindow(window->window_)) return;
    window->starting_environment_ = false;
    window->environment_generation_ = environment_generation_;
    if (FAILED(error) || !environment) {
        window->environment_failed_ = true;
        for (auto& tab : window->tabs_) if (!IsHome(tab->url) || tab->popup_args) {
            tab->failed = true;
            tab->creating = false;
            tab->error = L"Install or update the Microsoft Edge WebView2 Evergreen Runtime, then press Reload.\n\n"
                         L"Use Menu > Help for the official runtime download.\nError " + HResultText(error);
            if (tab->popup_args) tab->popup_args->put_Handled(TRUE);
            if (tab->popup_deferral) tab->popup_deferral->Complete();
            tab->popup_args.Reset(); tab->popup_deferral.Reset();
        }
        window->UpdateUI();
        return;
    }
    window->environment_ = environment;
    window->environment_failed_ = false;
    if (auto* tab = window->Active()) window->Ensure(*tab);
    if (window->delete_profile_pending_) {
        for (auto& tab : window->tabs_) {
            if (!tab->private_mode && tab->url == L"about:blank") { window->Ensure(*tab); break; }
        }
    }
    for (auto& tab : window->tabs_) if (tab->popup_args) window->Ensure(*tab);
}

void BrowserApp::Session::RequestEnvironment(const std::shared_ptr<Impl>& requester) {
    if (shutting_down_ || requester->closing_) return;
    if (environment_) { DeliverEnvironment(requester, S_OK, environment_.Get()); return; }
    requester->starting_environment_ = true;
    requester->environment_failed_ = false;
    if (environment_starting_) return;
    environment_starting_ = true;
    const auto generation = ++environment_generation_;
    const auto weak = weak_from_this();
    auto callback = MakeCallback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [weak, generation](HRESULT error, ICoreWebView2Environment* environment) -> HRESULT {
            auto session = weak.lock();
            if (!session || session->shutting_down_ || generation != session->environment_generation_) return S_OK;
            session->environment_starting_ = false;
            if (SUCCEEDED(error) && environment) session->environment_ = environment;
            const auto windows = session->windows_;
            for (const auto& window : windows) session->DeliverEnvironment(window, error, environment);
            return S_OK;
        });
    const auto data = root / L"User Data";
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(nullptr, data.c_str(), nullptr, callback.Get());
    if (FAILED(result) && generation == environment_generation_) {
        environment_starting_ = false;
        const auto windows = windows_;
        for (const auto& window : windows) DeliverEnvironment(window, result, nullptr);
    }
}

void BrowserApp::Session::InvalidateEnvironment(std::uint64_t expected_generation) {
    if (shutting_down_ || !environment_ || expected_generation != environment_generation_) return;
    ++environment_generation_; // Older queued crash notifications cannot reset a replacement environment.
    environment_starting_ = false;
    environment_.Reset();
    const auto windows = windows_;
    for (const auto& window : windows) {
        if (window->closing_) continue;
        for (auto& tab : window->tabs_) {
            window->Release(*tab);
            if (!IsHome(tab->url)) {
                tab->failed = true;
                tab->error = L"The shared browser engine stopped. Press Reload to recover this page.";
            }
        }
        window->environment_.Reset();
        window->starting_environment_ = false;
        window->environment_failed_ = true;
        window->UpdateUI();
    }
}

void BrowserApp::Session::OnWindowDestroyed(Impl* window) {
    std::erase_if(windows_, [&](const auto& candidate) { return candidate.get() == window; });
    if (!shutting_down_ && windows_.empty()) PostQuitMessage(0);
}

BrowserApp::BrowserApp(HINSTANCE instance) : session_(std::make_shared<Session>(instance)) {}
#ifdef WEBCOM_TEST_MODE
BrowserApp::BrowserApp(HINSTANCE instance, const std::filesystem::path& isolated_data_root)
    : session_(std::make_shared<Session>(instance, isolated_data_root)) {}
#endif
BrowserApp::~BrowserApp() = default;
bool BrowserApp::Create(int show_command, const std::wstring& initial_address, bool start_private,
                        const std::wstring& initial_profile) {
    return session_->OpenProfile(Utf8(initial_profile), initial_address, start_private, show_command);
}
bool BrowserApp::HandleKeyMessage(const MSG& message) { return session_->HandleKeyMessage(message); }
