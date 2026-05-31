#include "FileUtility.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

// Case-insensitive ASCII compare; matches the prior boost::iequals usage.
static bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(),
        [](char x, char y) {
            return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
        });
}

void FileUtility::Register(RE::BSScript::Internal::VirtualMachine* vm) {
    SKSEScriptRegistrar::Register(vm, "SKYBFileUtility", "FilesInFolder", FilesInFolder);
}

std::vector<RE::BSFixedString> FileUtility::FilesInFolder(RE::StaticFunctionTag*, RE::BSFixedString directoryPath,
                                                          RE::BSFixedString extension) {
    std::vector<RE::BSFixedString> arr;
    auto directoryPathData = directoryPath.data();
    if (directoryPathData && directoryPathData[0] != '\0') {
        fs::path dir(directoryPathData);
        std::error_code ec;
        if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
            auto extensionData = extension.data();
            std::string ext;
            if (extensionData[0] == '.')
                ext = extensionData;
            else {
                ext = ".";
                ext.append(extensionData);
            }
            Log::INFO("Getting files from directory {} with ext {}", directoryPath.c_str(), ext.c_str());
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.is_regular_file(ec)) {
                    const fs::path& filepath = entry.path();
                    std::string file = filepath.filename().generic_string();
                    std::string fileExt = filepath.extension().generic_string();
                    Log::INFO("Assessing file {} ext {}", file.c_str(), fileExt.c_str());
                    if (ext == ".*" || iequals(fileExt, ext)) {
                        RE::BSFixedString fileAsFixedString = RE::BSFixedString(file.c_str());
                        if (std::find(arr.begin(), arr.end(), fileAsFixedString) == arr.end()) {// WTM: Fallen claims to have witnessed duplication. I cannot reproduce it, but I am now preventing duplicate items.
                            Log::INFO("Returning file {} ext {}", file.c_str(), fileExt.c_str());
                            arr.push_back(fileAsFixedString);
                        }
                    }
                }
            }
        }
    }
    return arr;
}
