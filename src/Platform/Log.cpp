#include "Platform/Log.h"

#include <spdlog/spdlog.h>

// ----------------------------------------------------------------------------
//  The only code in the project that writes to the spdlog logger that
//  CommonLibSSE-NG installs; the rest of the codebase calls Log::.
// ----------------------------------------------------------------------------
void Log::detail::Write(std::source_location a_loc, Level a_level, std::string_view a_msg)
{
	// SKSE::Init() installs spdlog's default logger. The caller's source location
	// is passed through unchanged.
	static constexpr spdlog::level::level_enum kLevels[] = {
		spdlog::level::trace,
		spdlog::level::debug,
		spdlog::level::info,
		spdlog::level::warn,
		spdlog::level::err,
		spdlog::level::critical,
	};
	spdlog::log(
		spdlog::source_loc{ a_loc.file_name(), static_cast<int>(a_loc.line()), a_loc.function_name() },
		kLevels[static_cast<std::size_t>(a_level)],
		a_msg);
}
