#pragma once

// Brute Force (perk rework, 2026-11-04; player only): a melee power attack that lands on a
// locked container or door (bDoors) is an attempt to force the lock. [BruteForce] in SKYBSpellReflect.ini:
//  * the rank is the highest held of sRankPerks (Brute Force 052CE0 and its two new ranks);
//    the chance comes from that rank's row and the lock level; 0 = the lock cannot be forced;
//  * success unlocks the lock as lockpicking does (REFR_LOCK unlocked + AddLockChange);
//  * every attempt raises a detection event at the player (the call Papyrus
//    CreateDetectionEvent makes) and, when the lock is owned and the player is detected,
//    the trespass alarm lockpicking raises (1.7.104 id 51981: owner -> highest detection level
//    against the player -> Actor::TrespassAlarm(ref, owner, -1));
//  * with Degradation on, the weapon takes extra wear through SKYBDeg::DecayService::Degrade,
//    scaled by the lock level, from fWearMultFail or fWearMultSuccess (2026-11-05: more wear, and
//    more for a lock that gives way);
//  * the player hears sSuccessSound or sFailSound at the lock (2026-11-05; the detection event is
//    what NPCs hear), played on the main thread at the reference and following its 3D.
// A forced door is only unlocked (bOpenOnSuccess opens containers only: a load door would carry
// the player through). Lockpicking's jail-escape branch is not copied (gear is taken in jail).
// Two event-driven paths (no hook, no polling), either may make the attempt; fAttemptGap stops one
// swing counting twice:
//  * TESHitEvent with kPowerAttack on the container (the original path);
//  * the hit-frame path (bHitFrame, 2026-11-04 DLL round): the player's animation graphs send
//    "HitFrame"; when the high process's attack data is a power attack, the crosshair reference
//    (SKSE CrosshairRefEvent) within fReach is the target. The graph sink is added again whenever
//    a loading screen or the race menu closes (the graphs are rebuilt with the player's 3D).

namespace SpellReflect::BruteForce {
    // Resolve the rank perks and register the sinks. Call at SKSE kDataLoaded.
    void OnDataLoaded();
}
