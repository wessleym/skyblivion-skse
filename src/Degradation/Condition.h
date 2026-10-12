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

// Item condition = the engine's own temper value (ExtraHealth on the item's
// ExtraDataList): 1.0 = untempered base, 1.1 Fine ... 1.6 Legendary, and
// anything the smithing menu or this plugin writes beyond that. A missing
// ExtraHealth reads as 1.0 (the engine's rule). Decay lowers it toward
// QualityMin (INI); the repair menu raises it toward the Smithing-scaled cap.
namespace SKYBDeg::Condition
{
	enum class Kind
	{
		kNone,     // not something this system touches
		kWeapon,   // melee weapons and bows (not bound, staves or fists)
		kArmor,    // light/heavy body armor pieces
		kShield    // light/heavy shields: wear only from blocking
	};

	[[nodiscard]] Kind Classify(const RE::TESForm* a_form);

	[[nodiscard]] float GetHealth(const RE::ExtraDataList* a_list);  // 1.0 when absent
	void                SetHealth(RE::ExtraDataList* a_list, float a_value);

	// Material: first WeapMaterial*/ArmorMaterial* keyword with a matching key
	// in [WeaponMaterialWear] / [ArmorMaterialWear], else that section's Default.
	// Each field is a Settings row index (read live with Settings::At).
	struct Material
	{
		int         wear{ -1 };
		std::string name;  // "Steel", "Steel Plate", "" for default
		std::string key;   // keyword suffix: "Steel", "SteelPlate"
		int         cap{ -1 };   // [WeaponMaterialCaps] / [ArmorMaterialCaps]; -1 = RepairCapAtSkillMax
		int         hard{ -1 };  // [WeaponMaterialHardness] / [ArmorMaterialHardness] (or Default)
	};
	[[nodiscard]] const Material& MaterialOf(const RE::TESBoundObject* a_obj);
	[[nodiscard]] float           MaterialMult(const RE::TESBoundObject* a_obj);

	// Decay speed at condition q (multiplier on the base rate).
	[[nodiscard]] float CurveMult(float a_q);

	// Share of damage / armor rating kept at condition q (1.0 at or above 100%,
	// never below StatFloor (INI)). Applied by TemperHook inside the engine.
	[[nodiscard]] float StatMult(float a_q);

	// This item's repair limit at full Smithing (material cap + bonus).
	[[nodiscard]] float CapAtFullSkill(const RE::TESBoundObject* a_obj);

	// Material hardness (1 soft .. 5 Daedric); hammers break more on harder gear.
	[[nodiscard]] float Hardness(const RE::TESBoundObject* a_obj);

	// Smithing slows wear: 1 - SkillWearReduction (INI) x (skill / skill max).
	[[nodiscard]] float WearSkillMult();

	// Repair math (all read the player's current Smithing, fortify included).
	[[nodiscard]] float Smithing();
	[[nodiscard]] float RepairCap(const RE::TESBoundObject* a_obj);                  // player's Smithing
	[[nodiscard]] float RepairCap(const RE::TESBoundObject* a_obj, float a_skill);   // any smith's
	[[nodiscard]] float StrikeResult(float a_q, float a_cap);  // condition after one strike
	// Hammer tiers (Settings::Hammers()): index of a hammer form, or -1.
	[[nodiscard]] int HammerIndex(const RE::TESForm* a_form);
	// Break chance per strike of hammer a_hammer on this item:
	// base x (1 + OverTierMult x max(0, hardness - tier)) x (1 - SkillReduction x skill share)
	// x HammerBreakMult, floored at HammerBreakMin; a base of 0 never breaks.
	[[nodiscard]] float BreakChance(int a_hammer, const RE::TESBoundObject* a_obj);
	[[nodiscard]] bool  Unbreakable(int a_hammer);
	[[nodiscard]] std::int32_t Owned(int a_hammer);

	// Live inventory helpers (the entryList - never the GetInventory() copies).
	[[nodiscard]] RE::ExtraDataList* FindWornList(const RE::TESBoundObject* a_obj, bool a_leftHand);
	[[nodiscard]] bool               ListIsLive(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list);
	struct WornPiece
	{
		RE::TESBoundObject* obj;
		RE::ExtraDataList*  list;
		Kind                kind;
	};
	[[nodiscard]] std::vector<WornPiece> WornArmor();

	// Enchanted gear (record enchantment or one added to this instance). The
	// player needs EnchantRepairSkill (INI) Smithing to repair it; vendors never do.
	[[nodiscard]] bool IsEnchanted(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list);
	[[nodiscard]] bool PlayerMayRepair(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list);

	// Vendor repairs: the vendor's own Smithing (+ VendorSkillBonus (INI)), and
	// the gold asked to take this item from a_from to a_to:
	//   (points to 100% + points above 100% x ImproveCostMult) x GoldPerPoint
	//   x (1 + HardnessCostMult x (hardness - 1)) x (1 + SkillCostMult x skill / 100)
	//   x EnchantedCostMult (if enchanted) x PriceMult, rounded up, at least MinCost.
	[[nodiscard]] float        VendorSkill(RE::Actor* a_vendor);
	// The player's gold (Gold001, 0000000F). Not Actor::GetGoldAmount(): the
	// CommonLibSSE-NG 3.6.0 helper reads a bad pointer on 1.6.1170.
	[[nodiscard]] std::int32_t PlayerGold();
	[[nodiscard]] std::int32_t VendorCost(const RE::TESBoundObject* a_obj, bool a_enchanted, float a_from, float a_to,
										  float a_vendorSkill);

	// Recharge (vendor service). A weapon or staff holds a charge when it has an
	// enchantment with a charge pool: the record's (formEnchanting +
	// amountofEnchantment) or this copy's (ExtraEnchantment::charge). Current
	// charge = ExtraCharge (absent = full); for an EQUIPPED weapon the engine
	// tracks it in Right/LeftItemCharge, so that actor value is read instead.
	struct Charge
	{
		RE::EnchantmentItem* enchantment{ nullptr };
		float                current{ 0 };
		float                max{ 0 };
		bool                 valid{ false };
	};
	[[nodiscard]] Charge ChargeOf(const RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list);
	// Fills this copy (and, if equipped, the hand's charge actor value) to max.
	void                 FillCharge(const RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list);
	// Gold for a_missing charge points: missing x RechargeGoldPerCharge (INI)
	// x (1 + RechargeSkillCostMult (INI) x vendor Enchanting / 100) x RechargePriceMult (INI),
	// rounded up, at least RechargeMinCost (INI).
	[[nodiscard]] std::int32_t RechargeCost(float a_missing, float a_vendorEnchanting);

	[[nodiscard]] bool         IsRepairTool(const RE::TESForm* a_form);
	[[nodiscard]] std::int32_t CountRepairTools(RE::TESBoundObject** a_firstOwned = nullptr);

	// What the engine itself reports for this exact instance: displayed damage
	// for weapons, displayed armor rating for armor (skills, perks and - with
	// TemperHook installed - the condition penalty included).
	[[nodiscard]] float EngineStat(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list);
	// The same at a hypothetical condition (temporarily writes, then restores).
	[[nodiscard]] float EngineStatAt(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_q);

	// What the item really does now / at condition q (= the engine number;
	// TemperHook puts the condition penalty inside it).
	[[nodiscard]] float EffectiveStat(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list);
	[[nodiscard]] float EffectiveStatAt(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_q);

	[[nodiscard]] std::mt19937& Rng();
	[[nodiscard]] float         Roll();  // [0,1)
}
