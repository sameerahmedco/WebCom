#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace webcom {

// Geometry is expressed in DIPs and contains no HWND/GDI state. The renderer
// and pointer hit testing share these final rectangles after DPI conversion.
struct ChromeStripLayout {
    std::size_t first = 0, count = 0;
    int strip_left = 34, strip_width = 0, step = 0, new_tab_left = 38;
    int caption_left = 0;
};

inline ChromeStripLayout MakeChromeStripLayout(int width, std::size_t total,
                                               std::size_t active, std::size_t first) {
    ChromeStripLayout result;
    result.caption_left = std::max(0, width - 138);
    if (!total) return result;
    // Reserve the caption buttons, plus button and a genuine draggable gap.
    const int available = std::max(48, width - 232);
    const auto capacity = static_cast<std::size_t>(std::max(1, available / 104));
    result.count = std::min(total, capacity);
    active = std::min(active, total - 1);
    result.first = total <= capacity ? 0 : std::min(first, total - result.count);
    if (active < result.first) result.first = active;
    if (active >= result.first + result.count) result.first = active - result.count + 1;
    result.step = std::min(236, available / static_cast<int>(result.count));
    result.strip_width = static_cast<int>(result.count) * result.step + 16;
    result.new_tab_left = result.strip_left + result.strip_width + 4;
    return result;
}

inline unsigned ChromeHoverAmount(int index, int from, int to,
                                  unsigned long long elapsed_ms,
                                  unsigned duration_ms = 100) {
    if (duration_ms == 0) return index == to ? 100u : 0u;
    const auto capped = std::min<unsigned long long>(elapsed_ms, duration_ms);
    const unsigned progress = static_cast<unsigned>(capped * 100ull / duration_ms);
    if (index == to) return progress;
    if (index == from) return 100u - progress;
    return 0u;
}

inline unsigned ChromeReflowAmount(unsigned long long elapsed_ms, unsigned duration_ms = 160) {
    if (duration_ms == 0 || elapsed_ms >= duration_ms) return 100u;
    const double t = static_cast<double>(elapsed_ms) / static_cast<double>(duration_ms);
    const double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    return static_cast<unsigned>(std::clamp(eased * 100.0, 0.0, 100.0));
}

struct ChromeToolbarLayout {
    int omnibox_left = 120, omnibox_right = 0;
    int profile_left = 0, profile_width = 104, more_left = 0, download_left = 0;
    int edit_left = 156, edit_right = 0, action_left = 0;
    bool show_forward = true, show_ai = false, compact_profile = false, show_download = false;
};

inline ChromeToolbarLayout MakeChromeToolbarLayout(int width, int preferred_profile_width,
                                                    bool native_home, bool show_download = false) {
    ChromeToolbarLayout result;
    result.more_left = width - 40;
    result.compact_profile = width < 760;
    result.profile_width = result.compact_profile ? 36 : std::clamp(preferred_profile_width, 104, 140);
    if (width < 1000 && !result.compact_profile) result.profile_width = 104;
    result.profile_left = result.more_left - 6 - result.profile_width;
    result.show_download = show_download && width >= 560;
    result.download_left = result.profile_left - (result.show_download ? 40 : 0);
    result.omnibox_right = (result.show_download ? result.download_left : result.profile_left) - 8;
    result.show_forward = width >= 640;
    result.omnibox_left = result.show_forward ? 120 : 82;
    result.edit_left = result.omnibox_left + 36;
    // Focusing the address must not make its width jump. AI Mode is shown only
    // on native home pages with enough room, not in response to edit focus.
    result.show_ai = native_home && width >= 1000;
    result.action_left = result.omnibox_right - (result.show_ai ? 90 : 34);
    result.edit_right = native_home && !result.show_ai ? result.omnibox_right - 14 : result.action_left - 10;
    return result;
}

struct ChromeHomeLayout {
    int logo_top = 0, logo_width = 272, logo_height = 92;
    int search_left = 0, search_top = 0, search_width = 0;
    int shortcut_top = 0;
};

inline ChromeHomeLayout MakeChromeHomeLayout(int width, int content_height) {
    ChromeHomeLayout result;
    const bool compact = content_height < 520;
    result.logo_top = std::clamp((content_height - 360) / 4, 36, 112);
    result.logo_width = compact ? 232 : 272;
    result.logo_height = compact ? 78 : 92;
    result.search_width = std::min(680, std::max(160, width - 64));
    result.search_left = (width - result.search_width) / 2;
    result.search_top = result.logo_top + result.logo_height + (compact ? 28 : 40);
    result.shortcut_top = result.search_top + 76;
    return result;
}

struct ChromePrivateLayout {
    int left=0, width=600, top=72, icon=120, title_top=0;
    int intro_top=0, intro_height=66, columns_top=0, second_column_top=0;
    int card_top=0, card_height=116, extent=0;
    bool narrow=false;
};

