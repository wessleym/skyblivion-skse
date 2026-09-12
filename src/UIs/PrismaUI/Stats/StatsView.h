#pragma once

#include "../PrismaFeatureView.h"
#include "StatsContract.h"

class PrismaViewHandle;

namespace Stats {

	//The perk-tree screen shown in place of StatsMenu.
	//The view owns its tree document, loading perk-trees.js or the data built into the page;
	//C++ supplies only the player's standing.
	class StatsView : public PrismaFeatureView<StatsView> {
	public:
		static void Initialize(const PrismaUIService& service);

		//True once the view exists and its page has loaded.
		//StatsMenuHook suppresses the vanilla screen only while this holds.
		[[nodiscard]] static bool IsAvailable();

		//True while the view is shown and holds input focus.
		[[nodiscard]] static bool IsOpen();

		//Called by StatsMenuHook; both are idempotent.
		static void Open();
		static void Hide();

	private:
		static constexpr const char* kHtmlPath = "Stats/index.html";
		static constexpr const char* kVerifyJsFunc = StatsContract::JsFunc::StatsVerifyBridges;
		//Loaded by index.html with a script tag, in the game and in a browser alike.
		static constexpr const char* kTreeDocumentPath = "Stats/perk-trees.js";

		//PrismaFeatureView invokes this once the page's DOM is ready.
		static void OnViewLoaded(const PrismaViewHandle& view);

		//PrismaUI refcounts focus: a second Open would need a second Unfocus to release.
		static inline bool s_open = false;

		//JavaScript Listeners:
		static void OnCloseFromJS(const char* argument);
		static void OnPerkIdsFromJS(const char* argument);
		static void OnAcquireFromJS(const char* argument);
		static void OnSaveTreesFromJS(const char* argument);

		static void SendPlayerState();
	};

}
