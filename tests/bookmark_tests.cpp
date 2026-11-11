#include "bookmarks.h"
#include "core.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void Write(const std::filesystem::path& path, const std::string& value) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc); stream << value;
}
struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } };
}

int main() {
    using namespace webcom;
    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::temp_directory_path() / ("webcom-bookmark-tests-" + stamp);
    if (!std::filesystem::create_directory(root)) { std::cerr << "Cannot create test directory\n"; return 1; }
    Cleanup cleanup{root};
    const auto path = root / "Bookmarks" / "default.v1";
    std::string error;
    const auto defaults = LoadBookmarks(root, "Default");
    Check(defaults.size() == 3 && defaults[0].title == "Google" && defaults[0].url == "https://www.google.com/", "missing file defaults");
    Check(!std::filesystem::exists(path), "reading defaults does not create file");
    const std::vector<Bookmark> saved{{"Research 100% café 🌍", "https://example.com/a?q=one%20two&x=1#part", "Research"}, {"Local", "http://localhost:3000/", "Tools"}};
    Check(SaveBookmarks(root, "Default", saved, &error), "save Unicode and escaped data");
    Check(error.empty(), "successful save clears error");
    const auto loaded = LoadBookmarks(root, "DEFAULT");
    Check(loaded.size() == 2 && loaded[0].title == saved[0].title && loaded[0].url == saved[0].url && loaded[1].url == saved[1].url, "roundtrip and case-insensitive profile identity");
    Check(loaded.size()==2 && loaded[0].folder=="Research" && loaded[1].folder=="Tools", "bookmark folders round trip");
    const auto html=root/"export.html";
    Check(ExportBookmarksHtml(html,saved,&error), "bookmark HTML export succeeds");
    std::vector<Bookmark> imported;
    Check(ImportBookmarksHtml(html,imported,&error), "bookmark HTML import succeeds");
    Check(imported.size()==2 && imported[0].folder=="Research" && imported[1].folder=="Tools", "HTML import preserves folders");
    const std::vector<Bookmark> nested{{"Spec","https://spec.example/","Work/Research/2027"},{"Plan","https://plan.example/","Work/Planning"}};
    Check(NormalizeBookmarkFolderPath("  Work\\Research//2027  ")=="Work/Research/2027", "nested folder paths normalize separators and whitespace");
    Check(BookmarkFolderParent("Work/Research/2027")=="Work/Research", "nested folder parent is derived");
    std::vector<std::string> explicit_folders;
    Check(AddBookmarkFolder(explicit_folders,"Empty") && AddBookmarkFolder(explicit_folders,"Work/Empty Child"),
          "explicit empty and nested bookmark folders can be created");
    Check(!AddBookmarkFolder(explicit_folders,"empty"), "explicit folder names are unique ignoring case");
    Check(SaveBookmarkFolders(root,"Default",explicit_folders,&error), "explicit bookmark folders save atomically");
    const auto loaded_folders=LoadBookmarkFolders(root,"DEFAULT");
    Check(loaded_folders==explicit_folders, "explicit bookmark folders round trip by profile");
    const auto combined_paths=BookmarkFolderPaths(nested,loaded_folders,true);
    Check(std::find(combined_paths.begin(),combined_paths.end(),"Empty")!=combined_paths.end() &&
          std::find(combined_paths.begin(),combined_paths.end(),"Work/Empty Child")!=combined_paths.end(),
          "folder listing includes explicit empty folders and ancestors");
    auto folder_defs=explicit_folders; auto folder_bookmarks=nested;
    Check(RenameBookmarkFolderPath(folder_defs,folder_bookmarks,"Work","Projects") &&
          folder_bookmarks[0].folder=="Projects/Research/2027" && folder_defs[1]=="Projects/Empty Child",
          "renaming a folder tree updates bookmarks and empty descendant folders together");
    Check(RemoveBookmarkFolder(folder_defs,folder_bookmarks,"Empty"), "unused explicit folder can be removed");
    Check(!RemoveBookmarkFolder(folder_defs,folder_bookmarks,"Projects"), "folder containing bookmarks cannot be removed");
    const auto nested_paths=BookmarkFolderPaths(nested,true);
    Check(nested_paths.size()==4 && nested_paths[0]=="Work" && nested_paths[1]=="Work/Planning" && nested_paths[2]=="Work/Research" && nested_paths[3]=="Work/Research/2027",
          "folder list includes unique ancestors in stable depth order");
    auto renamed=nested;
    Check(RenameBookmarkFolder(renamed,"Work/Research","Archive/Research") && renamed[0].folder=="Archive/Research/2027" && renamed[1].folder=="Work/Planning",
          "renaming a folder updates descendants without touching siblings");
    const auto nested_html=root/"nested-export.html";
    Check(ExportBookmarksHtml(nested_html,nested,&error), "nested bookmark HTML export succeeds");
    std::vector<Bookmark> nested_imported;
    Check(ImportBookmarksHtml(nested_html,nested_imported,&error) && nested_imported.size()==2 && nested_imported[0].folder=="Work/Research/2027" && nested_imported[1].folder=="Work/Planning",
          "nested bookmark HTML round trip preserves full folder paths");
    std::vector<Bookmark> ordered{{"A","https://a.example/","One"},{"B","https://b.example/","One"},{"C","https://c.example/","One"},{"D","https://d.example/","Two"}};
    Check(ReorderBookmark(ordered,0,2) && ordered[0].title=="B" && ordered[1].title=="A" && ordered[2].title=="C",
          "drag reorder moves a bookmark within its folder");
    const auto before_cross=ordered;
    Check(!ReorderBookmark(ordered,0,3) && ordered[0].title==before_cross[0].title && ordered[3].folder=="Two",
          "drag reorder refuses accidental cross-folder moves");
    std::vector<Bookmark> folders{{"Root 1","https://r1.example/",""},{"Root 2","https://r2.example/",""},
        {"One A","https://oa.example/","One"},{"One B","https://ob.example/","One"},{"Two A","https://ta.example/","Two"}};
    Check(MoveBookmarkToFolder(folders,4,"One") && folders[4].title=="Two A" && folders[4].folder=="One",
          "folder move appends to target folder without sorting existing order");
    Check(MoveBookmarkToFolder(folders,3,"") && folders[2].title=="One B" && folders[2].folder.empty(),
          "folder move to root appends after existing root bookmarks");
    const auto unchanged=folders;
    Check(!MoveBookmarkToFolder(folders,0,"") && folders[0].title==unchanged[0].title,
          "folder move rejects no-op move");
    Check(!MoveBookmarkToFolder(folders,0,std::string(300,'x')) && folders[0].folder.empty(),
          "invalid nested folder path is rejected instead of becoming root");
    auto cycle=nested;
    Check(!RenameBookmarkFolder(cycle,"Work","Work/Research/Archive"),
          "folder rename rejects moving a folder into its own descendant");
    Check(LoadBookmarks(root, "Work").size() == 3, "profiles have separate bookmarks");
    const auto previous = Bytes(path);
    for (const auto& bad : std::vector<Bookmark>{{"bad", "javascript:alert(1)", ""}, {"bad", "data:text/plain,x", ""}, {"bad", "file:///etc/passwd", ""}, {"bad", "webcom:newtab", ""}, {"bad", "example.com", ""}, {"bad", "https://user:pass@example.com/", ""}, {std::string("bad\x01",4), "https://example.com/", ""}, {std::string("\xc0\xaf",2), "https://example.com/", ""}, {"", "https://example.com/", ""}, {std::string(1025, 'x'), "https://example.com/", ""}}) {
        Check(!SaveBookmarks(root, "Default", {bad}, &error) && !error.empty(), "invalid bookmark rejected");
        Check(Bytes(path) == previous, "invalid save preserves old file");
    }
    Check(!SaveBookmarks(root, "../escape", saved, &error), "traversal profile rejected");
    Check(LoadBookmarks(root, "../escape").empty(), "invalid load profile stays empty");
    Check(!SaveBookmarks("relative", "Default", saved, &error), "relative root rejected");
    Check(!SaveBookmarks(root, "Default", std::vector<Bookmark>(101, saved[0]), &error), "bookmark count bound");
    Check(Bytes(path) == previous, "count failure preserves old file");
    Check(SaveBookmarks(root, "Default", {}, &error), "save empty list");
    Check(LoadBookmarks(root, "Default").empty(), "empty saved list remains empty");
    Check(SaveBookmarks(root, "Default", saved, &error), "restore valid list");
    for (const auto& corrupt : std::vector<std::string>{"", "WEBCOM_BOOKMARKS\t1\n", "WEBCOM_BOOKMARKS\t99\nend\n", "WEBCOM_BOOKMARKS\t1\n%ZZ\thttps%3A%2F%2Fexample.com%2F\nend\n", previous + "garbage\n"}) {
        Write(path, corrupt);
        Check(LoadBookmarks(root, "Default").size() == 3, "corrupt metadata uses safe defaults");
    }
    Write(path, "WEBCOM_BOOKMARKS\t1\nUnsafe\t" + EscapeField("https://user:pass@example.com/") + "\nend\n");
    Check(LoadBookmarks(root, "Default").size() == 3, "credential URL rejected on load");
    Write(path, std::string(6 * 1024 * 1024 + 1, 'x'));
    Check(LoadBookmarks(root, "Default").size() == 3, "oversize file bounded");
    const auto blocked = root / "blocked"; Write(blocked, "not a folder");
    Check(!SaveBookmarks(blocked, "Default", saved, &error) && !error.empty(), "filesystem failure reported");
    bool temporary_left = false;
    for (const auto& entry : std::filesystem::directory_iterator(root / "Bookmarks")) if (entry.path().filename().string().find(".tmp-") != std::string::npos) temporary_left = true;
    Check(!temporary_left, "no temporary files retained");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
