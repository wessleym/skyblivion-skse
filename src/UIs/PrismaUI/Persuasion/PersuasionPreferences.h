#pragma once

#include "Actors/ActorExpression.h"

#include <array>
#include <string_view>
#include <utility>

namespace RE { class Actor; }

namespace Persuasion {

	class PersuasionPreferences {
	public:
		struct PreferenceInfo {
			std::string_view            name;// Must match an option of kPreferences in PersuasionPreferences.cpp
			ActorExpression::Expression expression;
			float                       strength;// 0 to 100
		};

		static constexpr std::size_t kActionCount = 4;

		static const PreferenceInfo* GetPreference(std::string_view preferenceName);
		static std::array<std::pair<std::string_view, std::string_view>, kActionCount> GetRandomPreferences(RE::Actor* target);
	};

}
