#include "tab_search_model.h"

#include <algorithm>
#include <locale>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace webcom {
namespace {

const std::locale& SearchLocale() {
    static const std::locale locale = [] {
        try { return std::locale(""); }
        catch (...) { return std::locale::classic(); }
    }();
    return locale;
}

std::wstring Fold(std::wstring_view value) {
#ifdef _WIN32
    if(!value.empty()){
        const int size=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,value.data(),
            static_cast<int>(value.size()),nullptr,0,nullptr,nullptr,0);
        if(size>0){
            std::wstring result(static_cast<std::size_t>(size),L'\0');
            if(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,value.data(),
                static_cast<int>(value.size()),result.data(),size,nullptr,nullptr,0)>0)return result;
        }
    }
#endif
    std::wstring result(value);
    if (!result.empty()) {
        const auto& characters = std::use_facet<std::ctype<wchar_t>>(SearchLocale());
        characters.toupper(result.data(), result.data() + result.size());
        // Keep basic Latin case matching deterministic even on a portable host
        // configured with the C locale. Windows uses its Unicode mapping above.
        for(wchar_t& c:result){
            if((c>=L'\u00e0' && c<=L'\u00f6') || (c>=L'\u00f8' && c<=L'\u00fe'))c-=0x20;
            else if(c==L'\u00ff')c=L'\u0178';
        }
    }
    return result;
}

} // namespace

std::vector<std::size_t> FilterTabSearchItems(std::span<const TabSearchItem> items,
                                             std::wstring_view query) {
    query = query.substr(0, 512);
    const auto& characters = std::use_facet<std::ctype<wchar_t>>(SearchLocale());
    std::vector<std::wstring> words;
    std::size_t start = 0;
    while (start < query.size()) {
        while (start < query.size() && characters.is(std::ctype_base::space, query[start])) ++start;
        std::size_t end = start;
        while (end < query.size() && !characters.is(std::ctype_base::space, query[end])) ++end;
        if (end != start) words.push_back(Fold(query.substr(start, end - start)));
        start = end;
    }
    std::vector<std::size_t> matches;
    const auto count = std::min<std::size_t>(items.size(), 200);
    matches.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& item = items[i];
        if (!item.id) continue;
        if (words.empty()) { matches.push_back(i); continue; }
        const auto title = Fold(std::wstring_view(item.title).substr(0, 512));
        const auto url = Fold(std::wstring_view(item.url).substr(0, 16384));
        const bool matches_all = std::all_of(words.begin(), words.end(), [&](const auto& word) {
            return title.find(word) != std::wstring::npos || url.find(word) != std::wstring::npos;
        });
        if (matches_all) matches.push_back(i);
    }
    return matches;
}

} // namespace webcom
