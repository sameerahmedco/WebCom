#include "update_retry_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    using namespace webcom;
    Check(UpdateRetryDelaySeconds(0)==0,"zero failures has no delay");
    Check(UpdateRetryDelaySeconds(1)==15,"first failure backs off 15 seconds");
    Check(UpdateRetryDelaySeconds(2)==30,"second failure backs off 30 seconds");
    Check(UpdateRetryDelaySeconds(5)==300 && UpdateRetryDelaySeconds(30)==300,"retry delay caps at five minutes");
    UpdateRetryState state;std::string error;
    Check(RegisterUpdateRetryFailure(state,1000,"transport",&error),"transport failure registers");
    Check(state.consecutive_failures==1&&state.retry_after_unix_seconds==1015,"first retry deadline recorded");
    Check(UpdateRetryRemainingSeconds(state,1007)==8,"remaining retry seconds calculated");
    Check(UpdateRetryRemainingSeconds(state,1015)==0,"retry becomes available at deadline");
    Check(RegisterUpdateRetryFailure(state,1015,"manifest-rejected",&error),"second failure registers");
    Check(state.consecutive_failures==2&&state.retry_after_unix_seconds==1045,"second retry deadline uses next delay");
    Check(!RegisterUpdateRetryFailure(state,1045,"bad category!",&error),"unsafe failure category rejected");
    const auto root=std::filesystem::temp_directory_path()/"webcom-update-retry-tests";std::error_code ec;std::filesystem::remove_all(root,ec);
    Check(SaveUpdateRetryState(root,state,&error),"retry state saves atomically");
    Check(LoadUpdateRetryState(root)==state,"retry state round trips");
    ResetUpdateRetry(state);Check(state==UpdateRetryState{},"successful check resets retry state");
    Check(SaveUpdateRetryState(root,state,&error),"idle retry state saves");
    Check(LoadUpdateRetryState(root)==state,"idle retry state round trips");
    {std::ofstream bad(root/"update-retry.v1",std::ios::binary|std::ios::trunc);bad<<"WEBCOM_UPDATE_RETRY\t1\nfailures\t1\nretry_after\t9\ncategory\tbad value!\n";}
    Check(LoadUpdateRetryState(root)==UpdateRetryState{},"corrupt retry state fails closed");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
