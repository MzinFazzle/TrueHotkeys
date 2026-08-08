#include "Hotkeys/VanillaControlSuppressor.h"

#include "Hotkeys/HotkeyManager.h"

namespace Hotkeys::VanillaControlSuppressor {
    namespace {
        using HandlerGetter = RE::PlayerInputHandler* (*)(RE::PlayerControls*);

        [[nodiscard]] RE::PlayerInputHandler* GetReadyWeapon(RE::PlayerControls* a_pc) { return a_pc->readyWeaponHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetSprint(RE::PlayerControls* a_pc) { return a_pc->sprintHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetAutoMove(RE::PlayerControls* a_pc) { return a_pc->autoMoveHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetToggleRun(RE::PlayerControls* a_pc) { return a_pc->toggleRunHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetActivate(RE::PlayerControls* a_pc) { return a_pc->activateHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetJump(RE::PlayerControls* a_pc) { return a_pc->jumpHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetShout(RE::PlayerControls* a_pc) { return a_pc->shoutHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetAttackBlock(RE::PlayerControls* a_pc) { return a_pc->attackBlockHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetRun(RE::PlayerControls* a_pc) { return a_pc->runHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetSneak(RE::PlayerControls* a_pc) { return a_pc->sneakHandler; }
        [[nodiscard]] RE::PlayerInputHandler* GetTogglePOV(RE::PlayerControls* a_pc) { return a_pc->togglePOVHandler; }

        struct HandlerMapping {
            std::string_view eventName;  // RE::UserEvents' documented event-name strings
            HandlerGetter getter;
        };

        constexpr HandlerMapping kMappings[] = {
            {"Ready Weapon", &GetReadyWeapon},
            {"Sprint", &GetSprint},
            {"SprintStart", &GetSprint},
            {"Auto-Move", &GetAutoMove},
            {"Toggle Always Run", &GetToggleRun},
            {"Activate", &GetActivate},
            {"Jump", &GetJump},
            {"Shout", &GetShout},
            {"Left Attack/Block", &GetAttackBlock},
            {"Right Attack/Block", &GetAttackBlock},
            {"Dual Attack", &GetAttackBlock},
            {"blockStart", &GetAttackBlock},
            {"Run", &GetRun},
            {"Sneak", &GetSneak},
            {"sneakStart", &GetSneak},
            {"Toggle POV", &GetTogglePOV},
        };
    }

    void Sync() {
        auto* controls = RE::PlayerControls::GetSingleton();
        auto* controlMap = RE::ControlMap::GetSingleton();
        auto* manager = HotkeyManager::GetSingleton();
        if (!controls || !controlMap || !manager) {
            return;
        }

        for (const auto& mapping : kMappings) {
            if (auto* handler = mapping.getter(controls)) {
                handler->SetInputEventHandlingEnabled(true);
            }
        }

        const auto& settings = manager->GetSettings();
        if (!settings.enabled) {
            return;
        }

        for (const auto& mapping : kMappings) {
            for (std::uint32_t key = 0; key < 0x100; ++key) {
                auto eventName = controlMap->GetUserEventName(key, RE::INPUT_DEVICE::kKeyboard);
                if (eventName != mapping.eventName) {
                    continue;
                }
                if (manager->HasBlockingUnmodifiedBindForKey(key)) {
                    if (auto* handler = mapping.getter(controls)) {
                        handler->SetInputEventHandlingEnabled(false);
                    }
                }
                break;  // at most one keyboard key maps to a given event
            }
        }
    }
}
