#include "diagnostics_model.h"
#include <iostream>
namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }
int main(){
    webcom::DiagnosticsSnapshot s; s.version=L"0.6.8"; s.runtime_version=L"157.0.0"; s.profile_key=L"Default"; s.profile_label=L"Work";
    s.update_channel=L"beta"; s.active_workspace=L"Research"; s.data_path=L"C:\\WebCom"; s.tabs=8; s.web_tabs=6; s.sleeping_tabs=2; s.workspaces=3; s.vertical_tabs=true; s.reduced_motion=true;
    const auto text=webcom::BuildDiagnosticsText(s);
    Check(text.find(L"Version: 0.6.8")!=std::wstring::npos,"version included");
    Check(text.find(L"WebView2 Runtime: 157.0.0")!=std::wstring::npos,"runtime included");
    Check(text.find(L"Profile: Work")!=std::wstring::npos && text.find(L"Profile key: Default")!=std::wstring::npos,"label and immutable key included");
    Check(text.find(L"8 total, 6 web, 2 sleeping")!=std::wstring::npos,"tab counts included");
    Check(text.find(L"Update channel: beta")!=std::wstring::npos,"channel included");
    std::cout<<checks<<" checks, "<<failures<<" failures\n"; return failures?1:0;
}
