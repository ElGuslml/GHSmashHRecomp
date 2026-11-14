# GHSmashHRecomp - static recompilation of Guitar Hero: Smash Hits (Xbox 360), built on the ReXGlue SDK, drawn by the SDK's Xenos
# emulation or by NativeXRenderer (native_x_renderer/, a submodule).
#
# The SDK is the thirdparty/rexglue-sdk git submodule: ElGuslml/rexglue-sdk, branch recomp, the official
# nightly of 2026-10-02 (v0.10.0 and 24 later commits) with the changes these projects and NativeXRenderer
# need, one commit each (RECOMP.md there).
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(ghsmashhits LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same CPU baseline as the official SDK builds (its win-amd64 preset): x86-64-v2, i.e. SSE4.2 and
# POPCNT on top of the SSSE3/SSE4.1 the SDK code requires. Not -march=native, so the binaries run on
# any x86-64-v2 CPU.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    add_compile_options(-march=x86-64-v2)
endif()

# Build the SDK from the submodule. RECOMP.md marks the recomp branch: a checkout of the official SDK lacks
# its changes and would not build this project.
set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rexglue-sdk" CACHE PATH
    "Path to the rexglue-sdk source tree" FORCE)
if(NOT EXISTS "${REXSDK_DIR}/RECOMP.md")
    message(FATAL_ERROR
        "The ReXGlue SDK submodule (${REXSDK_DIR}) is missing or not on the recomp branch of "
        "ElGuslml/rexglue-sdk. Run: git submodule sync && git submodule update --init --recursive --force")
endif()

# Both graphics backends: D3D12 and Vulkan.
if(WIN32)
    set(REXGLUE_USE_D3D12 ON CACHE BOOL "Enable the D3D12 graphics backend")
endif()
set(REXGLUE_USE_VULKAN ON CACHE BOOL "Enable the Vulkan graphics backend")

include(generated/rexglue.cmake)

