#include "DialogueSession.h"
#include "Actors/ActorExpression.h"
#include "Platform/MainThread.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace Persuasion {
	//<NEB> I'm saving this for later, but I'd like to redo the functionality of this file as an Character::Update
	// hook of some kind. But ideally I would use actor handles for these as well.
	// (Actor handles are complete.)

	namespace {
		//Bumped by Start, End and Abort: a keep-alive thread or a dialogue reopen started under an older value stops.
		std::atomic<std::uint32_t> s_generation{ 0 };
		//The NPC whose death closes the wheel (0: none), and the wheel's close.
		std::atomic<RE::FormID> s_watched{ 0 };
		std::atomic<void (*)()> s_closeWheel{ nullptr };

		//Into the movie: on the main thread, next frame.
		void SetDialogueMenuVisible(bool visible) {
			MainThread::Post([visible] {
				auto* ui = RE::UI::GetSingleton();
				if (!ui) {
					return;
				}
				auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME);
				if (!menu || !menu->uiMovie) {
					return;
				}
				RE::GFxValue value{ visible };
				menu->uiMovie->SetVariable("_root._visible", value);
			});
		}

		// Puts the NPC into the engine's native "in conversation with the player" state.
		// The engine clears it when the player ends the reopened dialogue with a normal goodbye.
		void SetActorDialogueWithPlayer(RE::Actor* actor, bool inDialogue) {
			actor->SetDialogueWithPlayer(inDialogue, false, nullptr);
		}

		// Attempts to log the intermittent frozen-mouth bug.
		void WatchForFrozenMouth(RE::ActorHandle handle) {
			auto heldSamples = std::make_shared<std::atomic<int>>(0);
			auto lastValue = std::make_shared<std::atomic<float>>(-1.0f);
			std::thread([handle, heldSamples, lastValue]() {
				for (int i = 0; i < 12; ++i) {
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
					auto* taskInterface = SKSE::GetTaskInterface();

					assert(taskInterface);

					taskInterface->AddTask([handle, heldSamples, lastValue]() {
						RE::ActorPtr actor = REBridge::Lookup(handle);
						RE::BSFaceGenAnimationData* faceGen = actor ? actor->GetFaceGenAnimationData() : nullptr;
						if (!faceGen) {
							return;
						}
						const float value = faceGen->phoneme3.GetMaxValue();
						if (value <= 0.01f || value != lastValue->exchange(value)) {
							heldSamples->store(0);
							return;
						}
						if (heldSamples->fetch_add(1) + 1 == 6) {//6 x 250ms = 1.5s unchanged
							Log::WARN("Frozen mouth: actor {:08X} phoneme3 held {:.2f} for 1.5s (expression override {}).",
								actor->formID, value, faceGen->exprOverride ? "on" : "off");
						}
						});
				}
				}).detach();
		}

		void StartConversationKeepAlive(RE::ActorHandle handle, std::uint32_t myGen) {
			std::thread([myGen, handle]() {
				while (s_generation.load() == myGen) {
					std::this_thread::sleep_for(std::chrono::seconds(30));
					if (s_generation.load() != myGen) {
						break;
					}
					auto* taskInterface = SKSE::GetTaskInterface();

					assert(taskInterface);

					taskInterface->AddTask([handle, myGen]() {
						if (s_generation.load() != myGen) {
							return;
						}
						if (RE::ActorPtr actor = REBridge::Lookup(handle)) {
							SetActorDialogueWithPlayer(actor.get(), true);
						}
						});
				}
				}).detach();
		}

		// Reopens the dialogue menu. Allows refreshing of topics.
		void ReopenDialogueWhenClosed(RE::ActorHandle handle, int attemptsLeft, std::uint32_t gen) {
			auto* taskInterface = SKSE::GetTaskInterface();

			assert(taskInterface);

			taskInterface->AddTask([handle, attemptsLeft, gen]() {
				auto* ui = RE::UI::GetSingleton();
				RE::ActorPtr actor = REBridge::Lookup(handle);
				//An Abort since the End that started this one: a game is loading.
				const bool aborted = s_generation.load() != gen;
				const bool menuClosed = !(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME));
				// If the menu is not closed or the actor is not found, sleep and try again.
				if (!aborted && (!menuClosed || !actor) && attemptsLeft > 0) {
					std::thread([handle, attemptsLeft, gen]() {
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						ReopenDialogueWhenClosed(handle, attemptsLeft - 1, gen);
						}).detach();
					return;
				}
				if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
					// Clear both goodbye flags: if the dialogue had already closed (combat), nothing consumed them,
					// and the next conversation would shut at once.
					mtm->forceGoodbye = false;
					mtm->shutMenu = false;
				}
				if (actor && (aborted || actor->IsDead())) {
					//Killed while the wheel was up (activating a corpse would open its inventory, not a conversation),
					//or a game is loading: no conversation is opened. Its AI (switched off for the goodbye) is still
					//given back.
					actor->EnableAI(true);
					Log::INFO("DialogueSession: {:08X}: {}; the dialogue is not reopened.", actor->GetFormID(),
						aborted ? "a game is loading" : "died during persuasion");
				}
				else if (actor) {
					actor->EnableAI(true);
					// The wheel's last expression is gone for certain before the new conversation's
					// greeting: a hover's Set that arrives after the close's Reset could otherwise leave
					// the override on, and an override holds the mouth still through every line that follows.
					ActorExpression::ResetNow(actor.get());
					if (auto* player = RE::PlayerCharacter::GetSingleton()) {
						// Activate NPC to start conversation (same frame as EnableAI -- no gap to turn).
						actor->ActivateRef(player, 0, nullptr, 1, false);
					}
				}
				// Make dialogue menu visible in case it was hidden earlier.
				SetDialogueMenuVisible(true);
				});
		}

		void ExitToRefreshedDialogue(RE::ActorHandle handle, std::uint32_t gen) {
			// Marshal onto the game thread.
			// Otherwise, calls like forceGoodbye and shutMenu are ignored, and the conversation never closes.
			auto* taskInterface = SKSE::GetTaskInterface();

			assert(taskInterface);

			taskInterface->AddTask([handle, gen]() {
				//Aborted before this ran (a load): nothing was changed yet, so nothing is.
				if (s_generation.load() != gen) {
					return;
				}
				// Make dialogue menu visible in case it was hidden earlier.
				SetDialogueMenuVisible(true);
				if (RE::ActorPtr actor = REBridge::Lookup(handle)) {
					// The face is the engine's again before the goodbye (see ReopenDialogueWhenClosed).
					ActorExpression::ResetNow(actor.get());
					// Freeze the NPC before the close releases the NPC so the NPC can't start walking away.
					// Re-enabled in ReopenDialogueWhenClosed.
					actor->EnableAI(false);
				}
				if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
					// Both of these seem necessary.
					mtm->forceGoodbye = true;
					mtm->shutMenu = true;
				}
				ReopenDialogueWhenClosed(handle, 40, gen);
				});
		}

		//The world runs under the wheel (it does not pause the game), so its NPC can be killed: the wheel closes,
		//once, on the main thread.
		class DeathSink : public RE::BSTEventSink<RE::TESDeathEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override {
				RE::FormID dying = a_event && a_event->actorDying ? a_event->actorDying->GetFormID() : 0;
				if (dying != 0 && s_watched.compare_exchange_strong(dying, 0)) {
					Log::INFO("DialogueSession: {:08X} died during persuasion; closing the wheel.", dying);
					if (auto* close = s_closeWheel.load()) {
						MainThread::Post(close);
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		DeathSink g_deathSink;
	}

	void DialogueSession::Register() {
		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESDeathEvent>(&g_deathSink);
		}
	}

	void DialogueSession::Start(RE::Actor* target, void (*a_closeWheel)()) {
		s_closeWheel = a_closeWheel;
		s_watched = target->GetFormID();
		const std::uint32_t gen = s_generation.fetch_add(1) + 1;
		// Start dialogue, hide menu, and keep it alive.
		SetActorDialogueWithPlayer(target, true);
		SetDialogueMenuVisible(false);
		// The pick that opened the wheel may have cut a line short: close the mouth it left open
		// (the "Frozen mouth" of WatchForFrozenMouth) instead of showing it under the wheel.
		{
			RE::ActorHandle handle = target->GetHandle();
			SKSE::GetTaskInterface()->AddTask([handle] {
				if (RE::ActorPtr actor = REBridge::Lookup(handle)) {
					ActorExpression::CloseMouthNow(actor.get());
				}
			});
		}
		StartConversationKeepAlive(target->GetHandle(), gen);
		WatchForFrozenMouth(target->GetHandle());
	}

	void DialogueSession::End(RE::ActorHandle handle) {
		s_watched = 0;
		ExitToRefreshedDialogue(handle, s_generation.fetch_add(1) + 1);
	}

	void DialogueSession::Abort() {
		s_watched = 0;
		s_generation.fetch_add(1);
		// The dialogue menu Start hid, should it still be open.
		SetDialogueMenuVisible(true);
	}

}
