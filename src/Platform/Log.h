#pragma once

//Logging Abstraction

#include <format>
#include <source_location>
#include <string_view>

namespace Log
{
	enum class Level
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warn = 3,
		Error = 4,
		Critical = 5,
	};

	namespace detail
	{
		void Write(std::source_location a_loc, Level a_level, std::string_view a_msg);
	}
}

#define SKY_DEFINE_LOG_LEVEL(a_name, a_level)                                    \
	template <class... T>                                                        \
	struct a_name                                                                \
	{                                                                            \
		a_name() = delete;                                                       \
		explicit a_name(                                                         \
			std::format_string<T...> a_fmt,                                      \
			T&&... a_args,                                                       \
			std::source_location a_loc = std::source_location::current())        \
		{                                                                        \
			detail::Write(a_loc, a_level,                                        \
				std::vformat(a_fmt.get(), std::make_format_args(a_args...)));    \
		}                                                                        \
	};                                                                           \
	template <>                                                                  \
	struct a_name<void>                                                          \
	{                                                                            \
		a_name() = delete;                                                       \
		explicit a_name(                                                         \
			std::string_view a_msg,                                              \
			std::source_location a_loc = std::source_location::current())        \
		{                                                                        \
			detail::Write(a_loc, a_level, a_msg);                                \
		}                                                                        \
	};                                                                           \
	template <class... T>                                                        \
	a_name(std::format_string<T...>, T&&...) -> a_name<T...>;                    \
	a_name(std::string_view) -> a_name<void>;

namespace Log
{
	SKY_DEFINE_LOG_LEVEL(TRACE, Level::Trace)
	SKY_DEFINE_LOG_LEVEL(DEBUG, Level::Debug)
	SKY_DEFINE_LOG_LEVEL(INFO, Level::Info)
	SKY_DEFINE_LOG_LEVEL(WARN, Level::Warn)
	SKY_DEFINE_LOG_LEVEL(ERROR, Level::Error)
	SKY_DEFINE_LOG_LEVEL(CRITICAL, Level::Critical)
}

#undef SKY_DEFINE_LOG_LEVEL
