#include "DialogueSession.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace Persuasion {

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
		void WatchForFrozenMouth(RE::FormID formID) {
			auto heldSamples = std::make_shared<std::atomic<int>>(0);
			auto lastValue = std::make_shared<std::atomic<float>>(-1.0f);
			std::thread([formID, heldSamples, lastValue]() {
				for (int i = 0; i < 12; ++i) {
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
					auto* taskInterface = SKSE::GetTaskInterface();
					if (!taskInterface) {
						return;
					}
					taskInterface->AddTask([formID, heldSamples, lastValue]() {
						auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
						auto* faceGen = actor ? actor->GetFaceGenAnimationData() : nullptr;
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
								formID, value);
						}
						});
				}
				}).detach();
		}

		void StartConversationKeepAlive(RE::FormID formID) {
			s_keepAliveActive.store(true);
			const std::uint32_t myGen = s_keepAliveGeneration.fetch_add(1) + 1;
			std::thread([myGen, formID]() {
				while (s_keepAliveActive.load() && s_keepAliveGeneration.load() == myGen) {
					std::this_thread::sleep_for(std::chrono::seconds(30));
					if (!s_keepAliveActive.load() || s_keepAliveGeneration.load() != myGen) {
						break;
					}
					auto* taskInterface = SKSE::GetTaskInterface();
					if (!taskInterface) {
						continue;
					}
					taskInterface->AddTask([formID, myGen]() {
						if (!s_keepAliveActive.load() || s_keepAliveGeneration.load() != myGen) {
							return;
						}
						if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
							SetActorDialogueWithPlayer(actor, true);
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
		void ReopenDialogueWhenClosed(RE::FormID formID, int attemptsLeft) {
			auto* taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface) {
				return;
			}
			taskInterface->AddTask([formID, attemptsLeft]() {
				auto* ui = RE::UI::GetSingleton();
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
				const bool menuClosed = !(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME));
				// If the menu is not closed or the actor is not found, sleep and try again.
				if ((!menuClosed || !actor) && attemptsLeft > 0) {
					std::thread([formID, attemptsLeft]() {
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						ReopenDialogueWhenClosed(formID, attemptsLeft - 1);
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

		void ExitToRefreshedDialogue(RE::FormID formID) {
			// Marshal onto the game thread.
			// Otherwise, calls like forceGoodbye and shutMenu are ignored, and the conversation never closes.
			auto* taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface) {
				return;
			}
			taskInterface->AddTask([formID]() {
				// Make dialogue menu visible in case it was hidden earlier.
				SetDialogueMenuVisible(true);
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
					// Freeze the NPC before the close releases the NPC so the NPC can't start walking away.
					// Re-enabled in ReopenDialogueWhenClosed.
					actor->EnableAI(false);
				}
				if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
					// Both of these seem necessary.
					mtm->forceGoodbye = true;
					mtm->shutMenu = true;
				}
				ReopenDialogueWhenClosed(formID, 40);
				});
		}
	}

	void DialogueSession::Start(RE::Actor* target) {
		// Start dialogue, hide menu, and keep it alive.
		SetActorDialogueWithPlayer(target, true);
		SetDialogueMenuVisible(false);
		StartConversationKeepAlive(target->GetFormID());
		WatchForFrozenMouth(target->GetFormID());
	}

	void DialogueSession::End(RE::FormID formID) {
		StopConversationKeepAlive();
		ExitToRefreshedDialogue(formID);
	}

}
