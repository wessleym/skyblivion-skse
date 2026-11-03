#include "Degradation/Condition.h"
#include "Resources.h"
#include "Degradation/Breakage.h"

#include "Degradation/Settings.h"
#include "Degradation/TemperHook.h"
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

using namespace std::literals;

namespace SKYBDeg::Condition
{
	namespace
	{
		std::string Spaced(std::string_view a_camel)
		{
			std::string out;
			for (std::size_t i = 0; i < a_camel.size(); ++i) {
				const char c = a_camel[i];
				if (i > 0 && std::isupper(static_cast<unsigned char>(c)) &&
					std::islower(static_cast<unsigned char>(a_camel[i - 1]))) {
					out.push_back(' ');
				}
				out.push_back(c);
			}
			return out;
		}

		template <class F>
		void ForEachLiveList(const RE::TESBoundObject* a_obj, F&& a_fn)
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			auto changes = player ? player->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) {
				return;
			}
			for (auto entry : *changes->entryList) {
				if (!entry || !entry->object || !entry->extraLists) {
					continue;
				}
				if (a_obj && entry->object != a_obj) {
					continue;
				}
				for (auto list : *entry->extraLists) {
					if (list && a_fn(entry->object, list)) {
						return;
					}
				}
			}
		}
	}

	Kind Classify(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return Kind::kNone;
		}
		if (auto weap = a_form->As<RE::TESObjectWEAP>()) {
			if (weap->IsBound() || weap->IsStaff() || weap->IsHandToHandMelee()) {
				return Kind::kNone;
			}
			return Kind::kWeapon;
		}
		if (auto armo = a_form->As<RE::TESObjectARMO>()) {
			if (!armo->IsHeavyArmor() && !armo->IsLightArmor()) {
				return Kind::kNone;  // clothing, jewelry
			}
			return armo->IsShield() ? Kind::kShield : Kind::kArmor;
		}
		return Kind::kNone;
	}

	float GetHealth(const RE::ExtraDataList* a_list)
	{
		if (a_list) {
			if (auto h = a_list->GetByType<RE::ExtraHealth>()) {
				return h->health;
			}
		}
		return 1.0f;
	}

	void SetHealth(RE::ExtraDataList* a_list, float a_value)
	{
		if (!a_list) {
			return;
		}
		if (auto h = a_list->GetByType<RE::ExtraHealth>()) {
			h->health = a_value;
		} else {
			a_list->Add(new RE::ExtraHealth(a_value));
		}
		// a broken weapon repaired far enough is usable again
		Breakage::OnConditionSet(a_list, a_value);
	}

	const Material& MaterialOf(const RE::TESBoundObject* a_obj)
	{
		static std::unordered_map<RE::FormID, Material> cache;
		if (auto it = cache.find(a_obj->GetFormID()); it != cache.end()) {
			return it->second;
		}

		const bool weapon = a_obj->Is(RE::FormType::Weapon);
		const std::string_view kwPrefix = weapon ? "WeapMaterial"sv : "ArmorMaterial"sv;
		const std::string_view wearSection = weapon ? "WeaponMaterialWear"sv : "ArmorMaterialWear"sv;

		Material mat;
		if (auto kwForm = a_obj->As<RE::BGSKeywordForm>()) {
			for (auto kw : kwForm->GetKeywords()) {
				if (!kw) {
					continue;
				}
				std::string_view edid = kw->GetFormEditorID();
				auto pos = edid.find(kwPrefix);  // also matches DLC2ArmorMaterial...
				if (pos == std::string_view::npos) {
					continue;
				}
				auto name = edid.substr(pos + kwPrefix.size());
				if (name.empty()) {
					continue;
				}
				if (const int idx = Settings::Find(wearSection, name); idx >= 0) {
					mat.wear = idx;
					mat.name = Spaced(name);
					mat.key = name;
					break;
				}
			}
		}
		{
			// hardness: the material's own row, else the Default row
			const auto hardSection = weapon ? "WeaponMaterialHardness"sv : "ArmorMaterialHardness"sv;
			mat.hard = mat.key.empty() ? -1 : Settings::Find(hardSection, mat.key);
			if (mat.hard < 0) {
				mat.hard = Settings::Find(hardSection, "Default");
			}
		}
		if (mat.name.empty()) {
			mat.wear = Settings::Find(wearSection, "Default");
		} else {
			// per-material repair limit at full skill
			mat.cap = Settings::Find(weapon ? "WeaponMaterialCaps"sv : "ArmorMaterialCaps"sv, mat.key);
		}
		return cache.emplace(a_obj->GetFormID(), std::move(mat)).first->second;
	}

	float MaterialMult(const RE::TESBoundObject* a_obj)
	{
		const auto& m = MaterialOf(a_obj);
		return m.wear >= 0 ? Settings::At(m.wear) : 1.0f;
	}

	float CapAtFullSkill(const RE::TESBoundObject* a_obj)
	{
		const auto& m = MaterialOf(a_obj);
		const float cap = m.cap >= 0 ? Settings::At(m.cap) : Settings::Get(G::RepairCapAtSkillMax);
		return cap + Settings::Get(G::RepairCapBonus);
	}

	float Hardness(const RE::TESBoundObject* a_obj)
	{
		const auto& m = MaterialOf(a_obj);
		return m.hard >= 0 ? Settings::At(m.hard) : 2.0f;
	}

	float WearSkillMult()
	{
		const float skillMax = std::max(1.0f, Settings::Get(G::RepairCapSkillMax));
		const float share = std::clamp(Smithing() / skillMax, 0.0f, 1.0f);
		return std::clamp(1.0f - Settings::Get(G::SkillWearReduction) * share, 0.0f, 1.0f);
	}

	float CurveMult(float a_q)
	{
		// Above the knee: accelerating. Knee .. band low: the working band,
		// easing from 1.0 down to BandLowMult. Band low .. floor: slowing
		// further to FloorMult at QualityMin (INI). Lower is always slower.
		const float knee = Settings::Get(G::CurveKnee);
		const float low = std::min(Settings::Get(G::CurveBandLow), knee - 0.001f);
		const float lowMult = Settings::Get(G::CurveBandLowMult);
		const float floorQ = std::min(Settings::Get(G::QualityMin), low - 0.001f);
		const float floorMult = Settings::Get(G::CurveFloorMult);
		float mult;
		if (a_q > knee) {
			mult = 1.0f + Settings::Get(G::CurveSteepness) * std::pow(a_q - knee, Settings::Get(G::CurveExponent));
		} else if (a_q >= low) {
			mult = lowMult + (1.0f - lowMult) * (a_q - low) / (knee - low);
		} else {
			mult = floorMult + (lowMult - floorMult) * std::clamp((a_q - floorQ) / (low - floorQ), 0.0f, 1.0f);
		}
		mult *= Settings::Get(G::CurveBelowKnee);
		if (a_q < 1.0f) {
			mult *= Settings::Get(G::CurveBelowBase);
		}
		return std::clamp(mult, 0.0f, Settings::Get(G::CurveMaxMult));
	}

	float StatMult(float a_q)
	{
		if (a_q >= 1.0f || !Settings::On(G::StatEnabled)) {
			return 1.0f;
		}
		const float q = std::max(a_q, Settings::Get(G::StatFloor));
		return std::clamp(1.0f - (1.0f - q) * Settings::Get(G::StatStrength), 0.0f, 1.0f);
	}

	float Smithing()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		return player ? player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing) : 0.0f;
	}

	float RepairCap(const RE::TESBoundObject* a_obj)
	{
		return RepairCap(a_obj, Smithing());
	}

	float RepairCap(const RE::TESBoundObject* a_obj, float a_skill)
	{
		const float skillMax = std::max(1.0f, Settings::Get(G::RepairCapSkillMax));
		const float t = std::pow(std::clamp(a_skill / skillMax, 0.0f, 1.0f),
								 std::max(0.01f, Settings::Get(G::RepairCapCurve)));
		const float lo = Settings::Get(G::RepairCapAtSkill0);
		const float hi = CapAtFullSkill(a_obj) - Settings::Get(G::RepairCapBonus);
		return std::max(Settings::Get(G::QualityMin), lo + (hi - lo) * t + Settings::Get(G::RepairCapBonus));
	}

	float StrikeResult(float a_q, float a_cap)
	{
		if (a_q >= a_cap) {
			return a_q;  // never lowers an item the smithing menu pushed past our cap
		}
		const float step = std::max(0.0f, (Settings::Get(G::RepairStepBase) +
										   Settings::Get(G::RepairStepPerSkill) * Smithing()) *
											  Settings::Get(G::RepairStepMult));
		const float improve = std::max(0.0f, Settings::Get(G::ImproveStepMult));
		float q = a_q;
		if (q < 1.0f) {
			const float toBase = 1.0f - q;
			if (step <= toBase) {
				q += step;
			} else {
				q = 1.0f + (step - toBase) * improve;  // leftover of the strike at the improve rate
			}
		} else {
			q += step * improve;
		}
		// a sliver left below the limit (under a quarter of the next strike)
		// is folded into this one, so no strike ever buys a fraction of a percent
		const float nextStep = step * (q >= 1.0f ? improve : 1.0f);
		if (a_cap - q < 0.25f * nextStep) {
			q = a_cap;
		}
		return std::min(q, a_cap);
	}

	int HammerIndex(const RE::TESForm* a_form)
	{
		const auto& hammers = Settings::Hammers();
		for (std::size_t i = 0; i < hammers.size(); ++i) {
			if (hammers[i].form == a_form) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool Unbreakable(int a_hammer)
	{
		const auto& hammers = Settings::Hammers();
		return a_hammer >= 0 && a_hammer < static_cast<int>(hammers.size()) && Settings::At(hammers[a_hammer].breakChance) <= 0.0f;
	}

	float BreakChance(int a_hammer, const RE::TESBoundObject* a_obj)
	{
		const auto& hammers = Settings::Hammers();
		if (a_hammer < 0 || a_hammer >= static_cast<int>(hammers.size())) {
			return 1.0f;
		}
		const auto& h = hammers[a_hammer];
		const float base = Settings::At(h.breakChance);
		if (base <= 0.0f) {
			return 0.0f;  // the unique hammer
		}
		const float over = a_obj ? std::max(0.0f, Hardness(a_obj) - Settings::At(h.tier)) : 0.0f;
		const float skillMax = std::max(1.0f, Settings::Get(G::RepairCapSkillMax));
		const float skill = 1.0f - Settings::Get(G::HammerSkillReduction) * std::clamp(Smithing() / skillMax, 0.0f, 1.0f);
		const float chance = base * (1.0f + Settings::Get(G::HammerOverTierMult) * over) * skill * Settings::Get(G::HammerBreakMult);
		return std::clamp(std::max(chance, Settings::Get(G::HammerBreakMin)), 0.0f, 1.0f);
	}

	std::int32_t Owned(int a_hammer)
	{
		const auto& hammers = Settings::Hammers();
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_hammer < 0 || a_hammer >= static_cast<int>(hammers.size())) {
			return 0;
		}
		return std::max(0, player->GetItemCount(hammers[a_hammer].form));
	}

	RE::ExtraDataList* FindWornList(const RE::TESBoundObject* a_obj, bool a_leftHand)
	{
		RE::ExtraDataList* found = nullptr;
		ForEachLiveList(a_obj, [&](RE::TESBoundObject*, RE::ExtraDataList* a_list) {
			if (a_leftHand ? a_list->HasType<RE::ExtraWornLeft>() : a_list->HasType<RE::ExtraWorn>()) {
				found = a_list;
				return true;
			}
			return false;
		});
		return found;
	}

	bool ListIsLive(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list)
	{
		bool live = false;
		ForEachLiveList(a_obj, [&](RE::TESBoundObject*, RE::ExtraDataList* a_candidate) {
			live = a_candidate == a_list;
			return live;
		});
		return live;
	}

	std::vector<WornPiece> WornArmor()
	{
		std::vector<WornPiece> out;
		ForEachLiveList(nullptr, [&](RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list) {
			const auto kind = Classify(a_obj);
			if ((kind == Kind::kArmor || kind == Kind::kShield) && a_list->HasType<RE::ExtraWorn>()) {
				out.push_back({ a_obj, a_list, kind });
			}
			return false;
		});
		return out;
	}

	bool IsEnchanted(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list)
	{
		if (a_list && a_list->HasType<RE::ExtraEnchantment>()) {
			return true;
		}
		auto ench = a_obj ? a_obj->As<RE::TESEnchantableForm>() : nullptr;
		return ench && ench->formEnchanting;
	}

	bool PlayerMayRepair(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list)
	{
		return !IsEnchanted(a_obj, a_list) || Smithing() >= Settings::Get(G::EnchantRepairSkill);
	}

	float VendorSkill(RE::Actor* a_vendor)
	{
		const float own = a_vendor ? a_vendor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing) : 0.0f;
		return std::clamp(own + Settings::Get(G::VendorSkillBonus), 0.0f, 100.0f);
	}

	std::int32_t PlayerGold()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		auto* gold = Resources::gold;
		return player && gold ? std::max(0, player->GetItemCount(gold)) : 0;
	}

	std::int32_t VendorCost(const RE::TESBoundObject* a_obj, bool a_enchanted, float a_from, float a_to, float a_vendorSkill)
	{
		if (a_to <= a_from + 1e-4f) {
			return 0;
		}
		const float repairPts = std::max(0.0f, std::min(a_to, 1.0f) - a_from) * 100.0f;
		const float improvePts = std::max(0.0f, a_to - std::max(a_from, 1.0f)) * 100.0f;
		float gold = (repairPts + improvePts * Settings::Get(G::VendorImproveCostMult)) * Settings::Get(G::VendorGoldPerPoint);
		gold *= 1.0f + Settings::Get(G::VendorHardnessCostMult) * std::max(0.0f, Hardness(a_obj) - 1.0f);
		gold *= 1.0f + Settings::Get(G::VendorSkillCostMult) * a_vendorSkill / 100.0f;
		if (a_enchanted) {
			gold *= Settings::Get(G::VendorEnchantedCostMult);
		}
		gold *= Settings::Get(G::VendorPriceMult);
		return std::max(static_cast<std::int32_t>(std::ceil(gold)), static_cast<std::int32_t>(Settings::Get(G::VendorMinCost)));
	}

	namespace
	{
		// While a weapon is equipped, the engine keeps its live charge on the AI process's own
		// entry (39806: middleHigh +0x220 left / +0x260 right), not on the inventory copy;
		// the hand's charge actor value is rebuilt from that entry.
		RE::AIProcess* PlayerProcess()
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			return player ? player->GetActorRuntimeData().currentProcess : nullptr;
		}

		RE::InventoryEntryData* EquippedWeaponEntry(bool a_left)
		{
			auto proc = PlayerProcess();
			if (!proc) {
				return nullptr;
			}
			// AE id 39806 is CommonLib's AIProcess::GetCurrentWeapon (SE 38781), so it holds on every runtime
			return proc->GetCurrentWeapon(a_left);
		}

		// the ExtraCharge on the process's entry for this equipped copy, if any
		RE::ExtraCharge* ProcessCharge(const RE::TESBoundObject* a_obj, const RE::ExtraDataList* a_list)
		{
			if (!a_list || !(a_list->HasType<RE::ExtraWorn>() || a_list->HasType<RE::ExtraWornLeft>())) {
				return nullptr;
			}
			auto entry = EquippedWeaponEntry(a_list->HasType<RE::ExtraWornLeft>());
			if (!entry || entry->object != a_obj || !entry->extraLists) {
				return nullptr;
			}
			for (auto list : *entry->extraLists) {
				if (list) {
					if (auto xc = list->GetByType<RE::ExtraCharge>()) {
						return xc;
					}
				}
			}
			return nullptr;
		}

		// which hand's charge actor value tracks this list, if it is equipped
		std::optional<RE::ActorValue> HandCharge(const RE::ExtraDataList* a_list)
		{
			if (!a_list) {
				return std::nullopt;
			}
			if (a_list->HasType<RE::ExtraWornLeft>()) {
				return RE::ActorValue::kLeftItemCharge;
			}
			if (a_list->HasType<RE::ExtraWorn>()) {
				return RE::ActorValue::kRightItemCharge;
			}
			return std::nullopt;
		}
	}

	Charge ChargeOf(const RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
	{
		Charge c;
		auto weap = a_obj ? a_obj->As<RE::TESObjectWEAP>() : nullptr;
		if (!weap || weap->IsBound()) {
			return c;
		}
		if (a_list) {
			if (auto xe = a_list->GetByType<RE::ExtraEnchantment>(); xe && xe->enchantment && xe->charge > 0) {
				c.enchantment = xe->enchantment;
				c.max = static_cast<float>(xe->charge);
			}
		}
		if (!c.enchantment && weap->formEnchanting && weap->amountofEnchantment > 0) {
			c.enchantment = weap->formEnchanting;
			c.max = static_cast<float>(weap->amountofEnchantment);
		}
		if (!c.enchantment || c.max <= 0.0f) {
			return c;
		}
		c.valid = true;
		c.current = c.max;
		if (a_list) {
			if (auto xc = a_list->GetByType<RE::ExtraCharge>()) {
				c.current = xc->charge;
			}
			// Equipped: the hand's charge actor value is the live charge (casting drains it,
			// the item card reads it via 51897 / 34609). ExtraCharge is synced from it only on
			// inventory open or unequip, so it can be stale after a load. The process entry
			// is the fallback while the hand value is not set up (base 0).
			bool fromHand = false;
			if (auto av = HandCharge(a_list)) {
				auto owner = RE::PlayerCharacter::GetSingleton()->AsActorValueOwner();
				if (owner->GetBaseActorValue(*av) > 0.0f) {
					c.current = owner->GetActorValue(*av);
					fromHand = true;
				}
			}
			if (!fromHand) {
				if (auto pc = ProcessCharge(a_obj, a_list)) {
					c.current = pc->charge;
				}
			}
		}
		c.current = std::clamp(c.current, 0.0f, c.max);
		return c;
	}

	void FillCharge(const RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
	{
		const auto c = ChargeOf(a_obj, a_list);
		if (!c.valid || !a_list) {
			return;  // no list = no ExtraCharge = already full
		}
		if (auto xc = a_list->GetByType<RE::ExtraCharge>()) {
			xc->charge = c.max;
		}
		if (auto pc = ProcessCharge(a_obj, a_list)) {
			pc->charge = c.max;  // the live copy the engine rebuilds the hand value from
		}
		if (auto av = HandCharge(a_list)) {
			// Put the hand's charge value right: base = the TRUE maximum (the
			// engine set it to the charge at equip time, which capped every
			// refill at that level), then clear the damage the weapon has used.
			auto owner = RE::PlayerCharacter::GetSingleton()->AsActorValueOwner();
			const float before = owner->GetActorValue(*av);
			const float baseBefore = owner->GetBaseActorValue(*av);
			owner->SetBaseActorValue(*av, c.max);
			const float missing = c.max - owner->GetActorValue(*av);
			if (missing > 0.0f) {
				// vfunc 06 (named RestoreActorValue in newer CommonLib builds)
				owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, *av, missing);
			}
			Log::INFO("recharge: equipped '{}' hand charge {:.1f} (base {:.1f}) -> {:.1f} (base {:.1f}), max {:.0f}",
						 a_obj->GetName(), before, baseBefore, owner->GetActorValue(*av), owner->GetBaseActorValue(*av), c.max);
		}
	}

	std::int32_t RechargeCost(float a_missing, float a_vendorEnchanting)
	{
		if (a_missing <= 0.5f) {
			return 0;
		}
		float gold = a_missing * Settings::Get(G::RechargeGoldPerCharge);
		gold *= 1.0f + Settings::Get(G::RechargeSkillCostMult) * a_vendorEnchanting / 100.0f;
		gold *= Settings::Get(G::RechargePriceMult);
		return std::max(static_cast<std::int32_t>(std::ceil(gold)), static_cast<std::int32_t>(Settings::Get(G::RechargeMinCost)));
	}

	bool IsRepairTool(const RE::TESForm* a_form)
	{
		auto tools = Settings::RepairTools();
		return a_form && tools && tools->HasForm(a_form);
	}

	std::int32_t CountRepairTools(RE::TESBoundObject** a_firstOwned)
	{
		auto tools = Settings::RepairTools();
		auto player = RE::PlayerCharacter::GetSingleton();
		if (a_firstOwned) {
			*a_firstOwned = nullptr;
		}
		if (!tools || !player) {
			return 0;
		}
		std::int32_t total = 0;
		auto inv = player->GetInventory([&](RE::TESBoundObject& a_obj) { return tools->HasForm(&a_obj); });
		for (auto& [obj, data] : inv) {
			if (data.first > 0) {
				total += data.first;
				if (a_firstOwned && !*a_firstOwned) {
					*a_firstOwned = obj;
				}
			}
		}
		return total;
	}

	float EngineStat(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_obj) {
			return 0.0f;
		}
		// A throwaway entry holding just this instance's list. The list and the
		// entry never own anything: the pointer is detached before either dies.
		// ~InventoryEntryData deletes extraLists, so the guard detaches the
		// stack list on every exit path.
		RE::BSSimpleList<RE::ExtraDataList*> lists;
		RE::InventoryEntryData                entry(a_obj, 1);
		struct Detach
		{
			RE::InventoryEntryData& e;
			~Detach()
			{
				e.extraLists = nullptr;
				e.object = nullptr;
			}
		} detach{ entry };
		if (a_list) {
			lists.push_front(a_list);
			entry.extraLists = std::addressof(lists);
		}
		return Classify(a_obj) == Kind::kWeapon ? player->GetDamage(std::addressof(entry)) :
												  player->GetArmorValue(std::addressof(entry));
	}

	float EngineStatAt(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_q)
	{
		if (!a_list) {
			return -1.0f;
		}
		auto h = a_list->GetByType<RE::ExtraHealth>();
		const bool temporary = h == nullptr;
		if (temporary) {
			// probe only: added for the measurement, removed again right after,
			// so opening the menu never changes an untouched item's extra data
			h = new RE::ExtraHealth(a_q);
			a_list->Add(h);
		}
		const float saved = h->health;
		h->health = a_q;
		const float value = EngineStat(a_obj, a_list);
		if (temporary) {
			a_list->Remove(h);  // not RemoveByType: it can dereference null on a one-entry list
			delete h;
		} else {
			h->health = saved;
		}
		return value;
	}

	float EffectiveStat(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
	{
		return EngineStat(a_obj, a_list);
	}

	float EffectiveStatAt(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_q)
	{
		return EngineStatAt(a_obj, a_list, a_q);
	}

	std::mt19937& Rng()
	{
		static std::mt19937 rng{ std::random_device{}() };
		return rng;
	}

	float Roll()
	{
		return std::uniform_real_distribution<float>(0.0f, 1.0f)(Rng());
	}
}
