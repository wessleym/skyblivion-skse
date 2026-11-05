#pragma once

// Weapon breakage. Tunables: [Breakage] in SKYBDegradation.ini (Settings.csv); the settings menu
// overrides the player-facing ones.
//
//   use       a landed hit, or a block with the weapon, below Threshold rolls a break chance that rises
//             from ChanceAtThreshold just below it to ChanceAtFloor at QualityMin (x ChanceMult, x
//             PowerAttackMult for power attacks and bashes, x BlockMult for blocks)
//   break     the weapon drops to BrokenCondition, " (Broken)" goes on its name, it is unequipped and a
//             message says so
//   equip     equipping a broken weapon below UsableAt is undone with a message
//   repair    raising its condition to UsableAt or more takes the mark off (Condition::SetHealth)
//
// The mark is the item's own name (ExtraTextDisplayData), so it travels with the item into any
// container and through saves, and the player sees it everywhere the name shows.
namespace SKYBDeg::Breakage
{
	[[nodiscard]] bool IsMarked(const RE::ExtraDataList* a_list);

	// After a use of this worn weapon instance (DecayService): may break it.
	void OnUse(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, bool a_power, bool a_block);

	// The condition of a marked instance changed to a_value: unmark it at UsableAt or above.
	void OnConditionSet(RE::ExtraDataList* a_list, float a_value);

	// Undoes the player equipping a broken weapon.
	class EquipGuard : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		static EquipGuard* GetSingleton()
		{
			static EquipGuard singleton;
			return std::addressof(singleton);
		}
		void Register();
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event,
											  RE::BSTEventSource<RE::TESEquipEvent>* a_source) override;
	};
}
