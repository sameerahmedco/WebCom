#include "saved_sessions.h"

#include <filesystem>
#include <iostream>
#include <vector>

namespace {
unsigned checks=0, failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
}

int main(){
    using namespace webcom;
    std::vector<NamedSession> sessions;
    std::string error;
    NamedSession focus{"Focus set",1234,{{"https://example.com/","Example",true,"Focus"},{"https://openai.com/","OpenAI",false,"Focus"}}};
    Check(UpsertNamedSession(sessions,focus,&error) && sessions.size()==1,"session inserted");
    focus.created_ms=5678;focus.tabs.resize(1);
    Check(UpsertNamedSession(sessions,focus,&error) && sessions.size()==1 && sessions[0].created_ms==5678,"case-exact session replaced");
    NamedSession lower=focus;lower.name="focus SET";
    Check(UpsertNamedSession(sessions,lower,&error) && sessions.size()==1 && sessions[0].name=="focus SET","case-insensitive session replaced");
    Check(DeleteNamedSession(sessions,"FOCUS set") && sessions.empty(),"case-insensitive session deletion");

    sessions={{"Research",100,{{"https://example.com/research","Research",false,"Research"}}},
              {"Morning",200,{{"https://news.example.com/","News",false,"Main"},{"https://example.org/","Example",true,"Main"}}}};
    const auto root=std::filesystem::temp_directory_path()/"webcom-named-sessions-test";
    std::error_code ec;std::filesystem::remove_all(root,ec);
    Check(SaveNamedSessions(root,"Work",sessions,&error),"sessions save");
    const auto loaded=LoadNamedSessions(root,"Work");
    Check(loaded.size()==2 && loaded[0].name=="Research" && loaded[1].tabs.size()==2,"sessions round trip");
    Check(loaded[1].tabs[1].pinned && loaded[1].tabs[1].workspace=="Main","tab metadata round trip");
    auto invalid=sessions;invalid[0].tabs[0].url="javascript:alert(1)";
    Check(!SaveNamedSessions(root,"Work",invalid,&error) && !error.empty(),"unsafe session URL rejected");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
