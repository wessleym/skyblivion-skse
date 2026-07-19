#pragma once

#include <cstdint>

namespace RE { class Actor; }

// Sets or resets NPC facial expression
class ActorExpression {
public:
    //Strong reactions are required for them to be visible on the NPC's face.
    enum class Expression : std::uint32_t {
        Anger = 0,//DialogueAnger
        Happy = 2,//DialogueHappy
        MoodAnger = 8,//MoodAnger
        MoodHappy = 10,//MoodHappy
    };

    static void Set(RE::Actor* actor, Expression expression, float value);
    static void Reset(RE::Actor* actor);
};
