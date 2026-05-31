# CommonLib backends

This plugin builds against either of two CommonLib variants, selected by the
`backend` xmake option. **CommonLibSSE-NG is the default**. It produces a single
DLL for Skyrim SE + AE + VR.

## Switching

```
xmake f --backend=ng       && xmake    # SE + AE + VR  (default)
xmake f --backend=libxse   && xmake    # SE + AE only
```

Run `xmake f` from **PowerShell**, not a bash or git-bash shell. From bash, xmake
misdetects the platform as `mingw`. Plain `xmake` builds are fine from either.

The `backend` option drives the submodule, the plugin rule, and the
`SKY_COMMONLIB` macro together. Switching forces a full plugin rebuild.

| backend | library | submodule | runtimes |
|---|---|---|---|
| `ng` (default) | CommonLibSSE-NG (`alandtse/CommonLibVR@ng`) | `lib/commonlibsse-ng` | SE + AE + VR |
| `libxse` | `libxse/commonlibsse` | `lib/commonlibsse` | SE + AE |

## Why NG

SKSE VR's loader requires the `SKSEPlugin_Query` export. The libxse build emits
only `SKSEPlugin_Version` (post-AE), so SKSE VR rejected the DLL
(*"does not appear to be an SKSE plugin"*). NG's `SKSEPluginInfo` macro emits
`SKSEPlugin_Query`, `SKSEPlugin_Load`, and `SKSEPlugin_Version`. All three are
verified present in the built DLL.

## Backend-specific code

All CommonLib-variant-specific code is confined to `src/Platform/`:

- `Backend.h`: backend selector macro (`SKY_COMMONLIB`, set by xmake).
- `Log.h` / `Log.cpp`: logging facade. NG logs via spdlog (`SKSE::log`);
  libxse via `REX::`.
- `Entry.cpp`: the SKSE entry export.
- `REBridge.h`: bridges `RE::` API that differs between backends. NG is
  multi-runtime: members whose offset varies across SE/AE/VR sit behind
  `GetXxxRuntimeData()` accessors, whereas libxse exposes them directly. Add a
  helper here for any future such divergence rather than `#if`-ing application
  code.

## What the NG migration required

Beyond adding the submodule and selecting the `commonlibsse-ng.plugin` rule:

1. `REBridge::PlayerData()`: `RE::PlayerCharacter` members `amountStolenSold`,
   `murder`, and `skills` are behind `GetPlayerRuntimeData()` in NG's
   multi-runtime build (a compile error without it).
2. `NOMINMAX` (in `xmake.lua`): NG's headers pull in `<Windows.h>` without it,
   and its `min` / `max` macros broke `std::min` / `std::max`.
3. `REBridge::AVOwner()` and `REBridge::ActorStateOf()`: `Actor` multiply-inherits
   `ActorValueOwner` and `ActorState` at version-varying offsets on NG. Reaching
   those bases--by `static_cast` **or** by calling an inherited method directly
   on an `Actor*` (`IsWeaponDrawn`)--**compiles** but adjusts `this` wrong.
   `static_cast<RE::ActorValueOwner*>` crashed (null-vtable AV in
   `DispositionSystem::SetInitialDisposition`, found via crash dump); `Actor`'s
   `AsActorValueOwner()` / `AsActorState()` give the correct upcast.

Only a handful of call sites changed. They route through `REBridge.h` helpers;
the bulk of the application code is untouched.
