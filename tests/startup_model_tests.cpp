#include "startup_model.h"

#include <iostream>
#include <string_view>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,std::string_view label){++checks;if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
}

int main(){
    using namespace webcom;
    SavedProfile profile;
    profile.startup_mode=StartupMode::Continue;
    auto plan=BuildStartupPlan(profile);
    Check(plan.restore_saved_session && plan.urls.empty(),"continue restores the saved session");

    profile.startup_mode=StartupMode::NewTab;
    plan=BuildStartupPlan(profile);
    Check(!plan.restore_saved_session && plan.urls.size()==1 && plan.urls[0]=="webcom:newtab","new-tab mode opens one new tab");

    profile.startup_mode=StartupMode::SpecificPages;
    profile.startup_pages={"https://example.com/","https://example.org/"};
    plan=BuildStartupPlan(profile);
    Check(!plan.restore_saved_session && plan.urls==profile.startup_pages,"specific-pages mode preserves configured order");

    profile.startup_pages.clear();
    plan=BuildStartupPlan(profile);
    Check(!plan.restore_saved_session && plan.urls.size()==1 && plan.urls[0]=="webcom:newtab","empty specific-pages mode safely falls back to new tab");

    profile.startup_pages={"https://example.com/"};
    plan=BuildStartupPlan(profile,true);
    Check(plan.restore_saved_session && plan.urls.empty(),"recovery override restores recovered tabs regardless of startup preference");

    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
