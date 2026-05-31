#pragma once

namespace Persuasion {

class BribeEconomy {
public:
    static constexpr float kDispositionMax = 100.0f;

    struct Outcome {
        int price;//price to the player
        int gain;//disposition points (0-100 scale) the bribe grants
    };

    [[nodiscard]] static Outcome Calculate(float disposition, float playerSpeech, float npcSpeech, int playerLevel, int npcLevel);
};

}
