-- include subprojects
includes("lib/commonlibsse")
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

-- define targets
target("Skyblivion")
    add_rules("commonlibsse.plugin", {
        name = "Skyblivion",
        author = "Skyblivion Team",
        description = "Skyblivion SKSE64 Plugin"
    })

    -- add dependencies
    add_deps("clib-util")

    -- prevent <Windows.h> from defining the ERROR macro (collides with REX::ERROR)
    add_defines("NOGDI")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