inline ChromePrivateLayout MakeChromePrivateLayout(int width, int content_height) {
    ChromePrivateLayout result;
    result.narrow=width<700;
    result.width=std::min(600,std::max(220,width-48));
    result.left=(width-result.width)/2;
    const bool compact=content_height<606;
    result.top=compact?20:72;result.icon=compact?80:120;
    result.title_top=result.top+result.icon+24;
    result.intro_top=result.title_top+48;
    result.intro_height=result.narrow?(result.width<360?144:result.width<460?108:84):66;
    result.columns_top=result.intro_top+result.intro_height+12;
    result.second_column_top=result.columns_top+(result.narrow?102:0);
    result.card_top=result.columns_top+(result.narrow?204:106);
    result.card_height=result.narrow?(result.width<360?170:result.width<460?144:126):116;
    result.extent=result.card_top+result.card_height+20;
    return result;
}

} // namespace webcom

// Compact pinned-tab planning. Pinned tabs are expected to occupy the leading
// contiguous region of the tab model. They keep a fixed favicon-sized footprint
// while ordinary tabs use the remaining strip width. Hidden ordinary tabs are
// represented by the absence of a slot; pinned tabs remain visible whenever the
// window can physically fit their compact minimum.
namespace webcom {
struct ChromeTabSlot {
    std::size_t index = 0;
    int left = 0;
    int right = 0;
    bool pinned = false;
};
struct ChromeTabStripPlan {
    std::vector<ChromeTabSlot> slots;
    std::size_t first_unpinned = 0;
    int strip_left = 34;
    int strip_width = 0;
    int new_tab_left = 38;
};
inline ChromeTabStripPlan MakeChromeTabStripPlan(int width, std::size_t total,
                                                  std::size_t pinned_count,
                                                  std::size_t active,
                                                  std::size_t first_unpinned) {
    ChromeTabStripPlan plan;
    if (!total) return plan;
    pinned_count = std::min(pinned_count, total);
    active = std::min(active, total - 1);
    const int available = std::max(48, width - 232);
    int pinned_step = 44;
    if (pinned_count && static_cast<int>(pinned_count) * pinned_step > available - 72)
        pinned_step = std::max(32, (available - 72) / static_cast<int>(pinned_count));
    const std::size_t visible_pinned = pinned_count ?
        std::min<std::size_t>(pinned_count, std::max(1, available / std::max(1, pinned_step))) : 0;
    int x = 8;
    for (std::size_t i = 0; i < visible_pinned; ++i) {
        plan.slots.push_back({i, x, x + std::max(28, pinned_step - 4), true});
        x += pinned_step;
    }
    const auto normal_total = total - pinned_count;
    const int normal_available = std::max(0, available - (x - 8));
    if (normal_total && normal_available >= 52) {
        const auto capacity = static_cast<std::size_t>(std::max(1, normal_available / 104));
        const auto count = std::min(normal_total, capacity);
        const std::size_t first_min = pinned_count;
        const std::size_t last_start = total - count;
        std::size_t first = std::clamp(first_unpinned, first_min, last_start);
        if (active >= pinned_count) {
            if (active < first) first = active;
            if (active >= first + count) first = active - count + 1;
        }
        plan.first_unpinned = first;
        const int step = std::min(236, normal_available / static_cast<int>(count));
        for (std::size_t n = 0; n < count; ++n) {
            const auto index = first + n;
            plan.slots.push_back({index, x, x + std::max(48, step - 4), false});
            x += step;
        }
    } else plan.first_unpinned = pinned_count;
    plan.strip_width = x + 8;
    plan.new_tab_left = plan.strip_left + plan.strip_width + 4;
    return plan;
}

// The pair is {group-key, collapsed}. An empty/default key means ungrouped.
// Collapsed groups expose one representative even if their tabs became
// temporarily non-contiguous after a move; expanded and ungrouped tabs remain
// individually visible. This is portable so the native strip policy is covered
// by the host regression tests.
template<class GroupKey>
inline std::vector<std::size_t> MakeChromeVisibleTabIndices(
    const std::vector<std::pair<GroupKey,bool>>& tabs,
    const GroupKey& empty_group = GroupKey{}) {
    std::vector<std::size_t> visible;
    std::vector<GroupKey> collapsed_seen;
    visible.reserve(tabs.size());
    for(std::size_t i=0;i<tabs.size();++i){
        const auto& [group,collapsed]=tabs[i];
        if(group==empty_group || !collapsed){visible.push_back(i);continue;}
        if(std::find(collapsed_seen.begin(),collapsed_seen.end(),group)==collapsed_seen.end()){
            collapsed_seen.push_back(group);visible.push_back(i);
        }
    }
    return visible;
}
} // namespace webcom
