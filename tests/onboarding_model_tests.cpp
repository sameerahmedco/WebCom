#include "onboarding_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;if(!value){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
}

int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/"webcom-onboarding-tests";
    std::error_code ec;std::filesystem::remove_all(root,ec);
    Check(!LoadOnboardingState(root).welcome_seen,"missing onboarding state defaults unseen");
    std::string error;
    Check(SaveOnboardingState(root,{true},&error),"welcome seen state saves");
    Check(LoadOnboardingState(root).welcome_seen,"welcome seen state round trips");
    Check(SaveOnboardingState(root,{false},&error),"welcome unseen state can be saved");
    Check(!LoadOnboardingState(root).welcome_seen,"welcome unseen state round trips");
    {
        std::ofstream bad(root/"onboarding.v1",std::ios::binary|std::ios::trunc);
        bad<<"WEBCOM_ONBOARDING\t99\nwelcome_seen\t1\n";
    }
    Check(!LoadOnboardingState(root).welcome_seen,"unknown onboarding schema fails closed");
    {
        std::ofstream bad(root/"onboarding.v1",std::ios::binary|std::ios::trunc);
        bad<<"WEBCOM_ONBOARDING\t1\nwelcome_seen\t2\n";
    }
    Check(!LoadOnboardingState(root).welcome_seen,"invalid onboarding flag fails closed");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
