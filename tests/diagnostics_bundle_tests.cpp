#include "diagnostics_bundle.h"
#include <iostream>
#include <string>

namespace { unsigned checks=0, failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    using namespace webcom;
    Check(DiagnosticsBundleNameSafe("diagnostics.txt"),"simple filename accepted");
    Check(!DiagnosticsBundleNameSafe("../secret.txt"),"parent traversal rejected");
    Check(!DiagnosticsBundleNameSafe("folder/file.txt"),"nested path rejected");
    Check(!DiagnosticsBundleNameSafe(".hidden"),"hidden filename rejected");

    std::string error;
    const std::vector<DiagnosticsBundleEntry> entries{
        {"diagnostics.txt","WebCom support diagnostics\r\nVersion: 0.6.9\r\n"},
        {"privacy.txt","No browsing URLs, cookies or password values are included.\r\n"},
        {"updates.txt","Channel: stable\r\nAutomatic install: disabled\r\n"}
    };
    const auto zip=BuildDiagnosticsZip(entries,&error);
    Check(zip.has_value(),"diagnostics zip builds");
    Check(zip && zip->size()>100,"diagnostics zip has content");
    Check(zip && (*zip)[0]==0x50 && (*zip)[1]==0x4b && (*zip)[2]==0x03 && (*zip)[3]==0x04,"zip local header signature present");
    if(zip){
        const std::string bytes(reinterpret_cast<const char*>(zip->data()),zip->size());
        Check(bytes.find("diagnostics.txt")!=std::string::npos,"diagnostics entry listed");
        Check(bytes.find("privacy.txt")!=std::string::npos,"privacy entry listed");
        Check(bytes.find("https://example.com/private") == std::string::npos,"unrelated browsing URL absent");
    }
    auto dup=entries;dup.push_back({"Diagnostics.TXT","x"});
    Check(!BuildDiagnosticsZip(dup),"case-insensitive duplicate filenames rejected");
    Check(!BuildDiagnosticsZip({{"../bad.txt","x"}}),"unsafe entry rejected by builder");
    Check(!BuildDiagnosticsZip({{"huge.txt",std::string(1024u*1024u+1u,'x')}}),"oversized single entry rejected");
    Check(!BuildDiagnosticsZip(entries,nullptr,128),"invalid tiny bundle limit rejected");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
