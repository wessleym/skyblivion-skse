#pragma once

#include <string_view>

// The forms this plugin reads from plugin files (globals, perks, form lists, keywords, sound records,
// Gold001), each resolved once and kept: a form cannot change mid-session, so there is no reason to
// look it up again on every menu open, slider change or perk push.
//
// Keys: an editor ID (needs po3 Tweaks' editor-ID map, which is built before kDataLoaded), a
// (local form ID, plugin) pair, or a full form ID for the game's own records. A key that does not
// resolve, or resolves to a record of another type, is logged once, under the name of the first caller,
// and reads as null from then on. The fixed set every session needs is resolved at kDataLoaded
// (OnDataLoaded); keys that come from data files at run time (perk rank refs, achievement globals,
// settings rows) resolve on first use into the same table. Before OnDataLoaded a lookup is answered
// but not kept, so an early miss cannot pin a null.
//
// Thread safe: achievement progress is read from the Papyrus VM thread, the menus from UI tasks.
namespace FormCache
{
	// kDataLoaded, before the services register: resolves the fixed set and marks the table ready.
	void OnDataLoaded();

	// By editor ID; with a (local ID, plugin) fallback for a load order without po3 Tweaks; by (local ID,
	// plugin); by full form ID. a_type is checked (kNone: any form). a_source names the caller in the log.
	RE::TESForm* Resolve(std::string_view a_editorID, RE::FormType a_type, std::string_view a_source);
	RE::TESForm* Resolve(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin,
						 RE::FormType a_type, std::string_view a_source);
	RE::TESForm* ResolveLocal(RE::FormID a_localID, std::string_view a_plugin, RE::FormType a_type, std::string_view a_source);
	RE::TESForm* ResolveFull(RE::FormID a_formID, RE::FormType a_type, std::string_view a_source);

	// A form found some other way (a script property), kept under an editor ID so later calls find it.
	void Keep(std::string_view a_editorID, RE::TESForm* a_form);

	template <class T>
	T* Get(std::string_view a_editorID, std::string_view a_source)
	{
		return static_cast<T*>(Resolve(a_editorID, T::FORMTYPE, a_source));
	}

	template <class T>
	T* Get(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin, std::string_view a_source)
	{
		return static_cast<T*>(Resolve(a_editorID, a_localID, a_plugin, T::FORMTYPE, a_source));
	}

	template <class T>
	T* GetLocal(RE::FormID a_localID, std::string_view a_plugin, std::string_view a_source)
	{
		return static_cast<T*>(ResolveLocal(a_localID, a_plugin, T::FORMTYPE, a_source));
	}

	// Gold001 (Skyrim.esm 0x00000F), the coin every price is paid in.
	RE::TESBoundObject* Gold();
}
