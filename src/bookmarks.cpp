#include "bookmarks.h"
#include "core.h"
#include <algorithm>
#include <atomic>
#include <cctype>
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
constexpr std::size_t MaxTitle = 1024, MaxUrl = 16384, MaxFolder = 256, MaxBytes = 6 * 1024 * 1024;
constexpr std::string_view HeaderV1 = "WEBCOM_BOOKMARKS\t1\n";
constexpr std::string_view HeaderV2 = "WEBCOM_BOOKMARKS\t2\n";

bool Fail(std::string* error, const std::string& message) { if (error) *error = message; return false; }
std::vector<Bookmark> Defaults() {
    return {{"Google", "https://www.google.com/", ""}, {"YouTube", "https://www.youtube.com/", ""},
            {"GitHub", "https://github.com/", ""}};
}
std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    std::string name(profile);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return root / "Bookmarks" / (name + ".v1");
}
bool Utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto first = static_cast<unsigned char>(s[i++]);
        if (first < 0x80) { if (first < 0x20 || first == 0x7f) return false; continue; }
        unsigned n = 0; std::uint32_t value = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { n = 1; value = first & 31; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { n = 2; value = first & 15; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { n = 3; value = first & 7; minimum = 0x10000; }
        else return false;
        if (n > s.size() - i) return false;
        while (n--) {
            const auto next = static_cast<unsigned char>(s[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}
bool ValidFolder(std::string_view folder) {
    if (folder.size() > MaxFolder || !Utf8(folder)) return false;
    return folder.find('\n') == std::string_view::npos && folder.find('\r') == std::string_view::npos &&
           folder.find('\t') == std::string_view::npos;
}
bool Valid(const Bookmark& b) {
    if (b.title.empty() || b.title.size() > MaxTitle || b.url.size() > MaxUrl ||
        !Utf8(b.title) || !Utf8(b.url) || !ValidFolder(b.folder)) return false;
    if (!b.url.starts_with("https://") && !b.url.starts_with("http://")) return false;
    const auto resolved = ResolveAddress(b.url);
    return resolved.allowed && resolved.url == b.url;
}
bool AtomicWrite(const std::filesystem::path& destination, const std::string& content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the bookmark temporary file.");
    DWORD written = 0;
    bool ok = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) && written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot write or replace the bookmark file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the bookmark temporary file.");
    std::size_t offset = 0; bool ok = true;
    while (offset < content.size()) {
        const auto n = write(file, content.data() + offset, content.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(n);
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), destination.c_str()) == 0;
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot write or replace the bookmark file."); }
#endif
    return true;
}
std::string HtmlEscape(std::string_view value) {
    std::string out; out.reserve(value.size() + 16);
    for (char c : value) {
        switch (c) {
        case '&': out += "&amp;"; break; case '<': out += "&lt;"; break; case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break; case '\'': out += "&#39;"; break; default: out += c; break;
        }
    }
    return out;
}
std::string HtmlDecode(std::string_view value) {
    std::string out; out.reserve(value.size());
    for (std::size_t i=0;i<value.size();) {
        if (value[i] != '&') { out += value[i++]; continue; }
        const auto end=value.find(';',i+1); if(end==std::string_view::npos || end-i>10){out+=value[i++];continue;}
        const auto entity=value.substr(i,end-i+1);
        if(entity=="&amp;") out+='&'; else if(entity=="&lt;") out+='<'; else if(entity=="&gt;") out+='>';
        else if(entity=="&quot;") out+='"'; else if(entity=="&#39;" || entity=="&apos;") out+='\'';
        else { out.append(entity); i=end+1; continue; }
        i=end+1;
    }
    return out;
}
std::string Trim(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n"); if(first==std::string::npos)return {};
    const auto last=value.find_last_not_of(" \t\r\n"); return value.substr(first,last-first+1);
}
std::string TagText(const std::string& line, std::string_view closing) {
    const auto last=line.find(closing); if(last==std::string::npos)return {};
    const auto open=last?line.rfind('<',last-1):std::string::npos; if(open==std::string::npos)return {};
    const auto first=line.find('>',open); if(first==std::string::npos || first>=last)return {};
    return HtmlDecode(line.substr(first+1,last-first-1));
}
std::string Href(const std::string& line) {
    auto pos=line.find("HREF="); if(pos==std::string::npos)pos=line.find("href=");
    if(pos==std::string::npos)return {};
    pos+=5;
    if(pos>=line.size())return {};
    const char quote=line[pos]; if(quote!='"' && quote!='\'')return {};
    const auto end=line.find(quote,pos+1); if(end==std::string::npos)return {};
    return HtmlDecode(line.substr(pos+1,end-pos-1));
}
} // namespace

std::string NormalizeBookmarkFolderPath(std::string_view folder) {
    std::string result;
    std::string part;
    auto flush = [&] {
        const auto first = part.find_first_not_of(" ");
        if (first == std::string::npos) { part.clear(); return; }
        const auto last = part.find_last_not_of(" ");
        const auto trimmed = part.substr(first, last - first + 1);
        if (!result.empty()) result.push_back('/');
        result += trimmed;
        part.clear();
    };
    for (char c : folder) {
        if (c == '/' || c == '\\') flush();
        else part.push_back(c);
    }
    flush();
    return ValidFolder(result) ? result : std::string{};
}

std::string BookmarkFolderParent(std::string_view folder) {
    const auto normalized = NormalizeBookmarkFolderPath(folder);
    const auto slash = normalized.rfind('/');
    return slash == std::string::npos ? std::string{} : normalized.substr(0, slash);
}

std::vector<std::string> BookmarkFolderPaths(const std::vector<Bookmark>& bookmarks, bool include_ancestors) {
    std::vector<std::string> result;
    for (const auto& bookmark : bookmarks) {
        const auto folder = NormalizeBookmarkFolderPath(bookmark.folder);
        if (folder.empty()) continue;
        std::size_t at = folder.size();
        while (true) {
            const auto candidate = folder.substr(0, at);
            if (std::find(result.begin(), result.end(), candidate) == result.end()) result.push_back(candidate);
            if (!include_ancestors) break;
            const auto slash = candidate.rfind('/');
            if (slash == std::string::npos) break;
            at = slash;
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const std::string& a, const std::string& b) {
        const auto depth = [](const std::string& value) { return std::count(value.begin(), value.end(), '/'); };
        const auto da = depth(a), db = depth(b);
        if (da != db) return da < db;
        return a < b;
    });
    return result;
}

bool RenameBookmarkFolder(std::vector<Bookmark>& bookmarks, std::string_view old_path, std::string_view new_path) {
    const auto old_folder = NormalizeBookmarkFolderPath(old_path);
    const auto new_folder = NormalizeBookmarkFolderPath(new_path);
    if (old_folder.empty() || new_folder.empty() || old_folder == new_folder) return false;
    if (new_folder.size() > old_folder.size() && new_folder.starts_with(old_folder) && new_folder[old_folder.size()] == '/') return false;
    bool changed = false;
    for (auto& bookmark : bookmarks) {
        if (bookmark.folder == old_folder) {
            bookmark.folder = new_folder;
            changed = true;
        } else if (bookmark.folder.size() > old_folder.size() && bookmark.folder.starts_with(old_folder) &&
                   bookmark.folder[old_folder.size()] == '/') {
            bookmark.folder = new_folder + bookmark.folder.substr(old_folder.size());
            changed = true;
        }
    }
    return changed;
}

std::vector<Bookmark> LoadBookmarks(const std::filesystem::path& root, std::string_view profile) {
    if (!IsValidProfileName(profile)) return {};
    if (root.empty() || !root.is_absolute()) return Defaults();
    try {
        const auto path = File(root, profile); std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > MaxBytes) return Defaults();
        std::ifstream stream(path, std::ios::binary); std::string text(static_cast<std::size_t>(size), '\0');
        if (!stream || !stream.read(text.data(), static_cast<std::streamsize>(size)) || stream.peek()!=std::char_traits<char>::eof()) return Defaults();
        bool v2=false; std::size_t header=0;
        if(text.starts_with(HeaderV2)){v2=true;header=HeaderV2.size();}
        else if(text.starts_with(HeaderV1)){header=HeaderV1.size();}
        else return Defaults();
        if(!text.ends_with("end\n"))return Defaults();
        std::string_view rows(text); rows.remove_prefix(header); rows.remove_suffix(4);
        std::vector<Bookmark> result;
        while(!rows.empty()){
            const auto end=rows.find('\n'); if(end==std::string_view::npos || result.size()==100)return Defaults();
            const auto row=rows.substr(0,end); rows.remove_prefix(end+1);
            std::vector<std::string_view> fields; std::size_t at=0;
            while(true){const auto sep=row.find('\t',at);fields.push_back(row.substr(at,sep==std::string_view::npos?sep:sep-at));if(sep==std::string_view::npos)break;at=sep+1;if(fields.size()>3)return Defaults();}
            if((v2 && fields.size()!=3) || (!v2 && fields.size()!=2))return Defaults();
            Bookmark item;
            if(v2){item.folder=UnescapeField(fields[0]);item.title=UnescapeField(fields[1]);item.url=UnescapeField(fields[2]);}
            else {item.title=UnescapeField(fields[0]);item.url=UnescapeField(fields[1]);}
            if(!Valid(item))return Defaults();
            if((v2 && (EscapeField(item.folder)!=fields[0] || EscapeField(item.title)!=fields[1] || EscapeField(item.url)!=fields[2])) ||
               (!v2 && (EscapeField(item.title)!=fields[0] || EscapeField(item.url)!=fields[1]))) return Defaults();
            result.push_back(std::move(item));
        }
        return result;
    } catch(const std::exception&){return Defaults();}
}

bool SaveBookmarks(const std::filesystem::path& root, std::string_view profile,
                   const std::vector<Bookmark>& bookmarks, std::string* error) {
    if(error)error->clear();
    try {
        if(!IsValidProfileName(profile))return Fail(error,"Invalid bookmark profile name.");
        if(root.empty() || !root.is_absolute())return Fail(error,"The bookmark root must be an absolute path.");
        if(bookmarks.size()>100)return Fail(error,"A profile can contain at most 100 bookmarks.");
        std::string content(HeaderV2);
        for(const auto& b:bookmarks){
            if(!Valid(b))return Fail(error,"A bookmark title, URL or folder is invalid. Use complete HTTP(S) URLs without credentials.");
            content+=EscapeField(b.folder)+'\t'+EscapeField(b.title)+'\t'+EscapeField(b.url)+'\n';
        }
        content+="end\n"; if(content.size()>MaxBytes)return Fail(error,"The bookmark file is too large.");
        const auto destination=File(root,profile);std::error_code ec;std::filesystem::create_directories(destination.parent_path(),ec);
        if(ec)return Fail(error,"Cannot create the bookmarks folder: "+ec.message());
        return AtomicWrite(destination,content,error);
    } catch(const std::exception& ex){return Fail(error,std::string("Cannot save bookmarks: ")+ex.what());}
}

bool ExportBookmarksHtml(const std::filesystem::path& destination,
                         const std::vector<Bookmark>& bookmarks, std::string* error) {
    if(error)error->clear();
    if(destination.empty())return Fail(error,"Choose a bookmark export file.");
    for(const auto& b:bookmarks)if(!Valid(b))return Fail(error,"A bookmark cannot be exported because its data is invalid.");
    std::ostringstream out;
    out << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
           "<TITLE>WebCom Bookmarks</TITLE>\n<H1>WebCom Bookmarks</H1>\n<DL><p>\n";
    std::vector<std::string> open;
    const auto parts=[](std::string_view folder){
        std::vector<std::string> result;std::size_t at=0;
        while(at<folder.size()){
            const auto slash=folder.find('/',at);
            result.emplace_back(folder.substr(at,slash==std::string_view::npos?folder.size()-at:slash-at));
            if(slash==std::string_view::npos) break;
            at=slash+1;
        }
        return result;
    };
    for(const auto& b:bookmarks){
        const auto wanted=parts(b.folder);std::size_t common=0;
        while(common<open.size() && common<wanted.size() && open[common]==wanted[common])++common;
        while(open.size()>common){
            out<<std::string(open.size()*4,' ')<<"</DL><p>\n";open.pop_back();
        }
        for(std::size_t i=common;i<wanted.size();++i){
            out<<std::string((open.size()+1)*4,' ')<<"<DT><H3>"<<HtmlEscape(wanted[i])<<"</H3>\n";
            out<<std::string((open.size()+1)*4,' ')<<"<DL><p>\n";open.push_back(wanted[i]);
        }
        out<<std::string((open.size()+1)*4,' ')<<"<DT><A HREF=\""<<HtmlEscape(b.url)<<"\">"<<HtmlEscape(b.title)<<"</A>\n";
    }
    while(!open.empty()){out<<std::string(open.size()*4,' ')<<"</DL><p>\n";open.pop_back();}
    out << "</DL><p>\n";
    const auto content=out.str(); if(content.size()>MaxBytes)return Fail(error,"The bookmark export is too large.");
    std::error_code ec;if(destination.has_parent_path())std::filesystem::create_directories(destination.parent_path(),ec);
    if(ec)return Fail(error,"Cannot create the export folder: "+ec.message());
    return AtomicWrite(destination,content,error);
}

bool ImportBookmarksHtml(const std::filesystem::path& source,
                         std::vector<Bookmark>& bookmarks, std::string* error) {
    if(error)error->clear();
    try{
        std::error_code ec;const auto size=std::filesystem::file_size(source,ec);
        if(ec || size==0 || size>MaxBytes)return Fail(error,"The bookmark import file is missing, empty or too large.");
        std::ifstream in(source,std::ios::binary);if(!in)return Fail(error,"Cannot open the bookmark import file.");
        std::vector<Bookmark> imported;std::vector<std::string> folders;std::string pending;std::string line;
        while(std::getline(in,line)){
            const auto h3=line.find("<H3");const auto h3lower=line.find("<h3");
            if(h3!=std::string::npos || h3lower!=std::string::npos){pending=Trim(TagText(line,h3!=std::string::npos?"</H3>":"</h3>"));continue;}
            if(line.find("<DL")!=std::string::npos || line.find("<dl")!=std::string::npos){if(!pending.empty()){folders.push_back(pending);pending.clear();}continue;}
            if(line.find("</DL")!=std::string::npos || line.find("</dl")!=std::string::npos){if(!folders.empty())folders.pop_back();continue;}
            if(line.find("<A ")==std::string::npos && line.find("<a ")==std::string::npos)continue;
            Bookmark item;item.url=Href(line);item.title=Trim(TagText(line,line.find("</A>")!=std::string::npos?"</A>":"</a>"));
            if(!folders.empty()){ for(std::size_t i=0;i<folders.size();++i){ if(i)item.folder.push_back('/'); item.folder+=folders[i]; } }
            if(!Valid(item))continue;
            if(imported.size()>=100)return Fail(error,"The import contains more than 100 supported bookmarks.");
            imported.push_back(std::move(item));
        }
        if(imported.empty())return Fail(error,"No supported HTTP(S) bookmarks were found in this HTML file.");
        bookmarks=std::move(imported);return true;
    }catch(const std::exception& ex){return Fail(error,std::string("Cannot import bookmarks: ")+ex.what());}
}

bool ReorderBookmark(std::vector<Bookmark>& bookmarks,std::size_t source,std::size_t target) {
    if(source>=bookmarks.size() || target>=bookmarks.size() || source==target)return false;
    if(bookmarks[source].folder!=bookmarks[target].folder)return false;
    auto moved=std::move(bookmarks[source]);
    bookmarks.erase(bookmarks.begin()+static_cast<std::ptrdiff_t>(source));
    if(source<target)--target;
    target=std::min(target,bookmarks.size());
    bookmarks.insert(bookmarks.begin()+static_cast<std::ptrdiff_t>(target),std::move(moved));
    return true;
}

bool MoveBookmarkToFolder(std::vector<Bookmark>& bookmarks,std::size_t index,std::string folder) {
    if(index>=bookmarks.size())return false;
    const bool requested_root=folder.empty();
    const auto normalized=NormalizeBookmarkFolderPath(folder);
    if(!requested_root && normalized.empty())return false;
    folder=normalized;
    if(bookmarks[index].folder==folder)return false;
    Bookmark moved=std::move(bookmarks[index]);
    bookmarks.erase(bookmarks.begin()+static_cast<std::ptrdiff_t>(index));
    moved.folder=std::move(folder);
    std::size_t insert_at=bookmarks.size();
    if(moved.folder.empty()) {
        insert_at=0;
        while(insert_at<bookmarks.size() && bookmarks[insert_at].folder.empty())++insert_at;
    } else {
        bool found=false;
        for(std::size_t i=0;i<bookmarks.size();++i)if(bookmarks[i].folder==moved.folder){insert_at=i+1;found=true;}
        if(!found)insert_at=bookmarks.size();
    }
    bookmarks.insert(bookmarks.begin()+static_cast<std::ptrdiff_t>(insert_at),std::move(moved));
    return true;
}

} // namespace webcom
