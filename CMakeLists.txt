# This file configures the "build" of this SKSE plugin
# - how to compile the C++ plugin (in addition to settings found in CMakePresets.json)
# - where to output the resulting .dll file
#
# This is a bare-bones CommonLibSSE-NG SKSE plugin template - just enough to
# compile and load, with none of the vcpkg/MSVC build headaches solved
# elsewhere already baked in. To start a new plugin from it:
#   1. Copy this whole folder and rename it to your new plugin's name.
#   2. Change PROJECT_NAME below (and VERSION, if you want to start fresh).
#   3. Add your own .cpp/.h files to the SOURCES list in add_commonlibsse_plugin().
# Everything else here (vcpkg.json, vcpkg-configuration.json, triplets/,
# CMakePresets.json) can stay as-is - see README.md before touching any of it.

cmake_minimum_required(VERSION 3.21)

# Project name - this becomes the name of your SKSE .dll file, your plugin's
# display name in skse64.log, and (via SKSE::PluginDeclaration) the name your
# own log file is written under.
project(TrueHotkeys VERSION 1.2.1 LANGUAGES CXX)

# #
# YOU DO NOT NEED TO EDIT ANYTHING BELOW HERE
# #

# If you're not using a mod manager, you probably want the SKSE plugin to go
# inside of your Skyrim "Data" folder.
#
# To do this automatically, set the `SKYRIM_FOLDER` environment variable
# to the path of your Skyrim Special Edition folder
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

# If you're using Mod Organizer 2 or Vortex, you might want this to go inside
# of your "mods" folder, inside of a subfolder named "<your mod>".
#
# To do this automatically, set the `SKYRIM_MODS_FOLDER` environment variable
# to the path of your "mods" folder
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Otherwise, you can set OUTPUT_FOLDER to any place you'd like :)
# set(OUTPUT_FOLDER "C:/path/to/any/folder")

# Setup your SKSE plugin as an SKSE plugin!
find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES
    src/plugin.cpp
    src/Hotkeys/Actions.cpp
    src/Hotkeys/ConfigUI.cpp
    src/Hotkeys/ContinuousMovement.cpp
    src/Hotkeys/EditorIDLookup.cpp
    src/Hotkeys/FormBrowser.cpp
    src/Hotkeys/HotkeyManager.cpp
    src/Hotkeys/InputDispatchHook.cpp
    src/Hotkeys/InputHandler.cpp
    src/Hotkeys/Profile.cpp
    src/Hotkeys/RelocDiagnostic.cpp
    src/Hotkeys/SyntheticTap.cpp
    src/Hotkeys/ToggleSprint.cpp
    src/Hotkeys/VanillaControlSuppressor.cpp
    # Add more .cpp/.h files here as your plugin grows.
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE src)

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure an output folder above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()

    # If you add Papyrus scripts under Scripts/Source (see README.md's
    # "Adding Papyrus functions" section), this copies all of them alongside
    # the DLL automatically - no need to list filenames here. Note this check
    # runs at CMake *configure* time, so re-run configure after adding the
    # Scripts/Source folder for the first time.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/Source")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}/Source/Scripts"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/Source" "${OUTPUT_FOLDER}/Source/Scripts"
            VERBATIM
        )
    endif()
endif()
