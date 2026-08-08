#pragma once


#include "Hotkeys/HotkeyManager.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace Hotkeys {
    class InputHandler final {
    public:
        static InputHandler* GetSingleton();

        [[nodiscard]] bool HandleKeyboardButtonEvent(RE::ButtonEvent& a_button);

        void BeginCapture();
        void CancelCapture();
        [[nodiscard]] bool IsCapturing() const noexcept { return m_capturing; }
        [[nodiscard]] bool TryConsumeCapturedKey(std::uint32_t& a_outIdCode);

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

        [[nodiscard]] bool HandleKeyEvent(std::uint32_t a_idCode, RE::ButtonEvent& a_button, const Settings& a_settings);

        bool m_modifierHeld = false;
        std::unordered_map<std::uint32_t, KeyPressState> m_pressStates;

        bool m_capturing = false;
        std::optional<std::uint32_t> m_capturedKey;
    };
}
