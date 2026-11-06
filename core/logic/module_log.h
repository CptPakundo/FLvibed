// FLvibed - a logic module's log channel. build.cmd's wrapper includes this INSIDE the module's namespace, right after the
// core headers (no include guard: once per module, which is the point): the module's plain Log(...) and LogSetSink(...) find
// these first, so its lines stay in its own file (FLSubtitles.log, FLController.log) while the logic shares one core\log.cpp.
// A line from core\game itself goes to the channel attached last (core\log.h LogAttach).
// Log is an object, not a function: a function would meet ::Log by argument lookup whenever an argument is a global type (a
// RECT's member, a GUID), and the call would be ambiguous.
inline int g_logChannel = -1;

inline void LogSetSink(void (*sink)(const char* line), const char* dllDir) { g_logChannel = ::LogAttach(sink, dllDir); }
// A cut module writes to the channel its entry already attached (its lines stay where they were), and leaves core's own alone.
inline void LogJoin(void (*sink)(const char* line)) { g_logChannel = ::LogChannelOf(sink); }

struct ModuleLog
{
    void operator()(const char* fmt, ...) const
    {
        va_list args;
        va_start(args, fmt);
        ::LogV(g_logChannel, fmt, args);
        va_end(args);
    }
};
inline constexpr ModuleLog Log{};
