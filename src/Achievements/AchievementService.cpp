#include "AchievementService.h"
#include "AchievementContract.h"
#include "AchievementData.h"
#include "AchievementStore.h"
#include "Papyrus/ModEvents.h"
#include "Papyrus/SKSEScriptRegistrar.h"
#include "Platform/MainThread.h"
#include "Settings/SettingsContract.h"
#include "Settings/SettingsService.h"
#include "Storage/CoSave.h"
#include "UIs/Scaleform/ScaleformUtility.h"
#include "UIs/Scaleform/Toast/ToastService.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_set>

namespace Achievements {

	namespace {
		struct Earned {
			//"YYYY-MM-DD HH:MM", local time.
			std::string real;
			//The game's own date line at the moment of the unlock, as the journal shows it.
			std::string gameDate;
			std::string character;
		};

		//This save's unlocks by the definition's id. Unlocks come from the Papyrus VM,
		//the co-save callbacks from the main thread and the menu's answer from a UI task.
		std::mutex g_lock;
		std::map<std::string, Earned> g_unlocks;

		//The journal page to show at its next opening, or -1.
		std::atomic<int> g_pendingJournalTab{ -1 };
		//The page to open the journal on once the achievements menu has closed, or -1.
		std::atomic<int> g_openJournalOnClose{ -1 };

		std::string PlayerName() {
			auto* player = RE::PlayerCharacter::GetSingleton();
			const char* name = player ? player->GetName() : nullptr;
			return name ? name : "";
		}

		std::string GameDate() {
			auto* calendar = RE::Calendar::GetSingleton();
			if (!calendar) {
				return "";
			}
			char buffer[256] = {};
			calendar->GetTimeDateString(buffer, sizeof(buffer), true);
			return buffer;
		}

		//A global's value by editor ID, or nullopt when it does not resolve (logged once per name).
		std::optional<double> GlobalValue(const std::string& editorID) {
			static std::mutex reportedLock;
			static std::unordered_set<std::string> reported;
			if (editorID.empty()) {
				return std::nullopt;
			}
			auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
			if (!global) {
				std::scoped_lock lock(reportedLock);
				if (reported.insert(editorID).second) {
					Log::WARN("AchievementService: Progress global '{}' not found. Is po3 Tweaks installed?", editorID);
				}
				return std::nullopt;
			}
			return static_cast<double>(global->value);
		}

		//count and total of a counted achievement; total <= 0 when it cannot be read (no bar is shown then).
		std::pair<double, double> ReadProgress(const Achievement& a) {
			if (!a.progress.present) {
				return { 0.0, 0.0 };
			}
			const auto countValue = GlobalValue(a.progress.countGlobal);
			if (!countValue) {
				return { 0.0, 0.0 };
			}
			const double count = *countValue;
			double total = a.progress.total;
			if (!a.progress.totalGlobal.empty()) {
				total = GlobalValue(a.progress.totalGlobal).value_or(total);
			}
			return { count, total };
		}

		// ------------------------------------------------------------------ co-save

		void WriteString(SKSE::SerializationInterface* intfc, const std::string& text) {
			const auto length = static_cast<std::uint32_t>(text.size());
			intfc->WriteRecordData(length);
			if (length > 0) {
				intfc->WriteRecordData(text.data(), length);
			}
		}

		bool ReadString(SKSE::SerializationInterface* intfc, std::string& out) {
			std::uint32_t length = 0;
			if (!intfc->ReadRecordData(length) || length > 4096) {
				return false;
			}
			out.resize(length);
			return length == 0 || intfc->ReadRecordData(out.data(), length) == length;
		}

		void OnSave(SKSE::SerializationInterface* intfc) {
			std::scoped_lock lock(g_lock);
			if (!intfc->OpenRecord(AchievementContract::RecordType, AchievementContract::RecordVersion)) {
				Log::WARN("AchievementService: Could not open the co-save record. This save's achievements are not written.");
				return;
			}
			const auto count = static_cast<std::uint32_t>(g_unlocks.size());
			intfc->WriteRecordData(count);
			for (const auto& [id, unlock] : g_unlocks) {
				WriteString(intfc, id);
				WriteString(intfc, unlock.real);
				WriteString(intfc, unlock.gameDate);
				WriteString(intfc, unlock.character);
			}
		}

