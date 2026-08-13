#include "Hotkeys/ConfigUI.h"
#include "Hotkeys/HotkeyManager.h"
#include "Hotkeys/InputDispatchHook.h"
#include "Hotkeys/Locale.h"
#include "Hotkeys/RelocDiagnostic.h"
#include "Hotkeys/VanillaControlSuppressor.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace {
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

    void MessageHandler(SKSE::MessagingInterface::Message* a_message) {
        switch (a_message->type) {
            case SKSE::MessagingInterface::kPostLoad:
                Hotkeys::Locale::GetSingleton()->InitializeEarly();
                Hotkeys::ConfigUI::Install();
                break;
            case SKSE::MessagingInterface::kDataLoaded:
                Hotkeys::HotkeyManager::GetSingleton()->Initialize();
                break;
            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                Hotkeys::VanillaControlSuppressor::Sync();
                break;
            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);
    InitializeLogging();

    SKSE::log::info("{} loading...", SKSE::PluginDeclaration::GetSingleton()->GetName());

    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);


    Hotkeys::InputDispatchHook::Install();

    SKSE::log::info("{} loaded", SKSE::PluginDeclaration::GetSingleton()->GetName());
    return true;
}
