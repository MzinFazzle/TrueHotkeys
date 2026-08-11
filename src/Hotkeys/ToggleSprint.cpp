#include "Hotkeys/ToggleSprint.h"

#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/HotkeyManager.h"

#include <chrono>

namespace Hotkeys::ToggleSprint {
    namespace {
        bool g_active = false;
        bool g_firstTick = false;
        bool g_pendingStop = false;
        bool g_hasSprintedThisCycle = false;
        std::chrono::steady_clock::time_point g_activatedAt;

        [[nodiscard]] const RE::BSFixedString& SprintUserEvent() { return RE::UserEvents::GetSingleton()->sprint; }

        [[nodiscard]] std::uint32_t CanonicalIdCode() { return DXScanCode::kLeftAlt; }

        [[nodiscard]] RE::InputEvent* MakeEvent(float a_value, float a_heldDownSecs) {
            return RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard, SprintUserEvent(), CanonicalIdCode(), a_value, a_heldDownSecs);
        }

        void Deactivate() {
            g_active = false;
            g_pendingStop = true;
        }
    }

    void Toggle() {
        if (g_active) {
            Deactivate();
        } else {
            g_active = true;
            g_firstTick = true;
            g_hasSprintedThisCycle = false;
            g_activatedAt = std::chrono::steady_clock::now();
        }
    }

    RE::InputEvent* BuildFrameEvents() {
        if (g_active) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (player->AsActorState()->IsSprinting()) {
                    g_hasSprintedThisCycle = true;
                } else if (g_hasSprintedThisCycle) {
                    Deactivate();
                }
            }
        }

        if (g_pendingStop) {
            g_pendingStop = false;
            float heldDownSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_activatedAt).count();
            if (heldDownSecs <= 0.0f) {
                heldDownSecs = 0.001f;
            }
            return MakeEvent(0.0f, heldDownSecs);
        }

        if (!g_active || !HotkeyManager::GetSingleton()->IsSafeToAct()) {
            return nullptr;
        }

        if (g_firstTick) {
            g_firstTick = false;
            return MakeEvent(1.0f, 0.0f);
        }
        float heldDownSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_activatedAt).count();
        return MakeEvent(1.0f, heldDownSecs);
    }

    void Stop() {
        if (g_active) {
            Deactivate();
        }
    }
}
