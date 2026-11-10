#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct Bookmark { std::string title; std::string url; std::string folder; };

// Folder paths use forward slashes (for example "Work/Research"). Existing
// one-level folder names remain valid. Helpers keep paths canonical so the UI
// can safely offer nested folder creation/rename without touching the file
// system.
std::string NormalizeBookmarkFolderPath(std::string_view folder);
std::string BookmarkFolderParent(std::string_view folder);
std::vector<std::string> BookmarkFolderPaths(const std::vector<Bookmark>& bookmarks,
                                             bool include_ancestors = true);
bool RenameBookmarkFolder(std::vector<Bookmark>& bookmarks,
                          std::string_view old_path,
                          std::string_view new_path);

// Missing/corrupt files use Google, YouTube and GitHub defaults. A valid saved
// empty list remains empty. Invalid profile names return an empty list.
std::vector<Bookmark> LoadBookmarks(const std::filesystem::path& root,
                                   std::string_view profile);

// Stores only explicit HTTP(S) metadata. The caller decides which pages to add;
// private browsing never adds a bookmark automatically. Rejected saves preserve
// the previous file. The root must be an absolute application-data directory.
bool SaveBookmarks(const std::filesystem::path& root, std::string_view profile,
                   const std::vector<Bookmark>& bookmarks,
                   std::string* error = nullptr);

// Netscape bookmark HTML import/export for interoperability with Chromium,
// Firefox and other desktop browsers. Nested folder paths are preserved;
// unsupported markup is ignored rather than executed.
bool ExportBookmarksHtml(const std::filesystem::path& destination,
                         const std::vector<Bookmark>& bookmarks,
                         std::string* error = nullptr);
bool ImportBookmarksHtml(const std::filesystem::path& source,
                         std::vector<Bookmark>& bookmarks,
                         std::string* error = nullptr);

// Reorders a bookmark within its current folder. Cross-folder drag operations
// are intentionally rejected so the manager cannot create disjoint duplicate
// folder sections; moving between folders remains an explicit command.
bool ReorderBookmark(std::vector<Bookmark>& bookmarks, std::size_t source,
                     std::size_t target);

// Moves a bookmark to another folder without alphabetically re-sorting the
// collection. Root bookmarks stay together at the front; an item moved into a
// named folder is appended to that folder's current manual order.
bool MoveBookmarkToFolder(std::vector<Bookmark>& bookmarks, std::size_t index,
                          std::string folder);

} // namespace webcom
