#pragma once
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

// Activating a repair tool (any form in SKYBDegRepairTools) in the inventory opens the repair menu.
// InventoryMenu registers its Scaleform callbacks in Accept() (FxDelegateHandler vfunc 01); the hook
// hands the original a proxy that swaps in our "ItemSelect" handler and forwards the rest. Ours opens
// the menu for a repair tool and calls the engine's handler for anything else. Vanilla and SkyUI
// inventories, mouse, keyboard and gamepad all go through this one delegate call.
namespace SKYBDeg::InventoryHook
{
	void Install();
}
