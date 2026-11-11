#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "Platform/Log.h"
#include "Platform/REBridge.h"

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA (RE::PlaySound).
#undef PlaySound
