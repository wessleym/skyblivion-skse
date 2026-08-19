#pragma once

// Sets the NPC's disposition once per save, the first time the player activates them
// (talk, pickpocket, etc.). Fires before dialogue conditions evaluate, so the disposition
// is in place before lines are selected.
class CharacterLoad3D {
public:
    static void Apply();

private:
    static RE::NiAVObject* thunk(RE::Actor* actor, bool a_backgroundLoading);

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx{ 0x6A };
};
