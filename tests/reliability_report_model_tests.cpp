#include "reliability_report_model.h"
#include <iostream>
namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }
int main(){
    webcom::ReliabilityReportSnapshot s; s.version="2.0.2";s.previous_session_unclean=true;s.update_phase="awaiting-health-check";s.update_target="2.0.2";s.update_previous="2.0.1";s.failed_health_starts=2;s.rollback_recommended=true;s.recovery_version="2.0.1";s.update_retry_failures=3;s.update_retry_remaining_seconds=42;s.update_retry_category="transport";
    const auto text=webcom::BuildReliabilityReportText(s);
    Check(text.find("Version: 2.0.2")!=std::string::npos,"version included");
    Check(text.find("Previous session unclean: yes")!=std::string::npos,"unclean evidence included");
    Check(text.find("Rollback recommended: yes")!=std::string::npos,"rollback recommendation included");
    Check(text.find("Recovery installer: 2.0.1")!=std::string::npos,"recovery version included");
    Check(text.find("Last update failure category: transport")!=std::string::npos,"retry category included");
    const auto privacy=webcom::ReliabilityReportPrivacyText();
    Check(privacy.find("not uploaded automatically")!=std::string::npos,"privacy text states no automatic upload");
    Check(privacy.find("password")!=std::string::npos,"privacy text names excluded password data");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
