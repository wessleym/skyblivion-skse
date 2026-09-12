"use strict";
//Generated from PerkTrees.json by tools/xEdit/sync-perk-trees.js. Do not edit.
//Fallback tree document, used when no perk-trees.js sits beside index.html.
class InitialPerkTrees {
    static createDocument() {
        return new PerkTreeDocument(InitialPerkTrees.data);
    }
}
InitialPerkTrees.data = {
    "trees": [
        {
            "id": "Strength",
            "name": "Strength",
            "rootId": "Strength/node_1900000000015",
            "nodes": [
                {
                    "id": "Strength/node_1900000000001",
                    "name": "Devastating Blow",
                    "description": "Standing power attacks do 25% bonus damage with a chance to decapitate your enemies.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "052D52",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000002",
                    "name": "Savage Strike",
                    "description": "One-handed standing power attacks do 25% bonus damage with a chance to decapitate your enemies.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "03AF81",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000003",
                    "name": "Cull",
                    "description": "Axes cause enemies to bleed for 4 seconds longer.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "078915",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "078C2C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F576",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000004",
                    "name": "Lambs to Slaughter",
                    "description": "Deal 10% more damage to bleeding targets and gain a 10% critical hit chance against them.",
                    "parentIds": [
                        "Strength/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080E0F",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1F32A2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000005",
                    "name": "Precision",
                    "description": "Blades gain an additional 10% critical hit chance.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "078F38",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "078F42",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F579",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000006",
                    "name": "Crushing Blow",
                    "description": "Mace and warhammer power attacks deal an additional 50% damage against blocking targets, and critical hits have a chance to knock a target to the ground.",
                    "parentIds": [
                        "Strength/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "07CEE9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "20B9C0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000007",
                    "name": "Shatter",
                    "description": "Hammers and maces ignore 50% of an enemy's armor.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080E0E",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "07CEE8",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F578",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000008",
                    "name": "Brute Force",
                    "description": "Force open weaker (Apprentice) locks. This creates a loud noise and draws the attention of anyone nearby.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CE0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000009",
                    "name": "Strong Back",
                    "description": "Increases carry weight by 30 points.",
                    "parentIds": [
                        "Strength/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "06C324",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "06E913",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "077C0B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F575",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000010",
                    "name": "Champion's Rush",
                    "description": "Killing a boss enemy grants +3 Strength. This can stack up to 10 times, and you lose a stack every 48 hours.",
                    "parentIds": [
                        "Strength/node_1900000000004",
                        "Strength/node_1900000000006",
                        "Strength/node_1900000000021"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088ECA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F597",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000011",
                    "name": "Disorient",
                    "description": "One-handed attacks reduce an enemy's damage output by 25% for 2 seconds.",
                    "parentIds": [
                        "Strength/node_1900000000002"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088BCD",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "193E2C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000012",
                    "name": "Exhaust",
                    "description": "Unarmed attacks deal 6 points of Stamina damage and prevent targets from regenerating Stamina for 6 seconds.",
                    "parentIds": [
                        "Strength/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "05E283",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "066E3F",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F577",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000013",
                    "name": "Heavy Handed",
                    "description": "Unarmed strikes deal bonus damage equal to 2% of your armor rating.",
                    "parentIds": [
                        "Strength/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "101E50",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000014",
                    "name": "Intense Training",
                    "description": "Deal 5% more melee damage and gain 10 additional carry weight.",
                    "parentIds": [
                        "Strength/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "05DD92",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CA9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CAA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CAB",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000015",
                    "name": "Iron Fist",
                    "description": "Unarmed strikes deal an additional 5 points of damage.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080E2D",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "05DE2C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "05DE6A",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F574",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000016",
                    "name": "Knockout",
                    "description": "Unarmed attacks gain a stagger chance based on the target's missing Stamina. When enemies are below 25%, critical hits have a chance to knock them to the ground.",
                    "parentIds": [
                        "Strength/node_1900000000018"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "064611",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000018",
                    "name": "Pugilist",
                    "description": "While unarmed, you gain a 5% critical hit chance for every 20% of Stamina an enemy is missing.",
                    "parentIds": [
                        "Strength/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "064606",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000019",
                    "name": "Scrappy Fighter",
                    "description": "Unarmed attacks deal 2% of your current Stamina while you are not wearing heavy gauntlets.",
                    "parentIds": [
                        "Strength/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "101E24",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000020",
                    "name": "Sweep",
                    "description": "Power attacks with two-handed weapons hit all targets in front of you.",
                    "parentIds": [
                        "Strength/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0CC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "193E30",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000021",
                    "name": "Cruel Cuts",
                    "description": "Blades deal an additional 150% critical damage.",
                    "parentIds": [
                        "Strength/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "07F494",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D02C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Strength/node_1900000000022",
                    "name": "Barbarian's Rush",
                    "description": "Perform a power attack while sprinting that deals double critical damage.",
                    "parentIds": [
                        "Strength/node_1900000000020"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "193E33",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Intelligence",
            "name": "Intelligence",
            "rootId": "Intelligence/node_1900000000008",
            "nodes": [
                {
                    "id": "Intelligence/node_1900000000001",
                    "name": "Ancient Sigils",
                    "description": "Runes now provide an additional effect depending on their type.",
                    "parentIds": [
                        "Intelligence/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDF",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000002",
                    "name": "Arcane Focus",
                    "description": "Gain 25% spell effectiveness when wielding a staff.",
                    "parentIds": [
                        "Intelligence/node_1900000000010"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "032941",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F583",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000003",
                    "name": "Bountiful Harvest",
                    "description": "Gain a chance to harvest bonus ingredients from dead enemies and plants.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CDE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000004",
                    "name": "Double Dose",
                    "description": "Poisons now apply for twice as many hits.",
                    "parentIds": [
                        "Intelligence/node_1900000000013"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0875A9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000006",
                    "name": "Dying Embers",
                    "description": "Flame spells deal 20% more damage to enemies below 50% health and cost 10% less Magicka when you are below 50% Magicka.",
                    "parentIds": [
                        "Intelligence/node_1900000000007",
                        "Intelligence/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "032943",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CAC",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000007",
                    "name": "Elemental Contract",
                    "description": "Choose Fire, Frost, or Shock as an element. Spells of that element are 35% stronger and 10% cheaper, but spells of the other elements are 75% weaker.",
                    "parentIds": [
                        "Intelligence/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EA4",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000008",
                    "name": "Enchanted Aptitude",
                    "description": "All equipped enchantments are 7% stronger.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D065",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CAD",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CAE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000009",
                    "name": "Excessive Casting",
                    "description": "Dual cast spells for 3x the power and 4x the cost.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CDB",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000010",
                    "name": "Font of Magicka",
                    "description": "While wielding a staff, spells cost 10% less and you regenerate Magicka 50% faster.",
                    "parentIds": [
                        "Intelligence/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EAF",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F584",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000011",
                    "name": "Frostbite",
                    "description": "Cold spells drain twice as much Stamina, and projectile frost spells have a chance to paralyze a target at low Stamina.",
                    "parentIds": [
                        "Intelligence/node_1900000000007",
                        "Intelligence/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EAD",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CB0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000012",
                    "name": "Coldhearted",
                    "description": "Frost spells increase in magnitude equal to 50% of your frost resistance.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CDD",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000013",
                    "name": "Hidden Toxin",
                    "description": "Created poisons are 25% stronger, and those who have not detected you are 50% weaker to poisons.",
                    "parentIds": [
                        "Intelligence/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EB0",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F590",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000014",
                    "name": "Innate Magic",
                    "description": "Novice spells of any school cost 75% less Magicka to cast.",
                    "parentIds": [
                        "Intelligence/node_1900000000009"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CB1",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000015",
                    "name": "Intense Heat",
                    "description": "Enemies within 15 feet are 25% weaker to fire damage, and enemies within 5 feet are 40% weaker.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "032942",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F586",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000016",
                    "name": "Oghma's Obscurity",
                    "description": "Gain 2% spell effectiveness per 100 books read, up to 8% at 400. Scrolls are 50% stronger, and reading new books provides scroll crafting materials.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D072",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000018",
                    "name": "Prepared Caster",
                    "description": "While not in combat, spell effectiveness increases by 30% and spells cast on you last 50% longer.",
                    "parentIds": [
                        "Intelligence/node_1900000000009"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080EF0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000019",
                    "name": "Purified",
                    "description": "Potions lose all negative effects, and poisons lose all positive effects.",
                    "parentIds": [
                        "Intelligence/node_1900000000024"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0875A7",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000020",
                    "name": "Raw Destruction",
                    "description": "Non-elemental Destruction spells are 30% stronger.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CDC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDB",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000021",
                    "name": "Rune Master",
                    "description": "Place runes three times farther away, and place one additional rune.",
                    "parentIds": [
                        "Intelligence/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EAB",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088EAC",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000022",
                    "name": "Sapping Sparks",
                    "description": "Shock spells drain twice as much Magicka and deal increased damage against targets with large Magicka reserves.",
                    "parentIds": [
                        "Intelligence/node_1900000000007",
                        "Intelligence/node_1900000000023"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088E9F",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F585",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000023",
                    "name": "Magicka Void",
                    "description": "Shock spells have a chance to overload targets with depleted Magicka, dealing 30 points of shock damage to nearby targets. This effect can occur once every 60 seconds.",
                    "parentIds": [
                        "Intelligence/node_1900000000025"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "24434A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000024",
                    "name": "Strange Diet",
                    "description": "Eating an alchemy ingredient reveals 1 additional effect.",
                    "parentIds": [
                        "Intelligence/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "033B8B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "033B8D",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0875A5",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Intelligence/node_1900000000025",
                    "name": "Superior Intellect",
                    "description": "Gain 3% more experience with all skills and 3% spell effectiveness.",
                    "parentIds": [
                        "Intelligence/node_1900000000008",
                        "Intelligence/node_1900000000021"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080ED8",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDD",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDE",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5C2",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Willpower",
            "name": "Willpower",
            "rootId": "Willpower/node_1900000000012",
            "nodes": [
                {
                    "id": "Willpower/node_1900000000001",
                    "name": "Balanced Scales",
                    "description": "Spell costs are 5% lower while above 40% health, and spell effectiveness is 10% higher while below 60% health.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09304B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09304F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000002",
                    "name": "Concentration",
                    "description": "If wearing robes, you take 20% less damage from attacks while charging or concentrating on a spell.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "028D2B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F582",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000003",
                    "name": "Conjured Might",
                    "description": "Each piece of bound armor equipped provides +10 to Health, Magicka, and Stamina, and lasts 150% longer.",
                    "parentIds": [
                        "Willpower/node_1900000000012",
                        "Willpower/node_1900000000020"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0A2E8B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0AE16A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000004",
                    "name": "Elemental Shielding",
                    "description": "Elemental Shield spells cause nearby enemies to take 5 points of fire, frost, or shock damage per second, based on which Elemental Shield you have active.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "03D511",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000005",
                    "name": "Essence Drain",
                    "description": "When you kill an NPC, you regenerate 20 Stamina and Magicka over 4 seconds.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E139",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000006",
                    "name": "Eternal Shroud",
                    "description": "Defensive spells last 25% longer. This bonus is tripled while wearing robes.",
                    "parentIds": [
                        "Willpower/node_1900000000004"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "093083",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "093084",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000007",
                    "name": "Familiar by Thy Side",
                    "description": "Gain a small bonus based on which summoned creatures you have under your control.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0307ED",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F589",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000008",
                    "name": "Free Flowing",
                    "description": "Gain 5% spell effectiveness while wearing a robe, and an additional 5% if not wearing armor.",
                    "parentIds": [
                        "Willpower/node_1900000000011"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080EEC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F581",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000009",
                    "name": "Intense Defense",
                    "description": "While you have an Elemental Shield spell active, being struck by an attacker has a chance to cause an explosion, dealing 30 points of fire, frost, or shock damage to nearby targets, based on which Elemental Shield is currently active.",
                    "parentIds": [
                        "Willpower/node_1900000000004"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D067",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000010",
                    "name": "Lifeward",
                    "description": "While above 97% health, you gain 500 armor and 50% magic resistance.",
                    "parentIds": [
                        "Willpower/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09305C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F587",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000011",
                    "name": "Limitless Spring",
                    "description": "Spells are 8% cheaper to cast while wearing a robe, and an additional 6% cheaper if wearing no armor.",
                    "parentIds": [
                        "Willpower/node_1900000000002",
                        "Willpower/node_1900000000006",
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080EEF",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F580",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000012",
                    "name": "Magical Aptitude",
                    "description": "Magicka regenerates 15% faster. Those under the Atronach sign gain an additional 15 Magicka.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "023022",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0AA36A",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "023023",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5C3",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000013",
                    "name": "Miracle",
                    "description": "When you drop below 20% health, once per day you are restored to full Health, Magicka, and Stamina.",
                    "parentIds": [
                        "Willpower/node_1900000000010"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D00A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000014",
                    "name": "Necromage",
                    "description": "Spells against the dead are 25% stronger and last 50% longer, with double strength for Turn Undead and Sun Damage spells.",
                    "parentIds": [
                        "Willpower/node_1900000000001",
                        "Willpower/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "093059",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000015",
                    "name": "Oblivion Bound",
                    "description": "Enemies struck by bound weapons take 50% more damage from summoned creatures.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D00F",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5CC",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000016",
                    "name": "Partial Infusion",
                    "description": "Bound weapons gain an additional effect based on the type of summons you have, and last 50% longer.",
                    "parentIds": [
                        "Willpower/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D011",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000017",
                    "name": "Parting Gift",
                    "description": "When a summon dies, it restores a small amount of health, Stamina, and Magicka based on its power.",
                    "parentIds": [
                        "Willpower/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B205E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000018",
                    "name": "Reaper's Bounty",
                    "description": "Essence Drain lasts twice as long and also restores health.",
                    "parentIds": [
                        "Willpower/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E1C9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58B",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000019",
                    "name": "Respite",
                    "description": "Any source of healing grows stronger at low health, including absorb spells. Restore health spells also restore half their value in Stamina.",
                    "parentIds": [
                        "Willpower/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FE0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000020",
                    "name": "Second Skin",
                    "description": "Bound armor is treated as both armor and robes by all perks.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0AE193",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000021",
                    "name": "Strategist",
                    "description": "Conjure creatures up to 5x farther away, and they last 25% longer when summoned out of combat.",
                    "parentIds": [
                        "Willpower/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "080EDA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000022",
                    "name": "Master Summoner",
                    "description": "Conjure or reanimate one additional minion.",
                    "parentIds": [
                        "Willpower/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FE1",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FE2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000023",
                    "name": "Wizard's Bastion",
                    "description": "Defensive spells are 15% stronger. This bonus is tripled while wearing robes.",
                    "parentIds": [
                        "Willpower/node_1900000000006"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0A2E79",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0A2E86",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Willpower/node_1900000000024",
                    "name": "Ward Absorb",
                    "description": "Wards recharge your magicka when hit with spells, absorbing a third of the spell's magicka.",
                    "parentIds": [
                        "Willpower/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "068BCC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F588",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Agility",
            "name": "Agility",
            "rootId": "Agility/node_1900000000005",
            "nodes": [
                {
                    "id": "Agility/node_1900000000001",
                    "name": "Light Foot",
                    "description": "You won't trigger pressure plates.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "05820C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000002",
                    "name": "Power Shot",
                    "description": "Arrows stagger all but the largest opponents 50% of the time.",
                    "parentIds": [
                        "Agility/node_1900000000017"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "058F62",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000003",
                    "name": "Silence",
                    "description": "Walking and running does not affect detection.",
                    "parentIds": [
                        "Agility/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "105F24",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000004",
                    "name": "Brotherhood Cocktail",
                    "description": "Reverse pickpocket poisons onto enemies.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088AAE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000005",
                    "name": "Cutpurse",
                    "description": "Gold, gems, and keys are 50% easier to steal.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088A6C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58D",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000006",
                    "name": "Dual Fury",
                    "description": "Dual-wielding attacks do 20% bonus damage, and dual-wielding power attacks do 50% bonus damage.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "02B46E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000007",
                    "name": "Dualist",
                    "description": "Dual-wielding attacks are 25% faster.",
                    "parentIds": [
                        "Agility/node_1900000000006"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "08686B",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000008",
                    "name": "Eagle Eyed",
                    "description": "Time slows by 30% while zooming with your bow.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "08B0C5",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "08B0C6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000009",
                    "name": "Hircine's Hunt",
                    "description": "While exploring, random animals will be marked with a golden glow. Killing a marked animal levels a random skill and provides additional loot.",
                    "parentIds": [
                        "Agility/node_1900000000010"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0D6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000010",
                    "name": "Honed Edge",
                    "description": "Sneak attacks deal 25% more damage, and all attacks deal 1 additional damage.",
                    "parentIds": [
                        "Agility/node_1900000000001",
                        "Agility/node_1900000000005",
                        "Agility/node_1900000000007",
                        "Agility/node_1900000000011"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBF7",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CB1",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CB2",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0891C4",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000011",
                    "name": "Knife Game",
                    "description": "While wielding a dagger, you attack 15% faster.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "08B0C7",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000012",
                    "name": "Marked for Death",
                    "description": "Your critical hit chance with a bow increases as a target's health falls, gaining a 5% chance for every 20% of health missing.",
                    "parentIds": [
                        "Agility/node_1900000000020"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09F734",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000013",
                    "name": "Mindful Hunter",
                    "description": "Recover almost all arrows from dead enemies.",
                    "parentIds": [
                        "Agility/node_1900000000022"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0D0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000014",
                    "name": "Monkey Tricks",
                    "description": "Pickpocket equipped weapons from NPCs.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088A84",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088AAD",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000015",
                    "name": "Piercer",
                    "description": "Arrows ignore 30% of a target's armor.",
                    "parentIds": [
                        "Agility/node_1900000000013"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09F72C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "096B2E",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5C1",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000016",
                    "name": "Pinning Strike",
                    "description": "Enemies shot with a bow have a 10% chance to be paralyzed for 5 seconds.",
                    "parentIds": [
                        "Agility/node_1900000000002"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09F72D",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000017",
                    "name": "Position of Power",
                    "description": "While standing still, you deal 100% more critical hit damage and gain a 25% critical hit chance.",
                    "parentIds": [
                        "Agility/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088A2C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000018",
                    "name": "Pure Skill",
                    "description": "Bows gain 50% critical damage and a 10% critical chance per 50 feet travelled, up to a maximum of 200 feet.",
                    "parentIds": [
                        "Agility/node_1900000000012",
                        "Agility/node_1900000000022"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0A045E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000019",
                    "name": "Serpent Fangs",
                    "description": "Your dagger attacks deal 3 poison damage per second on hit and reduce an enemy's magic resistance by 5% for 10 seconds. This effect can stack. Sneak attacks apply 10 times as many stacks.",
                    "parentIds": [
                        "Agility/node_1900000000011"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFC",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000020",
                    "name": "Sitting Duck",
                    "description": "Enemies that are not moving take an additional 25% damage. This is doubled for paralyzed enemies.",
                    "parentIds": [
                        "Agility/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "096B42",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F58C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000022",
                    "name": "Sniper",
                    "description": "Bows deal 10% more damage for every 15 feet the target is away from you, up to double damage at 150 feet.",
                    "parentIds": [
                        "Agility/node_1900000000010",
                        "Agility/node_1900000000024"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0A045F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000023",
                    "name": "Steady Aim",
                    "description": "Zoom in with a bow to get a better view of your target.",
                    "parentIds": [
                        "Agility/node_1900000000008",
                        "Agility/node_1900000000022"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0899FE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Agility/node_1900000000024",
                    "name": "Trickster's Arsenal",
                    "description": "Allows you to craft minor enchanted arrows.",
                    "parentIds": [
                        "Agility/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFB",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Speed",
            "name": "Speed",
            "rootId": "Speed/node_1900000000005",
            "nodes": [
                {
                    "id": "Speed/node_1900000000001",
                    "name": "Acrobat",
                    "description": "Falling damage is reduced by 25%.",
                    "parentIds": [
                        "Speed/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CF3",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CF5",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CF6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000002",
                    "name": "Bounding",
                    "description": "Jump 60% higher.",
                    "parentIds": [
                        "Speed/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0163A0",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0163F2",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "029E75",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000003",
                    "name": "Dodger",
                    "description": "While wearing a light armor chest piece, you have a 6% chance to avoid incoming damage. This bonus doubles if wearing all light armor.",
                    "parentIds": [
                        "Speed/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E134",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000004",
                    "name": "Endless Runner",
                    "description": "Stamina regenerates almost instantly when out of combat.",
                    "parentIds": [
                        "Speed/node_1900000000011"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E129",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000005",
                    "name": "Exhausting",
                    "description": "Gain 50 armor and 5% magic resistance for every 20 points of Speed while above half Stamina.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC05",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000006",
                    "name": "Heightened Reflexes",
                    "description": "When an enemy is about to land a killing blow, time slows down.",
                    "parentIds": [
                        "Speed/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "016398",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5C7",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000007",
                    "name": "Matching Set",
                    "description": "Gain 30% more armor if wearing a light armor chest piece. If you are wearing all light armor, this bonus doubles.",
                    "parentIds": [
                        "Speed/node_1900000000005",
                        "Speed/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E133",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000008",
                    "name": "Moving Target",
                    "description": "While sprinting, Destruction spells and arrows have a 25% chance to deal no damage to you.",
                    "parentIds": [
                        "Speed/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "029E76",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "02B46D",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000009",
                    "name": "Out of Focus",
                    "description": "While wearing a light armor chest piece, you gain a 10% chance to reflect spells back at their casters. This is doubled while wearing all light armor.",
                    "parentIds": [
                        "Speed/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFE",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFF",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000010",
                    "name": "Panic",
                    "description": "Gain 25 Speed while below 30% health.",
                    "parentIds": [
                        "Speed/node_1900000000006"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E131",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000011",
                    "name": "Proper Breathing",
                    "description": "Restore all Stamina at the start of combat, and regenerate it rapidly for the first 5 seconds.",
                    "parentIds": [
                        "Speed/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D033",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5C6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000012",
                    "name": "Rapid Assault",
                    "description": "Attack speed increases by 7%.",
                    "parentIds": [
                        "Speed/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E12E",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E12F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000014",
                    "name": "Slippery",
                    "description": "While wearing a light armor chest piece, you take 10% less damage while sprinting. This bonus doubles if wearing all light armor.",
                    "parentIds": [
                        "Speed/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E135",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E136",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000015",
                    "name": "Sprinter",
                    "description": "Gain 5 Speed while sprinting.",
                    "parentIds": [
                        "Speed/node_1900000000001",
                        "Speed/node_1900000000004",
                        "Speed/node_1900000000007",
                        "Speed/node_1900000000008",
                        "Speed/node_1900000000012"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "01639A",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "01639B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5CE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000016",
                    "name": "Weightless",
                    "description": "Heavy armor slows you 75% less, and light armor no longer slows you at all.",
                    "parentIds": [
                        "Speed/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E130",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Speed/node_1900000000017",
                    "name": "Rampage",
                    "description": "Killing an enemy increases damage dealt by 6% for 30 seconds, stacking up to 60%. Each kill resets the duration.",
                    "parentIds": [
                        "Speed/node_1900000000010"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E1C0",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Endurance",
            "name": "Endurance",
            "rootId": "Endurance/node_1900000000016",
            "nodes": [
                {
                    "id": "Endurance/node_1900000000001",
                    "name": "Arcane Blacksmith",
                    "description": "You can improve magical weapons and armor.",
                    "parentIds": [
                        "Endurance/node_1900000000009"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "05218E",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000002",
                    "name": "Power Bash",
                    "description": "Able to do a power bash.",
                    "parentIds": [
                        "Endurance/node_1900000000018"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "058F67",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C4",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000003",
                    "name": "Arcane Barrier",
                    "description": "While blocking, you gain 10% resistance to magic damage. This bonus doubles with a shield.",
                    "parentIds": [
                        "Endurance/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C0",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C1",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000004",
                    "name": "Basher",
                    "description": "Bashing deals 2% more damage per point of Stamina.",
                    "parentIds": [
                        "Endurance/node_1900000000002",
                        "Endurance/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C3",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000005",
                    "name": "Brave the Elements",
                    "description": "Gain elemental resistance based on your missing health, up to 20% at half health.",
                    "parentIds": [
                        "Endurance/node_1900000000012",
                        "Endurance/node_1900000000016",
                        "Endurance/node_1900000000020"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0AA",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000006",
                    "name": "Retribution",
                    "description": "While wearing a heavy armor chest piece, you reflect 25% of all physical damage back at nearby attackers, though you still take the damage. This bonus doubles if wearing all heavy armor.",
                    "parentIds": [
                        "Endurance/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0CA",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000007",
                    "name": "Bulwark",
                    "description": "Gain 8% magic resistance while wearing a heavy armor chest piece. This bonus doubles if wearing all heavy armor.",
                    "parentIds": [
                        "Endurance/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0B9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0BA",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000008",
                    "name": "Complete Set",
                    "description": "Gain 40% extra armor if wearing a heavy armor chest piece. This bonus doubles if wearing all heavy armor.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0B3",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57C",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000009",
                    "name": "Digger",
                    "description": "All ore veins produce 2 additional pieces of ore, and the chance to find gems is doubled.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088BC7",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5D6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000010",
                    "name": "Disperse",
                    "description": "While blocking, you gain a 30% chance to absorb spells. This bonus doubles with a shield.",
                    "parentIds": [
                        "Endurance/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0CE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000011",
                    "name": "Glancing Blows",
                    "description": "Gain a 10% chance to take half damage from any physical strike if wearing a heavy armor chest piece. If wearing all heavy armor, this chance doubles.",
                    "parentIds": [
                        "Endurance/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0AB",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57D",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000012",
                    "name": "Grit",
                    "description": "Melee attacks deal 3 more damage per 20% of health missing. This bonus is doubled for unarmed attacks.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C2",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F57B",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000013",
                    "name": "Hard Labor",
                    "description": "Study 4 ingots of a material at a forge to gain a better understanding of it. Any material you have studied can be improved twice as much.",
                    "parentIds": [
                        "Endurance/node_1900000000009"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088BA0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000014",
                    "name": "Material Secrets",
                    "description": "Any material you have studied with Hard Labor now also provides a small bonus per piece worn.",
                    "parentIds": [
                        "Endurance/node_1900000000013"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088BA2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000015",
                    "name": "Prospector",
                    "description": "Gain a lesser power to detect nearby precious materials: ore veins, ore, gems, and ingots. When you enter an area, there is a 15% chance that ore veins will be Rich, granting 4x the materials.",
                    "parentIds": [
                        "Endurance/node_1900000000009"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088BCA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5D5",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000016",
                    "name": "Regrowth",
                    "description": "Passively regenerate health when below 25% health.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088DA6",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088DA8",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088DA7",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088DAA",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000017",
                    "name": "Relentless",
                    "description": "Regenerate Stamina faster based on missing health.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0B2",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5CD",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000018",
                    "name": "Riposte",
                    "description": "After bashing an enemy, you have an additional 50% critical chance against them.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0C8",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000019",
                    "name": "Timed Block",
                    "description": "For the first second of blocking you take 30% less damage. Timed block can only be used once every 2 seconds.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "052CB9",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000020",
                    "name": "Unyielding",
                    "description": "Take less damage based on missing health, up to a maximum of 20% reduction at 20% health.",
                    "parentIds": [
                        "Endurance/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0AC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "13E0AD",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Endurance/node_1900000000021",
                    "name": "Weakling's Regret",
                    "description": "When wearing a heavy chest piece, you gain a 5% chance to reflect 300% additional damage back at your attacker. If wearing all heavy armor, this chance increases to 10%.",
                    "parentIds": [
                        "Endurance/node_1900000000006"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088679",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Personality",
            "name": "Personality",
            "rootId": "Personality/node_1900000000019",
            "nodes": [
                {
                    "id": "Personality/node_1900000000001",
                    "name": "Investor",
                    "description": "Invest 500 gold in a store, increasing the gold it has to barter with and giving it a chance to stock additional higher-quality goods.",
                    "parentIds": [
                        "Personality/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5E1",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5E2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000002",
                    "name": "Merchant",
                    "description": "Can sell any type of item to any kind of merchant.",
                    "parentIds": [
                        "Personality/node_1900000000023"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyrim.esm",
                            "formId": "058F7A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000003",
                    "name": "Academic Connections",
                    "description": "Bookstores, alchemists, and magic stores sell their goods to you for 15% less and stock more materials.",
                    "parentIds": [
                        "Personality/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF1",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000004",
                    "name": "Adoring Fan",
                    "description": "Charmed NPCs will not mind if you take their goods, and merchants offer you the best possible prices when you buy from them.",
                    "parentIds": [
                        "Personality/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CF0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000005",
                    "name": "Altered Perspective",
                    "description": "Illusion spells work on undead and daedra.",
                    "parentIds": [
                        "Personality/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF8",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000006",
                    "name": "Beloved Laborer",
                    "description": "Smiths and general goods stores stock more goods and have 500 more gold to barter with.",
                    "parentIds": [
                        "Personality/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000007",
                    "name": "Bewitched",
                    "description": "NPCs whose disposition towards you is 100 or higher ignore any non-violent crimes they witness.",
                    "parentIds": [
                        "Personality/node_1900000000004",
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CEF",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000008",
                    "name": "Business Contract",
                    "description": "Merchants you invest in now sell additional goods.",
                    "parentIds": [
                        "Personality/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FEF",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000009",
                    "name": "Captivate",
                    "description": "Paralysis and Charm spells have a chance, based on your Personality, to last 3x as long.",
                    "parentIds": [
                        "Personality/node_1900000000014",
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CF1",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000010",
                    "name": "Dream Visitor",
                    "description": "Activate a sleeping NPC to trigger one of several mind-altering effects.",
                    "parentIds": [
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000011",
                    "name": "Encouragement",
                    "description": "Commanded enemies and encouraged allies restore 1% of their maximum Stamina and Magicka per second.",
                    "parentIds": [
                        "Personality/node_1900000000015",
                        "Personality/node_1900000000022"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE9",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CEA",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000012",
                    "name": "Face to Face",
                    "description": "Any perk that requires you to wear all light or heavy armor no longer requires a helmet.",
                    "parentIds": [
                        "Personality/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000013",
                    "name": "Fearstruck",
                    "description": "While under the influence of a Fear spell, NPCs lose all armor and magic resistance.",
                    "parentIds": [
                        "Personality/node_1900000000005",
                        "Personality/node_1900000000024"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CEE",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000014",
                    "name": "Helpless Hostage",
                    "description": "Paralyzed and calmed NPCs can be activated to take anything from their inventory.",
                    "parentIds": [
                        "Personality/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CF2",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000015",
                    "name": "Influential Presence",
                    "description": "All spells are 20% stronger on nearby enemies. This bonus is doubled for Illusion spells.",
                    "parentIds": [
                        "Personality/node_1900000000019"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF6",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000016",
                    "name": "Inspiration",
                    "description": "Nearby allies gain a bonus to Health, Magicka, and Stamina based on your fame, and a bonus to damage based on your infamy.",
                    "parentIds": [
                        "Personality/node_1900000000021"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE4",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000017",
                    "name": "Person of Interest",
                    "description": "Gain +5 disposition with all NPCs, and Illusion spells are 15% stronger.",
                    "parentIds": [
                        "Personality/node_1900000000001",
                        "Personality/node_1900000000012",
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D03C",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D03B",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CDA",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5CB",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000018",
                    "name": "Piety",
                    "description": "Blessings are 1% stronger and last 5% longer per point of fame.",
                    "parentIds": [
                        "Personality/node_1900000000021"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0892D5",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000019",
                    "name": "Quiet Casting",
                    "description": "Illusion spells are silent to others.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF3",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0B1FF5",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000020",
                    "name": "Refined Rage",
                    "description": "Frenzied targets deal 50% more damage to targets other than you.",
                    "parentIds": [
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE7",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000021",
                    "name": "Reputation",
                    "description": "Give bonus for high fame, give different bonus for high infamy?",
                    "parentIds": [
                        "Personality/node_1900000000017"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE3",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000022",
                    "name": "Sacred Idol",
                    "description": "Commanded and encouraged NPCs deal 50% more damage to any enemy within 25 feet of you.",
                    "parentIds": [
                        "Personality/node_1900000000005"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CEB",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000023",
                    "name": "Select Stock",
                    "description": "Some merchants you invest in now sell additional unique goods.",
                    "parentIds": [
                        "Personality/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EBFD",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000024",
                    "name": "Siphon",
                    "description": "Absorb Magicka and Stamina from any feared NPC.",
                    "parentIds": [
                        "Personality/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CEC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CED",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000025",
                    "name": "Spreading Hate",
                    "description": "When a frenzied target dies, it attempts to cast Frenzy on nearby targets.",
                    "parentIds": [
                        "Personality/node_1900000000020"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE8",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Personality/node_1900000000026",
                    "name": "Veil of Infamy",
                    "description": "Your bounty decreases each day by half your Personality plus three times your infamy.",
                    "parentIds": [
                        "Personality/node_1900000000021"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE5",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        },
        {
            "id": "Luck",
            "name": "Luck",
            "rootId": "Luck/node_1900000000015",
            "nodes": [
                {
                    "id": "Luck/node_1900000000001",
                    "name": "Bound by Fate",
                    "description": "Gain an additional power based on your birthsign.",
                    "parentIds": [
                        "Luck/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CA5",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000002",
                    "name": "Certain Path",
                    "description": "Birthsigns and Doomstones are 20% stronger. This doubles if your Heaven Stone and birthsign match.",
                    "parentIds": [
                        "Luck/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "084CA4",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000003",
                    "name": "Chance Encounter",
                    "description": "A series of positive random encounters are significantly more likely to occur while exploring the wilderness.",
                    "parentIds": [
                        "Luck/node_1900000000015"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE1",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000004",
                    "name": "Clueless Chum",
                    "description": "Marked is more likely to occur when entering a town, providing additional gems and gold. Marked NPCs are 25% easier to pickpocket.",
                    "parentIds": [
                        "Luck/node_1900000000002"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC16",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000005",
                    "name": "Decimation",
                    "description": "Gain an additional 5% critical hit chance.",
                    "parentIds": [
                        "Luck/node_1900000000017"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D025",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D026",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D028",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000006",
                    "name": "Doomed Denizen",
                    "description": "Marked is more likely to occur when entering dungeons. Deal 30% more damage to marked targets.",
                    "parentIds": [
                        "Luck/node_1900000000014"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC15",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000007",
                    "name": "Draw of Destiny",
                    "description": "All Heaven Stones are marked on your map. Two can now be active at once, and they provide an additional minor bonus.",
                    "parentIds": [
                        "Luck/node_1900000000013"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC25",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000008",
                    "name": "Fluke",
                    "description": "While below 25% health, you have a 5% chance to ignore damage.",
                    "parentIds": [
                        "Luck/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0892FC",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F591",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F592",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000009",
                    "name": "Forced Fate",
                    "description": "Use a greater power once per day to trigger Marked.",
                    "parentIds": [
                        "Luck/node_1900000000007"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC1F",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000010",
                    "name": "Opportunist",
                    "description": "Deal 20% more damage for 4 seconds after a critical hit.",
                    "parentIds": [
                        "Luck/node_1900000000002"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D01E",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F595",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F596",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000011",
                    "name": "Marked",
                    "description": "When entering a location, a random NPC might be marked. Marked NPCs carry a large amount of additional loot.",
                    "parentIds": [
                        "Luck/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC08",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000012",
                    "name": "Nocturnal's Nod",
                    "description": "When entering a dungeon or a house, you might get a hunch that an incredible trove of treasure is hidden somewhere, and a container will hold a massive haul of loot.",
                    "parentIds": [
                        "Luck/node_1900000000011"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "1A8CE0",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000013",
                    "name": "Nose for Coin",
                    "description": "Anywhere there is gold to be found, you find a little bit more.",
                    "parentIds": [
                        "Luck/node_1900000000003"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "088DB0",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "16537D",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "16537E",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F5D7",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000014",
                    "name": "Razor's Edge",
                    "description": "Critical hits have a 3% chance to deal 25x damage.",
                    "parentIds": [
                        "Luck/node_1900000000008"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D01F",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F593",
                            "minAttribute": null
                        },
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "28F594",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000015",
                    "name": "Ringleader",
                    "description": "When entering a dungeon, an enemy might be significantly stronger, but it will also carry valuable loot and provide experience in a random skill when slain.",
                    "parentIds": [],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "04EC0A",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000016",
                    "name": "Scavenger",
                    "description": "People you kill are more likely to carry valuable loot, such as gold, books, and gems.",
                    "parentIds": [
                        "Luck/node_1900000000001"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "08BBE3",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000017",
                    "name": "Treasure Hunter",
                    "description": "Find more rare items in treasure chests.",
                    "parentIds": [
                        "Luck/node_1900000000016"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "0892FF",
                            "minAttribute": null
                        }
                    ]
                },
                {
                    "id": "Luck/node_1900000000018",
                    "name": "Lucky Break",
                    "description": "For 10 seconds after a critical hit you gain increased critical chance. This can stack.",
                    "parentIds": [
                        "Luck/node_1900000000010"
                    ],
                    "ranks": [
                        {
                            "plugin": "Skyblivion.esm",
                            "formId": "09D018",
                            "minAttribute": null
                        }
                    ]
                }
            ]
        }
    ]
};
//# sourceMappingURL=InitialPerkTrees.js.map