//Generated from the SKYBPerks<Attribute> FLST records by tools/xEdit/sync-perk-trees.js. Do not edit.
//Perks the editor offers when choosing the PERK behind a rank.
class PerkCatalog {
    public static forTree(treeId: string): PerkCatalogEntry[] {
        const entries = PerkCatalog.byTree[treeId];
        return entries == undefined ? [] : entries;
    }

    private static readonly byTree: { [treeId: string]: PerkCatalogEntry[] } = {
        "Agility": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "088AAE",
                "name": "Brotherhood Cocktail"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088A6C",
                "name": "Cutpurse"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "02B46E",
                "name": "Dual Fury"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "08686B",
                "name": "Dualist"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "08B0C5",
                "name": "Eagle Eyed"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0D6",
                "name": "Hircine's Hunt"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EBF7",
                "name": "Honed Edge"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "08B0C7",
                "name": "Knife Game"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "05820C",
                "name": "Light Foot"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09F734",
                "name": "Marked for Death"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0D0",
                "name": "Mindful Hunter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088A84",
                "name": "Monkey Tricks"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09F72C",
                "name": "Piercer"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09F72D",
                "name": "Pinning Strike"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088A2C",
                "name": "Position of Power"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "058F62",
                "name": "Power Shot"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0A045E",
                "name": "Pure Skill"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EBFC",
                "name": "Serpent Fangs"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "105F24",
                "name": "Silence"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "096B42",
                "name": "Sitting Duck"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0A045F",
                "name": "Sniper"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0899FE",
                "name": "Steady Aim"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EBFA",
                "name": "Trickster's Arsenal"
            }
        ],
        "Endurance": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0C0",
                "name": "Arcane Barrier"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0C3",
                "name": "Basher"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0AA",
                "name": "Brave the Elements"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0B9",
                "name": "Bulwark"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0B3",
                "name": "Complete Set"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088BC7",
                "name": "Digger"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0CE",
                "name": "Disperse"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "05218E",
                "name": "Enchanted Arms"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0AB",
                "name": "Glancing Blows"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0C2",
                "name": "Grit"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088BA0",
                "name": "Hard Labor"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088BA2",
                "name": "Material Secrets"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "058F67",
                "name": "Power Bash"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088BCA",
                "name": "Prospector"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088DA6",
                "name": "Regrowth"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0B2",
                "name": "Relentless"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0C9",
                "name": "Retribution"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0C8",
                "name": "Riposte"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CB9",
                "name": "Timed Block"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0AC",
                "name": "Unyielding"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088679",
                "name": "Weakling's Regret"
            }
        ],
        "Intelligence": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CDF",
                "name": "Ancient Sigils"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "032941",
                "name": "Arcane Focus"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CDE",
                "name": "Bountiful Harvest"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CDD",
                "name": "Coldhearted"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0875A9",
                "name": "Double Dose"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "032943",
                "name": "Dying Embers"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088EA4",
                "name": "Elemental Contract"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D065",
                "name": "Enchanted Aptitude"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CDB",
                "name": "Excessive Casting"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088EAF",
                "name": "Font of Magicka"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088EAD",
                "name": "Frostbite"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088EB0",
                "name": "Hidden Toxin"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CB1",
                "name": "Innate Magic"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "032942",
                "name": "Intense Heat"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "24434A",
                "name": "Magicka Void"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF9",
                "name": "Oghma's Obscurity"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080EF0",
                "name": "Prepared Caster"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0875A7",
                "name": "Purified"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CDC",
                "name": "Raw Destruction"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088EAB",
                "name": "Rune Master"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088E9F",
                "name": "Sapping Sparks"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "033B8B",
                "name": "Strange Diet"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080ED8",
                "name": "Superior Intellect"
            }
        ],
        "Luck": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "084CA5",
                "name": "Bound by Fate"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "084CA4",
                "name": "Certain Path"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE1",
                "name": "Chance Encounter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC16",
                "name": "Clueless Chum"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D025",
                "name": "Decimation"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC15",
                "name": "Doomed Denizen"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC25",
                "name": "Draw of Destiny"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0892FC",
                "name": "Fluke"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC1F",
                "name": "Forced Fate"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D018",
                "name": "Lucky Break"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC08",
                "name": "Marked"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE0",
                "name": "Nocturnal's Nod"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088DB0",
                "name": "Nose for Coin"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D01E",
                "name": "Opportunist"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D01F",
                "name": "Razor's Edge"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC0A",
                "name": "Ringleader"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "08BBE3",
                "name": "Scavenger"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0892FF",
                "name": "Treasure Hunter"
            }
        ],
        "Personality": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF1",
                "name": "Academic Connections"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CF0",
                "name": "Adoring Fan"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF8",
                "name": "Altered Perspective"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF0",
                "name": "Beloved Laborer"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CEF",
                "name": "Bewitched"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FEF",
                "name": "Business Contract"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CF1",
                "name": "Captivate"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE6",
                "name": "Dream Visitor"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE9",
                "name": "Encouragement"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE2",
                "name": "Face to Face"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CEE",
                "name": "Fearstruck"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CF2",
                "name": "Helpless Hostage"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF6",
                "name": "Influential Presence"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE4",
                "name": "Inspiration"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "28F5E1",
                "name": "Investor"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "058F7A",
                "name": "Merchant"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D03C",
                "name": "Person of Interest"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0892D5",
                "name": "Piety"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FF3",
                "name": "Quiet Casting"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE7",
                "name": "Refined Rage"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE3",
                "name": "Reputation"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CEB",
                "name": "Sacred Idol"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EBFD",
                "name": "Select Stock"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CEC",
                "name": "Siphon"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE8",
                "name": "Spreading Hate"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "1A8CE5",
                "name": "Veil of Infamy"
            }
        ],
        "Speed": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CF3",
                "name": "Acrobat"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0163A0",
                "name": "Bounding"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E134",
                "name": "Dodger"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E129",
                "name": "Endless Runner"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EC05",
                "name": "Exhausting"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "016398",
                "name": "Heightened Reflexes"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E133",
                "name": "Matching Set"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "029E76",
                "name": "Moving Target"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "04EBFE",
                "name": "Out of Focus"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E131",
                "name": "Panic"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D033",
                "name": "Proper Breathing"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E1C0",
                "name": "Rampage"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E12E",
                "name": "Rapid Assault"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E135",
                "name": "Slippery"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "01639A",
                "name": "Sprinter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E130",
                "name": "Weightless"
            }
        ],
        "Strength": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "193E33",
                "name": "Barbarian's Rush"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "052CE0",
                "name": "Brute Force"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088ECA",
                "name": "Champion's Rush"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "07F494",
                "name": "Cruel Cuts"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "07CEE9",
                "name": "Crushing Blow"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "078915",
                "name": "Cull"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "052D52",
                "name": "Devastating Blow"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "088BCD",
                "name": "Disorient"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "05E283",
                "name": "Exhaust"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "101E50",
                "name": "Heavy Handed"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "05DD92",
                "name": "Intense Training"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080E2D",
                "name": "Iron Fist"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "064611",
                "name": "Knockout"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080E0F",
                "name": "Lambs to Slaughter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "078F38",
                "name": "Precision"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "064606",
                "name": "Pugilist"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "03AF81",
                "name": "Savage Strike"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "101E24",
                "name": "Scrappy Fighter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080E0E",
                "name": "Shatter"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "06C324",
                "name": "Strong Back"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E0CC",
                "name": "Sweep"
            }
        ],
        "Willpower": [
            {
                "plugin": "Skyblivion.esm",
                "formId": "09304B",
                "name": "Balanced Scales"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "028D2B",
                "name": "Concentration"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0A2E8B",
                "name": "Conjured Might"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "03D511",
                "name": "Elemental Shielding"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E139",
                "name": "Essence Drain"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "093083",
                "name": "Eternal Shroud"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0307ED",
                "name": "Familiar by Thy Side"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080EEC",
                "name": "Free Flowing"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D067",
                "name": "Intense Defense"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09305C",
                "name": "Lifeward"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080EEF",
                "name": "Limitless Spring"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "023022",
                "name": "Magical Aptitude"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FE1",
                "name": "Master Summoner"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D00A",
                "name": "Miracle"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "093059",
                "name": "Necromage"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D00F",
                "name": "Oblivion Bound"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "09D011",
                "name": "Partial Infusion"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B205E",
                "name": "Parting Gift"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "13E1C9",
                "name": "Reaper's Bounty"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0B1FE0",
                "name": "Respite"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0AE193",
                "name": "Second Skin"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "080EDA",
                "name": "Strategist"
            },
            {
                "plugin": "Skyrim.esm",
                "formId": "068BCC",
                "name": "Ward Absorb"
            },
            {
                "plugin": "Skyblivion.esm",
                "formId": "0A2E79",
                "name": "Wizard's Bastion"
            }
        ]
    };
}
