#include "UISystem.h"
#include "StatsMenuHook.h"
#include "TweenMenuFilter.h"
#include "PrismaUI/PrismaUIService.h"
#include "PrismaUI/Persuasion/DialogueSession.h"
#include "PrismaUI/Persuasion/PersuasionView.h"
#include "PrismaUI/Persuasion/PersuasionSettings.h"
#include "PrismaUI/SpellMaking/SpellMakingStore.h"
#include "PrismaUI/SpellMaking/SpellMakingView.h"
#include "PrismaUI/Stats/StatsView.h"
#include "Scaleform/Dialogue/DialogueFeed.h"
#include "Scaleform/Factions/CharFactionsFeed.h"
#include "Scaleform/Alchemy/AlchemyMenuFeed.h"
#include "Scaleform/Advancement/AdvancementSound.h"
#include "Scaleform/CharacterSheet/CharRaceFeed.h"
#include "Scaleform/HoverCursor/HoverCursor.h"
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

namespace UIs
{
    namespace {
        // Owns the PrismaUI service for the program's lifetime.
        // The feature views borrow it by pointer (PrismaViewHandle::m_service).
        // Their OnDomReady callbacks fire asynchronously on the PrismaUI thread after OnDataLoaded returns,
        // so the service must outlive this function.
        std::optional<PrismaUI::PrismaUIService> g_prismaUI;

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
            Perks::PerkDataStore::LoadOnce();
            Perks::PerksFeed::Register();
            Perks::PerkCurrency::Register();
            Perks::PerkStick::Register();
            // Last: it needs only the Scaleform loader, and every menu opened afterwards sees its keys.
            Translation::TranslationService::Register();
        }

        void CreatePrismaViews() {
            if (g_prismaUI) {
                Log::INFO("UISystem: Views already created; skipping.");
                return;
            }
            g_prismaUI = PrismaUI::PrismaUIService::Initialize();
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
        //A load does not close the PrismaUI persuasion wheel: it is hidden here and the NPC let go.
        //(The Scaleform wheel gets the engine's kHide and does the same itself.)
        Persuasion::PersuasionView::OnPreLoadGame();
        Persuasion::DialogueSession::Abort();
        SpellMaking::SpellMakingStore::RestoreBeforeLoad(saveName);
    }

    void UISystem::OnDataLoaded() {
        Log::INFO("UISystem: OnDataLoaded...");
        RegisterScaleformServices();
        CreatePrismaViews();
        // Shared by both persuasion hosts: the wheel closes when its NPC dies.
        Persuasion::DialogueSession::Register();
        // After both: it opens the SKYBPerksView tree, else the Stats PrismaUI view, else the vanilla screen.
        StatsMenuHook::Initialize();
        // The Tween menu's tool options (TOOLS). Here, not at kNewGame / kPostLoadGame: 'coc' from the
        // title screen sends neither. Its pass runs the frame after the open, after Tween Menu Overhaul's sink.
        TweenMenuFilter::Register();
        Log::INFO("UISystem: OnDataLoaded Complete");
    }

    void UISystem::OpenPersuasion(RE::Actor* target) {
        // The Scaleform menu hosts the wheel; SkyblivionSKSE.ini [Persuasion.General] UsePrismaView = 1 opens
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
        // The vanilla crafting menu's alchemy data: SKSE takes Scaleform callbacks only while loading.
        Alchemy::AlchemyMenuFeed::InstallScaleform();
        // The pointing-hand cursor over anything clickable in any menu: a Mouse listener in every movie.
        HoverCursor::InstallScaleform();
    }
}
