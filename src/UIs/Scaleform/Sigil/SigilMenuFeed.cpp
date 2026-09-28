#include "SigilMenuFeed.h"
#include "SigilMenuContract.h"
#include "Papyrus/ModEvents.h"
#include "Sigil/SoulGems.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Sigil {

	namespace {
		//Amulet (slot 35), ring (36) and circlet (42): the mask the Papyrus builder used.
		constexpr std::uint32_t kJewelrySlotMask = 0x1060;
		//A safety limit only. The Papyrus builder capped at 120 because its Form array maxed at 128;
		//the CSV slot map and the SWF list have no limit.
		constexpr int kGearRowCap = 512;

		//Never emits an empty or separator-carrying field into the | ; ^ feeds, as the psc's padField did.
		std::string PadField(const char* raw) {
			std::string field = raw ? raw : "";
			for (auto& c : field) {
				if (c == '|' || c == ';' || c == '^') {
					c = '/';
				}
			}
			return field.empty() ? " " : field;
		}

		RE::BGSListForm* FormListFromField(std::string_view field) {
			return RE::TESForm::LookupByID<RE::BGSListForm>(ModEvents::ParseFormID(field));
		}

		//"name|magnitude|duration" for the costliest effect of an enchantment, or "" when it has none.
		std::string EnchantmentFields(RE::TESForm* form) {
			auto* enchantment = form ? form->As<RE::EnchantmentItem>() : nullptr;
			auto* effect = enchantment ? enchantment->GetCostliestEffectItem() : nullptr;
			if (!effect || !effect->baseEffect) {
				return "";
			}
			return std::format("{}|{}|{}", PadField(effect->baseEffect->GetFullName()),
				static_cast<int>(effect->effectItem.magnitude), effect->effectItem.duration);
		}

		RE::TESForm* FormAt(const RE::BGSListForm* list, int index) {
			return list && index >= 0 && index < static_cast<int>(list->forms.size()) ? list->forms[index] : nullptr;
		}
	}

	void SigilMenuFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("SigilMenuFeed: No mod-event source. Sigil menu requests will not be heard.");
			return;
		}
		Log::INFO("SigilMenuFeed: Registered.");
	}

	void SigilMenuFeed::PushGear() {
		const auto start = std::chrono::steady_clock::now();

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto movie = ScaleformUtility::OpenMenuMovie(SigilMenuContract::MenuName);
		if (!player || !movie) {
			Log::WARN("SigilMenuFeed: Gear requested, but the player or the {} movie is unavailable.", SigilMenuContract::MenuName);
			return;
		}

		auto* disallowEnchanting = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicDisallowEnchanting");
		if (!disallowEnchanting) {
			Log::WARN("SigilMenuFeed: MagicDisallowEnchanting keyword not resolved. Its filter is skipped.");
		}

		std::string rows;
		std::string formIDs;
		int count = 0;
		const auto addRow = [&](RE::TESBoundObject* object, int value, int damage, int armor, char type) {
			if (!rows.empty()) {
				rows += ';';
				formIDs += ',';
			}
			rows += std::format("{}|{}|{}|{}|{}|{}", count, PadField(object->GetName()), value, damage, armor, type);
			formIDs += ModEvents::FormatFormID(object->GetFormID());
			++count;
		};

		bool truncated = false;
		const auto inventory = player->GetInventory();
		for (const auto& [object, countAndEntry] : inventory) {
			if (count >= kGearRowCap) {
				truncated = true;
				break;
			}
			const auto& [itemCount, entry] = countAndEntry;
			if (itemCount < 1 || !object || (entry && entry->IsQuestObject())) {
				continue;
			}
			const char* name = object->GetName();
			if (!name || !*name) {
				continue;
			}

			if (auto* weapon = object->As<RE::TESObjectWEAP>()) {
				if (weapon->formEnchanting || weapon->IsStaff() || (disallowEnchanting && weapon->HasKeyword(disallowEnchanting))) {
					continue;
				}
				addRow(object, static_cast<int>(weapon->GetGoldValue()), weapon->GetAttackDamage(), 0, 'W');
			}
			else if (auto* armor = object->As<RE::TESObjectARMO>()) {
				if (armor->formEnchanting || (disallowEnchanting && armor->HasKeyword(disallowEnchanting))) {
					continue;
				}
				//Stored in hundredths, as Papyrus GetArmorRating reads it.
				const int armorRating = static_cast<int>(armor->armorRating / 100);
				const char type = (REBridge::SlotMaskBits(armor) & kJewelrySlotMask) ? 'J' : 'A';
				addRow(object, static_cast<int>(armor->GetGoldValue()), 0, armorRating, type);
			}
		}

		if (!rows.empty()) {
			RE::GFxValue rowsArg(rows.c_str());
			movie->Invoke(SigilMenuContract::AsPath::AddGear, nullptr, &rowsArg, 1);
		}
		RE::GFxValue countArg(static_cast<double>(count));
		movie->Invoke(SigilMenuContract::AsPath::GearDone, nullptr, &countArg, 1);

		ModEvents::Send(SigilMenuContract::Answer::GearFed, std::format("{}^{}", count, formIDs));

		Log::INFO("SigilMenuFeed: {} gear row(s) from {} inventory entries in {} us.", count, inventory.size(), ScaleformUtility::MicrosecondsSince(start));
		if (truncated) {
			Log::WARN("SigilMenuFeed: The gear list hit its {}-row cap. Some eligible gear is not listed.", kGearRowCap);
		}
	}

	void SigilMenuFeed::PushStones(const std::string& payload) {
		const auto start = std::chrono::steady_clock::now();

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto movie = ScaleformUtility::OpenMenuMovie(SigilMenuContract::MenuName);
		if (!player || !movie) {
			Log::WARN("SigilMenuFeed: Stones requested, but the player or the {} movie is unavailable.", SigilMenuContract::MenuName);
			return;
		}

		const auto sections = ModEvents::Split(payload, '^');
		if (sections.size() < 3) {
			Log::WARN("SigilMenuFeed: Malformed stone request '{}'.", payload);
			return;
		}
		const auto listIDs = ModEvents::Split(sections[0], ',');
		if (listIDs.size() < 4) {
			Log::WARN("SigilMenuFeed: Expected 4 FormList IDs, got {}.", listIDs.size());
			return;
		}
		auto* stones = FormListFromField(listIDs[0]);
		auto* weaponEnchantments = FormListFromField(listIDs[1]);
		auto* armorEnchantments = FormListFromField(listIDs[2]);
		auto* gems = FormListFromField(listIDs[3]);
		if (!stones || !weaponEnchantments || !armorEnchantments) {
			Log::WARN("SigilMenuFeed: The stone or enchantment FormLists did not resolve.");
			return;
		}
		const int pad = ModEvents::ParseInt(sections[1]);
		const int selection = ModEvents::ParseInt(sections[2]);

		std::unordered_map<const RE::TESForm*, int> stoneIndex;
		for (std::uint32_t i = 0; i < stones->forms.size(); ++i) {
			if (stones->forms[i]) {
				stoneIndex.emplace(stones->forms[i], static_cast<int>(i));
			}
		}

		//(list index, carried count), sorted by index so rows come in family and tier order.
		std::vector<std::pair<int, std::int32_t>> carried;
		const auto inventory = player->GetInventory([](RE::TESBoundObject& object) { return object.Is(RE::FormType::Misc); });
		for (const auto& [object, countAndEntry] : inventory) {
			if (countAndEntry.first < 1) {
				continue;
			}
			if (const auto found = stoneIndex.find(object); found != stoneIndex.end()) {
				carried.emplace_back(found->second, countAndEntry.first);
			}
		}
		std::sort(carried.begin(), carried.end());

		//Computed once per tier present. An empty name marks a tier not computed yet; PadField never returns one.
		std::array<int, 6> gemCounts{};
		std::array<std::string, 6> gemNames;
		const auto tierGemFields = [&](int tier) {
			if (gemNames[tier].empty()) {
				gemCounts[tier] = SoulGems::CountTierGems(player, tier);
				auto* gem = FormAt(gems, tier - 1);
				gemNames[tier] = gem ? PadField(gem->GetName()) : " ";
			}
			return std::format("{}|{}", gemCounts[tier], gemNames[tier]);
		};

		std::string rows;
		int rowCount = 0;
		for (const auto& [index, count] : carried) {
			const auto weaponFields = EnchantmentFields(FormAt(weaponEnchantments, index));
			const auto armorFields = EnchantmentFields(FormAt(armorEnchantments, index));
			if (weaponFields.empty() || armorFields.empty()) {
				Log::WARN("SigilMenuFeed: No enchantment wired for stone index {}. Stone hidden.", index);
				continue;
			}
			//Stones come in families of five tiers.
			const int tier = index % 5 + 1;
			if (!rows.empty()) {
				rows += ';';
			}
			rows += std::format("{}|{}|{}|{}|{}|{}|{}", index, PadField(stones->forms[index]->GetName()), count, tier,
				weaponFields, armorFields, tierGemFields(tier));
			++rowCount;
		}

		if (rowCount > 0) {
			RE::GFxValue args[3];
			args[0] = rows.c_str();
			args[1] = static_cast<double>(pad);
			std::uint32_t argCount = 2;
			if (selection >= 0) {
				args[2] = static_cast<double>(selection);
				argCount = 3;
			}
			movie->Invoke(SigilMenuContract::AsPath::SetStones, nullptr, args, argCount);
		}

		ModEvents::Send(SigilMenuContract::Answer::StonesFed, std::to_string(rowCount));
		Log::INFO("SigilMenuFeed: {} stone row(s) from {} misc entries in {} us.", rowCount, inventory.size(), ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl SigilMenuFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		//Inventory walks and GFx invokes belong on the main thread.
		if (a_event->eventName == SigilMenuContract::ModEvent::RequestGear) {
			SKSE::GetTaskInterface()->AddTask([] { PushGear(); });
		}
		else if (a_event->eventName == SigilMenuContract::ModEvent::RequestStones) {
			SKSE::GetTaskInterface()->AddTask([payload = ModEvents::StrArg(*a_event)] { PushStones(payload); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
