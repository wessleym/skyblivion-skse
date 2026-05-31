#include "SpellRecipeParser.h"

#include "SpellMakingContract.h"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace SpellMaking {

	namespace {
		RE::MagicSystem::CastingType CastingTypeFromString(std::string_view s) {
			return s == SpellMakingContract::CastingType::Concentration
				? RE::MagicSystem::CastingType::kConcentration
				: RE::MagicSystem::CastingType::kFireAndForget;
		}

		RE::MagicSystem::Delivery DeliveryFromString(std::string_view s) {
			if (s == SpellMakingContract::Delivery::Self) {
				return RE::MagicSystem::Delivery::kSelf;
			}
			if (s == SpellMakingContract::Delivery::Touch) {
				return RE::MagicSystem::Delivery::kTouch;
			}
			return RE::MagicSystem::Delivery::kAimed;
		}
	}

	bool SpellRecipeParser::Parse(const char* payload, SpellMakingStore::SpellRecipe& recipe, int& goldPrice) {
		if (!payload || !*payload) {
			Log::WARN("SpellRecipeParser::Parse: Empty Payload");
			return false;
		}

		const nlohmann::json j = nlohmann::json::parse(payload, nullptr, false);
		if (j.is_discarded() || !j.is_object()) {
			Log::WARN("SpellRecipeParser::Parse: Malformed JSON: {}", payload);
			return false;
		}

		try {
			recipe.name = j.value("name", std::string{});
			recipe.castingType = CastingTypeFromString(j.value("castingType", std::string{}));
			recipe.delivery = DeliveryFromString(j.value("delivery", std::string{}));
			recipe.magickaCost = j.value("magickaCost", 0);
			goldPrice = j.value("goldPrice", 0);

			if (const auto effects = j.find("effects"); effects != j.end() && effects->is_array()) {
				for (const auto& e : *effects) {
					if (!e.is_object()) {
						continue;
					}
					SpellMakingStore::EffectSpec spec{};
					spec.formID = e.value("formId", RE::FormID{ 0 });
					spec.editorId = e.value("editorId", std::string{});
					spec.magnitude = e.value("magnitude", 0);
					spec.duration = e.value("duration", 0);
					//Area is JSON null for "None" and a number 10..100 otherwise.
					if (const auto it = e.find("area"); it != e.end() && !it->is_null()) {
						spec.area = it->get<std::uint32_t>();
					}
					spec.magickaCost = e.value("magickaCost", 0);
					recipe.effects.push_back(std::move(spec));
				}
			}
		}
		catch (const nlohmann::json::exception& ex) {
			Log::WARN("SpellRecipeParser::Parse: Payload Processing Error: {}", ex.what());
			return false;
		}

		if (recipe.effects.empty()) {
			Log::WARN("SpellRecipeParser::Parse: No effects. {} not created.", recipe.name);
			return false;
		}
		return true;
	}

}
