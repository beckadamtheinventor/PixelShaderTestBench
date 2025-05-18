#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace FileDialogs {
    std::string NarrowString16To8(std::wstring w);
    std::wstring ExpandString8To16(std::string s);
    std::vector<std::filesystem::path> DirList(std::filesystem::path path, bool folders=false, bool recursive=false);
    void AddPinnedFolder(std::filesystem::path p);
    std::vector<std::filesystem::path> GetPinnedFolders();

    class FileDialog {
        bool needs_dirlist = true;
        std::filesystem::path path;
        std::string title;
        protected:
        std::vector<std::filesystem::path> listed_folders;
        std::vector<std::filesystem::path> listed_files;
        public:
        FileDialog(std::string title, std::filesystem::path path) : title(title), path(path) {}
        FileDialog(std::string title) : title(title), path(std::filesystem::current_path()) {}
        bool Show(std::filesystem::path& selected);
    };
}