#include "UISystem.h"
#include "PrismaUI/PrismaUIService.h"
#include "PrismaUI/Persuasion/PersuasionView.h"
#include "PrismaUI/SpellMaking/SpellMakingStore.h"
#include "PrismaUI/SpellMaking/SpellMakingView.h"

#include <optional>

namespace {
    // Owns the PrismaUI service for the program's lifetime. The feature views borrow it by
    // pointer (PrismaViewHandle::m_service); their OnDomReady callbacks fire asynchronously on
    // the PrismaUI thread AFTER OnDataLoaded returns, so the service must outlive this function.
    // (A stack-local here left every view handle dangling -> use-after-scope crash on DOM ready.)
    std::optional<PrismaUIService> g_prismaUI;
}

void UISystem::OnDataLoaded() {
    Log::INFO("UISystem: OnDataLoaded...");
    if (g_prismaUI) {
        Log::INFO("UISystem: Views already created; skipping.");
        return;
    }
    g_prismaUI = PrismaUIService::Initialize();
    if (!g_prismaUI) {
        Log::WARN("UISystem: PrismaUI not available. Related UIs will not function. PrismaUI may not be installed.");
        return;
    }
    Log::INFO("UISystem: PrismaUIService Acquired");
    Log::INFO("UISystem: Creating Views");
    Persuasion::PersuasionView::Initialize(*g_prismaUI);
    SpellMaking::SpellMakingView::Initialize(*g_prismaUI);
    SpellMaking::SpellMakingStore::Initialize();
    Log::INFO("UISystem: OnDataLoaded Complete");
}

void UISystem::OpenPersuasion(RE::Actor* target) {
    Persuasion::PersuasionView::Open(target);
}

void UISystem::OpenSpellMaking() {
    SpellMaking::SpellMakingView::Open();
}

void UISystem::Initialize() {
    SpellMaking::SpellMakingStore::RegisterSerialization();
}
