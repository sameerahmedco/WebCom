#include "home_model.h"

#include <iostream>

namespace {
int checks = 0;
int failures = 0;
void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
}

int main() {
    using namespace webcom;
    Check(HomeDisplayHost(L"https://www.Example.com/path") == L"example.com", "normalizes host");
    Check(HomeDisplayHost(L"http://sub.example.com:8080/a") == L"sub.example.com", "strips port");
    Check(HomeDisplayHost(L"webcom:newtab").empty(), "rejects internal page");

    std::vector<HistoryRecord> entries{
        {L"https://www.example.com/new", L"Newest Example", 6},
        {L"https://news.example.org/a", L"News", 5},
        {L"https://example.com/older", L"Older Example", 4},
        {L"http://github.com/openai", L"GitHub", 3},
        {L"file:///tmp/local.html", L"Local", 2},
        {L"https://openai.com/", L"OpenAI", 1},
    };
    const auto recent = BuildHomeRecentSites(entries, 6);
    Check(recent.size() == 4, "deduplicates hosts and excludes non-web");
    Check(recent[0].source_index == 0 && recent[0].title == L"Newest Example", "keeps newest visit per host");
    Check(recent[1].host == L"news.example.org", "preserves distinct subdomain");
    Check(recent[2].host == L"github.com", "normalizes github host");
    Check(recent[3].host == L"openai.com", "includes later unique host");
    Check(BuildHomeRecentSites(entries, 2).size() == 2, "respects requested bound");
    Check(BuildHomeRecentSites(entries, 99).size() == 4, "hard bound still preserves available sites");

    std::cout << (checks - failures) << " home model checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
