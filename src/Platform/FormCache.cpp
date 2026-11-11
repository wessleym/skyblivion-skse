#include "Platform/FormCache.h"

#include "UIs/Scaleform/Dialogue/DialogueContract.h"
#include "UIs/Scaleform/Factions/FactionsContract.h"
#include "UIs/Scaleform/Skills/SkillsContract.h"

#include <charconv>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace FormCache
{
	namespace
	{
		struct Entry
		{
			RE::TESForm* form = nullptr;
			bool         attempted = false;  // a null form after an attempt is a miss, logged once
		};

		std::mutex                             s_lock;
		std::unordered_map<std::string, Entry> s_table;
		bool                                   s_ready = false;  // OnDataLoaded has run

		std::string LocalKey(RE::FormID a_localID, std::string_view a_plugin)
		{
			return std::format("{}|{:06X}", a_plugin, a_localID & 0x00FFFFFF);
		}

		// The record of a_type behind a_key, or null: a_lookup is called once per key once the table is
		// ready, and its miss or type mismatch is logged once, under a_source. A kept record is checked
		// against a_type on every call: two callers may ask for the same key as different types.
		RE::TESForm* Cached(const std::string& a_key, RE::FormType a_type, std::string_view a_source,
							const std::function<RE::TESForm*()>& a_lookup, std::string_view a_hint)
		{
			const auto wrongType = [&](const RE::TESForm* a_form) {
				if (!a_form || a_type == RE::FormType::None || a_form->Is(a_type)) {
					return false;
				}
				Log::WARN("{}: {} is a {} record, not {} - treated as missing", a_source, a_key,
						  RE::FormTypeToString(a_form->GetFormType()), RE::FormTypeToString(a_type));
				return true;
			};
			std::scoped_lock lock(s_lock);
			auto&            entry = s_table[a_key];
			if (entry.attempted) {
				return wrongType(entry.form) ? nullptr : entry.form;
			}
			//First lookup: a record of the wrong type is kept as a miss, so it is logged this once.
			RE::TESForm* form = a_lookup();
			if (wrongType(form)) {
				form = nullptr;
			} else if (!form) {
				Log::WARN("{}: {} not resolved{}{}", a_source, a_key, a_hint, s_ready ? "" : " (before data load; not kept)");
			}
			if (s_ready) {
				entry.form = form;
				entry.attempted = true;
			} else {
				s_table.erase(a_key);
			}
			return form;
		}

		constexpr std::string_view kEditorIDHint = " (editor ID; is po3 Tweaks installed?)";

		// The plugin's own part of a form ID, whatever the caller wrote (an xEdit "0100ABCD" or "FE001801"
		// included): the low 24 bits, or 12 for a light plugin, whose local IDs never pass 0xFFF.
		RE::TESForm* LookupLocal(RE::FormID a_id, std::string_view a_plugin)
		{
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return nullptr;
			}
			const auto* file = dataHandler->LookupModByName(a_plugin);
			const RE::FormID local = a_id & (file && file->IsLight() ? 0x00000FFF : 0x00FFFFFF);
			return dataHandler->LookupForm(local, a_plugin);
		}
	}

	RE::TESForm* Resolve(std::string_view a_editorID, RE::FormType a_type, std::string_view a_source)
	{
		if (a_editorID.empty()) {
			return nullptr;
		}
		return Cached(std::string(a_editorID), a_type, a_source,
					  [&] { return RE::TESForm::LookupByEditorID(a_editorID); }, kEditorIDHint);
	}

	RE::TESForm* ResolveLocal(RE::FormID a_localID, std::string_view a_plugin, RE::FormType a_type, std::string_view a_source)
	{
		if (a_plugin.empty()) {
			return nullptr;
		}
		const auto lookup = [&]() -> RE::TESForm* { return LookupLocal(a_localID, a_plugin); };
		return Cached(LocalKey(a_localID, a_plugin), a_type, a_source, lookup, " (plugin|local ID; is the plugin loaded?)");
	}

	std::optional<Ref> ParseRef(std::string_view a_text)
	{
		const auto bar = a_text.find('|');
		if (bar == std::string_view::npos || bar == 0 || bar + 1 >= a_text.size()) {
			return std::nullopt;
		}
		RE::FormID localID = 0;
		const auto [end, error] = std::from_chars(a_text.data(), a_text.data() + bar, localID, 16);
		if (error != std::errc{} || end != a_text.data() + bar) {
			return std::nullopt;
		}
		return Ref{ localID, std::string(a_text.substr(bar + 1)) };
	}

	void Keep(std::string_view a_editorID, RE::TESForm* a_form)
	{
		if (a_editorID.empty() || !a_form) {
			return;
		}
		std::scoped_lock lock(s_lock);
		s_table[std::string(a_editorID)] = Entry{ a_form, true };
	}

	void OnDataLoaded()
	{
		{
			std::scoped_lock lock(s_lock);
			s_ready = true;
		}
		//The fixed set: what every session reads whatever the player does. Keys that depend on data
		//files (perk ranks, achievement globals, settings rows) are resolved by their readers on first use.
		std::size_t resolved = 0, total = 0;
		const auto count = [&](const RE::TESForm* a_form) {
			++total;
			if (a_form) {
				++resolved;
			}
		};
		count(Get<RE::BGSListForm>(Factions::FactionsContract::FactionListEditorID, "CharFactionsFeed"));
		count(Get<RE::BGSListForm>(Skills::SkillsContract::MasteryPerkListEditorID, "SkillData"));
		count(Get<RE::BGSKeyword>("MagicDisallowEnchanting", "SigilMenuFeed"));
		count(Get<RE::TESTopic>(Dialogue::DialogueContract::Forms::PersuasionTopicEditorID, "DialogueFeed"));
		count(Get<RE::TESFaction>(Dialogue::DialogueContract::Forms::NoPersuadeFactionEditorID, "DialogueFeed"));
		Log::INFO("FormCache: {} of {} fixed form(s) resolved at data load.", resolved, total);
	}
}
