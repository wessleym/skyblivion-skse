#include "Degradation/RepairMenu.h"

#include "Platform/FormCache.h"
#include "Degradation/Condition.h"
#include "Degradation/Settings.h"
#include "Platform/MainThread.h"
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
#include "UIs/Scaleform/ScaleformUtility.h"

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace SKYBDeg
{
	using Condition::Kind;

	namespace
	{
		// ---------------------------------------------------------------- rows --
		// Built on the main thread from the LIVE inventory; the movie only ever
		// sees an index into the table that was current at its last push.
		struct Row
		{
			RE::TESBoundObject* obj{ nullptr };
			RE::ExtraDataList*  list{ nullptr };  // nullptr = plain (untouched) copies
			std::int32_t        count{ 0 };
			Kind                kind{ Kind::kNone };
			bool                worn{ false };
			bool                enchanted{ false };
			float               q{ 1.0f };
			std::string         name;
			float               chargeCur{ 0 }, chargeMax{ 0 };  // recharge mode
			std::string         enchantName;                     // recharge mode
		};

		struct PushRow
		{
			std::string name, material, kind;
			bool        worn{ false }, canStrike{ false }, atCap{ false };
			std::int32_t count{ 0 };
			float       pct{ 0 }, nextPct{ 0 }, stat{ 0 }, nextStat{ -1 }, decay{ 0 };
			float       capPct{ 0 }, effPct{ 100 }, nextEffPct{ 100 }, breakPct{ -1 };
			bool        locked{ false };                     // player: enchanted, Smithing too low
			bool        canRepair{ false }, canLimit{ false };  // vendor: to 100% / to the vendor's limit
			std::int32_t costRepair{ 0 }, costLimit{ 0 };    // vendor: gold for each
			float       chargeCur{ 0 }, chargeMax{ 0 };      // recharge mode
		};

		struct PushHeader
		{
			std::int32_t hammers{ 0 };        // all hammers carried
			std::int32_t generation{ 0 };
			std::string  hammerName;          // the hammer in hand ("" = none)
			std::int32_t hammerCount{ 0 };
			std::int32_t hammerKinds{ 0 };    // hammer types carried (switching needs 2+)
			bool         unbreakable{ false };
			float        skill{ 0 }, floorPct{ 0 }, scalePct{ 180 }, statFloorPct{ 60 };
			bool         gamepad{ false };
			std::string  mode{ "player" };    // "player" | "vendor"
			std::string  vendorName;
			float        vendorSkill{ 0 };
			std::int32_t gold{ 0 };
			float        enchantSkill{ 50 };  // Smithing the player needs for enchanted gear
			std::int32_t totalCost{ 0 };      // recharge mode: Recharge All
		};

		struct StatusLine
		{
			std::string              key;
			std::vector<std::string> args;
		};

		std::vector<Row> g_rows;           // main thread only
		std::int32_t     g_generation = 0; // bumped per table; the movie echoes it with every strike
		bool             g_fromInventory = false;
		int              g_hammer = -1;         // index into Settings::Hammers(): the hammer in hand
		int              g_pendingHammer = -1;  // the hammer that opened the menu
		bool             g_vendorMode = false;  // opened from a vendor's Repair or Recharge service
		bool             g_recharge = false;    // vendor mode: the Recharge service (else Repair)
		RE::ActorHandle  g_vendor;

		RE::Actor* Vendor() { return g_vendorMode ? g_vendor.get().get() : nullptr; }

		// the carried hammer that breaks least (an unbreakable one first), or -1
		int BestOwned()
		{
			int best = -1;
			float bestBase = 2.0f;
			const auto& hammers = Settings::Hammers();
			for (int i = 0; i < static_cast<int>(hammers.size()); ++i) {
				const float base = Settings::At(hammers[i].breakChance);
				if (Condition::Owned(i) > 0 && base < bestBase) {
					best = i;
					bestBase = base;
				}
			}
			return best;
		}

		// keep the hammer in hand valid: still carried, else the best carried one
		void EnsureHammer()
		{
			if (g_hammer < 0 || Condition::Owned(g_hammer) <= 0) {
				g_hammer = BestOwned();
			}
		}

		void CycleHammer(int a_dir)
		{
			const int n = static_cast<int>(Settings::Hammers().size());
			if (n == 0) {
				return;
			}
			int i = g_hammer < 0 ? 0 : g_hammer;
			for (int step = 0; step < n; ++step) {
				i = ((i + a_dir) % n + n) % n;
				if (Condition::Owned(i) > 0) {
					g_hammer = i;
					return;
				}
			}
		}

		std::string Pct(float a_q) { return std::format("{:.0f}%", a_q * 100.0f); }

		int KindOrder(Kind a_kind)
		{
			switch (a_kind) {
				case Kind::kWeapon: return 0;
				case Kind::kShield: return 1;
				default:            return 2;
			}
		}

		std::vector<Row> Census()
		{
			std::vector<Row> rows;
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return rows;
			}
			auto inv = player->GetInventory(
				[](RE::TESBoundObject& a_obj) { return Condition::Classify(&a_obj) != Kind::kNone; });
			auto changes = player->GetInventoryChanges();
			for (auto& [obj, data] : inv) {
				const std::int32_t total = data.first;
				if (total <= 0) {
					continue;
				}
				const auto kind = Condition::Classify(obj);
				std::int32_t listed = 0;
				if (changes && changes->entryList) {
					for (auto entry : *changes->entryList) {
						if (!entry || entry->object != obj || !entry->extraLists) {
							continue;
						}
						for (auto list : *entry->extraLists) {
							if (!list) {
								continue;
							}
							Row r;
							r.obj = obj;
							r.list = list;
							r.count = std::max(1, list->GetCount());
							r.kind = kind;
							r.worn = list->HasType<RE::ExtraWorn>() || list->HasType<RE::ExtraWornLeft>();
							r.q = Condition::GetHealth(list);
							const char* name = list->GetDisplayName(obj);
							r.name = name && *name ? name : obj->GetName();
							r.enchanted = Condition::IsEnchanted(obj, list);
							listed += r.count;
							rows.push_back(std::move(r));
						}
					}
				}
				if (total > listed) {
					Row r;
					r.obj = obj;
					r.count = total - listed;
					r.kind = kind;
					r.q = 1.0f;
					r.enchanted = Condition::IsEnchanted(obj, nullptr);
					r.name = obj->GetName();
					rows.push_back(std::move(r));
				}
			}
			std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
				if (a.worn != b.worn) return a.worn;
				if (KindOrder(a.kind) != KindOrder(b.kind)) return KindOrder(a.kind) < KindOrder(b.kind);
				if (std::abs(a.q - b.q) > 1e-4f) return a.q < b.q;
				return a.name < b.name;
			});
			return rows;
		}

		// Recharge mode: every weapon or staff carried that holds a charge (full ones
		// too, so the player sees them), equipped first, emptiest first.
		std::vector<Row> RechargeCensus()
		{
			std::vector<Row> rows;
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return rows;
			}
			auto inv = player->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::Weapon); });
			auto changes = player->GetInventoryChanges();
			auto add = [&](RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, std::int32_t a_count) {
				const auto c = Condition::ChargeOf(a_obj, a_list);
				if (!c.valid) {
					return;
				}
				Row r;
				r.obj = a_obj;
				r.list = a_list;
				r.count = a_count;
				r.kind = Kind::kWeapon;
				r.worn = a_list && (a_list->HasType<RE::ExtraWorn>() || a_list->HasType<RE::ExtraWornLeft>());
				r.enchanted = true;
				r.chargeCur = c.current;
				r.chargeMax = c.max;
				r.q = c.current / c.max;
				r.enchantName = c.enchantment ? c.enchantment->GetName() : "";
				const char* name = a_list ? a_list->GetDisplayName(a_obj) : nullptr;
				r.name = name && *name ? name : a_obj->GetName();
				rows.push_back(std::move(r));
			};
			for (auto& [obj, data] : inv) {
				const std::int32_t total = data.first;
				if (total <= 0) {
					continue;
				}
				std::int32_t listed = 0;
				if (changes && changes->entryList) {
					for (auto entry : *changes->entryList) {
						if (!entry || entry->object != obj || !entry->extraLists) {
							continue;
						}
						for (auto list : *entry->extraLists) {
							if (list) {
								const auto n = std::max(1, list->GetCount());
								listed += n;
								add(obj, list, n);
							}
						}
					}
				}
				if (total > listed) {
					add(obj, nullptr, total - listed);  // untouched copies: full
				}
			}
			std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
				if (a.worn != b.worn) return a.worn;
				if (std::abs(a.q - b.q) > 1e-4f) return a.q < b.q;
				return a.name < b.name;
			});
			return rows;
		}

		float VendorEnchanting(RE::Actor* a_vendor)
		{
			return a_vendor ? a_vendor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kEnchanting) : 0.0f;
		}

		PushRow DescribeRecharge(const Row& a_row, float a_vendorEnchanting)
		{
			PushRow p;
			p.name = a_row.count > 1 ? std::format("{} ({})", a_row.name, a_row.count) : a_row.name;
			p.material = a_row.enchantName;
			p.kind = a_row.obj->As<RE::TESObjectWEAP>() && a_row.obj->As<RE::TESObjectWEAP>()->IsStaff() ? "staff" : "weapon";
			p.worn = a_row.worn;
			p.count = a_row.count;
			p.pct = a_row.q * 100.0f;
			p.nextPct = 100.0f;
			p.capPct = 100.0f;
			p.chargeCur = a_row.chargeCur;
			p.chargeMax = a_row.chargeMax;
			const float missing = (a_row.chargeMax - a_row.chargeCur) * static_cast<float>(a_row.count);
			p.canRepair = a_row.list && missing > 0.5f;
			p.canStrike = p.canRepair;
			p.costRepair = p.canRepair ? Condition::RechargeCost(missing, a_vendorEnchanting) : 0;
			p.atCap = !p.canRepair;
			return p;
		}

		PushRow DescribeVendor(const Row& a_row, float a_vendorSkill);

		PushRow Describe(const Row& a_row)
		{
			if (g_vendorMode && g_recharge) {
				return DescribeRecharge(a_row, VendorEnchanting(Vendor()));
			}
			if (g_vendorMode) {
				return DescribeVendor(a_row, Condition::VendorSkill(Vendor()));
			}
			const float a_cap = Condition::RepairCap(a_row.obj);
			PushRow p;
			p.locked = !Condition::PlayerMayRepair(a_row.obj, a_row.list);
			p.capPct = a_cap * 100.0f;
			p.name = a_row.count > 1 ? std::format("{} ({})", a_row.name, a_row.count) : a_row.name;
			p.material = Condition::MaterialOf(a_row.obj).name;
			p.kind = a_row.kind == Kind::kWeapon ? "weapon" : a_row.kind == Kind::kShield ? "shield" : "armor";
			p.worn = a_row.worn;
			p.count = a_row.count;
			p.pct = a_row.q * 100.0f;
			const float nq = Condition::StrikeResult(a_row.q, a_cap);
			p.nextPct = nq * 100.0f;
			p.canStrike = nq > a_row.q + 1e-4f;
			p.atCap = a_row.q >= a_cap - 1e-4f;
			// engine number (temper included, our perk suspended) x condition effectiveness
			const float eff = Condition::StatMult(a_row.q);
			const float nextEff = Condition::StatMult(nq);
			p.effPct = eff * 100.0f;
			p.nextEffPct = nextEff * 100.0f;
			p.stat = Condition::EffectiveStat(a_row.obj, a_row.list);
			if (p.canStrike && a_row.list && a_row.count == 1) {
				p.nextStat = Condition::EffectiveStatAt(a_row.obj, a_row.list, nq);
			}
			const float catMult = a_row.kind == Kind::kWeapon ? Settings::Get(G::WeapRateMult) : Settings::Get(G::ArmorRateMult);
			p.decay = Condition::MaterialMult(a_row.obj) * Condition::CurveMult(a_row.q) * Settings::Get(G::RateMult) * catMult *
					  Condition::WearSkillMult();
			p.breakPct = g_hammer >= 0 ? Condition::BreakChance(g_hammer, a_row.obj) * 100.0f : -1.0f;
			return p;
		}

		// Vendor mode: no strikes, no hammers - a price to 100% and to the
		// vendor's own limit (their Smithing on this item's material).
		PushRow DescribeVendor(const Row& a_row, float a_vendorSkill)
		{
			const float vcap = Condition::RepairCap(a_row.obj, a_vendorSkill);
			PushRow p;
			p.name = a_row.count > 1 ? std::format("{} ({})", a_row.name, a_row.count) : a_row.name;
			p.material = Condition::MaterialOf(a_row.obj).name;
			p.kind = a_row.kind == Kind::kWeapon ? "weapon" : a_row.kind == Kind::kShield ? "shield" : "armor";
			p.worn = a_row.worn;
			p.count = a_row.count;
			p.pct = a_row.q * 100.0f;
			p.capPct = vcap * 100.0f;
			p.canRepair = a_row.q < 1.0f - 1e-4f;
			p.canLimit = a_row.q < vcap - 1e-4f;
			p.costRepair = p.canRepair ? Condition::VendorCost(a_row.obj, a_row.enchanted, a_row.q, 1.0f, a_vendorSkill) : 0;
			p.costLimit = p.canLimit ? Condition::VendorCost(a_row.obj, a_row.enchanted, a_row.q, vcap, a_vendorSkill) : 0;
			const float nq = p.canLimit ? vcap : a_row.q;
			p.nextPct = nq * 100.0f;
			p.canStrike = p.canRepair || p.canLimit;
			p.atCap = !p.canLimit;
			p.effPct = Condition::StatMult(a_row.q) * 100.0f;
			p.nextEffPct = Condition::StatMult(nq) * 100.0f;
			p.stat = Condition::EffectiveStat(a_row.obj, a_row.list);
			if (p.canLimit && a_row.list && a_row.count == 1) {
				p.nextStat = Condition::EffectiveStatAt(a_row.obj, a_row.list, nq);
			}
			const float catMult = a_row.kind == Kind::kWeapon ? Settings::Get(G::WeapRateMult) : Settings::Get(G::ArmorRateMult);
			p.decay = Condition::MaterialMult(a_row.obj) * Condition::CurveMult(a_row.q) * Settings::Get(G::RateMult) * catMult *
					  Condition::WearSkillMult();
			return p;
		}

		// ------------------------------------------------------------ push -----
		void Push(PushHeader a_header, std::vector<PushRow> a_rows, std::int32_t a_select, std::vector<StatusLine> a_status)
		{
			//Into the movie on the main thread, next frame.
			MainThread::Post([header = std::move(a_header), rows = std::move(a_rows), a_select,
												 status = std::move(a_status)]() {
				auto ui = RE::UI::GetSingleton();
				auto menu = ui ? ui->GetMenu(RepairMenu::MENU_NAME) : nullptr;
				auto movie = menu ? menu->uiMovie : nullptr;
				if (!movie) {
					return;
				}
				RE::GFxValue h;
				movie->CreateObject(&h);
				h.SetMember("hammers", header.hammers);
				h.SetMember("generation", header.generation);
				h.SetMember("skill", static_cast<double>(header.skill));
				h.SetMember("hammerName", header.hammerName.c_str());
				h.SetMember("hammerCount", header.hammerCount);
				h.SetMember("hammerKinds", header.hammerKinds);
				h.SetMember("unbreakable", header.unbreakable);
				h.SetMember("mode", header.mode.c_str());
				h.SetMember("vendorName", header.vendorName.c_str());
				h.SetMember("vendorSkill", static_cast<double>(header.vendorSkill));
				h.SetMember("gold", header.gold);
				h.SetMember("enchantSkill", static_cast<double>(header.enchantSkill));
				h.SetMember("totalCost", header.totalCost);
				h.SetMember("floorPct", static_cast<double>(header.floorPct));
				h.SetMember("scalePct", static_cast<double>(header.scalePct));
				h.SetMember("statFloorPct", static_cast<double>(header.statFloorPct));
				h.SetMember("gamepad", header.gamepad);

				RE::GFxValue arr;
				movie->CreateArray(&arr);
				for (const auto& r : rows) {
					RE::GFxValue o;
					movie->CreateObject(&o);
					o.SetMember("name", r.name.c_str());
					o.SetMember("material", r.material.c_str());
					o.SetMember("kind", r.kind.c_str());
					o.SetMember("worn", r.worn);
					o.SetMember("count", r.count);
					o.SetMember("pct", static_cast<double>(r.pct));
					o.SetMember("nextPct", static_cast<double>(r.nextPct));
					o.SetMember("canStrike", r.canStrike);
					o.SetMember("atCap", r.atCap);
					o.SetMember("stat", static_cast<double>(r.stat));
					o.SetMember("nextStat", static_cast<double>(r.nextStat));
					o.SetMember("decay", static_cast<double>(r.decay));
					o.SetMember("capPct", static_cast<double>(r.capPct));
					o.SetMember("effPct", static_cast<double>(r.effPct));
					o.SetMember("nextEffPct", static_cast<double>(r.nextEffPct));
					o.SetMember("breakPct", static_cast<double>(r.breakPct));
					o.SetMember("locked", r.locked);
					o.SetMember("canRepair", r.canRepair);
					o.SetMember("canLimit", r.canLimit);
					o.SetMember("costRepair", r.costRepair);
					o.SetMember("costLimit", r.costLimit);
					o.SetMember("chargeCur", static_cast<double>(r.chargeCur));
					o.SetMember("chargeMax", static_cast<double>(r.chargeMax));
					arr.PushBack(o);
				}
				RE::GFxValue args[3] = { h, arr, RE::GFxValue(a_select) };
				movie->Invoke("_root.gearService.SetRepairData", nullptr, args, 3);

				{
					RE::GFxValue lines;
					movie->CreateArray(&lines);
					for (const auto& s : status) {
						RE::GFxValue line, largs;
						movie->CreateObject(&line);
						movie->CreateArray(&largs);
						line.SetMember("key", s.key.c_str());
						for (const auto& a : s.args) {
							largs.PushBack(RE::GFxValue(a.c_str()));
						}
						line.SetMember("args", largs);
						lines.PushBack(line);
					}
					RE::GFxValue sargs[1] = { lines };
					movie->Invoke("_root.gearService.SetStatus", nullptr, sargs, 1);
				}
			});
		}

		// Main thread: rebuild the table and send it. a_prefer picks the row to
		// keep selected (same base object, closest condition, worn first).
		void Refresh(std::vector<StatusLine> a_status, const RE::TESBoundObject* a_preferObj = nullptr,
					 float a_preferQ = 0.0f, bool a_preferWorn = false, std::int32_t a_fallbackSel = 0)
		{
			g_rows = g_vendorMode && g_recharge ? RechargeCensus() : Census();
			++g_generation;
			EnsureHammer();

			PushHeader header;
			header.hammers = Condition::CountRepairTools();
			header.generation = g_generation;
			header.skill = Condition::Smithing();
			if (g_hammer >= 0) {
				header.hammerName = Settings::Hammers()[g_hammer].form->GetName();
				header.hammerCount = Condition::Owned(g_hammer);
				header.unbreakable = Condition::Unbreakable(g_hammer);
			}
			for (int i = 0; i < static_cast<int>(Settings::Hammers().size()); ++i) {
				header.hammerKinds += Condition::Owned(i) > 0 ? 1 : 0;
			}
			header.enchantSkill = Settings::Get(G::EnchantRepairSkill);
			header.gold = Condition::PlayerGold();
			if (auto vendor = Vendor()) {
				header.mode = "vendor";
				header.vendorName = vendor->GetDisplayFullName() ? vendor->GetDisplayFullName() : vendor->GetName();
				header.vendorSkill = Condition::VendorSkill(vendor);
			}
			header.floorPct = Settings::Get(G::QualityMin) * 100.0f;
			header.statFloorPct = Settings::Get(G::StatFloor) * 100.0f;
			// bar scale: the best limit any listed item can reach at full skill (or an
			// item the smithing menu pushed higher), so bars stay comparable as skill grows
			float scale = 1.0f;
			for (const auto& r : g_rows) {
				scale = std::max({ scale, Condition::CapAtFullSkill(r.obj), r.q });
			}
			// (the vendor's limit never exceeds the material cap at full skill)
			if (g_vendorMode && g_recharge) {
				header.mode = "recharge";
				header.vendorSkill = VendorEnchanting(Vendor());
				scale = 1.0f;
				header.floorPct = 0.0f;
				header.statFloorPct = 0.0f;
				for (const auto& r : g_rows) {
					const float missing = (r.chargeMax - r.chargeCur) * static_cast<float>(r.count);
					if (r.list && missing > 0.5f) {
						header.totalCost += Condition::RechargeCost(missing, header.vendorSkill);
					}
				}
			}
			header.scalePct = scale * 100.0f;
			auto input = RE::BSInputDeviceManager::GetSingleton();
			header.gamepad = input && input->IsGamepadEnabled();

			std::vector<PushRow> rows;
			rows.reserve(g_rows.size());
			std::int32_t select = std::clamp<std::int32_t>(a_fallbackSel, 0, std::max<std::int32_t>(0, static_cast<std::int32_t>(g_rows.size()) - 1));
			float best = 1e9f;
			for (std::size_t i = 0; i < g_rows.size(); ++i) {
				const auto& r = g_rows[i];
				rows.push_back(Describe(r));
				if (a_preferObj && r.obj == a_preferObj) {
					const float score = std::abs(r.q - a_preferQ) + (r.worn == a_preferWorn ? 0.0f : 10.0f);
					if (score < best) {
						best = score;
						select = static_cast<std::int32_t>(i);
					}
				}
			}
			Push(std::move(header), std::move(rows), g_rows.empty() ? -1 : select, std::move(a_status));
		}

		// Main thread: the drop / re-pick trick for copies that have no list of
		// their own (plain) or share one (count > 1). The engine builds the new
		// instance's list; we only edit it. Never used on worn items (their
		// list is always a count-1 list edited in place).
		bool SplitAndSet(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_expectQ, float a_q)
		{
			auto player = RE::PlayerCharacter::GetSingleton();
			RE::NiPoint3 at = player->GetPosition();
			at.z -= 2000.0f;
			auto handle = player->RemoveItem(a_obj, 1, RE::ITEM_REMOVE_REASON::kDropping, a_list, nullptr, &at);
			auto ref = handle.get();
			if (!ref) {
				Log::ERROR("repair: drop of '{}' returned no reference", a_obj->GetName());
				return false;
			}
			// The engine chose which copy to drop. If it is not the copy the row
			// describes (a tempered / enchanted / worn one instead of a plain
			// copy), hand it back untouched rather than overwrite its condition.
			auto& xl = ref->extraList;
			const float got = Condition::GetHealth(std::addressof(xl));
			const bool wrongCopy = std::abs(got - a_expectQ) > 1e-3f || xl.HasType<RE::ExtraWorn>() ||
								   xl.HasType<RE::ExtraWornLeft>() ||
								   (!a_list && xl.HasType<RE::ExtraEnchantment>());
			if (wrongCopy) {
				Log::WARN("repair: engine dropped a different copy of '{}' ({:.1f}%, expected {:.1f}%); returned unchanged",
							 a_obj->GetName(), got * 100.0f, a_expectQ * 100.0f);
				player->PickUpObject(ref.get(), 1, false, false);
				return false;
			}
			Condition::SetHealth(std::addressof(xl), a_q);
			player->PickUpObject(ref.get(), 1, false, false);
			return true;
		}

		// Recharge (vendor service): the selected item, or every item that is not
		// full (Recharge All). The player pays; the vendor gets the gold.
		void DoRecharge(const Row& a_row, bool a_all, std::int32_t a_index)
		{
			auto vendor = Vendor();
			auto player = RE::PlayerCharacter::GetSingleton();
			auto goldForm = FormCache::Gold();
			if (!vendor || !player || !goldForm) {
				Refresh({ { "$SKYB_repair_status_failed", {} } }, a_row.obj, a_row.q, a_row.worn, a_index);
				return;
			}
			const float skill = VendorEnchanting(vendor);
			struct Job
			{
				const Row*   row;
				std::int32_t cost;
			};
			std::vector<Job> jobs;
			auto consider = [&](const Row& r) {
				if (!r.list || (r.list && !Condition::ListIsLive(r.obj, r.list))) {
					return;
				}
				const auto c = Condition::ChargeOf(r.obj, r.list);
				const float missing = (c.max - c.current) * static_cast<float>(r.count);
				if (c.valid && missing > 0.5f) {
					jobs.push_back({ &r, Condition::RechargeCost(missing, skill) });
				}
			};
			if (a_all) {
				for (const auto& r : g_rows) {
					consider(r);
				}
			} else {
				consider(g_rows[a_index]);
			}
			if (jobs.empty()) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { a_all ? "$SKYB_recharge_status_allfull" : "$SKYB_recharge_status_full", { a_row.name } } },
						a_row.obj, a_row.q, a_row.worn, a_index);
				return;
			}
			std::int32_t total = 0;
			for (const auto& j : jobs) {
				total += j.cost;
			}
			const auto gold = Condition::PlayerGold();
			if (gold < total) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { "$SKYB_repair_status_gold", { std::to_string(total), std::to_string(gold) } } }, a_row.obj,
						a_row.q, a_row.worn, a_index);
				return;
			}
			const std::string vendorName = vendor->GetDisplayFullName() ? vendor->GetDisplayFullName() : vendor->GetName();
			for (const auto& j : jobs) {
				const auto before = Condition::ChargeOf(j.row->obj, j.row->list);
				Condition::FillCharge(j.row->obj, j.row->list);
				const auto after = Condition::ChargeOf(j.row->obj, j.row->list);
				Log::INFO("recharge: {} recharged '{}' {:.0f} -> {:.0f} / {:.0f} for {} gold", vendorName, j.row->name,
							 before.current, after.current, after.max, j.cost);
			}
			player->RemoveItem(goldForm, total, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			vendor->AddObjectToContainer(goldForm, nullptr, total, player);
			RE::PlaySound("UIEnchantRecharge");
			std::vector<StatusLine> status;
			if (jobs.size() == 1) {
				status.push_back({ "$SKYB_recharge_status_done", { jobs[0].row->name, vendorName, std::to_string(total) } });
			} else {
				status.push_back({ "$SKYB_recharge_status_doneN", { std::to_string(jobs.size()), vendorName, std::to_string(total) } });
			}
			Refresh(std::move(status), a_row.obj, 1.0f, a_row.worn, a_index);
		}

		// A vendor restores the item in one go: to 100% (Repair) or to their own
		// limit (Improve). Unlimited hammers; the player pays; the vendor gets the gold.
		void DoVendorRepair(const Row& a_row, bool a_toLimit, std::int32_t a_index)
		{
			auto vendor = Vendor();
			auto player = RE::PlayerCharacter::GetSingleton();
			auto goldForm = FormCache::Gold();
			if (!vendor || !player || !goldForm) {
				Refresh({ { "$SKYB_repair_status_failed", {} } }, a_row.obj, a_row.q, a_row.worn, a_index);
				return;
			}
			const float skill = Condition::VendorSkill(vendor);
			const float vcap = Condition::RepairCap(a_row.obj, skill);
			const float q0 = Condition::GetHealth(a_row.list);
			const float target = a_toLimit ? vcap : 1.0f;
			if (target <= q0 + 1e-4f) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { a_toLimit ? "$SKYB_repair_status_vendor_limit" : "$SKYB_repair_status_nothing",
							{ a_row.name, Pct(vcap) } } }, a_row.obj, q0, a_row.worn, a_index);
				return;
			}
			const auto cost = Condition::VendorCost(a_row.obj, a_row.enchanted, q0, target, skill);
			const auto gold = Condition::PlayerGold();
			if (gold < cost) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { "$SKYB_repair_status_gold", { std::to_string(cost), std::to_string(gold) } } }, a_row.obj, q0,
						a_row.worn, a_index);
				return;
			}
			bool applied = true;
			if (a_row.list && a_row.count == 1) {
				Condition::SetHealth(a_row.list, target);
			} else {
				applied = SplitAndSet(a_row.obj, a_row.list, q0, target);
			}
			if (!applied) {
				Refresh({ { "$SKYB_repair_status_failed", {} } }, a_row.obj, q0, a_row.worn, a_index);
				return;
			}
			player->RemoveItem(goldForm, cost, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			vendor->AddObjectToContainer(goldForm, nullptr, cost, player);
			RE::PlaySound(a_row.kind == Kind::kWeapon ? "UISmithingImproveWeapon" : "UISmithingImproveArmor");
			const std::string vendorName = vendor->GetDisplayFullName() ? vendor->GetDisplayFullName() : vendor->GetName();
			Log::INFO("vendor repair: {} (Smithing {:.0f}) took '{}' {:.1f}% -> {:.1f}% for {} gold (hardness {:.1f}{})",
						 vendorName, skill, a_row.name, q0 * 100.0f, target * 100.0f, cost, Condition::Hardness(a_row.obj),
						 a_row.enchanted ? ", enchanted" : "");
			Refresh({ { "$SKYB_repair_status_vendor_done", { a_row.name, vendorName, Pct(target), std::to_string(cost) } } },
					a_row.obj, target, a_row.worn, a_index);
		}

		void DoStrike(std::int32_t a_row, bool a_toLimit, std::int32_t a_generation)
		{
			if (a_generation != g_generation || a_row < 0 || a_row >= static_cast<std::int32_t>(g_rows.size())) {
				// the movie acted on an older table (a push was still in flight)
				Refresh({ { "$SKYB_repair_status_stale", {} } }, nullptr, 0.0f, false, a_row);
				return;
			}
			const Row row = g_rows[a_row];
			if (row.list && !Condition::ListIsLive(row.obj, row.list)) {
				Refresh({ { "$SKYB_repair_status_stale", {} } });
				return;
			}
			if (g_vendorMode && g_recharge) {
				DoRecharge(row, a_toLimit, a_row);
				return;
			}
			if (g_vendorMode) {
				DoVendorRepair(row, a_toLimit, a_row);
				return;
			}
			if (!Condition::PlayerMayRepair(row.obj, row.list)) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { "$SKYB_repair_status_enchanted", { std::format("{:.0f}", Settings::Get(G::EnchantRepairSkill)) } } },
						row.obj, row.q, row.worn, a_row);
				return;
			}
			EnsureHammer();
			RE::TESBoundObject* tool = g_hammer >= 0 ? Settings::Hammers()[g_hammer].form : nullptr;
			if (!tool) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { "$SKYB_repair_status_nohammers", {} } }, row.obj, row.q, row.worn, a_row);
				return;
			}
			const float cap = Condition::RepairCap(row.obj);
			const float q0 = Condition::GetHealth(row.list);
			if (Condition::StrikeResult(q0, cap) <= q0 + 1e-4f) {
				RE::PlaySound("UIMenuCancel");
				Refresh({ { "$SKYB_repair_status_atcap", { row.name, Pct(cap) } } }, row.obj, q0, row.worn, a_row);
				return;
			}

			auto player = RE::PlayerCharacter::GetSingleton();
			const int used = g_hammer;
			const std::string hammerName = tool->GetName();
			const float chance = Condition::BreakChance(used, row.obj);
			float q = q0;
			std::int32_t strikes = 0, broke = 0;
			bool ranOut = false;
			while (true) {
				const float nq = Condition::StrikeResult(q, cap);
				if (nq <= q + 1e-4f) {
					break;
				}
				q = nq;
				++strikes;
				if (chance > 0.0f && Condition::Roll() < chance) {
					player->RemoveItem(tool, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					++broke;
					ranOut = Condition::Owned(used) <= 0;
				}
				if (!a_toLimit || ranOut || strikes >= 500) {
					break;
				}
			}

			bool applied = true;
			if (row.list && row.count == 1) {
				Condition::SetHealth(row.list, q);
			} else {
				applied = SplitAndSet(row.obj, row.list, q0, q);
			}
			if (!applied) {
				Refresh({ { "$SKYB_repair_status_failed", {} } }, row.obj, q0, row.worn, a_row);
				return;
			}

			player->AddSkillExperience(RE::ActorValue::kSmithing,
									   Settings::Get(G::RepairXPBase) * strikes + Settings::Get(G::RepairXPPerQuality) * (q - q0));
			RE::PlaySound(row.kind == Kind::kWeapon ? "UISmithingImproveWeapon" : "UISmithingImproveArmor");
			Log::INFO("repair: '{}' {:.1f}% -> {:.1f}% in {} strike(s) with '{}' ({:.1f}% break, hardness {:.1f}), {} broke, "
						 "cap {:.0f}%, skill {:.0f}",
						 row.name, q0 * 100.0f, q * 100.0f, strikes, hammerName, chance * 100.0f,
						 Condition::Hardness(row.obj), broke, cap * 100.0f, Condition::Smithing());

			std::vector<StatusLine> status{ { "$SKYB_repair_status_repaired", { row.name, Pct(q), std::to_string(strikes) } } };
			if (broke == 1) {
				status.push_back({ "$SKYB_repair_status_broke1", { hammerName } });
			} else if (broke > 1) {
				status.push_back({ "$SKYB_repair_status_brokeN", { hammerName, std::to_string(broke) } });
			}
			if (ranOut) {
				status.push_back({ "$SKYB_repair_status_ranout", { hammerName } });
			}
			Refresh(std::move(status), row.obj, q, row.worn, a_row);
		}
	}

	// ============================================================== menu =====
	RepairMenu::RepairMenu()
	{
		auto scaleform = RE::BSScaleformManager::GetSingleton();
		const bool loaded = scaleform->LoadMovieEx(this, FILE_NAME, [this](RE::GFxMovieDef* a_def) {
			fxDelegate.reset(new RE::FxDelegate());
			fxDelegate->RegisterHandler(this);
			a_def->SetState(RE::GFxState::StateType::kExternalInterface, fxDelegate.get());
			fxDelegate->Release();
		});
		if (!loaded) {
			//Closed at once: a menu without its movie would sit invisible and modal.
			Log::ERROR("repair menu: Interface\\{}.swf failed to load - closing", FILE_NAME);
			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}
		}

		depthPriority = 5;
		inputContext = Context::kMenuMode;
		menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kDisablePauseMenu, RE::UI_MENU_FLAGS::kModal,
					  RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kTopmostRenderedMenu,
					  RE::UI_MENU_FLAGS::kRequiresUpdate, RE::UI_MENU_FLAGS::kUpdateUsesCursor);
		auto input = RE::BSInputDeviceManager::GetSingleton();
		if (!input || !input->IsGamepadEnabled()) {
			menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
		}
	}

	void RepairMenu::Register()
	{
		if (auto ui = RE::UI::GetSingleton()) {
			ui->Register(MENU_NAME, Creator);
			Log::INFO("repair menu registered as '{}'", MENU_NAME);
		}
	}

	void RepairMenu::Open(bool a_fromInventory, const RE::TESForm* a_tool)
	{
		auto queue = RE::UIMessageQueue::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!queue || !ui || ui->IsMenuOpen(MENU_NAME)) {
			return;
		}
		if (!ScaleformUtility::MovieExists(FILE_NAME)) {
			Log::ERROR("repair menu: Interface\\{}.swf not found - not opened", FILE_NAME);
			return;
		}
		g_fromInventory = a_fromInventory;
		g_vendorMode = false;
		g_recharge = false;
		g_vendor = {};
		g_pendingHammer = Condition::HammerIndex(a_tool);
		if (a_fromInventory) {
			queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
		queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
	}

	void RepairMenu::OpenVendor(RE::Actor* a_vendor)
	{
		auto queue = RE::UIMessageQueue::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!queue || !ui || !a_vendor || ui->IsMenuOpen(MENU_NAME)) {
			return;
		}
		if (!ScaleformUtility::MovieExists(FILE_NAME)) {
			Log::ERROR("repair menu: Interface\\{}.swf not found - not opened", FILE_NAME);
			return;
		}
		g_fromInventory = false;
		g_vendorMode = true;
		g_recharge = false;
		g_vendor = a_vendor->GetHandle();
		g_pendingHammer = -1;
		Log::INFO("vendor repair: opened by {} (Smithing {:.0f})", a_vendor->GetName(), Condition::VendorSkill(a_vendor));
		queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);  // over the dialogue, which stays open
	}

	void RepairMenu::OpenRecharge(RE::Actor* a_vendor)
	{
		auto queue = RE::UIMessageQueue::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!queue || !ui || !a_vendor || ui->IsMenuOpen(MENU_NAME)) {
			return;
		}
		if (!ScaleformUtility::MovieExists(FILE_NAME)) {
			Log::ERROR("repair menu: Interface\\{}.swf not found - not opened", FILE_NAME);
			return;
		}
		g_fromInventory = false;
		g_vendorMode = true;
		g_recharge = true;
		g_vendor = a_vendor->GetHandle();
		g_pendingHammer = -1;
		Log::INFO("recharge: opened by {} (Enchanting {:.0f})", a_vendor->GetName(), VendorEnchanting(a_vendor));
		queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
	}

	void RepairMenu::Close()
	{
		if (auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
	}

	void RepairMenu::Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg)
	{
		a_cbReg->Process("SKYBRepair_Ready", OnReady);
		a_cbReg->Process("SKYBRepair_Strike", OnStrike);
		a_cbReg->Process("SKYBRepair_Close", OnClose);
		a_cbReg->Process("SKYBRepair_Hammer", OnHammer);
		a_cbReg->Process("PlaySound", OnPlaySound);
		a_cbReg->Process("SKYBRequestHotkeys", OnRequestHotkeys);
	}

	RE::UI_MESSAGE_RESULTS RepairMenu::ProcessMessage(RE::UIMessage& a_message)
	{
		if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
			const bool back = g_fromInventory && Settings::On(G::ReturnToInventory);
			g_fromInventory = false;
			SKSE::GetTaskInterface()->AddTask([back] {
				g_rows.clear();
				g_vendorMode = false;
				g_recharge = false;
				g_vendor = {};
				if (back) {
					if (auto queue = RE::UIMessageQueue::GetSingleton()) {
						queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
					}
				}
			});
		}
		return RE::IMenu::ProcessMessage(a_message);
	}

	void RepairMenu::OnReady(const RE::FxDelegateArgs&)
	{
		SKSE::GetTaskInterface()->AddTask([] {
			// the hammer that was activated, else the best one carried
			g_hammer = g_pendingHammer >= 0 && Condition::Owned(g_pendingHammer) > 0 ? g_pendingHammer : BestOwned();
			g_pendingHammer = -1;
			Settings::ReloadIfChanged();  // INI edits apply on the next menu open
			Refresh({});
		});
	}

	void RepairMenu::OnStrike(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsNumber()) {
			return;
		}
		const auto row = static_cast<std::int32_t>(a_params[0].GetNumber());
		const bool toLimit = a_params.GetArgCount() > 1 && a_params[1].IsNumber() && a_params[1].GetNumber() != 0.0;
		const auto generation = a_params.GetArgCount() > 2 && a_params[2].IsNumber() ?
									static_cast<std::int32_t>(a_params[2].GetNumber()) : -1;
		SKSE::GetTaskInterface()->AddTask([row, toLimit, generation] { DoStrike(row, toLimit, generation); });
	}

	void RepairMenu::OnHammer(const RE::FxDelegateArgs& a_params)
	{
		const int dir = a_params.GetArgCount() > 0 && a_params[0].IsNumber() && a_params[0].GetNumber() < 0.0 ? -1 : 1;
		SKSE::GetTaskInterface()->AddTask([dir] {
			if (g_vendorMode) {
				return;  // vendors bring their own hammers
			}
			const int before = g_hammer;
			CycleHammer(dir);
			if (g_hammer != before) {
				RE::PlaySound("UIMenuFocus");
			}
			Refresh({});
		});
	}

	void RepairMenu::OnClose(const RE::FxDelegateArgs&)
	{
		Close();
	}

	// The settings menu's HotkeyService answers the SKSE mod event SKYBRequestHotkeys. A movie loaded
	// by our own IMenu has no skse object, so it asks through GameDelegate and the event is sent here.
	void RepairMenu::OnRequestHotkeys(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsString()) {
			return;
		}
		const std::string arg = a_params[0].GetString();
		SKSE::GetTaskInterface()->AddTask([arg] {
			SKSE::ModCallbackEvent ev{ "SKYBRequestHotkeys", arg.c_str(), 0.0f, nullptr };
			if (auto src = SKSE::GetModCallbackEventSource()) {
				src->SendEvent(&ev);
			}
		});
	}

	void RepairMenu::OnPlaySound(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() >= 1 && a_params[0].IsString()) {
			RE::PlaySound(a_params[0].GetString());
		}
	}

	// ============================================================ papyrus ====
	// SKYBDegradation.OpenVendorRepair(Actor akVendor) global native - the
	// dialogue services strip calls it (SKYBDialogueServices.ini: [repair]
	// open=SKYBDegradation.OpenVendorRepair) with the speaker.
	namespace
	{
		void OpenVendorRepair(RE::StaticFunctionTag*, RE::Actor* a_vendor)
		{
			if (!a_vendor) {
				Log::WARN("OpenVendorRepair: no vendor");
				return;
			}
			const auto handle = a_vendor->GetHandle();
			SKSE::GetTaskInterface()->AddTask([handle] {
				if (auto vendor = handle.get()) {
					RepairMenu::OpenVendor(vendor.get());
				}
			});
		}
	}

	namespace
	{
		void OpenVendorRecharge(RE::StaticFunctionTag*, RE::Actor* a_vendor)
		{
			if (!a_vendor) {
				Log::WARN("OpenVendorRecharge: no vendor");
				return;
			}
			const auto handle = a_vendor->GetHandle();
			SKSE::GetTaskInterface()->AddTask([handle] {
				if (auto vendor = handle.get()) {
					RepairMenu::OpenRecharge(vendor.get());
				}
			});
		}
	}

	bool RepairMenu::RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("OpenVendorRepair", "SKYBDegradation", OpenVendorRepair);
		a_vm->RegisterFunction("OpenVendorRecharge", "SKYBDegradation", OpenVendorRecharge);
		Log::INFO("papyrus: SKYBDegradation.OpenVendorRepair + OpenVendorRecharge registered");
		return true;
	}

	// ============================================================ opener =====
	void RepairMenuOpener::Register()
	{
		if (auto source = SKSE::GetModCallbackEventSource()) {
			source->AddEventSink(this);
		}
	}

	RE::BSEventNotifyControl RepairMenuOpener::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
															RE::BSTEventSource<SKSE::ModCallbackEvent>*)
	{
		if (a_event && a_event->eventName == "SKYBDegOpenRepairMenu") {
			Log::INFO("mod event SKYBDegOpenRepairMenu -> repair menu");
			RepairMenu::Open(false, nullptr);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
