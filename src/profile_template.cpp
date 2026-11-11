#include "profile_template.h"
#include "core.h"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view Header = "WEBCOM_PROFILE_TEMPLATE\t1\n";
constexpr std::size_t MaxBytes = 16 * 1024;

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool ValidUtf8Text(std::string_view value, std::size_t max_bytes) {
    if (value.size() > max_bytes) return false;
    for (std::size_t i=0;i<value.size();) {
        const auto first=static_cast<unsigned char>(value[i++]);
        if(first<0x80){if(first<0x20 || first==0x7f)return false;continue;}
        unsigned remaining=0;std::uint32_t codepoint=0,minimum=0;
        if(first>=0xc2 && first<=0xdf){remaining=1;codepoint=first&31;minimum=0x80;}
        else if(first>=0xe0 && first<=0xef){remaining=2;codepoint=first&15;minimum=0x800;}
        else if(first>=0xf0 && first<=0xf4){remaining=3;codepoint=first&7;minimum=0x10000;}
        else return false;
        if(remaining>value.size()-i)return false;
        while(remaining--){const auto next=static_cast<unsigned char>(value[i++]);if((next&0xc0)!=0x80)return false;codepoint=(codepoint<<6)|(next&63);}
        if(codepoint<minimum || codepoint>0x10ffff || (codepoint>=0xd800 && codepoint<=0xdfff))return false;
    }
    return true;
}

bool Valid(const ProfileTemplate& value) {
    const auto theme=static_cast<int>(value.appearance.theme);
    return theme>=0 && theme<=3 &&
           (!value.appearance.accent_rgb || *value.appearance.accent_rgb<=0xffffff) &&
           ValidUtf8Text(value.appearance.display_name,80) &&
           value.appearance.reader_text_scale>=-2 && value.appearance.reader_text_scale<=4 &&
           value.appearance.reader_width_mode>=0 && value.appearance.reader_width_mode<=2 &&
           (!value.vertical_tabs_collapsed || value.vertical_tabs);
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary=destination.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(sequence.fetch_add(1));
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return Fail(error,"Cannot create the profile-template temporary file.");
    DWORD written=0;bool ok=WriteFile(file,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)&&written==content.size();
    if(ok)ok=FlushFileBuffers(file)!=FALSE;if(!CloseHandle(file))ok=false;
    if(ok)ok=MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok){DeleteFileW(temporary.c_str());return Fail(error,"Cannot replace the profile-template file.");}
#else
    const auto temporary=destination.string()+".tmp-"+std::to_string(getpid())+"-"+std::to_string(sequence.fetch_add(1));
    const int file=open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(file<0)return Fail(error,"Cannot create the profile-template temporary file.");
    std::size_t offset=0;bool ok=true;while(offset<content.size()){const auto n=write(file,content.data()+offset,content.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}offset+=static_cast<std::size_t>(n);}if(ok)ok=fsync(file)==0;if(close(file)!=0)ok=false;if(ok)ok=rename(temporary.c_str(),destination.c_str())==0;if(!ok){unlink(temporary.c_str());return Fail(error,"Cannot replace the profile-template file.");}
#endif
    return true;
}

bool ParseBool(std::string_view value, bool& out) {
    if(value=="1"){out=true;return true;} if(value=="0"){out=false;return true;} return false;
}
}

bool ExportProfileTemplate(const std::filesystem::path& destination,
                           const ProfileTemplate& value, std::string* error) {
    if(error)error->clear();
    if(destination.empty())return Fail(error,"Choose a profile-template file.");
    if(!Valid(value))return Fail(error,"Profile preferences contain invalid values.");
    std::ostringstream out;
    out<<Header;
    out<<"theme\t"<<static_cast<int>(value.appearance.theme)<<'\n';
    out<<"accent\t"<<(value.appearance.accent_rgb?FormatHexColor(*value.appearance.accent_rgb):"auto")<<'\n';
    out<<"bookmarks\t"<<(value.appearance.show_bookmarks?1:0)<<'\n';
    out<<"motion\t"<<(value.appearance.reduce_motion?1:0)<<'\n';
    out<<"contrast\t"<<(value.appearance.high_contrast?1:0)<<'\n';
    out<<"display\t"<<EscapeField(value.appearance.display_name)<<'\n';
    out<<"reader\t"<<value.appearance.reader_text_scale<<'\t'<<value.appearance.reader_width_mode<<'\t'<<(value.appearance.reader_serif?1:0)<<'\n';
    out<<"passwords\t"<<(value.save_passwords?1:0)<<'\n';
    out<<"autofill\t"<<(value.general_autofill?1:0)<<'\n';
    out<<"vertical\t"<<(value.vertical_tabs?1:0)<<'\t'<<(value.vertical_tabs_collapsed?1:0)<<'\n';
    out<<"end\n";
    const auto content=out.str();if(content.size()>MaxBytes)return Fail(error,"Profile template is too large.");
    std::error_code ec;if(destination.has_parent_path())std::filesystem::create_directories(destination.parent_path(),ec);
    if(ec)return Fail(error,"Cannot create the profile-template folder: "+ec.message());
    return AtomicWrite(destination,content,error);
}

