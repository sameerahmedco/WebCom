#include "update_recovery_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/"webcom-recovery-model-tests";std::error_code ec;std::filesystem::remove_all(root,ec);std::filesystem::create_directories(root,ec);
    const auto installer=root/"signed.exe";{std::ofstream out(installer,std::ios::binary);out<<"verified-package";}
    const auto size=std::filesystem::file_size(installer);
    RecoveryPackage meta{"0.7.4",std::string(64,'a'),size,std::string(64,'b')};std::string error;
    Check(PreserveRecoveryPackage(root,meta,installer,&error),"verified installer preserved for rollback");
    const auto loaded=LoadRecoveryPackage(root);Check(loaded&&*loaded==meta,"recovery metadata round trips");
    UpdateTransaction tx;tx.phase=UpdateTransactionPhase::AwaitingHealthCheck;tx.target_version="0.7.5";tx.previous_version="0.7.4";tx.package_sha256=std::string(64,'c');tx.package_size=99;tx.failed_health_starts=2;
    Check(EvaluateRecoveryReadiness(tx,loaded,"0.7.5",std::string(64,'b')).status==RecoveryReadiness::Ready,"matching known-good package is rollback ready");
    Check(EvaluateRecoveryReadiness(tx,loaded,"0.7.4",std::string(64,'b')).status==RecoveryReadiness::RunningVersionMismatch,"rollback requires failed target to be running");
    Check(EvaluateRecoveryReadiness(tx,std::nullopt,"0.7.5",std::string(64,'b')).status==RecoveryReadiness::PackageMissing,"missing recovery package reported");
    auto wrong_version=loaded;wrong_version->version="0.7.3";
    Check(EvaluateRecoveryReadiness(tx,wrong_version,"0.7.5",std::string(64,'b')).status==RecoveryReadiness::PackageVersionMismatch,"wrong recovery version rejected");
    Check(EvaluateRecoveryReadiness(tx,loaded,"0.7.5",std::string(64,'d')).status==RecoveryReadiness::PublisherMismatch,"recovery publisher must match running build pin");
    auto not_recommended=tx;not_recommended.failed_health_starts=1;
    Check(EvaluateRecoveryReadiness(not_recommended,loaded,"0.7.5",std::string(64,'b')).status==RecoveryReadiness::RollbackNotRecommended,"health policy gates rollback readiness");
    Check(RecoveryReadinessName(RecoveryReadiness::Ready)=="ready","recovery readiness status has stable log name");
    Check(RecoveryPackagePath(root,"0.7.4").filename()=="WebCom-Setup-0.7.4-x64.exe","recovery package path deterministic");
    Check(RecoveryPackagePath(root,"../bad").empty(),"invalid recovery version cannot escape root");
    auto bad=meta;bad.sha256="bad";Check(!PreserveRecoveryPackage(root,bad,installer,&error),"invalid recovery hash rejected");
    {std::ofstream out(RecoveryPackagePath(root,"0.7.4"),std::ios::binary|std::ios::app);out<<"tamper";}
    Check(!LoadRecoveryPackage(root).has_value(),"recovery size mismatch fails closed");
    Check(ClearRecoveryPackage(root,&error),"recovery package clears");
    Check(!LoadRecoveryPackage(root).has_value(),"cleared recovery package unavailable");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
