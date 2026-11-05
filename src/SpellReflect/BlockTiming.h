#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The parry window shared by arrow reflection and timed blocks: remembers when
// the player last pressed Left Attack/Block (input event sink, no polling) and
// sizes the parry window from an attribute ([Arrows] window settings: Speed since the perk
// rework; the timed block's own window is fixed).

namespace SpellReflect::BlockTiming {
    // Register the input sink. Call at SKSE kInputLoaded.
    void Register();

    // Resolve the window actor value. Call at SKSE kDataLoaded.
    void OnDataLoaded();

    // Seconds since the player last pressed Left Attack/Block, or a negative
    // value if it has not been pressed since the game started.
    [[nodiscard]] float SecondsSinceBlockPress();

    // fWindowBase + actor value x fWindowPerPoint, at most fWindowMax.
    [[nodiscard]] float Window(RE::Actor* a_actor);

    // The window actor value's current value (for the log).
    [[nodiscard]] float WindowPoints(RE::Actor* a_actor);
}
