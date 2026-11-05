#include "PerkSpells/PerkSpells.h"

#include "PerkSpells/EntryPointSpells.h"
#include "PerkSpells/Settings.h"

// Replaces the standalone plugin's main.cpp: its own spdlog file is gone (Skyblivion.log
// carries the lines) and the load runs from Plugin::OnLoad, at the same load point.

namespace PerkSpells {
    void Install() {
        Log::INFO("PerkSpells: SKYBPerkSpells 0.2.0 (folded into Skyblivion.dll) loading; runtime {}",
                  REL::Module::get().version().string());
        Settings::Load();
        EntryPointSpells::Install();
    }
}
