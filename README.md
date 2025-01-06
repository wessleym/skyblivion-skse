# skyblivion-skse
SKSE utilities for Skyblivion project

# Build Instructions
Open this folder with Visual Studio and then use CMake to build.

You might need to save CMakeLists.txt within Visual Studio to get things started. Then use the normal Visual Studio build functionality.

# Troubleshooting
For the first build, you might need to run Visual Studio as an administrator set Properties > Compatibility > Run this program as an administrator:
- C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe
- C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
- %localappdata%\vcpkg\downloads\tools\ninja\1.10.2-windows\ninja.exe

It's possible that not all of these needed to be changed. Later, I was able to turn off these compatibility settings and run Visual Studio as a normal user.

If problems continue, update the "baseline" values in vcpkg-configuration.json with the most recent Git hashes.