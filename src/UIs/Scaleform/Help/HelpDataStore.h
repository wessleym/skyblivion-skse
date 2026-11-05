#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Help {

	struct Topic {
		std::string id;
		std::string title;
		//Ready for the text field's htmlText: styles expanded, game text escaped, paragraphs joined by <br>.
		//Key tokens ([Activate]) are left for the SWF, which knows the player's controls.
		std::string body;
		double priority{ 100.0 };
		//Editor ID of a global; the topic shows only while it is non-zero. Empty: always shown.
		std::string visibilityGlobal;
	};

	struct Category {
		std::string id;
		std::string displayName;
		double priority{ 100.0 };
		std::string visibilityGlobal;
		//Sorted by priority, then title.
		std::vector<Topic> topics;
	};

	//Loads and caches the help tree from HelpContract::DataRoot.
	//
	//Every subfolder holding a valid category.json is a category; its other *.json files are its topics, read in
	//alphabetical order. Categories sort by priority, then id; a duplicate category id keeps the first folder.
	//Topics sort by priority, then title (case-insensitive), so topics left at the default read alphabetically, as
	//the vanilla page did. A game message claimed by a topic's "message" anywhere in the tree is left out of every
	//importFormList, so moving a vanilla topic into another category is one file.
	//
	//Loading is fail-soft per file: a bad file logs one line with its path and reason and is skipped. A form
	//reference that does not resolve logs once and drops what needed it. Game forms are read, so the first load
	//must come after kDataLoaded.
	//
	//All access is on the main thread (SKSE tasks), so the store needs no lock.
	class HelpDataStore {
	public:
		//Parses and caches the tree. Later calls do nothing, whether the first succeeded or not.
		static void LoadOnce();

		[[nodiscard]] static const std::vector<Category>& Categories();

	private:
		static inline bool s_loaded = false;
		static inline std::vector<Category> s_categories;
	};

}
