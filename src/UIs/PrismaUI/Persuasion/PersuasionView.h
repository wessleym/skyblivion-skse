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

	private:
		static constexpr const char* kHtmlPath = "Persuasion/index.html";
		static constexpr const char* kVerifyJsFunc = PersuasionContract::JsFunc::PersuasionVerifyBridges;
		static void RegisterListeners(const PrismaViewHandle& view);

		//<NEB> The game uses actor handles for menu targets
		static RE::ActorHandle s_currentTargetFormID;//ActorHandle of NPC currently being persuaded

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
