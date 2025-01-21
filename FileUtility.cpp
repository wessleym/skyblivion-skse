#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "SKSELog.cpp"

namespace fs = boost::filesystem;
class FileUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        vm->RegisterFunction("FilesInFolder", "SKYBFileUtility", FilesInFolder);
    }

private:
    // https://github.com/eeveelo/PapyrusUtil/blob/6212a5cdedbaceb5d805501b9518921ce5423e76/MiscUtil.cpp#L512
    static std::vector<RE::BSFixedString> FilesInFolder(RE::StaticFunctionTag*, RE::BSFixedString directoryPath,
                                                        RE::BSFixedString extension) {
        std::vector<RE::BSFixedString> arr;
        auto directoryPathData = directoryPath.data();
        if (directoryPathData && directoryPathData[0] != '\0') {
            fs::path dir(directoryPathData);
            fs::directory_iterator end_iter;
            if (fs::exists(dir) && fs::is_directory(dir)) {
                auto extensionData = extension.data();
                std::string ext;
                if (extensionData[0] == '.')
                    ext = extensionData;
                else {
                    ext = ".";
                    ext.append(extensionData);
                }
                SKSE::log::info("Getting files from directory {} with ext {}", directoryPath.c_str(), ext.c_str());
                for (fs::directory_iterator dir_iter(dir); dir_iter != end_iter; ++dir_iter) {
                    if (fs::is_regular_file(dir_iter->status())) {
                        fs::path filepath = dir_iter->path();
                        std::string file = filepath.filename().generic_string();
                        std::string fileExt = filepath.extension().generic_string();
                        SKSE::log::info("Assessing file {} ext {}", file.c_str(), fileExt.c_str());
                        if (ext == ".*" || boost::iequals(filepath.extension().generic_string(), ext)) {
                            RE::BSFixedString fileAsFixedString = RE::BSFixedString(file.c_str());
                            if (std::find(arr.begin(), arr.end(), fileAsFixedString) == arr.end()) {// WTM: Fallen claims to have witnessed duplication. I cannot reproduce it, but I am now preventing duplicate items.
                                SKSE::log::info("Returning file {} ext {}", file.c_str(), fileExt.c_str());
                                arr.push_back(fileAsFixedString);
                            }
                        }
                    }
                }
            }
        }
        return arr;
    }
};