# The first build of a fresh tree runs the codegen, which writes generated/default/sources.cmake;
# CMake has to configure again to pick up the generated sources (otherwise the link fails with
# PPCImageConfig). A marker file holds the sources.cmake this configure saw and is a configure
# dependency; it is refreshed with copy_if_different after the codegen, so it only changes (and
# triggers a new configure) when the list of generated sources changes. Ninja brings configure
# dependencies up to date before building anything else, so the first build runs the codegen,
# configures again and goes on within the same command. (The codegen stamp itself cannot be the
# dependency: the codegen touches it on every run.)
set(GHSMASHHITS_CODEGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated/default")
set(GHSMASHHITS_SOURCES_MARKER "${GHSMASHHITS_CODEGEN_DIR}/sources.cmake.configured")
if(EXISTS "${GHSMASHHITS_CODEGEN_DIR}/sources.cmake")
    file(COPY_FILE "${GHSMASHHITS_CODEGEN_DIR}/sources.cmake" "${GHSMASHHITS_SOURCES_MARKER}" ONLY_IF_DIFFERENT)
elseif(NOT EXISTS "${GHSMASHHITS_SOURCES_MARKER}")
    file(WRITE "${GHSMASHHITS_SOURCES_MARKER}" "")
endif()
add_custom_command(
    OUTPUT "${GHSMASHHITS_SOURCES_MARKER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${GHSMASHHITS_CODEGEN_DIR}/sources.cmake" "${GHSMASHHITS_SOURCES_MARKER}"
    DEPENDS "${GHSMASHHITS_CODEGEN_DIR}/codegen.build.stamp"
    COMMENT "Checking the list of generated sources"
    VERBATIM)
add_custom_target(ghsmashhits_sources_marker DEPENDS "${GHSMASHHITS_SOURCES_MARKER}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GHSMASHHITS_SOURCES_MARKER}")

# Sources
set(GHSMASHHITS_SOURCES
    src/debug_menu.cpp
    src/fmod_mix.cpp
    src/framerate.cpp
    src/intro.cpp
    src/keyboard.cpp
    src/main.cpp
    src/matchmaking.cpp
    src/native_x_renderer_ghsmashhits.cpp
    src/online_text.cpp
    src/options.cpp
    src/players.cpp
    src/quit_prompt.cpp
    src/region.cpp
    src/screen_fx.cpp
    src/startup.cpp
    src/thread_waits.cpp
    src/timer.cpp
    src/widescreen.cpp
)

if(WIN32)
    add_executable(ghsmashhits WIN32 ${GHSMASHHITS_SOURCES})
else()
    add_executable(ghsmashhits ${GHSMASHHITS_SOURCES})
endif()

rexglue_setup_target(ghsmashhits GPU_PLUGINS xenos)
add_dependencies(ghsmashhits ghsmashhits_sources_marker)

# NativeXRenderer: the native renderer (native_x_renderer/README.md). The game side (hooks, profile) is in
# src/native_x_renderer_ghsmashhits.cpp; the game draws on the Xenos path while native_x_renderer is off.
add_subdirectory(native_x_renderer)
target_link_libraries(ghsmashhits PRIVATE native_x_renderer)
# NVIDIA's DLSS runtime next to the executable.
native_x_renderer_copy_runtime(ghsmashhits)

# The target and the app keep the short name "ghsmashhits" (ghsmashhits.toml, logs\ghsmashhits_NNN.log); only the
# executable gets a descriptive name.
set_target_properties(ghsmashhits PROPERTIES OUTPUT_NAME "GHSmashHitsRecomp")

# With the SDK built from source, rexruntime does not pass the Dear ImGui include directory on to the
# host target (the installed SDK ships imgui.h in include/); rex_app.cpp and ghsmashhits_app.h need it. The
# symbols still come from rexruntime.dll.
target_include_directories(ghsmashhits PRIVATE "${REXSDK_DIR}/thirdparty/imgui")

# Game files: assets/ next to GHSmashHitsRecomp.exe (release layout) or, if absent, the repository's assets/.
target_compile_definitions(ghsmashhits PRIVATE
    GHSMASHHITS_DEFAULT_GAME_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# GH Smash Hits Launcher: the options, the first run from the player's ISO, the achievements and the updates, in a separate
# executable so that GHSmashHitsRecomp.exe starts right away (for example from Steam Big Picture); "Play" starts the game, and
# the launcher comes back when it closes. It writes ghsmashhits_options.toml next to GHSmashHitsRecomp.exe. The launcher's
# code is the RecompLauncher submodule (recomp_launcher/, the same in every game); the game's own names, files and
# options are launcher/game.h (with src/project.h, which the game shares) and launcher/game.cpp, its colors
# launcher/palette.h, its art launcher/art/, its resources res/launcher.rc. RECOMP_LAUNCHER_DIR points elsewhere to try
# a working copy of it.
set(RECOMP_LAUNCHER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recomp_launcher" CACHE PATH "The RecompLauncher tree")
if(NOT EXISTS "${RECOMP_LAUNCHER_DIR}/RecompLauncher.cmake")
    message(FATAL_ERROR "The RecompLauncher submodule (${RECOMP_LAUNCHER_DIR}) is missing: git submodule update --init")
endif()
if(WIN32)
    include("${RECOMP_LAUNCHER_DIR}/RecompLauncher.cmake")
    recomp_launcher_add(ghsmashhits_launcher
        OUTPUT_NAME "GH Smash Hits Launcher"
        GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/launcher"
        RC "${CMAKE_CURRENT_SOURCE_DIR}/res/launcher.rc"
        SDK_DIR "${REXSDK_DIR}"
        INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/res"
        DEFAULT_GAME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    # The game reads which of the launchers' profiles it runs with (src/startup.cpp, ConfigureProfile).
    recomp_launcher_add_profiles(ghsmashhits SDK_DIR "${REXSDK_DIR}")
endif()

# Windows resources: file properties (res/version.h) and the icon, res/icon.ico, when it exists. The
# executable's first icon is also the one SDL and the launcher use for their windows.
if(WIN32)
    target_sources(ghsmashhits PRIVATE res/game.rc)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico")
        target_compile_definitions(ghsmashhits PRIVATE RECOMP_HAS_ICON)
        target_compile_definitions(ghsmashhits_launcher PRIVATE RECOMP_HAS_ICON)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico")
    endif()
endif()
