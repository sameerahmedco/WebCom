# Third-party components

The WebCom source uses the Windows platform and Microsoft Edge WebView2 APIs.
The Windows portable folder must retain the notices supplied with components
distributed in that folder. This file accompanies the WebCom source and packaged Windows builds.

## Microsoft WebView2

- SDK package: `Microsoft.Web.WebView2` 1.0.4314.35.
- Official package: https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4314.35
- The portable preview includes the official x64 `WebView2Loader.dll`.
- Its accompanying `LICENSE-WebView2.txt` and `NOTICE-WebView2.txt` are copied
  unchanged from the SDK.
- The Evergreen Runtime is obtained separately from Microsoft and is not
  contained in these archives.

## Optional cross-built preview

The optional cross-build path uses Zig 0.13.0's Clang/MinGW target and
its C/C++ support libraries. Corresponding bundled toolchain notices are in
`licenses/`: Zig, MinGW, libc++, libc++abi and libunwind. Corresponding notices cover
the preview; the compiler itself is not redistributed.

The primary Windows source build uses the installed Microsoft or compatible
Windows development tools. The optional Linux script records the cross-build
used for this preview; it does not change third-party licensing terms.

## Google logo and website links

- Asset: `resources/googlelogo.png`, the official light Google logo at
  544×184 pixels, drawn at up to 272×92 device-independent pixels on the normal
  native new-tab page.
- Original asset URL:
  https://www.google.com/images/branding/googlelogo/2x/googlelogo_light_color_272x92dp.png
- The PNG is embedded as a local Windows resource. It is not fetched each time
  the new-tab page opens.
- The Google name and logo belong to Google. This notice records the asset's
  provenance; it does not grant a trademark license or imply endorsement.
- Google search and AI Mode actions open Google's websites. WebCom is an
  independent project and is not affiliated with or endorsed by Google.

Google, YouTube and GitHub are the initial bookmark destinations. Their names
identify the linked websites; the project does not claim affiliation with
their operators. The user's reference screenshot is a visual implementation
reference and is not bundled as application artwork.

## Website favicons

Website favicons shown on tabs belong to their respective owners. WebCom obtains
them through the WebView2 favicon API while browsing; they are not artwork
bundled with the application or a claim of affiliation. The shell makes no
separate favicon-URL request and retains only a small bitmap in memory. The
runtime manages its own network/cache behavior. These site images are not
covered by the WebCom application's artwork provenance below.

## WebCom application icon

`resources/webcom-icon.svg` and `resources/webcom.ico` contain the WebCom
application's own W-shaped artwork. The ICO is embedded as a Windows icon
resource for the executable and application windows. It is separate from the
Google logo on the native new-tab page. No third-party icon pack is bundled for
this application mark; toolbar symbols use Windows fonts and native drawing.
