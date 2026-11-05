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

    //The same two, applied at once on the calling thread (a game task that already runs on it).
    static void SetNow(RE::Actor* actor, Expression expression, float value);
    static void ResetNow(RE::Actor* actor) { SetNow(actor, Expression::Anger, 0.0f); }
    //Zeroes the dialogue phonemes (closes the mouth) and leaves the expression alone.
    static void CloseMouthNow(RE::Actor* actor);
};
