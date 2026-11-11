#include "update_runtime_model.h"

#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* label){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<label<<'\n';}} }

int main(){
    using namespace webcom;
    const std::string fingerprint="abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    ReleaseRuntimeConfig config{
        "https://updates.example.test/stable.v2.txt",
        "https://updates.example.test/beta.v2.txt",
        fingerprint
    };
    Check(IsTrustedPublisherFingerprint(fingerprint),"publisher fingerprint accepted");
    Check(!IsTrustedPublisherFingerprint("abc"),"short publisher fingerprint rejected");
    Check(ManifestUrlForChannel(config,ReleaseChannel::Stable).has_value(),"stable HTTPS endpoint accepted");
    auto bad_config=config; bad_config.stable_manifest_url="http://updates.example.test/stable";
    Check(!ManifestUrlForChannel(bad_config,ReleaseChannel::Stable),"HTTP manifest endpoint rejected");

    const std::string manifest=
        "WEBCOM_RELEASE\t2\nversion\t2.0.0\nchannel\tstable\n"
        "url\thttps://downloads.example.test/WebCom-Setup-2.0.0-x64.exe\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "size\t12345678\n"
        "signature\tauthenticode\n"
        "publisher_sha256\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
        "published\t2026-11-11T00:00:00Z\n";
    const auto available=EvaluateUpdateManifest("1.9.9",ReleaseChannel::Stable,config,manifest);
    Check(available.status==UpdateCheckStatus::UpdateAvailable && available.manifest.has_value(),"new signed release is available");
    const auto current=EvaluateUpdateManifest("2.0.0",ReleaseChannel::Stable,config,manifest);
    Check(current.status==UpdateCheckStatus::NoUpdate,"same release is not offered as update");
    const auto wrong_channel=EvaluateUpdateManifest("1.9.9",ReleaseChannel::Beta,config,manifest);
    Check(wrong_channel.status==UpdateCheckStatus::WrongChannel,"channel mismatch rejected");

    const std::string stable_prerelease=
        "WEBCOM_RELEASE\t2\nversion\t2.1.0-beta.1\nchannel\tstable\n"
        "url\thttps://downloads.example.test/WebCom-Setup-2.1.0-beta.1-x64.exe\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "size\t12345678\n"
        "signature\tauthenticode\n"
        "publisher_sha256\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
        "published\t2026-11-11T00:00:00Z\n";
    Check(EvaluateUpdateManifest("2.0.0",ReleaseChannel::Stable,config,stable_prerelease).status==UpdateCheckStatus::VersionPolicyRejected,
          "stable channel rejects prerelease version even when manifest channel says stable");

    const std::string beta_prerelease=
        "WEBCOM_RELEASE\t2\nversion\t2.1.0-beta.1\nchannel\tbeta\n"
        "url\thttps://downloads.example.test/WebCom-Setup-2.1.0-beta.1-x64.exe\n"
        "sha256\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "size\t12345678\n"
        "signature\tauthenticode\n"
        "publisher_sha256\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n"
        "published\t2026-11-11T00:00:00Z\n";
    Check(EvaluateUpdateManifest("2.0.0",ReleaseChannel::Beta,config,beta_prerelease).status==UpdateCheckStatus::UpdateAvailable,
          "beta channel accepts signed prerelease version");
    auto mismatch=config; mismatch.publisher_sha256=std::string(64,'1');
    const auto publisher=EvaluateUpdateManifest("1.9.9",ReleaseChannel::Stable,mismatch,manifest);
    Check(publisher.status==UpdateCheckStatus::PublisherMismatch,"manifest publisher must match build pin");
    auto no_endpoint=config; no_endpoint.stable_manifest_url.clear();
    Check(EvaluateUpdateManifest("1.9.9",ReleaseChannel::Stable,no_endpoint,manifest).status==UpdateCheckStatus::EndpointNotConfigured,
          "missing manifest endpoint reported");
    auto no_pin=config; no_pin.publisher_sha256.clear();
    Check(EvaluateUpdateManifest("1.9.9",ReleaseChannel::Stable,no_pin,manifest).status==UpdateCheckStatus::InvalidConfiguration,
          "missing publisher pin reported");
    const auto stage=StagedUpdatePackagePath("C:/WebComData","2.0.0");
    Check(!stage.empty() && stage.filename()=="WebCom-Setup-2.0.0-x64.exe","staging package path deterministic");
    Check(StagedUpdatePackagePath("C:/WebComData","../bad").empty(),"invalid version cannot escape staging root");
    std::cout<<checks<<" checks, "<<failures<<" failures\n"; return failures?1:0;
}
