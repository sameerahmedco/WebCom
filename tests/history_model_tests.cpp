#include "history_model.h"
#include <iostream>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

int main() {
    using namespace webcom;
    std::vector<HistoryRecord> entries{
        {L"https://example.com/research", L"Research Notes", 1000},
        {L"https://openai.com/", L"OpenAI", 2000},
        {L"https://example.org/", L"Example", 3000},
        {L"https://news.example.com/", L"Daily News", 4000}
    };
    Check(HistoryMatches(entries[0], L"RESEARCH"), "history search is case insensitive");
    Check(HistoryMatches(entries[1], L"openai.com"), "history search matches URL");
    Check(!HistoryMatches(entries[2], L"missing"), "history search rejects missing text");
    const auto filtered = FilterHistory(entries, L"example");
    Check(filtered.size() == 3 && filtered[0] == 0 && filtered[2] == 3, "filter returns matching indices in order");

    auto copy = entries;
    Check(DeleteHistoryMatches(copy, L"example") == 3 && copy.size() == 1 && copy[0].title == L"OpenAI",
          "bulk delete removes visible matches");
    copy = entries;
    Check(DeleteHistoryMatches(copy, L"") == 0 && copy.size() == entries.size(), "empty query does not bulk delete everything");
    copy = entries;
    Check(DeleteHistoryOlderThan(copy, 2500) == 2 && copy.size() == 2 && copy[0].visited_ms == 3000,
          "older-than deletion respects cutoff");
    copy = entries;
    Check(DeleteHistoryRange(copy, 3500, 1500) == 2 && copy.size() == 2 && copy[0].visited_ms == 1000 && copy[1].visited_ms == 4000,
          "range deletion accepts reversed bounds and is inclusive");
    std::vector<HistoryRecord> hosts{
        {L"https://www.example.com/a", L"A", 5},
        {L"https://example.com/b", L"B", 4},
        {L"https://news.example.org/", L"News", 3},
        {L"http://example.net/", L"Net", 2}
    };
    Check(HistoryHost(hosts[0].url) == L"example.com", "history host normalizes www");
    const auto summaries = BuildHistoryHostSummaries(hosts, 8);
    Check(summaries.size() == 3, "history host summary deduplicates hosts");
    Check(summaries[0].host == L"example.com" && summaries[0].visits == 2, "history host summary sorts by visit count");
    Check(DeleteHistoryHost(hosts, L"EXAMPLE.com") == 2, "delete history host is case insensitive");
    Check(hosts.size() == 2, "delete history host keeps other sites");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
