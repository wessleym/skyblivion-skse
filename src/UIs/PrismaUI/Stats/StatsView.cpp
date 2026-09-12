#include "StatsView.h"
#include "PerkAcquisition.h"
#include "PerkIndex.h"
#include "PerkListValidator.h"
#include "PlayerStateSource.h"
#include "StatsContract.h"

#include "../PrismaViewHandle.h"
#include "../ViewUtility.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace Stats {

	void StatsView::Initialize(const PrismaUIService& service) {
		PrismaFeatureView::Initialize(service, kHtmlPath, kVerifyJsFunc, &StatsView::OnViewLoaded);
	}

	void StatsView::OnViewLoaded(const PrismaViewHandle& view) {
		view.RegisterListener(StatsContract::Listener::StatsClose, OnCloseFromJS);
		view.RegisterListener(StatsContract::Listener::StatsSetPerkIds, OnPerkIdsFromJS);
		view.RegisterListener(StatsContract::Listener::StatsAcquire, OnAcquireFromJS);
		view.RegisterListener(StatsContract::Listener::StatsSaveTrees, OnSaveTreesFromJS);
		//The page loads its own tree, so none is sent.
		//Its ids are requested now that the listener above exists to receive them.
		view.InvokeByFunctionName(StatsContract::JsFunc::StatsSendPerkIds);
	}

	//The view holds input focus, so Escape reaches the page rather than the engine;
	//releasing focus is what returns control to the game.
	void StatsView::OnCloseFromJS(const char*) {
		Hide();

		//StatsMenu has normally finished closing behind the view.
		//If the player left during that close, this kHide lets it finish.
		auto* ui = RE::UI::GetSingleton();
		auto* queue = RE::UIMessageQueue::GetSingleton();
		if (ui && queue && ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)) {
			queue->AddMessage(RE::StatsMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
	}

	//Answering with the player's standing here populates the page before it is shown.
	void StatsView::OnPerkIdsFromJS(const char* payload) {
		if (PerkIndex::Set(payload)) {
			//Once per registration: the ESM does not change during the session.
			PerkListValidator::Validate();
			SendPlayerState();
		}
	}

	void StatsView::OnAcquireFromJS(const char* payload) {
		if (!payload) {
			Log::WARN("StatsView::OnAcquireFromJS: Null payload.");
			return;
		}
		const auto request = nlohmann::json::parse(payload, nullptr, false);
		const auto perk = request.is_discarded() ? std::nullopt : PerkIndex::ReadReference(request);
		if (!perk) {
			Log::WARN("StatsView::OnAcquireFromJS: Unusable payload: {}", payload);
			return;
		}
		if (PerkAcquisition::Acquire(*perk)) {
			//Only on success; a refusal changed nothing.
			SendPlayerState();
		}
	}

	//Writes the edited tree as a script the page loads on its next open.
	//Under a mod manager Data is virtualised, so this usually lands in the overwrite folder;
	//the path is logged.
	void StatsView::OnSaveTreesFromJS(const char* payload) {
		if (!payload) {
			Log::WARN("StatsView::OnSaveTreesFromJS: Null payload.");
			return;
		}
		const auto path = ViewUtility::ResolveViewFile(kTreeDocumentPath);
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file) {
			Log::ERROR("StatsView::OnSaveTreesFromJS: Could not write {}.", path.string());
			return;
		}
		file << "//Written by the Stats view's Save button. Loaded by index.html on the next open.\n";
		file << "var PerkTrees = " << payload << ";\n";
		Log::INFO("StatsView::OnSaveTreesFromJS: Wrote the perk tree document to {}.", path.string());
	}

	void StatsView::SendPlayerState() {
		if (!s_handle.IsValid() || PerkIndex::IsEmpty()) {
			return;
		}
		s_handle.InvokeByFunctionName(StatsContract::JsFunc::StatsSetPlayerState, PlayerStateSource::Capture());
	}

	bool StatsView::IsAvailable() {
		return Ready("StatsView::IsAvailable");
	}

	bool StatsView::IsOpen() {
		return s_open;
	}

	void StatsView::Open() {
		if (s_open || !Ready("StatsView::Open")) {
			return;
		}
		s_open = true;
		//Refreshed per open: levels, owned ranks and points change as the player plays.
		SendPlayerState();
		s_handle.Show();
		//pauseGame=true: StatsMenu is dismissed as the view opens, so nothing else holds the pause.
		//Paired with the single Unfocus in Hide.
		s_handle.Focus(true);
		s_handle.InvokeByFunctionName(StatsContract::JsFunc::StatsSetVisible, true);
	}

	void StatsView::Hide() {
		if (!s_open) {
			return;
		}
		s_open = false;
		s_handle.InvokeByFunctionName(StatsContract::JsFunc::StatsSetVisible, false);
		s_handle.UnfocusAndHide();
	}
}
