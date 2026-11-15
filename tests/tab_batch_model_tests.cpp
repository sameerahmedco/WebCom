#include "tab_batch_model.h"

#include <iostream>

namespace {
int checks=0, failures=0;
void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<'\n';}}
}

int main(){
    using namespace webcom;
    std::vector<TabBatchItem> items{
        {1,"https://example.com/","Work",false,true,false,false},
        {2,"https://example.com/","Work",false,false,false,false},
        {3,"https://example.com/","Work",false,false,true,false},
        {4,"https://example.com/","Work",false,false,false,true},
        {5,"https://example.com/","Personal",false,false,false,false},
        {6,"https://example.com/","Work",true,false,false,false},
        {7,"https://other.example/","Work",false,false,false,false}
    };
    const auto close=DuplicateTabCloseCandidates(items,1,"Work");
    Check(close.size()==1 && close[0]==2,"closes only safe duplicate");
    Check(DuplicateTabCloseCandidates(items,99,"Work").empty(),"missing target returns empty");
    Check(DuplicateTabCloseCandidates(items,1,"Personal").empty(),"workspace scope is enforced");
    const auto private_close=DuplicateTabCloseCandidates(items,6,"Work");
    Check(private_close.empty(),"private and normal tabs are distinct");
    std::cout<<(checks-failures)<<" tab batch checks, "<<failures<<" failures\n";
    return failures?1:0;
}
