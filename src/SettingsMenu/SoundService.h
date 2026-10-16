#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The settings menu's volume rows (applier "sound"): one of the game's sound categories each, the ones its
// own audio settings list. A row's target names the category: "Plugin.esm:XXXXXX" (its form ID in that
// plugin) or its editor ID.
//
// The value is the category's menu volume, as the journal's audio settings set it: changed at once
// (SetCategoryVolume and the game's own SoundCategory.SetVolume) and kept where the game keeps it,
// SkyrimPrefs.ini [AudioMenu] (the fValN beside the uIDN that holds the category's form ID; the file is
// written directly, the game registers no setting for those), so the game restores it on the next start.
// Each change plays a short sound of the category (not for music).
// The master volume is not one of these (the game applies it its own way); it stays in the journal.
namespace SKYBSettingsMenu
{
	class SoundService : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		// while the settings menu is open the samples' categories do not pause with the game (otherwise only UI and
		// footstep samples are heard); their flags go back when it closes
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
											  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

		static SoundService* GetSingleton();

		void Register();  // kDataLoaded: resolves the rows' categories, logs the game's menu categories

		[[nodiscard]] std::optional<float> Get(std::string_view a_target) const;
		void                               Set(std::string_view a_target, float a_value);
		// the category's own name (often a translation key such as "$Effects"); nullptr when unresolved
		[[nodiscard]] const char*          Label(std::string_view a_target) const;
		// the category's default menu volume, when resolved
		[[nodiscard]] std::optional<float> Default(std::string_view a_target) const;

	private:
		SoundService() = default;

		[[nodiscard]] RE::BGSSoundCategory* Find(std::string_view a_target) const;
		void                                PlaySample(std::string_view a_target);
		void                                Nudge();          // re-applies the interface volume, so the music follows
		void                                PlayMusicTest();  // the reveal sting, the music row's sample
		void                                SetMaster(float a_value);

		std::unordered_map<std::string, RE::BGSSoundCategory*> _byTarget;
		std::unordered_map<std::string, RE::BGSSoundDescriptorForm*> _demo;  // a short sound of each row's category
		// rows' categories under the one the game pauses in menus, moved from it while the settings menu is open
		std::vector<RE::BGSSoundCategory*>                                  _unpause;
		std::unordered_map<RE::BGSSoundCategory*, RE::BGSSoundCategory*>    _savedParent;
		// the value each row was last set to since the game started: what the row shows in a game (the music
		// category's own volume reads back 5% whatever is set, which would keep the slider from moving)
		std::unordered_map<std::string, float>                 _set;
	};
}
