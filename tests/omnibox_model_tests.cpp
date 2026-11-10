#include "omnibox_model.h"
#include <iostream>
using namespace webcom;
namespace {int checks=0,failures=0;void Check(bool c,const char* m){++checks;if(!c){++failures;std::cerr<<"FAIL: "<<m<<'\n';}}}
int main(){
    std::vector<OmniboxCandidate> c{
        {OmniboxSuggestionKind::History,L"Example Docs",L"https://example.com/docs",10,0,false},
        {OmniboxSuggestionKind::Bookmark,L"Example Home",L"https://example.com",0,0,false},
        {OmniboxSuggestionKind::OpenTab,L"Example Dashboard",L"https://example.com/dashboard",0,42,false},
        {OmniboxSuggestionKind::History,L"Other",L"https://other.test",20,0,false}};
    auto r=BuildOmniboxSuggestions(L"example",c,8);
    Check(!r.empty(),"non-empty query returns suggestions");
    Check(r.front().kind==OmniboxSuggestionKind::Search,"plain text keeps Google search first");
    bool tab=false,bookmark=false;for(const auto& s:r){tab|=s.tab_id==42;bookmark|=s.kind==OmniboxSuggestionKind::Bookmark;}
    Check(tab,"matching open tab is suggested");Check(bookmark,"matching bookmark is suggested");
    r=BuildOmniboxSuggestions(L"example.com",c,3);
    Check(r.size()<=3,"result limit is enforced");
    Check(r.size()>1 && r[1].kind==OmniboxSuggestionKind::Search,"URL-like input keeps direct match ahead of search");
    Check(BuildOmniboxSuggestions(L"   ",c).empty(),"blank query hides suggestions");
    std::cout<<checks<<" omnibox checks, "<<failures<<" failures\n";return failures?1:0;
}
