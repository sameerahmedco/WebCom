#!/usr/bin/env bash
# Optional Linux -> Windows x64 preview build. The normal Windows build is build.cmd.
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
zig_command="${WEBCOM_ZIG:-zig}"
sdk_directory="${WEBVIEW2_SDK_DIR:-}"
output_directory="$project_root/build/zig-x64-preview-v045"
build_ui_tests=false

usage() {
    printf '%s\n' \
        'Optional experimental Windows x64 cross-build (validated with Zig 0.13.0).' \
        'Usage: bash scripts/cross-build-preview.sh --sdk /path/to/extracted/sdk [--zig /path/to/zig] [--out /path/to/output] [--tests]' \
        '--tests also compiles the Windows native UI test executable; it does not run Windows code.' \
        'No compiler, SDK, runtime, or installer is downloaded by this script.'
}

while (( $# )); do
    case "$1" in
        --sdk|--zig|--out)
            if (( $# < 2 )); then usage >&2; exit 2; fi
            case "$1" in
                --sdk) sdk_directory="$2" ;;
                --zig) zig_command="$2" ;;
                --out) output_directory="$2" ;;
            esac
            shift 2 ;;
        -h|--help) usage; exit 0 ;;
        --tests) build_ui_tests=true; shift ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ -z "$sdk_directory" ]]; then usage >&2; exit 2; fi
if ! command -v -- "$zig_command" >/dev/null 2>&1; then
    printf 'Zig compiler not found: %s\n' "$zig_command" >&2; exit 1
fi
sdk_directory="$(cd -- "$sdk_directory" && pwd)"
for sdk_file in build/native/include/WebView2.h build/native/x64/WebView2Loader.dll.lib build/native/x64/WebView2Loader.dll; do
    if [[ ! -f "$sdk_directory/$sdk_file" ]]; then
        printf 'SDK file is missing: %s\n' "$sdk_directory/$sdk_file" >&2; exit 1
    fi
done

zig_version="$("$zig_command" version)"
if [[ "$zig_version" != '0.13.0' ]]; then
    printf 'Note: Zig %s is installed; this experimental path was validated with 0.13.0.\n' "$zig_version"
fi
mkdir -p -- "$output_directory"
output_directory="$(cd -- "$output_directory" && pwd)"
compatibility_directory="$output_directory/compat"
mkdir -p -- "$compatibility_directory"

# Keep Microsoft's interface UUIDs when using Zig's MinGW headers. These are
# build-local adapters only; neither the application nor the vendor SDK is edited.
cat > "$compatibility_directory/compat.h" <<'HEADER'
#pragma once
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <unknwn.h>
#undef __uuidof
#undef MIDL_INTERFACE
#define MIDL_INTERFACE(x) struct __declspec(uuid(x)) __declspec(novtable)
HEADER
cat > "$compatibility_directory/EventToken.h" <<'HEADER'
#pragma once
#include <eventtoken.h>
HEADER

compile_options=(
    -target x86_64-windows-gnu -std=c++20 -O2 -DNDEBUG -fms-extensions
    -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX
    -U_WIN32_WINNT -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
    -Wall -Wextra -Wpedantic -Wno-ignored-attributes -Wno-nonportable-include-path
    -I"$compatibility_directory"
    -isystem "$sdk_directory/build/native/include"
    -I"$project_root/src"
    -include "$compatibility_directory/compat.h"
)
objects=()
for source_name in main app core storage bookmarks appearance dark_menu tab_search tab_search_model omnibox_model; do
    printf 'Compiling %s.cpp...\n' "$source_name"
    "$zig_command" c++ "${compile_options[@]}" \
        -c "$project_root/src/$source_name.cpp" -o "$output_directory/$source_name.obj"
    objects+=("$output_directory/$source_name.obj")
done

"$zig_command" rc /I "$project_root/resources" \
    /fo "$output_directory/webcom.res" -- "$project_root/resources/webcom.rc"
"$zig_command" c++ -target x86_64-windows-gnu -O2 \
    -Wl,--subsystem,windows -municode \
    "${objects[@]}" "$output_directory/webcom.res" \
    "$sdk_directory/build/native/x64/WebView2Loader.dll.lib" \
    -lole32 -lshell32 -lshlwapi -lcomctl32 -ldwmapi -luser32 -lgdi32 -lgdiplus -luxtheme \
    -lwininet -lpsapi -ladvapi32 -luuid \
    -o "$output_directory/WebCom.exe"

if [[ "$build_ui_tests" == true ]]; then
    printf '%s\n' 'Compiling the isolated Windows native UI test harness...'
    "$zig_command" c++ "${compile_options[@]}" -DWEBCOM_TEST_MODE \
        -c "$project_root/tests/windows_ui_tests.cpp" -o "$output_directory/windows_ui_tests.obj"
    "$zig_command" c++ "${compile_options[@]}" -DWEBCOM_TEST_MODE \
        -c "$project_root/src/app.cpp" -o "$output_directory/app_test.obj"
    test_objects=("$output_directory/windows_ui_tests.obj" "$output_directory/app_test.obj")
    for source_name in core storage bookmarks appearance dark_menu tab_search tab_search_model omnibox_model; do
        test_objects+=("$output_directory/$source_name.obj")
    done
    "$zig_command" c++ -target x86_64-windows-gnu -O2 -Wl,--subsystem,console \
        "${test_objects[@]}" "$output_directory/webcom.res" \
        "$sdk_directory/build/native/x64/WebView2Loader.dll.lib" \
        -lole32 -lshell32 -lshlwapi -lcomctl32 -ldwmapi -luser32 -lgdi32 -lgdiplus -luxtheme \
        -lwininet -lpsapi -ladvapi32 -luuid \
        -o "$output_directory/WebCom-ui-tests.exe"
fi

cp -- "$sdk_directory/build/native/x64/WebView2Loader.dll" "$output_directory/WebView2Loader.dll"
for license_name in LICENSE NOTICE; do
    if [[ -f "$sdk_directory/$license_name.txt" ]]; then
        cp -- "$sdk_directory/$license_name.txt" "$output_directory/$license_name-WebView2.txt"
    fi
done
cp -- "$project_root/resources/webcom.ico" "$output_directory/WebCom.ico"
cp -- "$project_root/resources/webcom-icon.png" "$output_directory/WebCom-icon.png"
cp -- "$project_root/run.cmd" "$output_directory/run.cmd"
if [[ "$build_ui_tests" == true ]]; then
    cp -- "$project_root/Test-UI.cmd" "$output_directory/Test-UI.cmd"
fi
cp -- "$project_root/scripts/Measure-WebComMemory.ps1" "$output_directory/Measure-WebComMemory.ps1"
cp -- "$project_root/scripts/Open-RuntimeDownload.ps1" "$output_directory/Open-RuntimeDownload.ps1"
for document_name in README.md HOW_TO_BUILD.md BUILDING.md START-HERE.txt THIRD-PARTY-NOTICES.md; do
    if [[ -f "$project_root/$document_name" ]]; then
        cp -- "$project_root/$document_name" "$output_directory/$document_name"
    fi
done
for content_directory in docs licenses; do
    if [[ -d "$project_root/$content_directory" ]]; then
        cp -R -- "$project_root/$content_directory" "$output_directory/"
    fi
done
printf '\nCross-build produced: %s\n' "$output_directory/WebCom.exe"
if [[ "$build_ui_tests" == true ]]; then
    printf 'Windows UI test harness: %s\n' "$output_directory/WebCom-ui-tests.exe"
fi
printf '%s\n' 'This is a compile/link check and preview binary. Test it on Windows before treating it as a release.'
