// FLvibed - the movie event: Movie.dll's imports of Freelancer.exe and its CoCreateInstance, once, for the Subtitles and Movies modules
// (router::Client movie, moviePoll, in the order of router.cpp's kMovieOrder). Installed by the first of them to start.
#pragma once
#include "common.h"

namespace moviehooks
{
void Install();
// The DirectShow filter graph (IGraphBuilder*) Movie.dll made for the movie playing, or null.
void* Graph();
// The movie playing ends the way the game ends a finished one: the next UpdateMovie reports it over. False with none playing.
bool Skip();
// Debug `movieskip`.
void SkipVerb(int channel, const char* line, char* reply, size_t len);
} // namespace moviehooks
