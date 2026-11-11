#include "external_protocol.h"

#include <iostream>
#include <string>

namespace {
int checks=0, failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
}

int main(){
    using namespace webcom;
    auto r=ResolveExternalProtocol("mailto:person@example.com?subject=Hello%20there");
    Check(r.allowed && r.kind==ExternalProtocolKind::Mailto,"mailto allowed");
    r=ResolveExternalProtocol("TEL:+1-617-555-0100");
    Check(r.allowed && r.kind==ExternalProtocolKind::Telephone,"tel allowed case-insensitively");
    r=ResolveExternalProtocol("sms:+16175550100?body=Hello%20there");
    Check(r.allowed && r.kind==ExternalProtocolKind::Sms,"sms body query allowed");
    Check(!ResolveExternalProtocol("javascript:alert(1)").allowed,"javascript never leaves browser");
    Check(!ResolveExternalProtocol("custom-app:secret").allowed,"unknown schemes blocked");
    Check(!ResolveExternalProtocol("file:///C:/Windows/win.ini").allowed,"file scheme blocked");
    Check(!ResolveExternalProtocol("mailto://evil.example").allowed,"authority-style mailto blocked");
    Check(!ResolveExternalProtocol("tel:abc").allowed,"non-number tel blocked");
    Check(!ResolveExternalProtocol(std::string("mailto:test@example.com\nBcc:evil@example.com")).allowed,"control characters blocked");
    Check(!ResolveExternalProtocol(std::string(5000,'x')).allowed,"oversized URI blocked");
    Check(ExternalProtocolName(ExternalProtocolKind::Mailto)=="email application","protocol display name stable");
    std::cout<<checks<<" external protocol checks, "<<failures<<" failures\n";
    return failures?1:0;
}
