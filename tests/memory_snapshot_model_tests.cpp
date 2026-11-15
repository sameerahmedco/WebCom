#include "memory_snapshot_model.h"

#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    webcom::MemorySnapshot s;
    s.host_working_set=40ull*1048576; s.host_private_commit=30ull*1048576;
    s.webview_working_set=120ull*1048576; s.webview_private_commit=140ull*1048576;
    s.measured_processes=5; s.expected_processes=5; s.webview_processes=4;
    s.tabs=3; s.web_tabs=2; s.sleeping_tabs=1; s.workspaces=2; s.sleep_policy=L"15 minutes"; s.never_sleep_sites=3;
    const auto text=webcom::BuildMemorySnapshotText(s);
    Check(text.find(L"Total working set: 160.0 MiB")!=std::wstring::npos,"total working set");
    Check(text.find(L"WebCom host: 40.0 MiB")!=std::wstring::npos,"host split");
    Check(text.find(L"WebView2 group: 120.0 MiB")!=std::wstring::npos,"webview split");
    Check(text.find(L"3 total • 2 web • 1 sleeping")!=std::wstring::npos,"tab state");
    Check(text.find(L"Automatic sleep: 15 minutes")!=std::wstring::npos,"sleep policy");
    Check(text.find(L"Never-sleep sites: 3")!=std::wstring::npos,"never sleep count");
    s.runtime_process_list_available=false; s.measured_processes=1; s.expected_processes=4;
    const auto partial=webcom::BuildMemorySnapshotText(s);
    Check(partial.find(L"PARTIAL RESULT")!=std::wstring::npos,"partial result warning");
    Check(partial.find(L"snapshot is incomplete")!=std::wstring::npos,"incomplete measurement warning");
    std::cout<<checks<<" memory snapshot checks, "<<failures<<" failures\n"; return failures?1:0;
}
