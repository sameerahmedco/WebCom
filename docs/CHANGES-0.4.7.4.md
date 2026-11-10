# WebCom 0.4.7.4 MSVC compile hotfix

- Renames the favicon decode local `small` so it cannot collide with Windows/RPC macro definitions.
- Removes the duplicate `internal_page` declaration in native layout code.
- Casts Win32 `LONG` rectangle deltas before `std::max` so MSVC can deduce one template type.
- Splits filesystem path/temp declarations that used one `auto` declarator list with two different deduced types.
- Cleans the NameDialog shadow warning and the explicit-black appearance test signed/unsigned warning.
- Keeps the WebView2 NuGet restore fix from 0.4.7.2 and COM/GDI+ header fix from 0.4.7.3.
