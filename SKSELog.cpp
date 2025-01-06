//These used to be defined be SKSE, but they are no longer.
#define _MESSAGE(a_fmt, ...)	/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kMessage, a_fmt, __VA_ARGS__)*/SKSE::log::info(a_fmt)
#define _ERROR(a_fmt, ...)		/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kError, a_fmt, __VA_ARGS__)*/SKSE::log::error(a_fmt)
#define _FATALERROR(a_fmt, ...)	/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kFatalError, a_fmt, __VA_ARGS__)*/SKSE::log::critical(a_fmt)