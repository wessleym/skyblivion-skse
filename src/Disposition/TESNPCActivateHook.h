#pragma once

// Sets the NPC's disposition once per save, the first time the player activates them
// (talk, pickpocket, etc.). Fires before dialogue conditions evaluate, so the disposition
// is in place before lines are selected.
class TESNPCActivateHook {
public:
    static void Apply();

private:
    static bool Activate(
        RE::TESNPC* a_this,
        RE::TESObjectREFR* a_targetRef,
        RE::TESObjectREFR* a_activatorRef,
        std::uint8_t a_arg3,
        RE::TESBoundObject* a_object,
        std::int32_t a_targetCount);

    static inline REL::Relocation<decltype(Activate)> func;
    static constexpr std::size_t idx{ 0x37 };
};
