#include "update_automation_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

int main() {
    using namespace webcom;
    UpdateAutomationState state;
    Check(BackgroundUpdateCheckDue(state, 1000), "fresh profile is due for a background check");
    Check(BackgroundUpdateCheckDelaySeconds(state, 1000, 0, true)==60,
          "startup due check waits one minute");
    Check(BackgroundUpdateCheckDelaySeconds(state, 1000, 0, false)==15,
          "interactive enable schedules a prompt background check quickly");
    RecordBackgroundUpdateCheck(state,1000);
    Check(!BackgroundUpdateCheckDue(state,1001),"recent background check is not due again");
    Check(BackgroundUpdateCheckDelaySeconds(state,1000,0,false)==kBackgroundUpdateCheckIntervalSeconds,
          "successful scheduling uses six-hour cadence");
    Check(BackgroundUpdateCheckDue(state,1000+kBackgroundUpdateCheckIntervalSeconds),
          "six-hour cadence becomes due exactly at boundary");
    Check(BackgroundUpdateCheckDue(state,999),"clock moving backwards fails toward one safe recheck");
    Check(!BackgroundUpdateQuickRetryAllowed(0),"no failure does not request a quick background retry");
    Check(BackgroundUpdateQuickRetryAllowed(1)&&BackgroundUpdateQuickRetryAllowed(2),"first two failures allow bounded quick retries");
    Check(!BackgroundUpdateQuickRetryAllowed(3)&&!BackgroundUpdateQuickRetryAllowed(32),"third and later failures fall back to normal six-hour cadence");
    Check(BackgroundUpdateCheckDelaySeconds(state,1010,4,false)==15,
          "short retry is clamped away from a tight polling loop");
    Check(BackgroundUpdateCheckDelaySeconds(state,1010,45,false)==45,
          "normal retry backoff is preserved");
    Check(BackgroundUpdateCheckDelaySeconds(state,1010,600,false)==300,
          "background retry delay is capped at five minutes");

    const auto root=std::filesystem::temp_directory_path()/"webcom-update-automation-tests";
    std::error_code ec; std::filesystem::remove_all(root,ec);
    std::string error;
    Check(SaveUpdateAutomationState(root,state,&error),"background check timestamp saves atomically");
    Check(LoadUpdateAutomationState(root)==state,"background check timestamp round trips");
    {
        std::ofstream bad(root/"update-automation.v1",std::ios::binary|std::ios::trunc);
        bad << "WEBCOM_UPDATE_AUTOMATION\t1\nlast_background_check\tnot-a-number\n";
    }
    Check(LoadUpdateAutomationState(root)==UpdateAutomationState{},"corrupt automation state fails closed");
    std::filesystem::remove_all(root,ec);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
