#include "tab_search_model.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
}

int main() {
    using webcom::FilterTabSearchItems;
    using webcom::TabSearchItem;
    const std::vector<TabSearchItem> items{
        {81,L"Project Dashboard",L"https://example.com/work",false,false,false,false,true},
        {42,L"Music for focus",L"https://music.example.org/listen",false,true,false},
        {907,L"Private Research",L"https://search.example.net/query?q=windows",true},
        {308,L"Saved notes",L"https://notes.example.com/project",false,false,true,true},
        {0,L"Invalid item",L"https://invalid.example/"}
    };
    Check(FilterTabSearchItems(items,L"") == std::vector<std::size_t>{0,1,2,3}, "empty query preserves source order and excludes invalid ID");
    Check(FilterTabSearchItems(items,L"  \t\n ") == std::vector<std::size_t>{0,1,2,3}, "whitespace query");
    Check(FilterTabSearchItems(items,L"DASHBOARD") == std::vector<std::size_t>{0}, "case-insensitive title");
    Check(FilterTabSearchItems(items,L"EXAMPLE.ORG") == std::vector<std::size_t>{1}, "case-insensitive URL");
    Check(FilterTabSearchItems(items,L"project") == std::vector<std::size_t>{0,3}, "title or URL");
    Check(FilterTabSearchItems(items,L"project work") == std::vector<std::size_t>{0}, "words span title and URL");
    Check(FilterTabSearchItems(items,L"work project") == std::vector<std::size_t>{0}, "word order is irrelevant");
    Check(FilterTabSearchItems(items,L" music\tLISTEN  ") == std::vector<std::size_t>{1}, "tokenization trims separators");
    Check(FilterTabSearchItems(items,L"Music notes").empty(), "all words must match same tab");
    Check(FilterTabSearchItems(items,L"not-present").empty(), "no results");
    Check(FilterTabSearchItems(items,L"q=windows") == std::vector<std::size_t>{2}, "URL query can be searched");
    Check(FilterTabSearchItems(items,L"Research") == std::vector<std::size_t>{2}, "private tab is searchable in supplied window snapshot");
    Check(FilterTabSearchItems(items,L"sleeping").empty(), "state labels do not alter content matching");
    Check(FilterTabSearchItems(std::span<const TabSearchItem>{},L"x").empty(), "empty source");
    const auto found=FilterTabSearchItems(items,L"notes");
    Check(found.size()==1 && items[found[0]].id==308, "results resolve stable source IDs, not sorted indices");
    Check(items[0].title==L"Project Dashboard" && items[0].active, "filter does not mutate snapshot");
    const std::vector<TabSearchItem> unicode{{1,L"東京の調査",L"https://example.jp/"},{2,L"Café notes",L"https://example.fr/"}};
    Check(FilterTabSearchItems(unicode,L"東京") == std::vector<std::size_t>{0}, "non-Latin exact text");
    Check(FilterTabSearchItems(unicode,L"Café") == std::vector<std::size_t>{1}, "accented title");
    Check(FilterTabSearchItems(unicode,L"CAFÉ") == std::vector<std::size_t>{1}, "accented case-insensitive title");
    std::vector<TabSearchItem> many;
    for(std::uint64_t i=1;i<=205;++i)many.push_back({i,L"Tab "+std::to_wstring(i),L"https://example.com/"});
    Check(FilterTabSearchItems(many,L"").size()==200, "input bounded to 200 tabs");
    Check(FilterTabSearchItems(many,L"205").empty(), "items after bound are not scanned");
    const auto last=FilterTabSearchItems(many,L"200");
    Check(last==std::vector<std::size_t>{199}, "last allowed item remains searchable");
    std::vector<TabSearchItem> large{{1,std::wstring(513,L'a')+L"tail",std::wstring(16385,L'b')+L"end"}};
    Check(FilterTabSearchItems(large,L"tail").empty(), "title bound");
    Check(FilterTabSearchItems(large,L"end").empty(), "URL bound");
    Check(FilterTabSearchItems(items,std::wstring(512,L' ')+L"not-present").size()==4, "query length is bounded");
    Check(FilterTabSearchItems(items,L"project project") == std::vector<std::size_t>{0,3}, "repeated token");
    std::cout << checks << " tab-search checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
