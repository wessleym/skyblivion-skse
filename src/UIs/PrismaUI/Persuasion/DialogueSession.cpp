#include "DialogueSession.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace Persuasion {
	//<NEB> I'm saving this for later, but I'd like to redo the functionality of this file as an Character::Update
	// hook of some kind. But ideally I would use actor handles for these as well.


	namespace {
		std::atomic<bool>          s_keepAliveActive{ false };
		std::atomic<std::uint32_t> s_keepAliveGeneration{ 0 };

		void SetDialogueMenuVisible(bool visible) {
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
					
					//<NEB> Assert is better than a null check for skse interfaces
					assert(taskInterface);

					taskInterface->AddTask([handle, heldSamples, lastValue]() {
						RE::ActorPtr actor = handle.get();
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
							Log::WARN("Frozen mouth: actor {:08X} phoneme3 held {:.2f} for 1.5s.",
								actor->formID, value);
						}
						});
				}
				}).detach();
		}

		void StartConversationKeepAlive(RE::ActorHandle handle) {
			s_keepAliveActive.store(true);
			const std::uint32_t myGen = s_keepAliveGeneration.fetch_add(1) + 1;
			std::thread([myGen, handle]() {
				while (s_keepAliveActive.load() && s_keepAliveGeneration.load() == myGen) {
					std::this_thread::sleep_for(std::chrono::seconds(30));
					if (!s_keepAliveActive.load() || s_keepAliveGeneration.load() != myGen) {
						break;
					}
					auto* taskInterface = SKSE::GetTaskInterface();
					
					//<NEB> Assert is better than a null check for skse interfaces
					assert(taskInterface);

					taskInterface->AddTask([handle, myGen]() {
						if (!s_keepAliveActive.load() || s_keepAliveGeneration.load() != myGen) {
							return;
						}
						if (RE::ActorPtr actor = handle.get()) {
							SetActorDialogueWithPlayer(actor.get(), true);
						}
						});
				}
				}).detach();
		}

		void StopConversationKeepAlive() {
			s_keepAliveActive.store(false);
			s_keepAliveGeneration.fetch_add(1);//retire the running thread
		}

		// Reopens the dialogue menu. Allows refreshing of topics.
		void ReopenDialogueWhenClosed(RE::ActorHandle handle, int attemptsLeft) {
			auto* taskInterface = SKSE::GetTaskInterface();
			
			//<NEB> Assert is better than a null check for skse interfaces
			assert(taskInterface);

			taskInterface->AddTask([handle, attemptsLeft]() {
				auto* ui = RE::UI::GetSingleton();
				RE::ActorPtr actor = handle.get();
				const bool menuClosed = !(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME));
				// If the menu is not closed or the actor is not found, sleep and try again.
				if ((!menuClosed || !actor) && attemptsLeft > 0) {
					std::thread([handle, attemptsLeft]() {
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						ReopenDialogueWhenClosed(handle, attemptsLeft - 1);
						}).detach();
					return;
				}
				if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
					// Set forceGoodbye to false in case it was set to true earlier.
					mtm->forceGoodbye = false;
				}
				if (actor) {
					actor->EnableAI(true);
					if (auto* player = RE::PlayerCharacter::GetSingleton()) {
						// Activate NPC to start conversation (same frame as EnableAI -- no gap to turn).
						actor->ActivateRef(player, 0, nullptr, 1, false);
					}
				}
				// Make dialogue menu visible in case it was hidden earlier.
				SetDialogueMenuVisible(true);
				});
		}

		void ExitToRefreshedDialogue(RE::ActorHandle handle) {
			// Marshal onto the game thread.
			// Otherwise, calls like forceGoodbye and shutMenu are ignored, and the conversation never closes.
			auto* taskInterface = SKSE::GetTaskInterface();
			
			//<NEB> Assert is better than a null check for skse interfaces
			assert(taskInterface);

			taskInterface->AddTask([handle]() {
				// Make dialogue menu visible in case it was hidden earlier.
				SetDialogueMenuVisible(true);
				if (RE::ActorPtr actor = handle.get()) {
					// Freeze the NPC before the close releases the NPC so the NPC can't start walking away.
					// Re-enabled in ReopenDialogueWhenClosed.
					actor->EnableAI(false);
				}
				if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
					// Both of these seem necessary.
					mtm->forceGoodbye = true;
					mtm->shutMenu = true;
				}
				ReopenDialogueWhenClosed(handle, 40);
				});
		}
	}

	void DialogueSession::Start(RE::Actor* target) {
		// Start dialogue, hide menu, and keep it alive.
		SetActorDialogueWithPlayer(target, true);
		SetDialogueMenuVisible(false);
		StartConversationKeepAlive(target->GetHandle());
		WatchForFrozenMouth(target->GetHandle());
	}

	void DialogueSession::End(RE::ActorHandle handle) {
		StopConversationKeepAlive();
		ExitToRefreshedDialogue(handle);
	}

}
