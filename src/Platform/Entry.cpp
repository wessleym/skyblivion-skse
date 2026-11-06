#include "Plugin.h"

//SKSE Plugin Entry Point
//This is the only translation unit that exports an SKSE entry symbol. All
//initialization is in Plugin::OnLoad.
//
//SKSE_PLUGIN_LOAD exports SKSEPlugin_Load. CommonLibSSE-NG's plugin rule also
//emits SKSEPlugin_Version and SKSEPlugin_Query; SKSE VR's loader requires the
//latter to recognize the DLL.

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	return Plugin::OnLoad(a_skse);
}
