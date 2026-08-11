#pragma once


#include "Hotkeys/HotkeyManager.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace Hotkeys {
    class InputHandler final {
    public:
        static InputHandler* GetSingleton();

        [[nodiscard]] bool HandleKeyboardButtonEvent(RE::ButtonEvent& a_button);

        [[nodiscard]] bool HandleGamepadButtonEvent(RE::ButtonEvent& a_button);

        void BeginCapture(bool a_combinesWithModifier = true);
        void CancelCapture();
        [[nodiscard]] bool IsCapturing() const noexcept { return m_capturing; }

        [[nodiscard]] bool IsCaptureModifierHeld() const noexcept { return m_captureModifierHeld; }

        [[nodiscard]] bool TryConsumeCapturedKey(std::uint32_t& a_outIdCode, bool& a_outRequiresModifier);

        void ReportExternalCapture(std::uint32_t a_idCode);

    private:
        InputHandler() = default;
        InputHandler(const InputHandler&) = delete;
        InputHandler(InputHandler&&) = delete;

        struct KeyPressState {
            bool holdFired = false;             // Hold already triggered for this press
            bool modifierHeldAtPress = false;   // snapshot so releasing the modifier mid-hold doesn't change the outcome
            bool isMovementBind = false;
        };

        [[nodiscard]] bool HandleKeyEvent(std::uint32_t a_idCode, bool a_isDown, bool a_isPressed, float a_heldDuration, const Settings& a_settings, bool a_modifierHeld);

        [[nodiscard]] bool DispatchGamepadTrigger(std::uint32_t a_triggerCode, bool& a_heldPastThreshold, RE::ButtonEvent& a_button,
                                                    const Settings& a_settings);

        [[nodiscard]] bool IsGamepadCaptureDebounced() const;

        bool m_modifierHeld = false;
        bool m_gamepadModifierHeld = false;
        bool m_leftTriggerHeldPastThreshold = false;
        bool m_rightTriggerHeldPastThreshold = false;
        std::unordered_map<std::uint32_t, KeyPressState> m_pressStates;

        bool m_capturing = false;
        std::optional<std::uint32_t> m_capturedKey;
        bool m_captureModifierHeld = false;
        bool m_captureRequiresModifier = false;
        bool m_captureCombinesWithModifier = true;

        std::chrono::steady_clock::time_point m_gamepadCaptureStartTime{};
    };
}
