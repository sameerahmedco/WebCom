#include "recently_closed_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace webcom;

namespace {
int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "check failed: " #x " at line " << __LINE__ << "\n"; return 1; } } while (0)
}

int main() {
    auto root = std::filesystem::temp_directory_path() / "webcom-recently-closed-tests";
    std::error_code ec; std::filesystem::remove_all(root, ec);

    std::vector<RecentlyClosedEntry> entries;
    CHECK(AddRecentlyClosed(entries,{"https://example.com/a?x=1","Example","Research","Docs",true,true,42}));
    CHECK(entries.size()==1);
    CHECK(entries.front().pinned && entries.front().keep_awake);
    CHECK(!AddRecentlyClosed(entries,{"javascript:alert(1)","bad","Main","",false,false,1}));
    CHECK(!AddRecentlyClosed(entries,{"https://user:pass@example.com/","bad","Main","",false,false,1}));

    for (int i=0;i<25;++i) CHECK(AddRecentlyClosed(entries,{"https://example.com/"+std::to_string(i),"T","Main","",false,false,static_cast<std::uint64_t>(100+i)}));
    CHECK(entries.size()==kMaxRecentlyClosedEntries);
    CHECK(entries.front().url=="https://example.com/24");

    std::string error;
    CHECK(SaveRecentlyClosed(root,"Default",entries,&error));
    const auto loaded=LoadRecentlyClosed(root,"Default");
    CHECK(loaded.size()==entries.size());
    CHECK(loaded.front().url==entries.front().url);
    CHECK(loaded.front().closed_at_ms==entries.front().closed_at_ms);

    RecentlyClosedEntry removed;
    auto mutable_loaded=loaded;
    CHECK(RemoveRecentlyClosed(mutable_loaded,0,&removed));
    CHECK(removed.url==loaded.front().url);
    CHECK(mutable_loaded.size()+1==loaded.size());
    CHECK(!RemoveRecentlyClosed(mutable_loaded,999,nullptr));

    CHECK(!SaveRecentlyClosed(root,"../bad",entries,&error));

    const auto file=root/"Recovery"/"default.recently-closed.v1";
    { std::ofstream out(file,std::ios::binary|std::ios::trunc); out << "BROKEN\n"; }
    CHECK(LoadRecentlyClosed(root,"Default").empty());

    std::cout << checks << " recently-closed checks passed\n";
    std::filesystem::remove_all(root,ec);
    return 0;
}
