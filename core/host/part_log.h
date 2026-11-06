// FLvibed - a module host part's log channel. build.cmd's part wrapper includes this INSIDE the part's namespace, after
// core\host\all.h (no include guard: once per part, which is the point): the part's plain Log(...) finds this first, so its
// lines stay in its own file (FLSubtitles.log, FLController.log) while the host shares one core\log.cpp. The host core's own
// lines go to the file opened last (core\log.h LogOpen). An object, not a function: see core\logic\module_log.h.
inline int g_logChannel = -1;

struct PartLog
{
    void operator()(const char* fmt, ...) const
    {
        va_list args;
        va_start(args, fmt);
        LogV(g_logChannel, fmt, args);
        va_end(args);
    }
};
inline constexpr PartLog Log{};
