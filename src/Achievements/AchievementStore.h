#pragma once

#include <string>
#include <unordered_map>

namespace Achievements {

	//Keeps a record of every achievement any character has earned on this machine.
	//Each save keeps its own unlocks (AchievementService, in the co-save), so no save can see another's;
	//this file is what the menu's "All characters" view shows.
	//
	//File: { "version": 1, "achievements": { "<id>": { "first": "YYYY-MM-DD HH:MM", "character": "<name>" } } }
	//
	//The store only adds: the first character to earn an id keeps the entry. It writes only when a new id arrives.
	//
	//The file is never overwritten unread. An unreadable file is renamed to SKYBAchievements.unreadable-<time>.json,
	//and if that fails, saving is off for the session. Saves go through a temporary file, so a failed write leaves the old one intact.
	//
	//Locked: unlocks arrive on the Papyrus VM's thread, the menu reads from UI tasks.
	class AchievementStore {
	public:
		struct Entry {
			//Local date and time of the first unlock on this machine.
			std::string first;
			std::string character;
		};

		//Adds the id unless some character already earned it.
		static void Record(const std::string& id, const std::string& character);

		//Every id any character has earned, with its first unlock.
		[[nodiscard]] static std::unordered_map<std::string, Entry> All();

		//"YYYY-MM-DD HH:MM", local time (UTC when the time zone database is unavailable).
		[[nodiscard]] static std::string Now();
	};

}
