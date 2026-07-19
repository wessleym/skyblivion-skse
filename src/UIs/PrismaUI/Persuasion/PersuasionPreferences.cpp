#include "PersuasionPreferences.h"
#include "Actors/ActorExpression.h"

#include <random>

namespace Persuasion {

	namespace {
		constexpr std::array<PersuasionPreferences::PreferenceInfo, 4> kPreferences = { {
			{ "Love", ActorExpression::Expression::Happy, 100.0f},
			{ "Like",  ActorExpression::Expression::Happy, 50.0f },
			{ "Dislike", ActorExpression::Expression::Anger, 50.0f},
			{ "Hate", ActorExpression::Expression::Anger, 100.0f},
		} };

		// Fixed action quadrants. Their count drives the preference shuffle below.
		constexpr std::array<std::string_view, 4> kActions = { "Admire", "Boast", "Joke", "Coerce" };
		static_assert(PersuasionPreferences::kActionCount == kPreferences.size(), "each action must map to exactly one preference");
	}

	const PersuasionPreferences::PreferenceInfo* PersuasionPreferences::GetPreference(std::string_view preferenceName) {
		for (const auto& info : kPreferences) {
			if (info.name == preferenceName) {
				return &info;
			}
		}
		return nullptr;
	}

	//Pairs Love/Like/Dislike/Hate preferences with Admire/Boast/Joke/Coerce actions.
	//Randomly paired by Form ID to remain consistent between game sessions.
	std::array<std::pair<std::string_view, std::string_view>, PersuasionPreferences::kActionCount> PersuasionPreferences::GetRandomPreferences(RE::Actor* target) {
		std::array<int, kActionCount> idx{};
		for (std::size_t i = 0; i < kActionCount; ++i) {
			idx[i] = static_cast<int>(i);
		}
		std::mt19937 rng(target->GetFormID());
		for (int i = static_cast<int>(kActionCount) - 1; i > 0; --i) {
			std::uniform_int_distribution<int> dist(0, i);
			int j = dist(rng);
			std::swap(idx[i], idx[j]);
		}
		std::array<std::pair<std::string_view, std::string_view>, kActionCount> result;
		for (std::size_t i = 0; i < kActionCount; ++i) {
			result[i] = { kActions[i], kPreferences[idx[i]].name };
		}
		return result;
	}

}
