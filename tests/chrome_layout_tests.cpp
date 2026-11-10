#include "chrome_layout.h"
#include <array>
#include <iostream>

namespace {
int checks=0, failures=0;
void Check(bool condition, const char* message) {
    ++checks;
    if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
}

int main() {
    using namespace webcom;
    constexpr std::array widths{320,360,480,640,760,820,1000,1240,1920,3840};
    bool active_visible=true, ranges_bounded=true, caption_clear=true, positive_tabs=true;
    for(int width:widths)for(std::size_t total:{1u,2u,6u,20u,200u})
        for(std::size_t active:{std::size_t{0},total/2,total-1})
            for(std::size_t first:{std::size_t{0},total/2,total+100}){
                const auto l=MakeChromeStripLayout(width,total,active,first);
                active_visible &= active>=l.first && active<l.first+l.count;
                ranges_bounded &= l.count>0 && l.first+l.count<=total;
                caption_clear &= l.new_tab_left+32+8<=l.caption_left;
                positive_tabs &= l.step>=48 && l.step<=236 && l.strip_width>0;
            }
    Check(active_visible,"selected tab stays in the visible interval");
    Check(ranges_bounded,"overflow intervals stay within live tabs");
    Check(caption_clear,"tabs and plus button never overlap caption buttons");
    Check(positive_tabs,"tab widths remain positive and capped");
    const auto empty=MakeChromeStripLayout(820,0,5,99);
    Check(empty.count==0 && empty.strip_width==0,"empty strips do not divide by zero");
    const auto one=MakeChromeStripLayout(1920,1,0,0);
    Check(one.step==236 && one.new_tab_left+32<one.caption_left,"one tab leaves draggable caption space");
    const auto end=MakeChromeStripLayout(820,200,199,0);
    Check(end.first+end.count==200,"last tab brings the overflow window to the end");
    const auto start=MakeChromeStripLayout(820,200,0,180);
    Check(start.first==0,"switching to first tab resets an old overflow offset");
    const auto invalid=MakeChromeStripLayout(820,6,100,100);
    Check(invalid.first+invalid.count<=6,"out-of-range saved index remains bounded");
    Check(ChromeHoverAmount(2,2,4,0)==100 && ChromeHoverAmount(4,2,4,0)==0,
        "tab hover animation starts from the previously hovered tab");
    Check(ChromeHoverAmount(2,2,4,50)==50 && ChromeHoverAmount(4,2,4,50)==50,
        "tab hover animation crossfades at its midpoint");
    Check(ChromeHoverAmount(2,2,4,100)==0 && ChromeHoverAmount(4,2,4,100)==100,
        "tab hover animation finishes on the destination tab");
    Check(ChromeHoverAmount(4,2,4,500)==100 && ChromeHoverAmount(7,2,4,500)==0,
        "tab hover animation clamps elapsed time without leaking to other tabs");

    bool fields_fit=true, buttons_clear=true, focus_stable=true;
    for(int width:widths)for(int preferred:{36,104,140,500})for(bool home:{false,true})for(bool downloads:{false,true}){
        const auto l=MakeChromeToolbarLayout(width,preferred,home,downloads);
        fields_fit &= l.omnibox_left>=0 && l.edit_left>l.omnibox_left && l.edit_right-l.edit_left>=54 && l.edit_right<l.omnibox_right;
        const int right_neighbor=l.show_download?l.download_left:l.profile_left;
        buttons_clear &= l.omnibox_right+8<=right_neighbor && (!l.show_download || l.download_left+34<=l.profile_left) &&
            l.profile_left+l.profile_width+6<=l.more_left && l.more_left+34<=width;
        focus_stable &= !l.show_ai || (home && width>=1000);
    }
    Check(fields_fit,"address text retains a usable width at narrow and large sizes");
    Check(buttons_clear,"omnibox profile and menu controls do not intersect");
    Check(focus_stable,"AI chip is determined by page and width rather than focus");
    Check(!MakeChromeToolbarLayout(480,140,false).show_forward,"narrow layout drops the redundant forward slot");
    Check(MakeChromeToolbarLayout(820,140,false).show_forward,"ordinary desktop layout retains forward control");
    Check(MakeChromeToolbarLayout(640,140,false).compact_profile,"small displays retain a compact profile button");
    Check(!MakeChromeToolbarLayout(820,140,false).compact_profile,"normal widths expose the profile name");
    Check(MakeChromeToolbarLayout(1000,104,true).show_ai,"wide native home exposes Google AI shortcut");
    Check(MakeChromeToolbarLayout(820,104,false,true).show_download,"desktop layout reserves a downloads button when recent downloads exist");
    Check(!MakeChromeToolbarLayout(480,104,false,true).show_download,"narrow layout hides downloads button before squeezing the address field");

    bool home_fits=true, home_ordered=true;
    for(int width:widths)for(int height:{360,459,520,719,959,2040}){
        const auto l=MakeChromeHomeLayout(width,height);
        home_fits &= l.search_left>=24 && l.search_left+l.search_width<=width-24;
        home_ordered &= l.logo_top>=36 && l.search_top>=l.logo_top+l.logo_height+28 && l.shortcut_top>=l.search_top+52+24;
    }
    Check(home_fits,"home search keeps horizontal margins at all supported widths");
    Check(home_ordered,"home logo search and shortcuts never overlap");
    Check(MakeChromeHomeLayout(820,360).shortcut_top+92<=360,"compact home content fits the shortest tested canvas");
    Check(MakeChromeHomeLayout(1920,959).search_width==680,"wide search field uses a readable maximum width");
    Check(MakeChromeHomeLayout(820,459).logo_height<MakeChromeHomeLayout(820,719).logo_height,"short canvases scale the logo down");
    bool private_fits=true,private_ordered=true,private_scrolls=true;
    for(int width:widths)for(int height:{360,459,606,959}){
        const auto l=MakeChromePrivateLayout(width,height);
        private_fits &= l.left>=24 && l.left+l.width<=width-24;
        private_ordered &= l.title_top>=l.top+l.icon+24 && l.columns_top>=l.intro_top+l.intro_height+12 &&
            l.card_top>=l.second_column_top+100;
        private_scrolls &= l.extent>=l.card_top+l.card_height+20;
    }
    Check(private_fits,"private content retains narrow-window margins");
    Check(private_ordered,"private text sections never overlap");
    Check(private_scrolls,"private scroll extent includes the complete final card");
    Check(ChromeReflowAmount(0)==0,"tab reflow starts at zero");
    Check(ChromeReflowAmount(80)>50 && ChromeReflowAmount(80)<100,"tab reflow uses ease-out motion");
    Check(ChromeReflowAmount(160)==100,"tab reflow finishes at duration");
    Check(ChromeReflowAmount(500)==100,"tab reflow remains complete after duration");
    Check(MakeChromePrivateLayout(640,459).narrow,"narrow private pages stack their text columns");
    Check(!MakeChromePrivateLayout(820,459).narrow,"desktop private pages retain reference columns");
    const auto pinned=MakeChromeTabStripPlan(1200,7,2,5,2);
    Check(pinned.slots.size()>=3 && pinned.slots[0].pinned && pinned.slots[1].pinned,
        "pinned tabs occupy the leading compact slots");
    Check(pinned.slots[0].right-pinned.slots[0].left<=44 && pinned.slots[2].right-pinned.slots[2].left>44,
        "pinned slots are favicon-sized while normal tabs retain title space");
    Check(pinned.new_tab_left<1200-138,"new-tab button stays clear of caption buttons with pinned tabs");
    const std::vector<std::pair<int,bool>> grouped{{1,true},{1,true},{0,false},{2,false},{2,false},{1,true}};
    const auto visible=MakeChromeVisibleTabIndices(grouped,0);
    Check(visible==std::vector<std::size_t>({0,2,3,4}),"collapsed groups expose one representative even when temporarily split");
    const std::vector<std::pair<int,bool>> expanded{{1,false},{1,false},{2,true},{2,true}};
    Check(MakeChromeVisibleTabIndices(expanded,0)==std::vector<std::size_t>({0,1,2}),"expanded groups keep all tabs visible while collapsed groups keep one");
    std::cout<<checks<<" chrome layout checks, "<<failures<<" failures\n";
    return failures?1:0;
}
