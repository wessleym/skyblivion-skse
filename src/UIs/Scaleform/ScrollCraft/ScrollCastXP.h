#pragma once

//Casting a scroll or firing a staff trains Enchanting (Scott, 2026-11-04: "Enchanting experience for casting scrolls
//and staves not the casting school"; vanilla gives neither any skill experience).
//
//The event: the game's TESSpellCastEvent, for the player only. A scroll's cast names the ScrollItem; a staff's cast
//names the staff's EnchantmentItem, counted only when a staff equipped in either hand carries that enchantment (so a
//weapon's on-hit enchantment never counts). One cast, one event: the staff's charge use is not hooked, so nothing is
//counted twice.
//
//The amount: the item's tier, from the game's own spell levels: 1 + (the highest Minimum Skill Level of its effects) / 25,
//clamped 1 to 5 (Novice 0 .. Master 100; the ESM's scroll tiers match it), times a scale, in Enchanting skill uses (the
//engine turns a use into experience with Enchanting's Use Mult, 900). Writing a scroll is 0.12 uses per tier; casting
//starts at 0.02 per tier (a Novice cast 18 experience, a Master cast 90). The spell-cast rule (uses = base magicka cost)
//does not fit Enchanting: the costs run from 1 to over 40,000 and Enchanting's Use Mult is 900, so a few scrolls would
//level the skill in one cast.
//
//Settings: Data/SKSE/Plugins/SKYBScrollCrafting.ini, [CastingXP] Enabled (1), ScrollScale (0.02), StaffScale (0.02),
//LogCasts (1). Read at kDataLoaded and again at every new game and game load. Event-driven, main thread, no polling.
namespace ScrollCraft {

	class ScrollCastXP : public RE::BSTEventSink<RE::TESSpellCastEvent> {
	public:
		static constexpr const char* IniFile = "Data/SKSE/Plugins/SKYBScrollCrafting.ini";

		static ScrollCastXP* GetSingleton()
		{
			static ScrollCastXP singleton;
			return &singleton;
		}

		//kDataLoaded: reads the INI and adds the sink once
		void Register();
		//kNewGame / kPostLoadGame: reads the INI again
		void Reload();

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event,
											  RE::BSTEventSource<RE::TESSpellCastEvent>* a_source) override;

	private:
		bool  m_registered = false;
		bool  m_enabled = true;
		float m_scrollScale = 0.02f;
		float m_staffScale = 0.02f;
		bool  m_log = true;
	};
}
