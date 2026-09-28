#pragma once

#include "StringsFormat.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Translation {

	//Loads Skyblivion's UI strings from many small files into the Scaleform translator,
	//so $keys resolve in every Scaleform menu without one monolithic translation file.
	//
	//The engine loads exactly one translation file per plugin
	//(Interface\Translations\<Plugin>_<language>.txt), which becomes a merge bottleneck once
	//everything lives in Skyblivion.esm. This service writes into the same map that file feeds.
	//PrismaUI views do not read this map.
	//
	//Layout: Data\Interface\Skyblivion\Strings\<language>\*.txt.
	//The language is sLanguage:General, falling back to english,
	//and a missing language folder falls back to english too, so a partial localisation never blanks the UI.
	//
	//File format (the parser is StringsFormat.h):
	//	UTF-8, BOM optional. The engine's own files are UTF-16LE, which diff and merge badly.
	//	One "$KEY<TAB>Value" per line. The value is everything after the tab, verbatim, edge spaces included.
	//	"KEY = Value" is also accepted when no tab is present, and that value is trimmed.
	//	Lines starting with # or ; are comments; blank lines are ignored.
	//	\n, \t and \\ in values are expanded. A missing leading $ on a key is added.
	//
	//Files load in sorted filename order, and a later entry replaces an earlier one for the same key,
	//so a "zz_overrides.txt" can patch anything. Keys compare case-insensitively, as the game compares them,
	//and every replacement is logged with both files named.
	//
	//All access is on the main thread (kDataLoaded, then an SKSE task per reload), so the service needs no lock.
	class TranslationService {
	public:
		//Loads every file, injects it, and takes the reload sink. Call once at kDataLoaded.
		static void Register();

	private:
		//Re-reads the folder and re-applies it.
		//Returns the number of entries applied, or -1 when the translator is unavailable.
		static int Reload();

		//Reads one file into s_staged. Returns the entries parsed, or -1 when the file cannot be read.
		static int LoadFile(const std::filesystem::path& path);

		//Writes s_staged into the live translation map.
		static int Apply();

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;

		//One entry per key as the game compares keys, the latest definition kept, in load order.
		struct Staged {
			std::string  folded; //StringsFormat::FoldKey(key)
			std::wstring key;    //With the $, as written.
			std::wstring value;
		};

		static inline std::vector<Staged> s_staged;
		//The file each key came from, so a collision can name both sides.
		static inline StringsFormat::Origins s_origins;
	};

}
