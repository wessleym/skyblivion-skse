#pragma once

namespace SKSE
{
	class LoadInterface;
}

//Plugin::OnLoad is the plugin's entry point: it initializes SKSE, registers every
//subsystem, and installs the single SKSE message listener. The exported SKSE symbol
//(SKSEPlugin_Load) is in src/Platform/Entry.cpp, which forwards to OnLoad.
namespace Plugin
{
	// Invoked once by the SKSE loader when the DLL is loaded. Return false to
	// tell SKSE to mark the plugin as failed/incompatible and unload it.
	bool OnLoad(const SKSE::LoadInterface* a_skse);
}
