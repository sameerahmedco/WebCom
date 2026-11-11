#include "runtime_log.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/"webcom-runtime-log-tests";
    std::error_code ec;std::filesystem::remove_all(root,ec);std::string error;
    bool unclean=false;
    Check(BeginRuntimeSession(root,"0.7.5",&unclean,&error)&&!unclean,"first session starts clean");
    auto state=LoadRuntimeSessionState(root);
    Check(state.running&&state.version=="0.7.5"&&state.sequence==1,"runtime marker records active session");
    Check(AppendRuntimeLog(root,RuntimeLogLevel::Warning,"update.health.retry","see https://example.com/private token\tvalue",&error),"runtime event appends");
    auto text=ReadRuntimeLogBundleText(root);
    Check(text.find("update.health.retry")!=std::string::npos,"runtime bundle contains event");
    Check(text.find("https://example.com/private")==std::string::npos&&text.find("[redacted-url]")!=std::string::npos,"runtime details redact URLs");
    Check(EndRuntimeSession(root,"0.7.5",&error),"clean shutdown marker saves");
    Check(!LoadRuntimeSessionState(root).running,"clean shutdown clears running marker");
    Check(BeginRuntimeSession(root,"0.7.5",&unclean,&error)&&!unclean,"next clean session is not flagged unclean");
    // Simulate a crash by beginning another process/session without ending the current marker.
    Check(BeginRuntimeSession(root,"0.7.5",&unclean,&error)&&unclean,"unclean previous session is detected");
    Check(ReadRuntimeLogBundleText(root).find("session.previous_unclean")!=std::string::npos,"unclean start is logged");
    Check(!AppendRuntimeLog(root,RuntimeLogLevel::Info,"bad event","x",&error),"invalid event name rejected");
    const auto safe=SanitizeRuntimeLogDetail("file:///C:/secret.html\nhttps://example.test/a");
    Check(safe.find("file:///")==std::string::npos&&safe.find("https://")==std::string::npos,"sanitizer strips file and web URLs");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
