# WebCom 0.4.3 smoothness update

- Added 100 ms crossfade interpolation for tab hover enter/leave transitions.
- Kept selected/new-tab animation separate at 140 ms and scheduled on the UI frame timer.
- No hidden WebView is created for animation; native paint buffers remain the animation surface.
- History menu rows now display the local visit time.
- Native Win32 tooltip popup creation remains disabled to prevent top-left/yellow tooltip flashes.
- Google remains the default address-bar and new-tab search engine.
- WebCom's approved square W artwork remains the canonical application icon.
