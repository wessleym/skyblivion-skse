#pragma once
// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Activating a repair tool (any form in SKYBDegRepairTools) in the inventory
// opens the repair menu.
//
// How: the InventoryMenu registers its Scaleform callbacks in Accept()
// (FxDelegateHandler vfunc 01). We hook that vfunc and hand the original a
// proxy CallbackProcessor that swaps the "ItemSelect" handler for ours and
// forwards everything else untouched. Our handler checks the selected item;
// for a repair tool it opens the menu, for anything else it calls the
// engine's own handler with the same arguments. Works the same for vanilla
// and SkyUI inventories, mouse, keyboard and gamepad, because they all end
// in the one "ItemSelect" delegate call.
namespace SKYBDeg::InventoryHook
{
	void Install();
}
