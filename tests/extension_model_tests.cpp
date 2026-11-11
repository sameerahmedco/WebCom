#include "extension_model.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }
int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/"webcom-extension-model-tests";
    std::error_code ec;std::filesystem::remove_all(root,ec);std::filesystem::create_directories(root,ec);
    Check(!InspectExtensionFolder(root/"missing").valid,"missing folder rejected");
    Check(!InspectExtensionFolder(root).valid,"folder without manifest rejected");
    {
        std::ofstream out(root/"manifest.json",std::ios::binary|std::ios::trunc);
        out<<R"({"manifest_version":3,"name":"Example","version":"1.2.3","description":"Example helper","minimum_chrome_version":"120","permissions":["storage","tabs"],"host_permissions":["https://example.com/*"],"optional_permissions":["downloads"],"optional_host_permissions":["https://optional.example/*"],"background":{"service_worker":"worker.js"},"content_scripts":[{"matches":["https://example.com/*"],"js":["content.js"]}],"options_page":"options.html","action":{"default_popup":"popup.html"},"side_panel":{"default_path":"side.html"},"commands":{"do-work":{"suggested_key":{"default":"Ctrl+Shift+Y"}}},"omnibox":{"keyword":"ex"},"devtools_page":"devtools.html","web_accessible_resources":[{"resources":["asset.png"],"matches":["https://example.com/*"]}]})";
    }
    const auto valid=InspectExtensionFolder(root);
    Check(valid.valid,"valid unpacked extension preview accepted");
    Check(valid.name=="Example"&&valid.version=="1.2.3"&&valid.manifest_version==3,"manifest identity parsed");
    Check(valid.description=="Example helper","description parsed");
    Check(valid.minimum_chrome_version=="120","minimum Chromium version parsed");
    Check(valid.permissions.size()==2&&valid.permissions[0]=="storage"&&valid.permissions[1]=="tabs","permissions parsed");
    Check(valid.host_permissions.size()==1&&valid.host_permissions[0]=="https://example.com/*","host permissions parsed");
    Check(valid.optional_permissions.size()==1&&valid.optional_permissions[0]=="downloads","optional permissions parsed");
    Check(valid.optional_host_permissions.size()==1&&valid.optional_host_permissions[0]=="https://optional.example/*","optional host permissions parsed");
    Check(valid.has_background,"background/service worker detected");
    Check(valid.has_content_scripts,"content scripts detected");
    Check(valid.has_options_page,"options page detected");
    Check(valid.declares_toolbar_ui,"toolbar UI declaration detected");
    Check(valid.has_side_panel&&valid.has_commands&&valid.has_omnibox&&valid.has_devtools_page&&valid.has_web_accessible_resources,"developer-facing extension surfaces detected");
    const auto summary=ExtensionCompatibilitySummary(valid);
    Check(summary.find("Permissions: storage, tabs")!=std::string::npos,"details summary contains permissions");
    Check(summary.find("Optional permissions: downloads")!=std::string::npos,"details summary contains optional permissions");
    Check(summary.find("Host permissions: https://example.com/*")!=std::string::npos,"details summary contains host permissions");
    Check(summary.find("Minimum Chromium version: 120")!=std::string::npos,"details summary contains minimum Chromium version");
    Check(summary.find("WebView2 may not expose")!=std::string::npos,"details summary warns about toolbar limitation");
    Check(summary.find("side-panel host UI")!=std::string::npos,"details summary warns about side-panel limitation");
    {
        std::ofstream out(root/"manifest.json",std::ios::binary|std::ios::trunc);
        out<<R"({"manifest_version":3,"name":"Content helper","version":"2.0"})";
    }
    const auto content=InspectExtensionFolder(root);
    Check(content.valid&&!content.declares_toolbar_ui,"content-only extension has no toolbar warning");
    Check(content.permissions.empty()&&content.host_permissions.empty()&&content.optional_permissions.empty()&&content.optional_host_permissions.empty(),"missing permission arrays remain empty");
    Check(!content.has_background&&!content.has_content_scripts&&!content.has_options_page&&!content.has_side_panel&&!content.has_commands&&!content.has_omnibox&&!content.has_devtools_page&&!content.has_web_accessible_resources,"missing optional features remain false");
    {
        std::ofstream out(root/"manifest.json",std::ios::binary|std::ios::trunc);out<<R"({"name":"Broken","version":"1"})";
    }
    Check(!InspectExtensionFolder(root).valid,"missing manifest_version rejected by preview");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
