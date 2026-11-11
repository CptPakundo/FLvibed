#pragma once
// FLvibed's version: the one place it is set. Read by the DLL's version resource (core\flvibed.rc, Windows' file
// properties), the modules' start line in their logs and the options page's About; CHANGELOG.md names each release.
// Only macros (no code), so the header also works inside the build's module namespaces and in the resource compiler.

#define FLV_VERSION_MAJOR 0
#define FLV_VERSION_MINOR 4
#define FLV_VERSION_PATCH 6
#define FLV_VERSION_BUILD 0
#define FLV_VERSION "0.4.6" // (the owner, 11 Nov 2026: the bug sweep is 0.4.6)
