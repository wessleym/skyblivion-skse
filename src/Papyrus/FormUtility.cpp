#include "FormUtility.h"

void FormUtility::Register(RE::BSScript::Internal::VirtualMachine* vm) {
	SKSEScriptRegistrar::Register(vm, "SKYBFormUtility", "GetAssociatedMenuForm", GetAssociatedMenuForm);
}

std::vector<RE::TESForm*> FormUtility::GetAssociatedMenuForm(RE::BSScript::Internal::VirtualMachine* /*a_vm*/, RE::VMStackID /*a_stackID*/, RE::StaticFunctionTag*, RE::BSFixedString a_menu) {
	std::vector<RE::TESForm*> result{};

	// BookMenu - Book / BookRef
	// BarterMenu - Actor / Vendor chest
	// Console - Selected ref
	// Container - ObjectReference
	// GiftMenu - GiftActor
	// Lockpicking Menu - Locked door
	// Training Menu - Training Actor
	// CraftingMenu - Furniture of menu. Not handled.
	// Dialogue Menu- Talking actor
	if (a_menu == "BookMenu") {
		result.push_back(RE::BookMenu::GetTargetForm());
		result.push_back(RE::BookMenu::GetTargetReference().get());
	}
	else if (a_menu == "ContainerMenu") {
		auto handle = RE::ContainerMenu::GetTargetRefHandle();

		if (handle) {
			auto container = RE::TESObjectREFR::LookupByHandle(handle);

			if (container) {
				result.push_back(container.get());
			}
		}
	}
	else if (a_menu == "Console") {
		auto selected = RE::Console::GetSelectedRef();
		result.push_back(selected.get());
	}
	else if (a_menu == "BarterMenu") {
		auto handle = RE::BarterMenu::GetTargetRefHandle();

		if (handle) {
			auto vendor = RE::Actor::LookupByHandle(handle);

			if (vendor) {
				result.push_back(vendor.get());
				auto faction = vendor->GetVendorFaction();
				RE::TESObjectREFR* container = faction ? faction->vendorData.merchantContainer : nullptr;
				result.push_back(container);
			}
		}
	}
	else if (a_menu == "Lockpicking Menu") {
		result.push_back(RE::LockpickingMenu::GetTargetReference().get());
	}
	else if (a_menu == "Training Menu") {
		// Not implemented. The trainer actor is the probable target; unverified.
	}
	else if (a_menu == "GiftMenu") {
		auto handle = RE::GiftMenu::GetReceiverRefHandle();

		if (handle) {
			auto receiver = RE::Actor::LookupByHandle(handle);

			if (receiver) {
				result.push_back(receiver.get());
			}
		}

	}
	else if (a_menu == "Dialogue Menu") {
		// Always two entries, speaker then last speaker: callers index them. Each is null when the manager is unavailable.
		auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
		result.push_back(menuTopicManager && menuTopicManager->speaker ? menuTopicManager->speaker.get().get() : nullptr);
		result.push_back(menuTopicManager && menuTopicManager->lastSpeaker ? menuTopicManager->lastSpeaker.get().get() : nullptr);
	}
	else {
		// TODO: report the unsupported menu name
	}

	result.shrink_to_fit();

	return result;
}
