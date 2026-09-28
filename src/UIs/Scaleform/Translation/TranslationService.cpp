#include "TranslationService.h"
#include "TranslationContract.h"
#include "Papyrus/ModEvents.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <string_view>

namespace Translation {

	namespace {
		std::string ToLowerAscii(std::string text) {
			std::transform(text.begin(), text.end(), text.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::string CurrentLanguage() {
			if (auto* setting = RE::GetINISetting("sLanguage:General")) {
				if (setting->GetType() == RE::Setting::Type::kString) {
					if (const char* value = setting->GetString(); value && *value) {
						return ToLowerAscii(value);
					}
				}
			}
			return TranslationContract::FallbackLanguage;
		}

		//The translation map is keyed by wide strings. The files are UTF-8 so they diff and merge like source.
		std::wstring Widen(std::string_view utf8) {
			if (utf8.empty()) {
				return {};
			}
			const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
			if (needed <= 0) {
				return {};
			}
			std::wstring wide(static_cast<std::size_t>(needed), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
			return wide;
		}

		std::optional<std::filesystem::path> ResolveFolder(const std::string& language) {
			std::error_code error;
			const std::filesystem::path root{ TranslationContract::StringsRoot };
			if (auto preferred = root / language; std::filesystem::is_directory(preferred, error)) {
				return preferred;
			}
			if (auto fallback = root / TranslationContract::FallbackLanguage; std::filesystem::is_directory(fallback, error)) {
				Log::WARN("TranslationService: No '{}' folder. Falling back to '{}'.", language, TranslationContract::FallbackLanguage);
				return fallback;
			}
			return std::nullopt;
		}

		//Papyrus Debug.Notification: this CommonLib has no native notification helper.
		void Notify(const std::string& message) {
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			auto* args = RE::MakeFunctionArguments(RE::BSFixedString(message));
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("Debug", "Notification", args, callback);
		}

		void StripUtf8Bom(std::string& line) {
			if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
				static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
				line.erase(0, 3);
			}
		}
	}

	void TranslationService::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("TranslationService: No mod-event source. {} will not be heard.", TranslationContract::ModEvent::ReloadStrings);
		}
		Reload();
	}

	int TranslationService::LoadFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		const auto fileName = path.filename().string();
		if (!file) {
			Log::ERROR("TranslationService: Cannot open {}.", fileName);
			return -1;
		}

		int parsed = 0;
		int lineNumber = 0;
		std::string line;
		while (std::getline(file, line)) {
			++lineNumber;
			if (lineNumber == 1) {
				StripUtf8Bom(line);
			}
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}

			std::string key;
			std::string value;
			switch (StringsFormat::Parse(line, key, value)) {
			case StringsFormat::Line::kSkip:
				continue;
			case StringsFormat::Line::kNoSeparator:
				Log::WARN("TranslationService: {}({}) has no separator. Skipped.", fileName, lineNumber);
				continue;
			case StringsFormat::Line::kEmptyKey:
				Log::WARN("TranslationService: {}({}) has an empty key. Skipped.", fileName, lineNumber);
				continue;
			case StringsFormat::Line::kEntry:
				break;
			}

			auto wideKey = Widen(key);
			auto wideValue = Widen(StringsFormat::ExpandEscapes(value));
			if (wideKey.empty()) {
				Log::WARN("TranslationService: {}({}) has a key that failed to convert. Skipped.", fileName, lineNumber);
				continue;
			}

			auto folded = StringsFormat::FoldKey(key);
			if (auto previous = s_origins.Define(key, fileName)) {
				//Not an error, since the later file wins, but logged because cross-file key collisions are otherwise silent.
				if (previous->key == key) {
					Log::WARN("TranslationService: Key {} redefined in {} (was {}).", key, fileName, previous->file);
				}
				else {
					Log::WARN("TranslationService: Key {} redefined in {} (was {} in {}; keys are case-insensitive).",
						key, fileName, previous->key, previous->file);
				}
				std::erase_if(s_staged, [&folded](const Staged& entry) { return entry.folded == folded; });
			}
			s_staged.push_back({ std::move(folded), std::move(wideKey), std::move(wideValue) });
			++parsed;
		}

		Log::INFO("TranslationService: {} -> {} entries.", fileName, parsed);
		return parsed;
	}

	int TranslationService::Apply() {
		auto* manager = RE::BSScaleformManager::GetSingleton();
		auto* translator = manager && manager->loader
			? manager->loader->GetStateAddRef<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator)
			: nullptr;
		if (!translator) {
			Log::ERROR("TranslationService: Scaleform translator unavailable. Nothing applied.");
			return -1;
		}

		auto& map = translator->translator.translationMap;
		int applied = 0;
		for (const auto& entry : s_staged) {
			RE::BSFixedStringW mapKey{ entry.key.c_str() };
			//Replaces rather than skips: a key migrated out of the engine-loaded file must take the Skyblivion text.
			if (auto existing = map.find(mapKey); existing != map.end()) {
				existing->second = RE::BSFixedStringW{ entry.value.c_str() };
			}
			else {
				map.insert({ mapKey, RE::BSFixedStringW{ entry.value.c_str() } });
			}
			++applied;
		}
		//Balances GetStateAddRef.
		translator->Release();
		return applied;
	}

	int TranslationService::Reload() {
		s_staged.clear();
		s_origins.Clear();

		const auto language = CurrentLanguage();
		const auto folder = ResolveFolder(language);
		if (!folder) {
			Log::WARN("TranslationService: No folder at {}/<language>. Nothing loaded.", TranslationContract::StringsRoot);
			return 0;
		}

		//Sorted so load order is deterministic and an override file can be named to sort last.
		std::vector<std::filesystem::path> files;
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(*folder, error)) {
			if (entry.is_regular_file(error) && ToLowerAscii(entry.path().extension().string()) == ".txt") {
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end());

		if (files.empty()) {
			Log::WARN("TranslationService: {} contains no .txt files.", folder->string());
			return 0;
		}

		for (const auto& file : files) {
			LoadFile(file);
		}

		const int applied = Apply();
		if (applied >= 0) {
			Log::INFO("TranslationService: {} file(s), {} key(s) applied from {}.", files.size(), applied, folder->string());
		}
		return applied;
	}

	RE::BSEventNotifyControl TranslationService::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == TranslationContract::ModEvent::ReloadStrings) {
			SKSE::GetTaskInterface()->AddTask([] {
				const int applied = Reload();
				Notify(applied >= 0
					? std::format("Skyblivion: {} strings reloaded", applied)
					: std::string("Skyblivion: String reload failed (see log)"));
			});
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
