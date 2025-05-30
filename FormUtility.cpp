class FormUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        vm->RegisterFunction("GetAssociatedMenuForm", "SKYBFormUtility", GetAssociatedMenuForm);
    }

private:
    //From https://github.com/NoahBoddie/rogues-gallery-papyrus/blob/main/src/Papyrus.cpp
    //Used with permission
    static std::vector<RE::TESForm*> GetAssociatedMenuForm(RE::BSScript::Internal::VirtualMachine* a_vm,
                                                             RE::VMStackID a_stackID,
                                                             RE::StaticFunctionTag*, RE::BSFixedString a_menu) {
        std::vector<RE::TESForm*> result{};

        // BookMenu - Book / BookRef
        // BarterMenu - Actor / Vendor chest
        // Console - Selected ref
        // Container - ObjectReference
        // GiftMenu - GiftActor
        // Lockpicking Menu - Locked door
        // Training Menu - Training Actor
        // CraftingMenu - Furniture of menu. Kinda hard-ish to do, gonna not handle it for now.
        // Dialogue Menu- Talking actor
        if (a_menu == "BookMenu") {
            result.push_back(RE::BookMenu::GetTargetForm());
            result.push_back(RE::BookMenu::GetTargetReference());
        } else if (a_menu == "ContainerMenu") {
            auto handle = RE::ContainerMenu::GetTargetRefHandle();

            if (handle) {
                auto container = RE::TESObjectREFR::LookupByHandle(handle);

                if (container) {
                    result.push_back(container.get());
                }
            }
        } else if (a_menu == "Console") {
            auto selected = RE::Console::GetSelectedRef();
            result.push_back(selected.get());
        } else if (a_menu == "BarterMenu") {
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
        } else if (a_menu == "Lockpicking Menu") {
            result.push_back(RE::LockpickingMenu::GetTargetReference());
        } else if (a_menu == "Training Menu") {
            // Likely the first thing is the actor but it's not confirmed yet and I don't want to rn
        } else if (a_menu == "GiftMenu") {
            auto handle = RE::GiftMenu::GetTargetRefHandle();

            if (handle) {
                auto vendor = RE::Actor::LookupByHandle(handle);

                if (vendor) {
                    result.push_back(vendor.get());
                }
            }

        } else if (a_menu == "Dialogue Menu") {
            // Not really sure where this one's is
            auto topic_man = RE::MenuTopicManager::GetSingleton();
            result.push_back(topic_man->speaker ? topic_man->speaker.get().get() : nullptr);
            result.push_back(topic_man->lastSpeaker ? topic_man->lastSpeaker.get().get() : nullptr);
        } else {
            // TODO: tell someone
        }

        result.shrink_to_fit();

        return result;
    }
};