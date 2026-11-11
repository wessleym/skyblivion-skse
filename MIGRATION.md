# CommonLib

The plugin builds against **CommonLibSSE-NG** (`alandtse/CommonLibVR@ng`, submodule
`lib/commonlibsse-ng`) and produces one DLL for Skyrim SE, AE and VR.

```
xmake f -m releasedbg    # configure (once)
xmake                    # build
```

Run `xmake f` from **PowerShell**, not bash or Git Bash: from bash, xmake detects the
platform as `mingw`. `xmake` itself runs from either.

## Why NG

SKSE VR's loader requires the `SKSEPlugin_Query` export. The libxse build exported
only `SKSEPlugin_Version` (the post-AE convention), so SKSE VR rejected the DLL
(*"does not appear to be an SKSE plugin"*). NG's plugin rule exports
`SKSEPlugin_Query`, `SKSEPlugin_Load` and `SKSEPlugin_Version`; all three are present
in the built DLL.

## The retired libxse backend

Until 2026-11-05 an xmake `backend` option could also build the plugin against
`libxse/commonlibsse` (SE and AE only). That option was removed:

- No released DLL used it. The deployed DLL is NG's, and libxse does not support VR.
- PR #3's hooks (the Main::Update frame hook, SpellReflect, PerkSpells, the temper
  patches) use NG's multi-runtime address API (`RelocationID`, `VariantID`,
  `REL::Module::IsAE`/`IsVR`), which has no libxse equivalent.
  Supporting libxse would require a second implementation of each hook, with no
  build that runs it.

The last commit that builds on libxse is `8449ff7`, the parent of the commit that removed
it (`2cd667f`). Restoring the backend means restoring, from that commit, the `backend`
option in `xmake.lua`, the `lib/commonlibsse` submodule, `src/Platform/Backend.h` and
the `SKY_COMMONLIB` branches in `src/Platform/`, and then writing libxse versions of the
hooks.

## Runtime-specific code

Code that depends on the runtime (SE, AE, VR) belongs in `src/Platform/`:

- `Log.h` / `Log.cpp`: the logging interface. Everything else logs through `Log::`.
- `Entry.cpp`: the SKSE entry export.
- `MainThread.h` / `MainThread.cpp`: a task queue drained once a frame on the main thread,
  from a hook on the frame loop's call to `Main::Update` (SKSE's UI queue when the hook
  cannot be installed). Calls into Scaleform movies go through `MainThread::Post`.
- `FormCache.h` / `FormCache.cpp`: forms resolved once, by editor ID or local form ID, with
  each miss logged once after data load.
- `REBridge.h`: the project's one way to reach `RE::` members whose NG accessor is
  wrong on some runtime, or whose direct access compiles to a wrong offset. Add a
  helper there instead of reaching into runtime data from application code.

Known cases (most covered by a `REBridge` helper):

1. `RE::PlayerCharacter` runtime data: NG's `GetPlayerRuntimeData()` and
   `GetInfoRuntimeData()` read the wrong memory on VR. Use the per-field helpers
   (`PlayerSkills`, `AmountStolenSold`, `GameStats`).
2. `Actor` inherits `ActorValueOwner` and `ActorState` at runtime-dependent offsets.
   A `static_cast` to either base, or calling an inherited method directly on an
   `Actor*` (e.g. `IsWeaponDrawn`), compiles to a fixed offset and crashes at run
   time. Use `REBridge::AVOwner()` / `REBridge::ActorStateOf()`.
3. VR's address library holds only part of SE's ids, and NG resolves most functions on
   first call: a missing id is a fatal error when that code first runs, not at load.
   Check each new NG function against `version-1-4-15-0.csv`. Cases found so far:
   - The non-const `BSPointerHandle::get()` (id 12785). `REBridge::Lookup()` calls the
     const overload (id 12204) instead. `Console::GetSelectedRef()` and
     `ExtraDataList::GetDisplayName()` use the non-const overload internally; use
     `REBridge::ConsoleSelectedRef()` and `REBridge::DisplayName()`.
   - `TESObjectREFR::AddLockChange()` (id 19110). `REBridge::AddLockChange()` supplies
     the VR address.
   - `GiftMenu::GetReceiverRefHandle()` (id 519571). `FormUtility.cpp` answers
     `GetAssociatedMenuForm("GiftMenu")` with no form on VR.
   - The journal's saved page and opener (ids 520167, 52428). `AchievementService::OpenJournalAt`
     shows the journal with a plain message on VR, whose pages may come up empty (VR's
     opener is at 0x92F0F0; the saved page is not yet found).

`xmake.lua` defines `NOMINMAX` and `NOGDI`: NG's headers include `<Windows.h>`, whose
`min` / `max` macros break `std::min` / `std::max` and whose `ERROR` macro collides
with `Log::ERROR`.
