#include "Platform/Log.h"

#include "Platform/Backend.h"

#if SKY_COMMONLIB == SKY_COMMONLIB_LIBXSE
#	include <REX/LOG.h>
#elif SKY_COMMONLIB == SKY_COMMONLIB_NG
#	include <spdlog/spdlog.h>
#endif

// ----------------------------------------------------------------------------
//  Backend hand-off. This is the single place in the project that knows which
//  CommonLib variant is in use; the rest of the codebase only sees Log::.
// ----------------------------------------------------------------------------
void Log::detail::Write(std::source_location a_loc, Level a_level, std::string_view a_msg)
{
#if SKY_COMMONLIB == SKY_COMMONLIB_LIBXSE

	// libxse/commonlibsse funnels all logging through REX. Log::Level is laid
	// out to match REX::ELogLevel value-for-value, so the cast is exact.
	REX::Impl::Log(a_loc, static_cast<REX::ELogLevel>(a_level), a_msg);

#elif SKY_COMMONLIB == SKY_COMMONLIB_NG

	// CommonLibSSE-NG is spdlog-backed; SKSE::Init() installs the default
	// logger. Forward the captured source location straight through to spdlog.
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

#else
#	error "SKY_COMMONLIB is not set to a known CommonLib backend (see Platform/Backend.h)"
#endif
}
