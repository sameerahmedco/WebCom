cmake_minimum_required(VERSION 3.21)

# /MT keeps the app self-contained apart from Windows and the WebView2 Runtime.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(WebCom VERSION 0.5.1 LANGUAGES CXX)

option(WEBCOM_BUILD_APP "Build the native Windows browser" ${WIN32})
option(WEBCOM_BUILD_UI_TESTS "Build the native Windows UI regression harness" ${WIN32})
include(CTest)

add_library(webcom_core STATIC src/core.cpp)
target_include_directories(webcom_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(webcom_core PUBLIC cxx_std_20)
set_target_properties(webcom_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(webcom_core PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(webcom_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(webcom_core_tests tests/core_tests.cpp)
    target_link_libraries(webcom_core_tests PRIVATE webcom_core)
    if(MSVC)
        target_compile_options(webcom_core_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    endif()
    add_test(NAME webcom_core_tests COMMAND webcom_core_tests)

    add_executable(webcom_storage_tests tests/storage_tests.cpp src/storage.cpp)
    target_link_libraries(webcom_storage_tests PRIVATE webcom_core)
    if(WIN32)
        target_compile_definitions(webcom_storage_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(webcom_storage_tests PRIVATE shell32 ole32 uuid)
    endif()
    if(MSVC)
        target_compile_options(webcom_storage_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(webcom_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME webcom_storage_tests COMMAND webcom_storage_tests)

    add_executable(webcom_bookmark_tests tests/bookmark_tests.cpp src/bookmarks.cpp)
    target_link_libraries(webcom_bookmark_tests PRIVATE webcom_core)
    if(WIN32)
        target_compile_definitions(webcom_bookmark_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        target_compile_options(webcom_bookmark_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(webcom_bookmark_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME webcom_bookmark_tests COMMAND webcom_bookmark_tests)

    add_executable(webcom_launch_request_tests tests/launch_request_tests.cpp)
    target_link_libraries(webcom_launch_request_tests PRIVATE webcom_core)
    add_test(NAME webcom_launch_request_tests COMMAND webcom_launch_request_tests)

    add_executable(webcom_tab_search_tests tests/tab_search_tests.cpp src/tab_search_model.cpp)
    target_include_directories(webcom_tab_search_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(webcom_tab_search_tests PRIVATE cxx_std_20)
    set_target_properties(webcom_tab_search_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME webcom_tab_search_tests COMMAND webcom_tab_search_tests)

    add_executable(webcom_appearance_tests tests/appearance_tests.cpp src/appearance.cpp)
    target_link_libraries(webcom_appearance_tests PRIVATE webcom_core)
    if(WIN32)
        target_compile_definitions(webcom_appearance_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    add_test(NAME webcom_appearance_tests COMMAND webcom_appearance_tests)

    add_executable(webcom_omnibox_tests tests/omnibox_model_tests.cpp src/omnibox_model.cpp)
    target_include_directories(webcom_omnibox_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(webcom_omnibox_tests PRIVATE cxx_std_20)
    set_target_properties(webcom_omnibox_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME webcom_omnibox_tests COMMAND webcom_omnibox_tests)

    add_executable(webcom_history_tests tests/history_model_tests.cpp src/history_model.cpp)
    target_include_directories(webcom_history_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(webcom_history_tests PRIVATE cxx_std_20)
    set_target_properties(webcom_history_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME webcom_history_tests COMMAND webcom_history_tests)

    add_executable(webcom_site_permissions_tests tests/site_permissions_tests.cpp src/site_permissions.cpp)
    target_include_directories(webcom_site_permissions_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(webcom_site_permissions_tests PRIVATE cxx_std_20)
    set_target_properties(webcom_site_permissions_tests PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(webcom_site_permissions_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(webcom_site_permissions_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME webcom_site_permissions_tests COMMAND webcom_site_permissions_tests)

    add_executable(webcom_chrome_layout_tests tests/chrome_layout_tests.cpp)
    target_include_directories(webcom_chrome_layout_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(webcom_chrome_layout_tests PRIVATE cxx_std_20)
    set_target_properties(webcom_chrome_layout_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME webcom_chrome_layout_tests COMMAND webcom_chrome_layout_tests)

    foreach(test_target webcom_launch_request_tests webcom_tab_search_tests webcom_appearance_tests webcom_chrome_layout_tests webcom_omnibox_tests webcom_history_tests)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
        else()
            target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
endif()

if(WEBCOM_BUILD_APP)
    if(NOT WIN32)
        message(FATAL_ERROR "The browser UI requires Windows. Use -DWEBCOM_BUILD_APP=OFF for portable core tests.")
    endif()
    if(NOT MSVC)
        message(FATAL_ERROR "This build uses the Windows SDK WRL headers. Use MSVC or clang-cl in a Visual Studio developer environment.")
    endif()
    if(MSVC_VERSION LESS 1929)
        message(FATAL_ERROR "WebCom requires MSVC 19.29 or newer. Install current Visual Studio 2022 Build Tools with Desktop development with C++.")
    endif()
    enable_language(RC)

    set(WEBVIEW2_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/Microsoft.Web.WebView2.1.0.4314.35"
        CACHE PATH "Extracted Microsoft.Web.WebView2 NuGet package directory")
    if(NOT EXISTS "${WEBVIEW2_SDK_DIR}/build/native/include/WebView2.h")
        message(FATAL_ERROR "WebView2 SDK missing. Run scripts/Restore-WebView2.ps1, build.cmd, or set WEBVIEW2_SDK_DIR.")
    endif()

    # Ninja derives this from the selected MSVC compiler; VS uses its platform.
    if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(ARM64|ARM64EC)$" OR
       CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64$" OR
       CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(WEBCOM_LOADER_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WEBCOM_LOADER_ARCH x64)
    else()
        message(FATAL_ERROR "WebCom targets x64 or ARM64 Windows. Select the x64 developer environment.")
    endif()

    set(WEBCOM_LOADER_DIR "${WEBVIEW2_SDK_DIR}/build/native/${WEBCOM_LOADER_ARCH}")
    foreach(required_file WebView2Loader.dll WebView2Loader.dll.lib)
        if(NOT EXISTS "${WEBCOM_LOADER_DIR}/${required_file}")
            message(FATAL_ERROR "Incomplete WebView2 SDK: ${WEBCOM_LOADER_DIR}/${required_file}")
        endif()
    endforeach()
    add_library(WebView2Loader SHARED IMPORTED)
    set_target_properties(WebView2Loader PROPERTIES
        IMPORTED_IMPLIB "${WEBCOM_LOADER_DIR}/WebView2Loader.dll.lib"
        IMPORTED_LOCATION "${WEBCOM_LOADER_DIR}/WebView2Loader.dll")

    set(WEBCOM_NATIVE_SOURCES
        src/app.cpp src/storage.cpp src/bookmarks.cpp src/appearance.cpp src/dark_menu.cpp
        src/tab_search.cpp src/tab_search_model.cpp src/omnibox_model.cpp src/history_model.cpp src/site_permissions.cpp resources/webcom.rc)

    function(webcom_configure_native_target target_name)
        target_compile_features(${target_name} PRIVATE cxx_std_20)
        set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
        target_include_directories(${target_name} PRIVATE
            "${WEBVIEW2_SDK_DIR}/build/native/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources")
        target_compile_definitions(${target_name} PRIVATE
            UNICODE _UNICODE NOMINMAX
            WINVER=0x0A00 _WIN32_WINNT=0x0A00)
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8;/EHsc;/guard:cf>")
        target_link_options(${target_name} PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf /MANIFEST:NO)
        target_link_libraries(${target_name} PRIVATE webcom_core WebView2Loader
            ole32 shell32 shlwapi comctl32 comdlg32 dwmapi user32 gdi32 gdiplus uxtheme wininet psapi advapi32 uuid)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${WEBCOM_LOADER_DIR}/WebView2Loader.dll" "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Copying the WebView2 runtime loader")
    endfunction()

    add_executable(WebCom WIN32 src/main.cpp ${WEBCOM_NATIVE_SOURCES})
    webcom_configure_native_target(WebCom)

    if(BUILD_TESTING AND WEBCOM_BUILD_UI_TESTS)
        # A separate app.cpp compilation keeps the isolated-root test hook out
        # of the production executable; no production main.cpp is linked here.
        add_executable(webcom_windows_ui_tests tests/windows_ui_tests.cpp ${WEBCOM_NATIVE_SOURCES})
        set_target_properties(webcom_windows_ui_tests PROPERTIES OUTPUT_NAME "WebCom-ui-tests")
        webcom_configure_native_target(webcom_windows_ui_tests)
        target_compile_definitions(webcom_windows_ui_tests PRIVATE WEBCOM_TEST_MODE)
        add_test(NAME webcom_windows_ui_tests COMMAND webcom_windows_ui_tests)
        set_tests_properties(webcom_windows_ui_tests PROPERTIES
            TIMEOUT 90 RUN_SERIAL TRUE
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:webcom_windows_ui_tests>")
    endif()

    install(TARGETS WebCom RUNTIME DESTINATION .)
    install(FILES "${WEBCOM_LOADER_DIR}/WebView2Loader.dll" DESTINATION .)
endif()
