#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Achievements {

	//An achievement counted by a global: the menu shows count / total, and SKYBAchievements.CheckProgress
	//unlocks once the count reaches the total. Globals are named by editor ID.
	struct Progress {
		bool present = false;
		std::string countGlobal;
		//The total: totalGlobal's value when it is set and resolves, else total.
		double total = 0.0;
		std::string totalGlobal;
	};

	struct Achievement {
		std::string id;
		std::string categoryId;
		double priority = 100.0;
		//$keys or plain text.
		std::string name;
		std::string description;
		//Shown while locked. A hidden achievement defaults both to "???".
		std::string lockedName;
		std::string lockedDescription;
		bool hidden = false;
		//A file in Interface\Skyblivion\Art\Awards; empty = the drawn star.
		std::string icon;
		Progress progress;
	};

	struct Category {
		std::string id;
		std::string displayName;
		double priority = 100.0;
		std::vector<Achievement> entries;
	};

	//The achievement definitions, read once per session from AchievementContract::DataRoot.
	//
	//Discovery: every subfolder with a valid category.json is a category; every other *.json in it is one achievement.
	//Fail-soft: a bad file is logged and skipped. Ids are unique case-insensitively across all categories:
	//categories in priority order (then folder name), files in name order, the first one wins.
	//Categories and their entries are listed by priority, then id.
	class AchievementData {
	public:
		static void LoadOnce();

		[[nodiscard]] static const std::vector<Category>& Categories();

		//Null for an id nobody defined. Case-insensitive.
		[[nodiscard]] static const Achievement* Find(std::string_view id);

		[[nodiscard]] static std::size_t Count();
	};

}
