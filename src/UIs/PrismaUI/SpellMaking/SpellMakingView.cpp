#include "SpellMakingView.h"
#include "SpellMakingStore.h"
#include "SpellRecipeParser.h"
#include "SpellMakingContract.h"

#include "../PrismaViewHandle.h"

#include <format>
#include <string>

namespace SpellMaking {

	void SpellMakingView::Initialize(const PrismaUIService& service) {
		PrismaFeatureView::Initialize(service, kHtmlPath, kVerifyJsFunc, &SpellMakingView::RegisterListeners);
	}

	void SpellMakingView::RegisterListeners(const PrismaViewHandle& view) {
		view.RegisterListener(SpellMakingContract::Listener::SpellMakingBuy, OnBuy);
		view.RegisterListener(SpellMakingContract::Listener::SpellMakingClose, OnCloseFromJS);
	}

	void SpellMakingView::OnBuy(const char* payload) {
		SpellMakingStore::SpellRecipe recipe;
		int goldPrice = 0;
		if (!SpellRecipeParser::Parse(payload, recipe, goldPrice)) {
			Close();
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Log::WARN("SpellMakingView::OnBuy: Player Unavailable");
			Close();
			return;
		}

		const int playerGold = player->GetGoldAmount();
		if (playerGold < goldPrice) {
			Log::INFO("SpellMakingView::OnBuy: Insufficient gold (have {}, need {}). {} not created.", playerGold, goldPrice, recipe.name);
			Close();
			return;
		}

		auto* spell = SpellMakingStore::CreateSpell(recipe);
		if (!spell) {
			Log::WARN("SpellMakingView::OnBuy: Placeholder pool exhausted or unavailable. {} not created.", recipe.name);
			Close();
			return;
		}

		//Deduct gold.
		if (goldPrice > 0) {
			if (auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F)) {
				player->RemoveItem(gold, goldPrice, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			}
			else {
				Log::ERROR("SpellMakingView::OnBuy: Gold001 (0x0000000F) form missing. {} not created (price {}).", recipe.name, goldPrice);
				return;
			}
		}

		player->AddSpell(spell);
		Log::INFO("SpellMakingView::OnBuy: Created {} ({:08X}), {} Effect(s), Magicka Cost {}, Charged {} Gold.",
			recipe.name, spell->GetFormID(), spell->effects.size(), recipe.magickaCost, goldPrice);

		if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
			const std::string notification = std::format("Congratulations! You have created a new spell: {}", recipe.name);//matches Oblivion message
			auto* args = RE::MakeFunctionArguments(RE::BSFixedString{ notification.c_str() });
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchStaticCall("Debug", "Notification", args, callback);
		}

		Close();
	}

	void SpellMakingView::OnCloseFromJS(const char* closeMethod) {
		Log::DEBUG("SpellMakingView::OnCloseFromJS: {}", closeMethod ? closeMethod : "(null)");
		Close();
	}

	void SpellMakingView::Open() {
		if (!Ready("SpellMakingView::Open")) {
			return;
		}
		s_handle.InvokeByFunctionName(SpellMakingContract::JsFunc::SpellMakingReset);
		s_handle.Show();
		//pauseGame=true: like Oblivion, the world freezes while the player composes a spell.
		s_handle.Focus(true);
	}

	void SpellMakingView::Close() {
		s_handle.UnfocusAndHide();
	}
}