		void OnLoad(SKSE::SerializationInterface* intfc, std::uint32_t version, std::uint32_t) {
			if (version != AchievementContract::RecordVersion) {
				Log::WARN("AchievementService: Co-save record version {} is not {}. Skipped.", version, AchievementContract::RecordVersion);
				return;
			}
			std::scoped_lock lock(g_lock);
			std::uint32_t count = 0;
			if (!intfc->ReadRecordData(count)) {
				return;
			}
			for (std::uint32_t i = 0; i < count; ++i) {
				std::string id;
				Earned unlock;
				if (!ReadString(intfc, id) || !ReadString(intfc, unlock.real) || !ReadString(intfc, unlock.gameDate) ||
					!ReadString(intfc, unlock.character)) {
					Log::WARN("AchievementService: Co-save record cut short after {} of {} unlock(s).", i, count);
					return;
				}
				g_unlocks[id] = std::move(unlock);
			}
		}

		void OnRevert(SKSE::SerializationInterface*) {
			std::scoped_lock lock(g_lock);
			g_unlocks.clear();
		}

		void OnLoaded() {
			std::scoped_lock lock(g_lock);
			Log::INFO("AchievementService: This save has {} achievement(s).", g_unlocks.size());
			for (const auto& [id, unlock] : g_unlocks) {
				if (!AchievementData::Find(id)) {
					Log::INFO("AchievementService: '{}' is earned in this save but defined nowhere now; kept, not shown.", id);
				}
			}
		}

		// ------------------------------------------------------------------ menu

		RE::GFxValue Str(const std::string& text) {
			return RE::GFxValue(text.c_str());
		}

		void PushList(const std::string& request) {
			const auto start = std::chrono::steady_clock::now();
			const auto callback = MenuCallback::Open(request, "AchievementService");
			if (!callback) {
				return;
			}
			auto* movie = callback->movie.get();
			const auto record = AchievementStore::All();
			std::map<std::string, Earned> unlocks;
			{
				std::scoped_lock lock(g_lock);
				unlocks = g_unlocks;
			}

			RE::GFxValue root;
			movie->CreateObject(&root);
			RE::GFxValue categories;
			movie->CreateArray(&categories);
			int earned = 0;
			int total = 0;
			for (const auto& category : AchievementData::Categories()) {
				RE::GFxValue c;
				movie->CreateObject(&c);
				c.SetMember("id", Str(category.id));
				c.SetMember("name", Str(category.displayName));
				RE::GFxValue entries;
				movie->CreateArray(&entries);
				int categoryEarned = 0;
				for (const auto& a : category.entries) {
					RE::GFxValue e;
					movie->CreateObject(&e);
					e.SetMember("id", Str(a.id));
					e.SetMember("name", Str(a.name));
					e.SetMember("description", Str(a.description));
					e.SetMember("lockedName", Str(a.lockedName));
					e.SetMember("lockedDescription", Str(a.lockedDescription));
					e.SetMember("hidden", RE::GFxValue(a.hidden));
					e.SetMember("icon", Str(a.icon));
					const auto mine = unlocks.find(a.id);
					const bool unlocked = mine != unlocks.end();
					e.SetMember("unlocked", RE::GFxValue(unlocked));
					if (unlocked) {
						++categoryEarned;
						e.SetMember("date", Str(mine->second.real));
						e.SetMember("gameDate", Str(mine->second.gameDate));
						e.SetMember("character", Str(mine->second.character));
					}
					const auto other = record.find(a.id);
					if (other != record.end()) {
						e.SetMember("recordDate", Str(other->second.first));
						e.SetMember("recordCharacter", Str(other->second.character));
					}
					if (a.progress.present) {
						const auto [count, of] = ReadProgress(a);
						if (of > 0.0) {
							e.SetMember("count", RE::GFxValue(count));
							e.SetMember("total", RE::GFxValue(of));
						}
					}
					entries.PushBack(e);
				}
				c.SetMember("entries", entries);
				c.SetMember("earned", RE::GFxValue(static_cast<double>(categoryEarned)));
				categories.PushBack(c);
				earned += categoryEarned;
				total += static_cast<int>(category.entries.size());
			}
			root.SetMember("categories", categories);
			root.SetMember("earned", RE::GFxValue(static_cast<double>(earned)));
			root.SetMember("total", RE::GFxValue(static_cast<double>(total)));
			root.SetMember("character", Str(PlayerName()));
			//The journal's header line, which the menu repeats: level, progress to the next, the date.
			root.SetMember("today", Str(GameDate()));
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				root.SetMember("level", RE::GFxValue(static_cast<double>(player->GetLevel())));
				auto* skills = REBridge::PlayerSkills(player);
				if (skills && skills->data && skills->data->levelThreshold > 0.0f) {
					root.SetMember("levelProgress", RE::GFxValue(static_cast<double>(skills->data->xp / skills->data->levelThreshold)));
				}
			}
			const bool shared = Settings::SettingsService::GetInt(Settings::SettingsContract::AchievementsSection,
				Settings::SettingsContract::SharedKey, 0) != 0;
			root.SetMember("allCharacters", RE::GFxValue(shared));
			callback->Invoke(root);
			Log::INFO("AchievementService: {} of {} achievement(s) -> {} in {} us.", earned, total, callback->target,
				ScaleformUtility::MicrosecondsSince(start));
		}

