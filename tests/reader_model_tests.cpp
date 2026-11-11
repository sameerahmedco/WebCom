#include "reader_model.h"
#include <iostream>

namespace { unsigned checks=0, failures=0; void Check(bool v,const char* label){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<label<<'\n';}} }

int main(){
    using webcom::DecodeJsonString;
    Check(DecodeJsonString(L"\"hello\\nworld\"") == std::optional<std::wstring>(L"hello\nworld"),"basic escapes decode");
    Check(DecodeJsonString(L"\"quote: \\\" slash: \\\\\"") == std::optional<std::wstring>(L"quote: \" slash: \\"),"quote and slash decode");
    Check(DecodeJsonString(L"\"caf\\u00e9\"") == std::optional<std::wstring>(L"caf\u00e9"),"unicode escape decodes");
    const auto emoji=DecodeJsonString(L"\"\\ud83d\\ude80\"");
    Check(emoji && !emoji->empty(),"surrogate pair decodes");
    Check(!DecodeJsonString(L"{\"text\":\"no\"}"),"non-string JSON rejected");
    Check(!DecodeJsonString(L"\"bad\\q\""),"unknown escape rejected");
    const auto capped=DecodeJsonString(L"\"123456789\"",4);
    Check(capped && *capped==L"1234","decoded text respects size cap");
    const auto html=webcom::BuildReaderHtml(L"A <Title>",L"https://example.com/?a=1&b=2",L"Hello & goodbye",1,2,true);
    Check(html.find("&lt;Title&gt;")!=std::string::npos && html.find("&amp;b=2")!=std::string::npos,"reader HTML escapes metadata");
    Check(html.find("Georgia,serif")!=std::string::npos && html.find("max-width:960px")!=std::string::npos,"reader HTML preserves reading preferences");
    const auto markdown=webcom::BuildReaderMarkdown(L"Title",L"https://example.com",L"Body");
    Check(markdown.find("# Title") == 0 && markdown.find("Source: https://example.com")!=std::string::npos,"reader Markdown contains title and source");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
