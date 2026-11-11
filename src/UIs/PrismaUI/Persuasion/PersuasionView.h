#pragma once

#include "../PrismaFeatureView.h"
#include "PersuasionContract.h"

namespace RE { class Actor; }
class PrismaViewHandle;

namespace Persuasion {

	class PersuasionView : public PrismaFeatureView<PersuasionView> {
	public:
		static void Initialize(const PrismaUIService& service);
		static void Open(RE::Actor* target);
		static void Close();
		// A game is loading: the wheel is hidden and forgets its target, so no End follows the load.
		static void OnPreLoadGame();

	private:
		static constexpr const char* kHtmlPath = "Persuasion/index.html";
		static constexpr const char* kVerifyJsFunc = PersuasionContract::JsFunc::PersuasionVerifyBridges;
		static void RegisterListeners(const PrismaViewHandle& view);

		static RE::ActorHandle s_currentTarget;//ActorHandle of NPC currently being persuaded

		// The NPC died (DialogueSession): the script stops without a closing disposition, then Close.
		static void CloseForDeath();

		// Resolves the current target actor, returning null and logging on failure
		static RE::Actor* GetCurrentActorOrWarn(const char* context);

		//JavaScript Listeners:
		static void OnWedgeHover(const char* argument);
		static void OnCloseFromJS(const char* argument);
		static void OnDispositionChanged(const char* argument);
		static void OnBribe(const char* argument);

		static void SendBribeResult(bool success, int price, int gain, float disposition, const char* reason);
		static void SetDisposition(RE::Actor* actor, float value);
	};

}
