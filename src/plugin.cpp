#include "Hotkeys/ConfigUI.h"
#include "Hotkeys/HotkeyManager.h"
#include "Hotkeys/InputDispatchHook.h"
#include "Hotkeys/Locale.h"
#include "Hotkeys/RelocDiagnostic.h"
#include "Hotkeys/VanillaControlSuppressor.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace {
    // Sets up spdlog to write to SKSE's standard log folder (Documents/My
    // Games/Skyrim Special Edition/SKSE/<PluginName>.log), named after this
    // plugin via SKSE::PluginDeclaration (auto-populated by
    // add_commonlibsse_plugin() in CMakeLists.txt from PROJECT_NAME/VERSION -
    // there's no separate declaration to write by hand).
    void InitializeLogging() {
        auto logsFolder = SKSE::log::log_directory();
        if (!logsFolder) {
            SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
        }

        auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
        auto logger = std::make_shared<spdlog::logger>("log", std::move(fileSink));

        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S] [%l] %v");
    }

    // Handles messages from SKSE's cross-plugin messaging bus. kDataLoaded is
    // the earliest point it's safe to touch game forms/records - everything
    // from every loaded plugin (ESP/ESL/ESM) has been resolved by then. Other
    // useful message types include kPostLoad (all SKSE plugins loaded, but
    // game data isn't yet) and kNewGame/kPostLoadGame (save-related timing).
    void MessageHandler(SKSE::MessagingInterface::Message* a_message) {
        switch (a_message->type) {
            case SKSE::MessagingInterface::kPostLoad:
                // Localization (moved here 2026-08-13, was kDataLoaded) -
                // has to run before ConfigUI::Install() below, not after.
                // AddSectionItem (called by Install()) only reads the
                // Settings/Key Binds/Profiles section names once, at
                // registration time - unlike every other UI string in this
                // plugin, which re-evaluates T()/TR() every frame - so if
                // Locale initializes afterward, those three names are stuck
                // in English for the whole session no matter what language
                // is selected. InitializeEarly() peeks Settings.ini's
                // Language= line directly (pure filesystem I/O, same as the
                // rest of Locale's own loading) instead of waiting for
                // HotkeyManager::Initialize()'s full settings+profile load
                // at kDataLoaded - see Locale.h's own comment on
                // InitializeEarly() for why that's safe this early.
                Hotkeys::Locale::GetSingleton()->InitializeEarly();
                // Every SKSE plugin has finished loading by now, including
                // SKSEMenuFramework.dll if it's installed - safe to register
                // our config UI tabs with it.
                Hotkeys::ConfigUI::Install();
                break;
            case SKSE::MessagingInterface::kDataLoaded:
                // Forms and records are safe to access now - load settings
                // and the active hotkey profile.
                Hotkeys::HotkeyManager::GetSingleton()->Initialize();
                // Temporary diagnostic tool (see RelocDiagnostic.h's own
                // comment) - registers a passive BSInputDeviceManager
                // AddEventSink, so it needs that singleton to exist first.
                // Moved here from kPostLoad in 1.1.17 - see DESIGN.md's
                // dated entry: 1.1.16 registered it at kPostLoad on an
                // untested assumption that BSInputDeviceManager already
                // exists by then, and Josh's very next test run showed
                // this Install() call's own log line missing even though
                // the game went on to reach kDataLoaded successfully - a
                // genuine, unexplained anomaly that made kPostLoad timing
                // the leading suspect. kDataLoaded is a strictly later,
                // more conservative point with no known downside for this
                // tool's purpose (a debug hotkey Josh only presses much
                // later, well in-game).
                // Disabled for release in 1.1.49 - see DESIGN.md's dated
                // entry. Code is kept intact; uncomment the line below to
                // re-enable for a future debugging session.
                // Hotkeys::RelocDiagnostic::Install();
                break;
            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                // RE::PlayerControls/RE::ControlMap aren't guaranteed
                // constructed yet at kDataLoaded, so a vanilla-suppression
                // sync attempted there (from Initialize's bind loading) can
                // silently no-op. By the time a save is loaded or a new
                // game starts, the player/world - and these singletons -
                // definitely exist, so re-sync here to catch that case.
                Hotkeys::VanillaControlSuppressor::Sync();
                break;
            default:
                break;
        }
    }
}

// SKSE's entry point. The SKSEPluginLoad macro (from CommonLibSSE-NG) expands
// into the exported symbols SKSE actually looks for (SKSEPlugin_Load /
// SKSEPlugin_Version) - you don't need to write those yourself.
SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    InitializeLogging();

    SKSE::log::info("{} loading...", SKSE::PluginDeclaration::GetSingleton()->GetName());

    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    // To expose native Papyrus functions from this plugin: write a function
    // elsewhere that registers them with RE::BSScript::IVirtualMachine (see
    // README.md's "Adding Papyrus functions" section for the pattern), then
    // call it here, e.g.:
    //   SKSE::GetPapyrusInterface()->Register(YourNamespace::RegisterFunctions);

    // Patches a fixed call site in the game's own compiled code (the input-
    // event dispatch call), so - unlike BSInputDeviceManager::AddEventSink,
    // which needs that singleton constructed first - it has no runtime-
    // object dependency and can be installed unconditionally right here,
    // matching CamDirector's own confirmed-working pattern for its
    // equivalent hook.
    Hotkeys::InputDispatchHook::Install();

    SKSE::log::info("{} loaded", SKSE::PluginDeclaration::GetSingleton()->GetName());
    return true;
}
