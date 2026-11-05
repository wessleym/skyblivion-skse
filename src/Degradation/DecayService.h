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

// Wears the PLAYER's gear down on combat hits (TESHitEvent, main thread):
//   player lands a weapon hit      -> that weapon (the worn instance in the hand)
//   player blocks a hit            -> the shield, else the blocking weapon
//   player takes an unblocked hit  -> ArmorPiecesPerHit (INI) random worn armor pieces
// Loss per event = base rate x category mult x RateMult (INI) x material
// x curve(condition) x (power / bash / sneak / attacker-damage scaling),
// floored at QualityMin (INI). NPC gear does not wear.
namespace SKYBDeg
{
	class DecayService : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		static DecayService* GetSingleton()
		{
			static DecayService singleton;
			return std::addressof(singleton);
		}

		void Register();

		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
											  RE::BSTEventSource<RE::TESHitEvent>* a_source) override;

		// Lowers one instance; returns the new condition (or the old one if unchanged).
		static float Degrade(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_rate, const char* a_why);

	private:
		void OnPlayerAttack(const RE::TESHitEvent& a_event, RE::PlayerCharacter* a_player);
		void OnPlayerStruck(const RE::TESHitEvent& a_event, RE::PlayerCharacter* a_player);
		static float EventMult(const RE::TESHitEvent& a_event);
	};
}