bool ImportProfileTemplate(const std::filesystem::path& source,
                           ProfileTemplate& value, std::string* error) {
    if(error)error->clear();
    try {
        std::error_code ec;const auto size=std::filesystem::file_size(source,ec);
        if(ec || size==0 || size>MaxBytes)return Fail(error,"Profile template is missing, empty or too large.");
        std::ifstream in(source,std::ios::binary);if(!in)return Fail(error,"Cannot open the profile-template file.");
        std::string text(static_cast<std::size_t>(size),'\0');if(!in.read(text.data(),static_cast<std::streamsize>(size)) || in.peek()!=std::char_traits<char>::eof())return Fail(error,"Cannot read the profile-template file.");
        if(!text.starts_with(Header) || !text.ends_with("end\n"))return Fail(error,"Unsupported profile-template format.");
        std::string_view rows(text);rows.remove_prefix(Header.size());rows.remove_suffix(4);
        auto take=[&](std::string_view key)->std::optional<std::string_view>{const auto end=rows.find('\n');if(end==std::string_view::npos)return std::nullopt;const auto row=rows.substr(0,end);rows.remove_prefix(end+1);if(!row.starts_with(key)||row.size()<=key.size()||row[key.size()]!='\t')return std::nullopt;return row.substr(key.size()+1);};
        const auto theme=take("theme");
        const auto accent=take("accent");
        const auto bookmarks=take("bookmarks");
        const auto motion=take("motion");
        const auto contrast=take("contrast");
        const auto display=take("display");
        const auto reader=take("reader");
        const auto passwords=take("passwords");
        const auto autofill=take("autofill");
        const auto vertical=take("vertical");
        if(!theme||!accent||!bookmarks||!motion||!contrast||!display||!reader||!passwords||!autofill||!vertical||!rows.empty())return Fail(error,"Profile template is incomplete.");
        ProfileTemplate parsed;
        if(*theme=="0")parsed.appearance.theme=BrowserTheme::Graphite;
        else if(*theme=="1")parsed.appearance.theme=BrowserTheme::Charcoal;
        else if(*theme=="2")parsed.appearance.theme=BrowserTheme::Midnight;
        else if(*theme=="3")parsed.appearance.theme=BrowserTheme::Forest;
        else return Fail(error,"Profile template theme is invalid.");
        if(*accent!="auto"){parsed.appearance.accent_rgb=ParseHexColor(*accent);if(!parsed.appearance.accent_rgb)return Fail(error,"Profile template color is invalid.");}
        if(!ParseBool(*bookmarks,parsed.appearance.show_bookmarks)||!ParseBool(*motion,parsed.appearance.reduce_motion)||!ParseBool(*contrast,parsed.appearance.high_contrast)||!ParseBool(*passwords,parsed.save_passwords)||!ParseBool(*autofill,parsed.general_autofill))return Fail(error,"Profile template boolean field is invalid.");
        parsed.appearance.display_name=UnescapeField(*display);if(EscapeField(parsed.appearance.display_name)!=*display)return Fail(error,"Profile template display label is invalid.");
        {const auto first=reader->find('\t');
         const auto second=first==std::string_view::npos?std::string_view::npos:reader->find('\t',first+1);
         if(first==std::string_view::npos||second==std::string_view::npos||reader->find('\t',second+1)!=std::string_view::npos)return Fail(error,"Reader preferences are invalid.");
         const auto scale=reader->substr(0,first);
         const auto width=reader->substr(first+1,second-first-1);
         if(scale=="-2")parsed.appearance.reader_text_scale=-2;else if(scale=="-1")parsed.appearance.reader_text_scale=-1;else if(scale=="0")parsed.appearance.reader_text_scale=0;else if(scale=="1")parsed.appearance.reader_text_scale=1;else if(scale=="2")parsed.appearance.reader_text_scale=2;else if(scale=="3")parsed.appearance.reader_text_scale=3;else if(scale=="4")parsed.appearance.reader_text_scale=4;else return Fail(error,"Reader preferences are invalid.");
         if(width=="0")parsed.appearance.reader_width_mode=0;else if(width=="1")parsed.appearance.reader_width_mode=1;else if(width=="2")parsed.appearance.reader_width_mode=2;else return Fail(error,"Reader preferences are invalid.");
         bool serif=false;if(!ParseBool(reader->substr(second+1),serif))return Fail(error,"Reader preferences are invalid.");parsed.appearance.reader_serif=serif;}
        {const auto sep=vertical->find('\t');if(sep==std::string_view::npos||vertical->find('\t',sep+1)!=std::string_view::npos||!ParseBool(vertical->substr(0,sep),parsed.vertical_tabs)||!ParseBool(vertical->substr(sep+1),parsed.vertical_tabs_collapsed))return Fail(error,"Vertical-tab preferences are invalid.");}
        if(!Valid(parsed))return Fail(error,"Profile template values are out of range.");
        value=std::move(parsed);return true;
    } catch(const std::exception& ex){return Fail(error,std::string("Cannot import profile template: ")+ex.what());}
}

} // namespace webcom
