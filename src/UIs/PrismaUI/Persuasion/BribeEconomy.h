#pragma once

namespace Persuasion {

//Oblivion's bribery formula with every constant exposed. The values come from SKYBPersuasion.ini
//([Bribe]) through PersuasionRules::BribeParams, perks and skill rules laid over them. The
//Speechcraft 100 price halving is a costMult rule of SKYBPersuasionPerks.ini (or of the built-in
//rules in PersuasionPerks.cpp when that file is missing).
class BribeEconomy {
public:
    struct Params {
        float priceCurve;              //exponent on combined speech in the price
        float gainCurve;               //exponent on combined speech in the gain
        float npcLevelMult;            //weight of NPC level vs. player level
        float scale;                   //global price multiplier
        float speechcraftMult;         //weight of speechcraft vs. levels in the price
        float priceScale;              //overall price multiplier on top of scale
        float gainScale;               //overall gain multiplier on the speech-curve term
        float dispositionPriceDivisor; //dispositionMult = 1 + disposition / divisor (smaller = steeper)
        float costMult;                //settings menu and perks: x on the price
        float gainMult;                //settings menu and perks: x on the gain
        int   minPrice;
        int   minGain;
        float maxDisposition;          //the gain's falloff reaches 0 here (floored at 5 %)
    };

    struct Outcome {
        int price;//price to the player
        int gain;//disposition points the bribe grants
        float exactGain;//the gain before rounding and MinGain (under 1 the wheel offers no bribe)
    };

    [[nodiscard]] static Outcome Calculate(const Params& params, float disposition, float playerSpeech, float npcSpeech, int playerLevel, int npcLevel);
};

}
