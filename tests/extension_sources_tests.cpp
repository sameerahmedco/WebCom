#include "extension_sources.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* l){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<l<<'\n';}} }

int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/"webcom-extension-sources-tests";
    std::error_code ec;std::filesystem::remove_all(root,ec);
    std::vector<ExtensionSourceRecord> records;
    Check(IsValidExtensionId("abcdefghijklmnopabcdefghijklmnop"),"normal extension id accepted");
    Check(!IsValidExtensionId("bad/id"),"slash rejected in extension id");
    Check(IsValidExtensionSourceStamp("0123456789abcdef"),"16-hex source stamp accepted");
    Check(!IsValidExtensionSourceStamp("not-a-stamp"),"malformed source stamp rejected");
    Check(RememberExtensionSource(records,"abc123",root/"Extensions"/"One","0123456789abcdef"),"source remembered with stamp");
    Check(RememberExtensionSource(records,"def456",root/"Extensions"/"Two Folder"),"second source remembered without baseline");
    Check(records.size()==2,"two records retained");
    Check(RememberExtensionSource(records,"abc123",root/"Updated","fedcba9876543210"),"existing mapping updated");
    Check(records.size()==2 && FindExtensionSource(records,"abc123") && FindExtensionSource(records,"abc123")->folder==root/"Updated" && FindExtensionSource(records,"abc123")->source_stamp=="fedcba9876543210","updated mapping replaces path and stamp");
    Check(RememberExtensionSource(records,"new789",root/"Updated","0011223344556677"),"new extension id can replace the source-folder owner");
    Check(!FindExtensionSource(records,"abc123") && FindExtensionSource(records,"new789") && records.size()==2,"same folder keeps only its current extension id");
    std::string error;
    Check(SaveExtensionSources(root,"Work",records,&error),"source metadata saves atomically");
    const auto loaded=LoadExtensionSources(root,"work");
    Check(loaded==records,"v2 source metadata round trips case-insensitive profile filename");

    const auto source=root/"LiveSource";
    std::filesystem::create_directories(source/"sub",ec);
    { std::ofstream out(source/"manifest.json",std::ios::binary|std::ios::trunc);out<<"{\"manifest_version\":3,\"name\":\"Stamp\",\"version\":\"1\"}"; }
    { std::ofstream out(source/"sub"/"content.js",std::ios::binary|std::ios::trunc);out<<"console.log('one');"; }
    const auto first=SnapshotExtensionSource(source);
    Check(first.available && first.file_count==2 && first.stamp.size()==16,"source snapshot records a bounded deterministic stamp");
    ExtensionSourceRecord stamped{"stampid",source,first.stamp};
    Check(!ExtensionSourceChanged(stamped,first),"fresh source matches recorded stamp");
    { std::ofstream out(source/"sub"/"extra.js",std::ios::binary|std::ios::trunc);out<<"console.log('two');"; }
    const auto second=SnapshotExtensionSource(source);
    Check(second.available && second.file_count==3 && second.stamp!=first.stamp,"file addition changes source stamp");
    Check(ExtensionSourceChanged(stamped,second),"changed source is detected against recorded baseline");
    std::filesystem::remove(source/"sub"/"extra.js",ec);
    std::filesystem::remove(source/"sub"/"content.js",ec);
    const auto third=SnapshotExtensionSource(source);
    Check(third.available && third.file_count==1 && third.stamp!=first.stamp,"file removal changes source stamp");
    Check(!SnapshotExtensionSource(root/"missing-source").available,"missing source snapshot fails closed");

    {
        std::filesystem::create_directories(ExtensionSourcesPath(root,"Legacy").parent_path(),ec);
        std::ofstream schema1(ExtensionSourcesPath(root,"Legacy"),std::ios::binary|std::ios::trunc);
        schema1<<"WEBCOM_EXTENSION_SOURCES\t1\nsource\tsourcev1\tC%3A%2FExt\n";
    }
    const auto migrated=LoadExtensionSources(root,"Legacy");
    Check(migrated.size()==1 && migrated[0].extension_id=="sourcev1" && migrated[0].source_stamp.empty(),"v1 source metadata migrates with an unknown baseline");

    Check(ForgetExtensionSource(records,"new789"),"explicit source removal succeeds");
    Check(!FindExtensionSource(records,"new789") && records.size()==1,"removed mapping disappears");
    Check(!ForgetExtensionSource(records,"missing"),"missing mapping removal is harmless");
    Check(SaveExtensionSources(root,"Work",records,&error),"updated metadata saves");
    {
        std::ofstream bad(ExtensionSourcesPath(root,"Work"),std::ios::binary|std::ios::trunc);
        bad<<"WEBCOM_EXTENSION_SOURCES\t99\nsource\tabc123\tC%3A%2FExt\n";
    }
    Check(LoadExtensionSources(root,"Work").empty(),"unknown schema fails closed");
    {
        std::ofstream bad(ExtensionSourcesPath(root,"Work"),std::ios::binary|std::ios::trunc);
        bad<<"WEBCOM_EXTENSION_SOURCES\t2\nsource\tbad%2Fid\tC%3A%2FExt\t0123456789abcdef\n";
    }
    Check(LoadExtensionSources(root,"Work").empty(),"invalid persisted extension id fails closed");
    {
        std::ofstream bad(ExtensionSourcesPath(root,"Work"),std::ios::binary|std::ios::trunc);
        bad<<"WEBCOM_EXTENSION_SOURCES\t2\nsource\tabc123\tC%3A%2FExt\tbadstamp\n";
    }
    Check(LoadExtensionSources(root,"Work").empty(),"invalid persisted source stamp fails closed");
    std::filesystem::remove_all(root,ec);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
