#include "omnibox_model.h"
#include <algorithm>
#include <cwctype>
#include <set>

namespace webcom {
namespace {
std::wstring Fold(std::wstring_view value) {
    std::wstring out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
    return out;
}
std::wstring Trim(std::wstring_view value) {
    std::size_t first=0,last=value.size();
    while(first<last && std::iswspace(value[first])) ++first;
    while(last>first && std::iswspace(value[last-1])) --last;
    return std::wstring(value.substr(first,last-first));
}
int MatchScore(const std::wstring& folded_query, const std::wstring& title, const std::wstring& url) {
    const auto t = Fold(title);
    const auto u = Fold(url);
    int score=-1;
    if(!folded_query.empty()) {
        if(u.starts_with(folded_query)) score=900;
        else if(t.starts_with(folded_query)) score=820;
        else if(auto url_pos=u.find(folded_query);url_pos!=std::wstring::npos) score=650-static_cast<int>(std::min<std::size_t>(url_pos,120));
        else if(auto title_pos=t.find(folded_query);title_pos!=std::wstring::npos) score=600-static_cast<int>(std::min<std::size_t>(title_pos,120));
    }
    return score;
}
}

std::vector<OmniboxSuggestion> BuildOmniboxSuggestions(std::wstring_view raw_query,
    const std::vector<OmniboxCandidate>& candidates,std::size_t max_results) {
    std::vector<OmniboxSuggestion> result;
    if(!max_results) return result;
    const auto query=Trim(raw_query);
    if(query.empty()) return result;
    const auto folded=Fold(query);
    std::vector<OmniboxSuggestion> matched;
    matched.reserve(candidates.size());
    for(const auto& item:candidates) {
        int score=MatchScore(folded,item.title,item.url);
        if(score<0) continue;
        switch(item.kind) {
        case OmniboxSuggestionKind::OpenTab: score+=220+(item.active?15:0); break;
        case OmniboxSuggestionKind::Bookmark: score+=170; break;
        case OmniboxSuggestionKind::History: score+=110+static_cast<int>(std::min<std::uint64_t>(item.recency/60000,60)); break;
        default: break;
        }
        matched.push_back({item.kind,item.title,item.kind==OmniboxSuggestionKind::OpenTab?L"Switch to this tab":item.url,
            item.url,item.tab_id,score});
    }
    std::stable_sort(matched.begin(),matched.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    std::set<std::wstring> seen;
    for(auto& item:matched) {
        const auto key=Fold(item.url);
        if(!key.empty() && !seen.insert(key).second) continue;
        result.push_back(std::move(item));
        if(result.size()+1>=max_results) break;
    }
    OmniboxSuggestion search{OmniboxSuggestionKind::Search,L"Search Google for \""+query+L"\"",L"Google Search",L"",0,500};
    if(result.empty()) result.push_back(std::move(search));
    else {
        const bool looks_url=query.find(L'.')!=std::wstring::npos || query.starts_with(L"http://") || query.starts_with(L"https://");
        const std::size_t pos=looks_url?std::min<std::size_t>(1,result.size()):0;
        result.insert(result.begin()+static_cast<std::ptrdiff_t>(pos),std::move(search));
        if(result.size()>max_results) result.resize(max_results);
    }
    return result;
}

} // namespace webcom
