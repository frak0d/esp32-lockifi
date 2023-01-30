#pragma once

#ifndef LOG_LEVEL
    #define LOG_LEVEL log::level::warn
#endif

#include <cstdio>

namespace log
{

enum class level : char
{
    none=0, error=1, warn=2, info=3, debug=4
};

constexpr auto loglevel = log::level(LOG_LEVEL);

inline void
debug(const char* fmt, auto... args) noexcept
{
    if constexpr (loglevel >= level::debug)
        std::printf(fmt, args...);
}

inline void
info(const char* fmt, auto... args) noexcept
{
    if constexpr (loglevel >= level::info)
        std::printf(fmt, args...);
}

inline void
warn(const char* fmt, auto... args) noexcept
{
    if constexpr (loglevel >= level::warn)
        std::printf(fmt, args...);
}

inline void
error(const char* fmt, auto... args) noexcept
{
    if constexpr (loglevel >= level::error)
        std::printf(fmt, args...);
}

}
