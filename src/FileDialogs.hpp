#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace FileDialogs {
    std::string NarrowString16To8(std::wstring w);
    std::wstring ExpandString8To16(std::string s);
    std::vector<std::filesystem::path> DirList(std::filesystem::path path, bool folders=false, bool recursive=false);
    void AddPinnedFolder(std::filesystem::path p);
    std::vector<std::filesystem::path> GetPinnedFolders();

    class FileDialog {
        bool needs_dirlist=true, saveas, folder;
        std::filesystem::path path;
        std::string title;
        protected:
        std::vector<std::filesystem::path> listed_folders;
        std::vector<std::filesystem::path> listed_files;
        public:
        FileDialog(std::string title, std::filesystem::path path, bool saveas=false, bool folder=false) : title(title), path(path), saveas(saveas), folder(folder) {}
        FileDialog(std::string title, bool saveas=false, bool folder=false) : title(title), path(std::filesystem::current_path()), saveas(saveas), folder(folder) {}
        bool Show(std::filesystem::path& selected);
    };

    class FileDialogManager :
    protected std::vector<std::pair<std::string,std::pair<FileDialog*,std::function<bool (std::string)>>>> {
        public:
        void show();
        bool isOpen(std::string name, FileDialog** dialog=nullptr);
        void open(std::string id, std::string title, std::function<bool(std::string)> cb, bool saveas=false, bool folder=false);
        void open(std::string title, std::function<bool(std::string)> cb, bool saveas=false, bool folder=false){
            open(title, title, cb, saveas, folder);
        }
        bool openIfNotAlready(std::string id, std::string title, std::function<bool(std::string)> cb, bool saveas=false, bool folder=false);
        bool openIfNotAlready(std::string title, std::function<bool(std::string)> cb, bool saveas=false, bool folder=false){ 
            return openIfNotAlready(title, title, cb, saveas, folder);
        }
    };
}