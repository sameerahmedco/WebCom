#include "startup_model.h"

#include <filesystem>
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

    const auto temp=std::filesystem::absolute(std::filesystem::path("qa-temp")).lexically_normal();
    Check(IsAcceptanceDataRootAllowed(temp / "WebCom-Acceptance-1234",temp),"acceptance root allows reserved direct child");
    Check(IsAcceptanceDataRootAllowed(temp / "WebCom-Acceptance-run_01",temp),"acceptance root allows bounded QA suffix characters");
    Check(!IsAcceptanceDataRootAllowed(temp / "WebCom",temp),"acceptance root rejects ordinary product folder");
    Check(!IsAcceptanceDataRootAllowed(temp / "WebCom-Acceptance-",temp),"acceptance root requires a nonempty suffix");
    Check(!IsAcceptanceDataRootAllowed(temp / "nested" / "WebCom-Acceptance-1",temp),"acceptance root rejects nested paths");
    Check(!IsAcceptanceDataRootAllowed(temp.parent_path() / "WebCom-Acceptance-1",temp),"acceptance root rejects sibling paths");
    Check(!IsAcceptanceDataRootAllowed(std::filesystem::path("WebCom-Acceptance-1"),temp),"acceptance root rejects relative paths");

    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
