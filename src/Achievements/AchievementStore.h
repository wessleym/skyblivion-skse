#pragma once

#include <string>
#include <unordered_set>

namespace Achievements {

	//Keeps a record of every achievement any character has unlocked on this machine.
	//Achievements live as globals inside each save, so no save can see another's unlocks.
	//
	//File: { "version": 1, "achievements": { "<key>": { "first": "YYYY-MM-DD HH:MM", "character": "<name>" } } }
	//
	//The store only adds. A character with the achievement locked, or a console `set <global> to 0`,
	//never removes an entry. It writes only when a sync brings a new key.
	//
	//The file is never overwritten unread. An unreadable file is renamed to SKYBAchievements.unreadable-<time>.json,
	//and if that fails, saving is off for the session. Saves go through a temporary file, so a failed write leaves the old one intact.
	//
	//All access is on the main thread (SKSE tasks and UI tasks), so the store needs no lock.
	class AchievementStore {
	public:
		//Call once, at kDataLoaded.
		static void Register();

		//Every key any character has unlocked. Main thread only.
		[[nodiscard]] static std::unordered_set<std::string> UnlockedKeys();

	private:
		//Union-merges the unlocked keys from a feed cache CSV into the store.
		static void Sync(const std::string& csv);
		static void AnswerFileRequest();

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
