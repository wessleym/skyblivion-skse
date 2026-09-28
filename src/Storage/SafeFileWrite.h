#pragma once

#include <filesystem>
#include <string_view>

//Replaces a small text file without ever leaving it half-written.
//The text goes to "<file>.tmp", the stream is checked after closing, and the temporary is renamed over the file.
//Any failure logs a warning under logOwner and leaves the existing file intact.
class SafeFileWrite {
public:
	//True when the file now holds text.
	static bool Write(const std::filesystem::path& path, std::string_view text, std::string_view logOwner);
};
