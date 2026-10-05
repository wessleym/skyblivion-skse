#include "SkillData.h"
#include "SkillsContract.h"

#include <algorithm>

namespace Skills::SkillData {

	std::optional<std::size_t> IndexOf(RE::ActorValue actorValue) {
		const auto found = std::find(kSkillOrder.begin(), kSkillOrder.end(), actorValue);
		if (found == kSkillOrder.end()) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(found - kSkillOrder.begin());
	}

	std::optional<std::array<Reading, 18>> ReadAll(std::string_view source) {
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* actorValues = player ? REBridge::AVOwner(player) : nullptr;
		auto* playerSkills = player ? REBridge::PlayerSkills(player) : nullptr;
		auto* skills = playerSkills ? playerSkills->data : nullptr;
		if (!actorValues || !skills) {
			Log::WARN("{}: Player actor values or skill data unavailable.", source);
			return std::nullopt;
		}

		std::array<Reading, 18> readings{};
		for (std::size_t i = 0; i < kSkillOrder.size(); ++i) {
			const auto actorValue = kSkillOrder[i];
			//PlayerSkills holds one entry per skill, from One-Handed in actor value order.
			const auto& skill = skills->skills[static_cast<int>(actorValue) - static_cast<int>(RE::ActorValue::kOneHanded)];
			const float threshold = skill.levelThreshold;
			readings[i] = Reading{
				actorValue,
				actorValues->GetActorValue(actorValue),
				actorValues->GetBaseActorValue(actorValue),
				actorValues->GetPermanentActorValue(actorValue),
				skill.xp,
				threshold,
				threshold > 0.0f ? skill.xp / threshold * 100.0f : 0.0f
			};
		}
		return readings;
	}

	std::optional<std::vector<MasteryPerk>> MasteryPerks(std::size_t index, std::string_view source) {
		auto* skillPerkLists = RE::TESForm::LookupByEditorID<RE::BGSListForm>(SkillsContract::MasteryPerkListEditorID);
		if (!skillPerkLists) {
			Log::WARN("{}: {} not resolved. Is po3 Tweaks installed?", source, SkillsContract::MasteryPerkListEditorID);
			return std::nullopt;
		}
		if (index >= skillPerkLists->forms.size()) {
			Log::WARN("{}: Skill index {} out of range ({} inner lists).", source, index, skillPerkLists->forms.size());
			return std::nullopt;
		}
		auto* skillPerkListForm = skillPerkLists->forms[static_cast<RE::BSTArray<RE::TESForm*>::size_type>(index)];
		auto* skillPerkList = skillPerkListForm ? skillPerkListForm->As<RE::BGSListForm>() : nullptr;
		if (!skillPerkList) {
			Log::WARN("{}: Entry {} of {} is not a FormList.", source, index, SkillsContract::MasteryPerkListEditorID);
			return std::nullopt;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		std::vector<MasteryPerk> perks;
		for (auto* form : skillPerkList->forms) {
			auto* perk = form ? form->As<RE::BGSPerk>() : nullptr;
			if (!perk) {
				continue;
			}
			RE::BSString description;
			//Null parent, as in the retired DLL, which resolved description tags with no parent form.
			perk->GetDescription(description, nullptr);
			const char* fullName = perk->GetFullName();
			perks.push_back(MasteryPerk{
				perk,
				fullName ? fullName : "",
				description.c_str() ? description.c_str() : "",
				player && player->HasPerk(perk)
			});
		}
		return perks;
	}

}
