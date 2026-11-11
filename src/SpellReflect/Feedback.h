#pragma once

#include <cstdint>

// Sound + visual when a spell is reflected, an arrow is reflected or a timed
// block lands. Records come from the [Feedback] INI section (editor IDs); the
// visual may be an art object (ARTO) or an effect shader (EFSH).

namespace SpellReflect::Feedback {
    enum class Event : std::uint8_t {
        kSpellReflect,
        kArrowReflect,
        kTimedBlock,
        kTotal
    };

    // Resolve the records. Call at SKSE kDataLoaded.
    void OnDataLoaded();

    // Play the event's sound and visual on a_actor. Safe from any thread: the
    // work is queued to the main thread. Repeats within 0.25 s per actor and
    // event are dropped (concentration spells reflect many times a second).
    void Play(Event a_event, RE::Actor* a_actor);
}
