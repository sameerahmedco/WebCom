#include "profile_template.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace { unsigned checks=0,failures=0; void Check(bool v,const char* label){++checks;if(!v){++failures;std::cerr<<"FAIL: "<<label<<'\n';}} struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}}; }

int main(){
    using namespace webcom;
    const auto root=std::filesystem::temp_directory_path()/("webcom-profile-template-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);Cleanup cleanup{root};const auto file=root/"profile.webcom-profile";std::string error;
    ProfileTemplate saved; saved.appearance.theme=BrowserTheme::Forest;saved.appearance.accent_rgb=0x257fc2;saved.appearance.show_bookmarks=false;saved.appearance.reduce_motion=true;saved.appearance.high_contrast=true;saved.appearance.display_name="Research";saved.appearance.reader_text_scale=2;saved.appearance.reader_width_mode=2;saved.appearance.reader_serif=true;saved.save_passwords=false;saved.general_autofill=false;saved.vertical_tabs=true;saved.vertical_tabs_collapsed=true;
    Check(ExportProfileTemplate(file,saved,&error),"safe profile template exports");
    ProfileTemplate loaded;Check(ImportProfileTemplate(file,loaded,&error),"safe profile template imports");
    Check(loaded.appearance==saved.appearance && loaded.save_passwords==saved.save_passwords && loaded.general_autofill==saved.general_autofill && loaded.vertical_tabs && loaded.vertical_tabs_collapsed,"all safe preferences round trip");
    saved.vertical_tabs=false;saved.vertical_tabs_collapsed=true;Check(!ExportProfileTemplate(root/"invalid.webcom-profile",saved,&error),"collapsed vertical rail cannot export when vertical tabs are off");
    std::ofstream bad(root/"bad.webcom-profile",std::ios::binary|std::ios::trunc);bad<<"WEBCOM_PROFILE_TEMPLATE\t1\ntheme\t0\npassword\tsecret\nend\n";bad.close();ProfileTemplate unchanged=loaded;Check(!ImportProfileTemplate(root/"bad.webcom-profile",loaded,&error) && loaded.appearance==unchanged.appearance,"malformed or secret-like file is rejected without mutating destination");
    std::ofstream trailing(root/"trailing.webcom-profile",std::ios::binary|std::ios::trunc);trailing<<"WEBCOM_PROFILE_TEMPLATE\t1\ntheme\t0x\naccent\tauto\nbookmarks\t1\nmotion\t0\ncontrast\t0\ndisplay\t\nreader\t0\t1\t0\npasswords\t1\nautofill\t1\nvertical\t0\t0\nend\n";trailing.close();
    Check(!ImportProfileTemplate(root/"trailing.webcom-profile",loaded,&error),"numeric fields reject trailing characters");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
