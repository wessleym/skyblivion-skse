#include "SpellReflect/SpellReflect.h"

#include "SpellReflect/BlockTiming.h"
#include "SpellReflect/BruteForce.h"
#include "SpellReflect/Feedback.h"
#include "SpellReflect/Reflect.h"
#include "SpellReflect/Settings.h"
#include "SpellReflect/TimedBlock.h"

// Replaces the standalone plugin's main.cpp: its own spdlog file is gone (Skyblivion.log
// carries the lines) and each step runs from Plugin.cpp at the same load point as there.

namespace SpellReflect {
    void Install() {
        Log::INFO("SpellReflect: SKYBSpellReflect 0.3.0 (folded into Skyblivion.dll) loading; runtime {}",
                  REL::Module::get().version().string());
        Settings::Load();
        Reflect::Install();
        TimedBlock::Install();
    }

    void OnInputLoaded() {
        BlockTiming::Register();
    }

    void OnDataLoaded() {
        BlockTiming::OnDataLoaded();
        Feedback::OnDataLoaded();
        Reflect::OnDataLoaded();
        TimedBlock::OnDataLoaded();
        BruteForce::OnDataLoaded();
    }
}