		//Switches the journal to the page the achievements menu asked for, once its class can take the call.
		void ShowPendingJournalTab() {
			const int tab = g_pendingJournalTab.exchange(-1);
			if (tab < 0) {
				return;
			}
			ScaleformUtility::RetryOnUIThread(20, std::chrono::milliseconds(50), [tab] {
				auto movie = ScaleformUtility::OpenMenuMovie(AchievementContract::JournalMenuName);
				if (!movie) {
					return true;  //closed again: nothing to do
				}
				RE::GFxValue argument(static_cast<double>(tab));
				return movie->Invoke(AchievementContract::JournalShowTab, nullptr, &argument, 1);
			}, [tab] {
				Log::WARN("AchievementService: The journal did not take page {} (is the journal patch installed?).", tab);
			});
		}

		//Opens the journal on a page the way the game's own journal key does: its saved page, then the game's
		//journal opening. A plain show message skips the game's setup and the pages come up empty.
		//The two addresses are the game's (Address Library ids, SE / AE); on VR the plain message is used.
		void OpenJournalAt(int tab) {
			if (REL::Module::IsVR()) {
				if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
					queue->AddMessage(AchievementContract::JournalMenuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
				return;
			}
			static REL::Relocation<std::uint32_t*> savedPage{ RELOCATION_ID(520167, 406697) };
			using open_t = void (*)(bool);
			static REL::Relocation<open_t> openJournal{ RELOCATION_ID(52428, 53327) };
			*savedPage = static_cast<std::uint32_t>(tab);
			openJournal(false);
			Log::INFO("AchievementService: Journal opened on page {}.", tab);
		}

		//Called by the quest menu's ACHIEVEMENTS item (SKYBQuestTabItem.swf): the quest menu has no skse object.
		class QuestMenuHandOver : public RE::GFxFunctionHandler {
		public:
			void Call(Params&) override {
				Log::INFO("AchievementService: ACHIEVEMENTS chosen in the quest menu.");
				ModEvents::Send(AchievementContract::QuestMenu::MenuSwitch, AchievementContract::QuestMenu::MenuSwitchTab);
				SKSE::GetTaskInterface()->AddUITask([] {
					if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
						queue->AddMessage(AchievementContract::QuestMenu::Name, RE::UI_MESSAGE_TYPE::kHide, nullptr);
					}
				});
			}
		};
		class QuestMenuLog : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override {
				if (a_params.argCount > 0 && a_params.args[0].IsString()) {
					Log::INFO("[SKYBQuestTab] {}", a_params.args[0].GetString());
				}
			}
		};

		//Adds the ACHIEVEMENTS item to the quest menu's tab row once the menu's root clip exists.
		void InjectQuestTabItem() {
			namespace QM = AchievementContract::QuestMenu;
			ScaleformUtility::RetryOnUIThread(20, std::chrono::milliseconds(50), [] {
				auto movie = ScaleformUtility::OpenMenuMovie(QM::Name);
				if (!movie) {
					return true;  //closed again
				}
				RE::GFxValue holder;
				if (!movie->GetVariable(&holder, QM::Holder) || !holder.IsDisplayObject()) {
					return false;
				}
				//The item's two calls, on the holder (loadMovie replaces the clip's own members).
				static RE::GPtr<QuestMenuHandOver> handOver = RE::make_gptr<QuestMenuHandOver>();
				static RE::GPtr<QuestMenuLog> log = RE::make_gptr<QuestMenuLog>();
				RE::GFxValue fn;
				movie->CreateFunction(&fn, handOver.get());
				holder.SetMember(QM::HandOverMember, fn);
				movie->CreateFunction(&fn, log.get());
				holder.SetMember(QM::LogMember, fn);
				RE::GFxValue args[2];
				args[0] = QM::ClipName;
				args[1] = QM::ClipDepth;
				movie->Invoke(QM::CreateClip, nullptr, args, 2);
				RE::GFxValue path(QM::Movie);
				const bool loaded = movie->Invoke(QM::LoadMovie, nullptr, &path, 1);
				Log::INFO("AchievementService: ACHIEVEMENTS item loaded into the quest menu, delivered={}.", loaded);
				return true;
			}, [] {
				Log::WARN("AchievementService: The quest menu had no {} to add the ACHIEVEMENTS item to.", QM::Holder);
			});
		}

		class Sinks :
			public RE::BSTEventSink<SKSE::ModCallbackEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->eventName == AchievementContract::ModEvent::Request) {
					//Built on the main thread, next frame.
					MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushList(request); });
				}
				else if (a_event->eventName == AchievementContract::ModEvent::JournalTab) {
					//The menu closes itself next; the journal opens once it has.
					g_pendingJournalTab = static_cast<int>(a_event->numArg);
					g_openJournalOnClose = static_cast<int>(a_event->numArg);
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
				if (a_event && a_event->opening && a_event->menuName == AchievementContract::JournalMenuName) {
					ShowPendingJournalTab();
				}
				else if (a_event && a_event->opening && a_event->menuName == AchievementContract::QuestMenu::Name) {
					InjectQuestTabItem();
				}
				else if (a_event && !a_event->opening && a_event->menuName == AchievementContract::CustomMenuName) {
					const int page = g_openJournalOnClose.exchange(-1);
					if (page >= 0) {
						MainThread::Post([page] { OpenJournalAt(page); });
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		Sinks g_sinks;

		// ------------------------------------------------------------------ Papyrus

		bool PapyrusUnlock(RE::StaticFunctionTag*, RE::BSFixedString id) {
			return AchievementService::Unlock(id.c_str());
		}
		bool PapyrusIsUnlocked(RE::StaticFunctionTag*, RE::BSFixedString id) {
			return AchievementService::IsUnlocked(id.c_str());
		}
		bool PapyrusCheckProgress(RE::StaticFunctionTag*, RE::BSFixedString id) {
			return AchievementService::CheckProgress(id.c_str());
		}
		bool PapyrusExists(RE::StaticFunctionTag*, RE::BSFixedString id) {
			return AchievementData::Find(id.c_str()) != nullptr;
		}
		std::int32_t PapyrusUnlockedCount(RE::StaticFunctionTag*) {
			return AchievementService::UnlockedCount();
		}
		std::int32_t PapyrusTotalCount(RE::StaticFunctionTag*) {
			return static_cast<std::int32_t>(AchievementData::Count());
		}
	}

	void AchievementService::InstallSerialization() {
		Storage::CoSave::Store store;
		store.name = "Achievements";
		store.recordType = AchievementContract::RecordType;
		store.save = OnSave;
		store.load = OnLoad;
		store.revert = OnRevert;
		store.loaded = OnLoaded;
		Storage::CoSave::Add(store);
	}

	void AchievementService::Register() {
		AchievementData::LoadOnce();
		if (!ModEvents::AddSink(&g_sinks)) {
			Log::WARN("AchievementService: No mod-event source. The achievements menu will get no data.");
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_sinks);
		}
		Log::INFO("AchievementService: Registered.");
	}

	void AchievementService::RegisterPapyrus(RE::BSScript::Internal::VirtualMachine* vm) {
		const auto* script = AchievementContract::ScriptName;
		SKSEScriptRegistrar::Register(vm, script, "Unlock", PapyrusUnlock);
		SKSEScriptRegistrar::Register(vm, script, "IsUnlocked", PapyrusIsUnlocked);
		SKSEScriptRegistrar::Register(vm, script, "CheckProgress", PapyrusCheckProgress);
		SKSEScriptRegistrar::Register(vm, script, "Exists", PapyrusExists);
		SKSEScriptRegistrar::Register(vm, script, "GetUnlockedCount", PapyrusUnlockedCount);
		SKSEScriptRegistrar::Register(vm, script, "GetTotalCount", PapyrusTotalCount);
	}

	bool AchievementService::Unlock(std::string_view id) {
		const auto* a = AchievementData::Find(id);
		if (!a) {
			Log::WARN("AchievementService: Unlock('{}'): no achievement has that id.", id);
			return false;
		}
		Earned earned{ AchievementStore::Now(), GameDate(), PlayerName() };
		{
			std::scoped_lock lock(g_lock);
			if (!g_unlocks.try_emplace(a->id, earned).second) {
				return false;
			}
		}
		Log::INFO("AchievementService: '{}' earned by '{}' ({}).", a->id, earned.character, earned.gameDate);

		const bool toasts = Settings::SettingsService::GetInt(Settings::SettingsContract::AchievementsSection,
			Settings::SettingsContract::ToastKey, 1) != 0;
		if (toasts) {
			Toast::ToastService::Show(AchievementContract::ToastHeader, a->name, a->description, a->icon);
		}
		//The record writes a file, and the event reaches Papyrus listeners: both off the caller's thread.
		SKSE::GetTaskInterface()->AddTask([id = a->id, character = earned.character] {
			AchievementStore::Record(id, character);
			ModEvents::Send(AchievementContract::ModEvent::Earned, id);
		});
		return true;
	}

	bool AchievementService::IsUnlocked(std::string_view id) {
		const auto* a = AchievementData::Find(id);
		if (!a) {
			return false;
		}
		std::scoped_lock lock(g_lock);
		return g_unlocks.contains(a->id);
	}

	bool AchievementService::CheckProgress(std::string_view id) {
		const auto* a = AchievementData::Find(id);
		if (!a) {
			Log::WARN("AchievementService: CheckProgress('{}'): no achievement has that id.", id);
			return false;
		}
		if (IsUnlocked(a->id)) {
			return true;
		}
		if (!a->progress.present) {
			Log::WARN("AchievementService: CheckProgress('{}'): it has no progress in its JSON; use Unlock.", a->id);
			return false;
		}
		const auto [count, total] = ReadProgress(*a);
		if (total <= 0.0 || count < total) {
			return false;
		}
		Unlock(a->id);
		return true;
	}

	int AchievementService::UnlockedCount() {
		std::scoped_lock lock(g_lock);
		int count = 0;
		for (const auto& [id, earned] : g_unlocks) {
			if (AchievementData::Find(id)) {
				++count;
			}
		}
		return count;
	}

}
