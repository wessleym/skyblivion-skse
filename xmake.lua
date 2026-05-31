-- ============================================================================
--  CommonLib backend selection
-- ----------------------------------------------------------------------------
--  Switch backend, then build:
--      xmake f --backend=ng       &&  xmake     (default: Skyrim SE/AE/VR)
--      xmake f --backend=libxse   &&  xmake     (Skyrim SE/AE only)
--  The choice persists until the next `xmake f`. This one switch drives all
--  three moving parts: the CommonLib submodule, the plugin rule, and the
--  SKY_COMMONLIB macro that src/Platform/ compiles against. Switching forces a
--  full rebuild (the backends use entirely different headers). See MIGRATION.md.
--
--    libxse  libxse/commonlibsse  -- Skyrim SE/AE only.
--            submodule: lib/commonlibsse
--    ng      CommonLibSSE-NG (alandtse/CommonLibVR@ng) -- one DLL for
--            SE + AE + VR. submodule: lib/commonlibsse-ng
-- ============================================================================
option("backend", function()
    set_default("ng")
    set_values("libxse", "ng")
    set_description("CommonLib backend: 'libxse' (SE/AE) or 'ng' (SE/AE/VR)")
end)

local backend_ng = is_config("backend", "ng")

-- include subprojects (CommonLib backend driven by the `backend` option)
if backend_ng then
    includes("lib/commonlibsse-ng")
else
    includes("lib/commonlibsse")
end
includes("lib/clib-util")

-- set project constants
set_project("Skyblivion")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- default build mode, so `xmake f --backend=...` need not re-specify -m
set_defaultmode("releasedbg")

-- third-party packages (backend-agnostic — pulled directly, not via a CommonLib)
add_requires("nlohmann_json v3.12.0")  -- JSON parsing for the spell-making buy payload

-- plugin metadata (shared by whichever CommonLib plugin rule is used)
local plugin_desc = {
    name = "Skyblivion",
    author = "Skyblivion Team",
    description = "Skyblivion SKSE64 Plugin"
}

-- define targets
target("Skyblivion")
    -- CommonLib plugin rule + SKY_COMMONLIB macro, driven by the `backend`
    -- option above. NG migration: confirm NG's plugin rule name (MIGRATION.md).
    if backend_ng then
        add_rules("commonlibsse-ng.plugin", plugin_desc)
        add_defines("SKY_COMMONLIB=2")  -- SKY_COMMONLIB_NG
    else
        add_rules("commonlibsse.plugin", plugin_desc)
        add_defines("SKY_COMMONLIB=1")  -- SKY_COMMONLIB_LIBXSE
    end

    -- add dependencies
    add_deps("clib-util")
    add_packages("nlohmann_json")

    -- keep <Windows.h> from defining macros that collide with the C++ stdlib:
    --   NOGDI    -> ERROR   (collides with Log::ERROR)
    --   NOMINMAX -> min/max (collide with std::min / std::max)
    add_defines("NOGDI", "NOMINMAX")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
