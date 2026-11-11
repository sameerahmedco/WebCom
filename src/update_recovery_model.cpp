#include "update_recovery_model.h"
#include "update_model.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace webcom {
namespace {
constexpr std::string_view kHeader="WEBCOM_RECOVERY_PACKAGE\t1";

bool Fail(std::string* error,std::string value){if(error)*error=std::move(value);return false;}
bool Hex64(std::string_view value){return value.size()==64&&std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isdigit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');});}
std::string Lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
std::filesystem::path RecoveryRoot(const std::filesystem::path& root){return root/"Updates"/"recovery";}
std::filesystem::path MetadataPath(const std::filesystem::path& root){return RecoveryRoot(root)/"recovery.v1";}
bool Valid(const RecoveryPackage& m){return IsValidReleaseVersion(m.version)&&Hex64(m.sha256)&&Hex64(m.publisher_sha256)&&m.size_bytes>0&&m.size_bytes<=4ull*1024ull*1024ull*1024ull;}
}


std::string RecoveryReadinessName(RecoveryReadiness status){
    switch(status){
    case RecoveryReadiness::Ready:return "ready";
    case RecoveryReadiness::RollbackNotRecommended:return "rollback-not-recommended";
    case RecoveryReadiness::RunningVersionMismatch:return "running-version-mismatch";
    case RecoveryReadiness::PackageMissing:return "package-missing";
    case RecoveryReadiness::PackageVersionMismatch:return "package-version-mismatch";
    case RecoveryReadiness::PublisherMismatch:return "publisher-mismatch";
    case RecoveryReadiness::MetadataInvalid:return "metadata-invalid";
    }
    return "metadata-invalid";
}

RecoveryEvaluation EvaluateRecoveryReadiness(const UpdateTransaction& transaction,
                                             const std::optional<RecoveryPackage>& package,
                                             std::string_view running_version,
                                             std::string_view pinned_publisher_sha256){
    if(!UpdateRollbackRecommended(transaction))
        return {RecoveryReadiness::RollbackNotRecommended,"Rollback is not recommended by the current update health state."};
    if(running_version!=transaction.target_version)
        return {RecoveryReadiness::RunningVersionMismatch,"The running WebCom version does not match the failed update target."};
    if(!package)
        return {RecoveryReadiness::PackageMissing,"No preserved previous-version installer is available locally."};
    if(!Valid(*package))
        return {RecoveryReadiness::MetadataInvalid,"The preserved recovery metadata is invalid."};
    if(package->version!=transaction.previous_version)
        return {RecoveryReadiness::PackageVersionMismatch,"The preserved installer does not match the recorded previous WebCom version."};
    if(!Hex64(pinned_publisher_sha256) || Lower(package->publisher_sha256)!=Lower(std::string(pinned_publisher_sha256)))
        return {RecoveryReadiness::PublisherMismatch,"The preserved installer publisher does not match this WebCom build trust pin."};
    return {RecoveryReadiness::Ready,"The preserved previous-version installer matches the rollback transaction and publisher policy."};
}

std::filesystem::path RecoveryPackageDirectory(const std::filesystem::path& root,std::string_view version){if(!IsValidReleaseVersion(version))return {};return RecoveryRoot(root)/std::string(version);}
std::filesystem::path RecoveryPackagePath(const std::filesystem::path& root,std::string_view version){const auto dir=RecoveryPackageDirectory(root,version);if(dir.empty())return {};return dir/("WebCom-Setup-"+std::string(version)+"-x64.exe");}

std::optional<RecoveryPackage> LoadRecoveryPackage(const std::filesystem::path& root){
    std::ifstream input(MetadataPath(root),std::ios::binary);if(!input)return std::nullopt;
    std::string header,version,sha,size,publisher,extra;
    if(!std::getline(input,header)||header!=kHeader||!std::getline(input,version)||!std::getline(input,sha)||!std::getline(input,size)||!std::getline(input,publisher))return std::nullopt;
    for(auto* line:{&version,&sha,&size,&publisher})if(!line->empty()&&line->back()=='\r')line->pop_back();
    if(!version.starts_with("version\t")||!sha.starts_with("sha256\t")||!size.starts_with("size\t")||!publisher.starts_with("publisher_sha256\t"))return std::nullopt;
    RecoveryPackage result;result.version=version.substr(8);result.sha256=Lower(sha.substr(7));result.publisher_sha256=Lower(publisher.substr(17));
    const auto number=size.substr(5);if(number.empty()||number.size()>20)return std::nullopt;std::uint64_t parsed=0;
    for(unsigned char c:number){if(!std::isdigit(c))return std::nullopt;const unsigned d=c-'0';if(parsed>(std::numeric_limits<std::uint64_t>::max()-d)/10u)return std::nullopt;parsed=parsed*10u+d;}result.size_bytes=parsed;
    if(std::getline(input,extra)&&!extra.empty()&&extra!="\r")return std::nullopt;
    if(!Valid(result))return std::nullopt;
    std::error_code ec;const auto package=RecoveryPackagePath(root,result.version);if(package.empty()||!std::filesystem::is_regular_file(package,ec)||ec)return std::nullopt;
    if(std::filesystem::file_size(package,ec)!=result.size_bytes||ec)return std::nullopt;
    return result;
}

bool PreserveRecoveryPackage(const std::filesystem::path& root,const RecoveryPackage& metadata,const std::filesystem::path& verified_installer,std::string* error){
    if(!Valid(metadata))return Fail(error,"Recovery package metadata is invalid.");
    std::error_code ec;if(!std::filesystem::is_regular_file(verified_installer,ec)||ec)return Fail(error,"Verified installer is unavailable for recovery preservation.");
    if(std::filesystem::file_size(verified_installer,ec)!=metadata.size_bytes||ec)return Fail(error,"Recovery installer size does not match metadata.");
    const auto recovery=RecoveryRoot(root);const auto directory=RecoveryPackageDirectory(root,metadata.version);const auto destination=RecoveryPackagePath(root,metadata.version);
    if(directory.empty()||destination.empty())return Fail(error,"Recovery package path is invalid.");
    const auto temp=recovery/"recovery.v1.tmp";
    std::filesystem::create_directories(directory,ec);if(ec)return Fail(error,"Recovery directory could not be created.");
    std::filesystem::copy_file(verified_installer,destination,std::filesystem::copy_options::overwrite_existing,ec);if(ec)return Fail(error,"Verified installer could not be preserved for rollback.");
    {
        std::ofstream output(temp,std::ios::binary|std::ios::trunc);if(!output)return Fail(error,"Recovery metadata could not be written.");
        output<<kHeader<<"\nversion\t"<<metadata.version<<"\nsha256\t"<<Lower(metadata.sha256)<<"\nsize\t"<<metadata.size_bytes<<"\npublisher_sha256\t"<<Lower(metadata.publisher_sha256)<<"\n";
        if(!output)return Fail(error,"Recovery metadata could not be completed.");
    }
    const auto metadata_path=MetadataPath(root);std::filesystem::rename(temp,metadata_path,ec);if(ec){std::filesystem::remove(metadata_path,ec);ec.clear();std::filesystem::rename(temp,metadata_path,ec);}if(ec)return Fail(error,"Recovery metadata could not be committed.");
    for(const auto& entry:std::filesystem::directory_iterator(recovery,ec)){if(ec)break;if(!entry.is_directory())continue;if(entry.path()!=directory)std::filesystem::remove_all(entry.path(),ec);ec.clear();}
    return true;
}

bool ClearRecoveryPackage(const std::filesystem::path& root,std::string* error){std::error_code ec;std::filesystem::remove_all(RecoveryRoot(root),ec);return !ec||Fail(error,"Recovery package could not be removed.");}

} // namespace webcom
