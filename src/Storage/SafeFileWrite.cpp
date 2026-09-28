#include "SafeFileWrite.h"

#include <fstream>

bool SafeFileWrite::Write(const std::filesystem::path& path, std::string_view text, std::string_view logOwner) {
	auto temporary = path;
	temporary += ".tmp";
	std::error_code error;
	{
		std::ofstream outputFile(temporary, std::ios::trunc);
		if (!outputFile.is_open()) {
			Log::WARN("{}: Cannot create {}. {} left unchanged.", logOwner, temporary.filename().string(), path.filename().string());
			return false;
		}
		outputFile << text;
		outputFile.close();
		if (!outputFile) {
			Log::WARN("{}: Writing {} failed. {} left unchanged.", logOwner, temporary.filename().string(), path.filename().string());
			std::filesystem::remove(temporary, error);
			return false;
		}
	}
	std::filesystem::rename(temporary, path, error);
	if (error) {
		Log::WARN("{}: Cannot replace {} ({}). It is left unchanged.", logOwner, path.filename().string(), error.message());
		std::filesystem::remove(temporary, error);
		return false;
	}
	return true;
}
