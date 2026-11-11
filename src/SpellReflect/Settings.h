#pragma once

// SpellReflect settings, read once at load from Data\SKSE\Plugins\SKYBSpellReflect.ini (the
// standalone plugin's file name). Missing file or keys fall back to the defaults below.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace SpellReflect::Settings {
    enum class Mode : std::uint8_t {
        kSwap,   // the spell lands normally, but its effects go to the caster
        kBounce  // the projectile turns around and flies back at the caster
    };

    enum class Aim : std::uint8_t {
        kRetrace,  // turned projectiles fly back along their incoming path
        kDirect    // straight at the shooter's torso (can clip cover)
    };

    // One timed-block rank: a Block mastery perk (highest owned wins).
    struct BlockRank {
        std::string perk;                  // editor ID or Plugin|0xFormID
        float       damageReduction;       // percent of the hit's damage taken away (100 = the hit is not applied)
        float       cooldown;              // seconds before the next timed block
        float       staminaRestorePercent; // of max stamina, per timed block
        bool        noStaminaCost;         // a reduced (not negated) hit costs no block stamina
        bool        stagger;               // the attacker is staggered
    };
    inline constexpr std::size_t kBlockRanks = 4;

    // Brute Force: lock levels as the engine numbers them (Novice 0 .. Master 4; 5 = key only, never).
    inline constexpr std::size_t kLockLevels = 5;
    inline constexpr std::size_t kBruteRanks = 3;
    using LockTable = std::array<float, kLockLevels>;

    struct Values {
        // [Reflect]
        std::string actorValue = "WardDeflection";
        float       maxChance = 25.0f;
        // Effects with this keyword (the shield enchantment) count only while
        // the actor blocks, on top of the capped actor value, up to maxBlockingChance.
        std::string blockingKeyword = "SKYBMagicReflectWhileBlocking";
        float       maxBlockingChance = 50.0f;
        Mode        mode = Mode::kSwap;
        Aim         returnAim = Aim::kRetrace;  // turned arrows and bounced spells
        bool        reflectTouch = true;
        bool        reflectExplosive = false;
        bool        reflectShouts = false;
        bool        reflectStaffs = true;
        bool        ignoreNoReflectFlag = false;

        // [Arrows] - the parry rules, shared by [SpellParry]
        bool        arrowsEnabled = true;
        std::string arrowPerk = "SKYBPFReflectArrows1";        // rank 1: a weapon (or bare-handed) block
        std::string arrowShieldPerk = "SKYBPFReflectArrows2";  // rank 2: a shield block too
        bool        arrowAllowShield = false;  // on: rank 1 already reflects with a shield
        std::string windowActorValue = "LightArmorSkillAdvance";  // Skyblivion's Speed
        float       windowBase = 0.20f;
        float       windowPerPoint = 0.001f;
        float       windowMax = 0.35f;
        float       facingAngle = 75.0f;
        float       arrowCooldown = 1.0f;  // seconds between parries (arrows and spells share it)

        // [SpellParry] - a spell projectile turned back by a timed block, under the [Arrows] rules
        bool        parryEnabled = true;
        std::string parryPerk = "SKYBPFReflectSpells";
        bool        parryExplosive = true;  // projectiles with an explosion turn back too

        // [TimedBlock] - a FIXED window, larger than the parry window; the ranks are the
        // Block mastery perks (Skyblivion.esm 13E0E6 / 13E0E8 / 13E0E9 / 13E0EC)
        bool        timedBlockEnabled = true;
        float       timedBlockWindow = 0.75f;
        std::array<BlockRank, kBlockRanks> blockRanks{ {
            { "Skyblivion.esm|0x13E0E6", 80.0f, 4.0f, 0.0f, true, false },   // Apprentice
            { "Skyblivion.esm|0x13E0E8", 100.0f, 3.0f, 0.0f, true, false },  // Journeyman
            { "Skyblivion.esm|0x13E0E9", 100.0f, 2.0f, 20.0f, true, false }, // Expert
            { "Skyblivion.esm|0x13E0EC", 100.0f, 1.0f, 20.0f, true, true },  // Master
        } };
        std::string staggerSpell = "SKYBPFTimedBlockStaggerSpell";

        // [BruteForce] - a melee power attack on a locked container (perk rework, 2026-11-04)
        bool        bruteEnabled = true;
        std::array<std::string, kBruteRanks> bruteRankPerks{ "Skyblivion.esm|0x052CE0", "SKYBPFBruteForce2", "SKYBPFBruteForce3" };
        // percent chance per rank (rows) and lock level (Novice, Apprentice, Adept, Expert, Master); 0 = cannot be forced
        std::array<LockTable, kBruteRanks> bruteChance{ {
            { 50.0f, 30.0f, 0.0f, 0.0f, 0.0f },
            { 70.0f, 50.0f, 30.0f, 0.0f, 0.0f },
            { 85.0f, 65.0f, 45.0f, 25.0f, 0.0f },
        } };
        bool        bruteAllowUnarmed = false;
        bool        bruteDoors = true;  // locked doors as well as containers
        std::int32_t bruteNoiseLevel = 100;  // the detection event's sound level (as Papyrus CreateDetectionEvent)
        // extra weapon wear per attempt, x Degradation's [Wear] WeapRate x WeapRateMult, by lock level;
        // one table for a lock that holds, one for a lock that gives way (fWearMult sets both)
        LockTable   bruteWearMultFail{ 55.0f, 75.0f, 95.0f, 110.0f, 130.0f };
        LockTable   bruteWearMultSuccess{ 85.0f, 110.0f, 140.0f, 170.0f, 200.0f };
        // sounds the player hears at the lock (the detection event above is what NPCs hear); a sound
        // descriptor as "Plugin|0xID" (SNDR editor IDs are not kept at run time); empty = none.
        // A lock too strong to force plays the fail sound.
        std::string bruteSuccessSound = "Skyrim.esm|0x0C1917";  // UILockpickingUnlock
        std::string bruteFailSound = "Skyrim.esm|0x023F0E";     // DRSLockedSD (a locked door rattled)
        bool        bruteOpenOnSuccess = false;  // open the container menu at once
        float       bruteAttemptGap = 0.5f;      // seconds: one attempt per container per swing (both hands, enchantments)
        float       bruteCrimeRepeat = 10.0f;    // seconds before the same container is reported again
        std::string bruteSuccessMessage = "The lock gives way.";
        std::string bruteFailMessage = "The lock holds.";
        std::string bruteTooStrongMessage = "This lock is too strong to force.";
        // the hit-frame path: the player's animation graph's HitFrame during a power attack, aimed at the
        // crosshair reference within this many units (does not need a TESHitEvent on the container)
        bool        bruteHitFrame = true;
        float       bruteReach = 250.0f;

        // [Feedback] editor IDs (or Plugin|0xID); visual = ARTO or EFSH; empty = none.
        // Sound records lose their editor IDs at runtime, so the sounds come from
        // a FormList (entries in event order); a s*Sound value overrides its entry
        // (use Plugin|0xID for sounds).
        std::string soundList = "SKYBPFReflectFeedbackSounds";
        std::string spellReflectSound;
        std::string spellReflectVisual = "SKYBSpellAltReflectSpellHitArt";
        std::string arrowReflectSound;
        std::string arrowReflectVisual = "SKYBSpellAltReflectSpellHitArt";
        std::string timedBlockSound;
        std::string timedBlockVisual = "SKYBShieldGenericFXShader";
        float       visualDuration = 0.6f;

        // [Debug]
        bool          observeOnly = false;
        std::uint32_t logEvents = 500;
    };

    void             Load();
    const Values&    Get();
    std::string_view Name(Mode a_mode);
}
