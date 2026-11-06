-- CommonLibSSE-NG (alandtse/CommonLibVR@ng): one DLL for Skyrim SE, AE and VR. See MIGRATION.md.
includes("lib/commonlibsse-ng")
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

-- default build mode
set_defaultmode("releasedbg")

-- third-party packages (pulled directly, not via CommonLib)
add_requires("nlohmann_json v3.12.0")  -- JSON parsing for the spell-making buy payload

-- plugin metadata
local plugin_desc = {
    name = "Skyblivion",
    author = "Skyblivion Team",
    description = "Skyblivion SKSE Plugin"
}

-- define targets
target("Skyblivion")
    add_rules("commonlibsse-ng.plugin", plugin_desc)

    -- add dependencies
    add_deps("clib-util")
    add_packages("nlohmann_json")

    -- keep <Windows.h> from defining macros that collide with the C++ stdlib:
    --   NOGDI    -> ERROR   (collides with Log::ERROR)
    --   NOMINMAX -> min/max (collide with std::min / std::max)
    add_defines("NOGDI", "NOMINMAX")

    -- IntelliSense only: the build already gets these from the commonlibsse-ng dep, but
    -- xmake's vsxmake generator doesn't copy dep flags into the .vcxproj, so without them
    -- the IDE parses CommonLib with no runtime target defined. Harmless build duplicates.
    add_defines("ENABLE_SKYRIM_SE=1", "ENABLE_SKYRIM_AE=1", "ENABLE_SKYRIM_VR=1")
    add_includedirs("lib/commonlibsse-ng/extern/openvr/headers")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
