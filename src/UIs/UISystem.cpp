#include "UISystem.h"
#include "StatsMenuHook.h"
#include "PrismaUI/PrismaUIService.h"
#include "PrismaUI/Persuasion/PersuasionView.h"
#include "PrismaUI/Persuasion/PersuasionSettings.h"
#include "PrismaUI/SpellMaking/SpellMakingStore.h"
#include "PrismaUI/SpellMaking/SpellMakingView.h"
#include "PrismaUI/Stats/StatsView.h"
#include "Scaleform/Dialogue/DialogueFeed.h"
#include "Scaleform/Factions/CharFactionsFeed.h"
#include "Scaleform/Advancement/AdvancementSound.h"
#include "Scaleform/CharacterSheet/CharRaceFeed.h"
#include "Scaleform/Cursor/HoverCursor.h"
#include "Scaleform/Help/HelpFeed.h"
#include "Scaleform/LoadingScreen/LoadingScreenService.h"
#include "Scaleform/Perks/PerkCurrency.h"
#include "Scaleform/Perks/PerkDataStore.h"
#include "Scaleform/Perks/PerkStick.h"
#include "Scaleform/Perks/PerksFeed.h"
#include "Scaleform/Persuasion/PersuasionMenu.h"
#include "Scaleform/Sigil/SigilMenuFeed.h"
#include "Scaleform/SleepWait/WeatherArt.h"
#include "Scaleform/Skills/SkillSheetFeed.h"
#include "Scaleform/StartMenu/StartMenuService.h"
#include "Scaleform/Toast/ToastService.h"
#include "Scaleform/Translation/TranslationService.h"

#include <optional>

namespace {
    // Owns the PrismaUI service for the program's lifetime.
    // The feature views borrow it by pointer (PrismaViewHandle::m_service).
    // Their OnDomReady callbacks fire asynchronously on the PrismaUI thread after OnDataLoaded returns,
    // so the service must outlive this function.
    std::optional<PrismaUIService> g_prismaUI;

    bool g_scaleformRegistered = false;

    // The Scaleform menus' native services. They need no PrismaUI.
    void RegisterScaleformServices() {
        if (g_scaleformRegistered) {
            return;
        }
        g_scaleformRegistered = true;
        Toast::ToastService::Register();
        // Registered already at kPostLoad; this catches a sink whose source did not exist yet.
        LoadingScreen::LoadingScreenService::Register();
        // Feeds of the skill sheet and the character sheet.
        Skills::SkillSheetFeed::Register();
        Factions::CharFactionsFeed::Register();
        CharacterSheet::CharRaceFeed::Register();
        Dialogue::DialogueFeed::Register();
        Sigil::SigilMenuFeed::Register();
        // The journal's System > Help page, from the JSON tree under ConfigFiles/Help.
        Help::HelpFeed::Register();
        // The persuasion wheel as a Scaleform menu.
        Persuasion::PersuasionMenu::Register();
        // The level-up screen's music and its cancel sound.
        Advancement::AdvancementSound::Register();
        StartMenu::StartMenuService::Register();
        // The wait menu's weather picture, chosen from the sky's weather when the menu opens.
        SleepWait::WeatherArt::Register();
        // The hover cursor's menu sink (its movie listeners were registered at plugin load).
        HoverCursor::Register();
        // Parsed now so the first stats-menu request knows whether the perk tree has any trees.
        PerksView::PerkDataStore::LoadOnce();
        PerksView::PerksFeed::Register();
        PerksView::PerkCurrency::Register();
        PerksView::PerkStick::Register();
        // Last: it needs only the Scaleform loader, and every menu opened afterwards sees its keys.
        Translation::TranslationService::Register();
    }

    void CreatePrismaViews() {
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
        Stats::StatsView::Initialize(*g_prismaUI);
    }
}

void UISystem::OnPostLoad() {
    // The first loading screen of a session is up before kDataLoaded.
    LoadingScreen::LoadingScreenService::Register();
}

void UISystem::OnInputLoaded() {
    LoadingScreen::LoadingScreenService::Register();
    // The input manager exists from here on.
    LoadingScreen::LoadingScreenService::RegisterInput();
}

void UISystem::OnGameStarting() {
    LoadingScreen::LoadingScreenService::EndBootPhase();
}

void UISystem::OnPreLoadGame(std::string_view saveName) {
    OnGameStarting();
    SpellMaking::SpellMakingStore::RestoreBeforeLoad(saveName);
}

void UISystem::OnDataLoaded() {
    Log::INFO("UISystem: OnDataLoaded...");
    RegisterScaleformServices();
    CreatePrismaViews();
    // After both: it opens the SKYBPerksView tree, else the Stats PrismaUI view, else the vanilla screen.
    StatsMenuHook::Initialize();
    Log::INFO("UISystem: OnDataLoaded Complete");
}

void UISystem::OpenPersuasion(RE::Actor* target) {
    // The Scaleform menu hosts the wheel; SKYBPersuasion.ini [General] UsePrismaView = 1 opens
    // the PrismaUI view instead.
    if (Persuasion::Settings::On(Persuasion::P::UsePrismaView)) {
        Persuasion::PersuasionView::Open(target);
    } else {
        Persuasion::PersuasionMenu::Open(target);
    }
}

void UISystem::OpenSpellMaking() {
    SpellMaking::SpellMakingView::Open();
}

void UISystem::Initialize() {
    SpellMaking::SpellMakingStore::RegisterSerialization();
}
