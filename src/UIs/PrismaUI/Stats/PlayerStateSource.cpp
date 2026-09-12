#include "PlayerStateSource.h"
#include "PerkIndex.h"

#include <array>
#include <string>
#include <utility>

namespace Stats {

	namespace {
		//Tree id -> the actor value holding that attribute's level.
		//RE::ActorValue has no slots for Oblivion's attributes,
		//so Skyblivion.esm repurposes eight per-skill experience accumulators,
		//renaming their AVIF records. The members below are the original names.
		//A tree absent from this table reports a null level.
		constexpr std::array<std::pair<std::string_view, RE::ActorValue>, 8> kTreeActorValues{ {
			{ "Strength", RE::ActorValue::kTwoHandedSkillAdvance },
			{ "Intelligence", RE::ActorValue::kDestructionSkillAdvance },
			{ "Willpower", RE::ActorValue::kEnchantingSkillAdvance },
			{ "Agility", RE::ActorValue::kSneakingSkillAdvance },
			{ "Speed", RE::ActorValue::kLightArmorSkillAdvance },
			{ "Endurance", RE::ActorValue::kHeavyArmorSkillAdvance },
			{ "Personality", RE::ActorValue::kSpeechcraftSkillAdvance },
			{ "Luck", RE::ActorValue::kPickpocketSkillAdvance }
		} };
	}

	std::optional<RE::ActorValue> PlayerStateSource::ActorValueForTree(std::string_view a_treeId) {
		for (const auto& [id, actorValue] : kTreeActorValues) {
			if (id == a_treeId) {
				return actorValue;
			}
		}
		return std::nullopt;
	}

	PlayerStateData PlayerStateSource::Capture() {
		PlayerStateData state;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Log::WARN("PlayerStateSource::Capture: Player unavailable. Reporting empty state.");
			return state;
		}
		state.perkPoints = REBridge::GameStats(player).perkCount;

		auto* actorValues = REBridge::AVOwner(player);
		std::size_t resolved = 0;
		std::size_t unresolved = 0;
		std::string levels;

		const auto& trees = PerkIndex::Trees();
		state.attributes.reserve(trees.size());
		for (const auto& tree : trees) {
			PlayerAttributeData attribute;
			attribute.id = tree.id;
			if (const auto actorValue = ActorValueForTree(tree.id); actorValue) {
				if (actorValues) {
					//Base, not current: a Fortify effect must not unlock a perk.
					attribute.attributeLevel = static_cast<int>(actorValues->GetBaseActorValue(*actorValue));
				}
				//The AVIF record holds the renamed attribute name.
				//The view reports that rather than the name in the generated document.
				if (auto* info = RE::ActorValueList::GetActorValueInfo(*actorValue)) {
					if (const auto* fullName = info->GetFullName(); fullName && *fullName) {
						attribute.name = fullName;
					}
				}
			}
			if (!levels.empty()) {
				levels += ", ";
			}
			levels += (attribute.name.empty() ? tree.id : attribute.name) + " " +
				(attribute.attributeLevel ? std::to_string(*attribute.attributeLevel) : "none");

			attribute.perks.reserve(tree.nodes.size());
			for (const auto& node : tree.nodes) {
				PlayerPerkData perk{ node.id, 0 };
				for (const auto& reference : node.perks) {
					auto* form = PerkIndex::Resolve(reference);
					if (!form) {
						++unresolved;
						continue;
					}
					++resolved;
					if (player->HasPerk(form)) {
						++perk.ownedRanks;
					}
				}
				attribute.perks.push_back(std::move(perk));
			}
			state.attributes.push_back(std::move(attribute));
		}

		Log::INFO("PlayerStateSource: {} perk point(s). Attributes: {}", state.perkPoints, levels);

		//Logged only when the id count changes:
		//resolution depends on the loaded plugins, not on the player's progress.
		static std::size_t reportedFor = 0;
		if (reportedFor != resolved + unresolved) {
			reportedFor = resolved + unresolved;
			Log::INFO("PlayerStateSource: {} of {} perk id(s) resolved to forms.", resolved, reportedFor);
			if (resolved == 0 && reportedFor > 0) {
				Log::WARN("PlayerStateSource: No perk ids resolved. Owned ranks will all read as zero.");
			}
		}

		return state;
	}
}
