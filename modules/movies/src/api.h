// The Movies module's host part and its logic: one table each way (the logic is opened at the first movie, as the subtitles' is: the
// startup movies play before the first frame).
#pragma once
#include "common.h"
#include "settings_bus.h"

constexpr u32 kMoviesApiVersion = 3; // v3: SetIntroOverride gone (issue 80 removed movie_intro)

struct MoviesHostApi
{
    u32 version;
    const char* dllDir;
    void (*LogWrite)(const char* line);
    const SettingsBus* (*GetSettingsBus)();
    void* (*GetMovieGraph)();                    // the DirectShow graph of the movie playing, or null
    bool (*SkipMovie)();                         // the movie ends as a finished one
    bool (*ModuleOn)(int row);                   // a [Modules] row (core\modules_rules.h)
};

struct MoviesImplApi
{
    u32 version;
    void (*Init)(const MoviesHostApi* host);
    void (*Shutdown)();
    // 0 = PlayMovie(hwnd, path), 1 = UpdateMovie (each pass of the movie loop), 2 = StopMovie.
    void (*OnMovie)(int event, void* hwnd, const char* path);
};
