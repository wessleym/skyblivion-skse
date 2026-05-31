#pragma once

namespace SKSE
{
	class LoadInterface;
}

//This is platform agnostic.  Plugin::OnLoad is the plugin's entry point: it
//initializes SKSE, registers every subsystem, and installs the single SKSE
//message listener. It contains no CommonLib-variant-specific code.
//The actual exported SKSE symbol (SKSEPlugin_Load) is in
//src/Platform/Entry.cpp, which forwards to OnLoad. Keeping the  export and the
//logic in separate files is what lets the CommonLib backend be swapped without
//touching this code. See src/Platform/Backend.h.
namespace Plugin
{
	// Invoked once by the SKSE loader when the DLL is loaded. Return false to
	// tell SKSE to mark the plugin as failed/incompatible and unload it.
	bool OnLoad(const SKSE::LoadInterface* a_skse);
}
