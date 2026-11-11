#include "update_model.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0, failures=0; void Check(bool v,const char* label){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<label<<'\n';}} }

int main(){
    using namespace webcom;
    Check(ParseReleaseChannel("stable") == ReleaseChannel::Stable,"stable channel parses");
    Check(ParseReleaseChannel("BETA") == ReleaseChannel::Beta,"beta channel is case insensitive");
    Check(!ParseReleaseChannel("nightly"),"unknown channel rejected");
    Check(IsValidReleaseVersion("2.0.0"),"stable semantic version accepted");
    Check(IsValidReleaseVersion("2.1.0-beta.1"),"prerelease semantic version accepted");
    Check(IsPrereleaseReleaseVersion("2.1.0-beta.1"),"prerelease version classified");
    Check(!IsPrereleaseReleaseVersion("2.1.0"),"stable version is not prerelease");
    Check(IsReleaseVersionAllowedOnChannel("2.1.0",ReleaseChannel::Stable),"stable version allowed on stable channel");
    Check(!IsReleaseVersionAllowedOnChannel("2.1.0-beta.1",ReleaseChannel::Stable),"prerelease blocked on stable channel");
    Check(IsReleaseVersionAllowedOnChannel("2.1.0-beta.1",ReleaseChannel::Beta),"prerelease allowed on beta channel");
    Check(IsReleaseVersionAllowedOnChannel("2.1.0",ReleaseChannel::Beta),"final release allowed on beta channel");
    Check(!IsValidReleaseVersion("2.0"),"short version rejected");
    Check(IsNewerReleaseVersion("1.9.9","2.0.0"),"new patch is newer");
    Check(IsNewerReleaseVersion("2.1.0-beta.1","2.1.0"),"stable beats same prerelease");
    Check(!IsNewerReleaseVersion("2.0.0","1.9.9"),"older release not newer");

    const std::string schema1_manifest =
        "WEBCOM_RELEASE\t1\nversion\t2.0.0\nchannel\tstable\n"
        "url\thttps://downloads.example.test/WebCom-2.0.0.zip\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "published\t2026-11-10T22:00:00Z\n";
    std::string error;
    const auto parsed_schema1=ParseUpdateManifest(schema1_manifest,&error);
    Check(parsed_schema1 && parsed_schema1->schema_version==1,"schema v1 manifest still parses");
    Check(parsed_schema1 && !IsInstallEligibleUpdateManifest(*parsed_schema1),"schema v1 manifest is never install eligible");

    const std::string signed_manifest =
        "WEBCOM_RELEASE\t2\nversion\t2.0.0\nchannel\tstable\n"
        "url\thttps://downloads.example.test/WebCom-2.0.0-installer.exe\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "size\t12345678\n"
        "signature\tauthenticode\n"
        "publisher_sha256\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
        "published\t2026-11-10T22:00:00Z\n";
    const auto parsed=ParseUpdateManifest(signed_manifest,&error);
    Check(parsed && parsed->schema_version==2 && parsed->size_bytes==12345678,"signed v2 manifest parses");
    Check(parsed && parsed->signature==PackageSignatureKind::Authenticode,"authenticode requirement parses");
    Check(parsed && IsInstallEligibleUpdateManifest(*parsed),"complete v2 manifest is install eligible after binary verification");

    const std::string bad_stable_prerelease =
        "WEBCOM_RELEASE\t2\nversion\t2.1.0-beta.2\nchannel\tstable\n"
        "url\thttps://downloads.example.test/WebCom-2.1.0-beta.2-installer.exe\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "size\t12345678\n"
        "signature\tauthenticode\n"
        "publisher_sha256\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
        "published\t2026-11-10T22:00:00Z\n";
    const auto parsed_bad_stable_prerelease=ParseUpdateManifest(bad_stable_prerelease,&error);
    Check(parsed_bad_stable_prerelease && !IsInstallEligibleUpdateManifest(*parsed_bad_stable_prerelease),"stable prerelease manifest parses but is not install eligible");
    Check(!ParseUpdateManifest(signed_manifest+"extra\tvalue\n"),"unknown manifest field rejected");
    auto bad=signed_manifest; bad.replace(bad.find("https://"),8,"http://");
    Check(!ParseUpdateManifest(bad),"non-HTTPS update URL rejected");
    bad=signed_manifest; bad.replace(bad.find("authenticode"),12,"none");
    Check(!ParseUpdateManifest(bad),"unsigned package type rejected");
    bad=signed_manifest; bad.replace(bad.find("size\t12345678"),13,"size\t0");
    Check(!ParseUpdateManifest(bad),"zero package size rejected");
    bad=signed_manifest; const auto pub=bad.find("publisher_sha256\t"); bad.erase(pub+17,1);
    Check(!ParseUpdateManifest(bad),"invalid publisher fingerprint rejected");

    const auto root=std::filesystem::temp_directory_path()/"webcom-update-model-tests";
    std::error_code ec; std::filesystem::remove_all(root,ec);
    Check(LoadUpdateSettings(root).channel==ReleaseChannel::Stable,"missing settings default stable");
    Check(!LoadUpdateSettings(root).checks_enabled,"missing settings default update checks off");
    Check(SaveUpdateSettings(root,{ReleaseChannel::Beta,true},&error),"beta update setting saves");
    const auto settings=LoadUpdateSettings(root);
    Check(settings.channel==ReleaseChannel::Beta && settings.checks_enabled,"beta channel and update-check consent round trip");
    {
        std::ofstream older_settings(root / "updates.v1", std::ios::binary | std::ios::trunc);
        older_settings << "WEBCOM_UPDATES\t1\nchannel\tstable\n";
    }
    const auto migrated=LoadUpdateSettings(root);
    Check(migrated.channel==ReleaseChannel::Stable && !migrated.checks_enabled,"v1 settings migrate with checks disabled");

    UpdateTransaction transaction;
    Check(parsed && StageVerifiedUpdate(transaction,*parsed,"1.9.9",&error),"verified signed update can be staged");
    Check(transaction.phase==UpdateTransactionPhase::VerifiedPackage && transaction.target_version=="2.0.0" && transaction.previous_version=="1.9.9",
          "staged transaction records target and previous versions");
    UpdateTransaction schema1_transaction;
    Check(parsed_schema1 && !StageVerifiedUpdate(schema1_transaction,*parsed_schema1,"1.9.9",&error),"schema v1 unsigned manifest cannot be staged");
    UpdateTransaction stable_prerelease_transaction;
    Check(parsed_bad_stable_prerelease && !StageVerifiedUpdate(stable_prerelease_transaction,*parsed_bad_stable_prerelease,"1.9.9",&error),"stable prerelease manifest cannot be staged");
    Check(MarkUpdateInstallScheduled(transaction,&error) && transaction.phase==UpdateTransactionPhase::InstallScheduled,"verified package can be scheduled");
    Check(MarkUpdateAwaitingHealthCheck(transaction,&error) && transaction.phase==UpdateTransactionPhase::AwaitingHealthCheck,"scheduled install enters health-check state");
    Check(!UpdateRollbackRecommended(transaction),"rollback is not recommended before repeated health failures");
    Check(RecordUpdateHealthFailure(transaction,&error),"first update health failure records");
    Check(!UpdateRollbackRecommended(transaction),"one failed health start does not yet recommend rollback");
    Check(RecordUpdateHealthFailure(transaction,&error) && UpdateRollbackRecommended(transaction),"two failed health starts recommend rollback");
    Check(SaveUpdateTransaction(root,transaction,&error),"update transaction saves atomically");
    Check(LoadUpdateTransaction(root)==transaction,"update transaction round trips");
    Check(!MarkUpdateHealthy(transaction,"1.9.9",&error),"wrong running version cannot mark update healthy");
    Check(MarkUpdateHealthy(transaction,"2.0.0",&error) && transaction.phase==UpdateTransactionPhase::None,"matching target clears successful update transaction");
    Check(SaveUpdateTransaction(root,transaction,&error),"idle update transaction saves");
    Check(LoadUpdateTransaction(root).phase==UpdateTransactionPhase::None,"idle update transaction loads");

    UpdateTransaction invalid_transaction;
    invalid_transaction.phase=UpdateTransactionPhase::VerifiedPackage;
    invalid_transaction.target_version="2.0.0";
    invalid_transaction.previous_version="1.9.9";
    invalid_transaction.package_sha256=std::string(64,'a');
    invalid_transaction.package_size=100;
    invalid_transaction.failed_health_starts=1;
    Check(!SaveUpdateTransaction(root,invalid_transaction,&error),"failed-start count rejected outside health-check phase");

    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n"; return failures?1:0;
}
