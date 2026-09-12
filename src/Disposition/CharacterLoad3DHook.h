#pragma once

// Sets an NPC's initial disposition the first time their 3D loads in a save, so the value
// is in place before dialogue conditions are evaluated -- including for dialogue the NPC
// initiates, which never routes through TESNPC::Activate.
//
// Load3D can run on the background loading thread, so the thunk only queues work; the
// disposition is calculated and set on the main thread.
class CharacterLoad3DHook {
public:
    static void Apply();

private:
    static RE::NiAVObject* thunk(RE::Actor* actor, bool a_backgroundLoading);

    static inline REL::Relocation<decltype(thunk)> func;
    static constexpr std::size_t idx{ 0x6A };
};
