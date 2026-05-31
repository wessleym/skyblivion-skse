#pragma once

//CommonLib Backend Selection
//----------------------------------------------------------------------------
//This plugin can be built against two CommonLib variants:
//
//    SKY_COMMONLIB_LIBXSE  libxse/commonlibsse (Skyrim SE only)
//                          Logging is in the REX:: namespace.
//
//    SKY_COMMONLIB_NG      CommonLibSSE-NG (Skyrim SE and VR)
//                          Logging is spdlog-backed (SKSE::log and spdlog).
//
//Everything that differs between the two variants is confined to src/Platform/:
//    Log.cpp    selects the logging sink.
//    Entry.cpp  defines the exported SKSE entry point.
//
//The active backend is selected by the `backend` option in xmake.lua, which
//passes -DSKY_COMMONLIB=<n> and selects the matching submodule + plugin
//rule. Switch with `xmake f --backend=ng` (or `--backend=libxse`), then `xmake`.
//
//The #ifndef fallback below only applies when SKY_COMMONLIB is not supplied by
//the build (e.g., via IntelliSense). Full migration steps are in MIGRATION.md.

#define SKY_COMMONLIB_LIBXSE 1
#define SKY_COMMONLIB_NG     2

#ifndef SKY_COMMONLIB
#	define SKY_COMMONLIB SKY_COMMONLIB_LIBXSE
#endif
