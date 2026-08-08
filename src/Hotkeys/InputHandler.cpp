#include "Hotkeys/InputHandler.h"

#include "Hotkeys/ConfigUI.h"

namespace Hotkeys {
    InputHandler* InputHandler::GetSingleton() {
        static InputHandler singleton;
        return &singleton;
    }

    void InputHandler::BeginCapture() {
        m_capturing = true;
        m_capturedKey.reset();
    }

    void InputHandler::CancelCapture() {
        m_capturing = false;
        m_capturedKey.reset();
    }

    bool InputHandler::TryConsumeCapturedKey(std::uint32_t& a_outIdCode) {
        if (!m_capturedKey) {
            return false;
        }
        a_outIdCode = *m_capturedKey;
        m_capturedKey.reset();
        return true;
    }

    void InputHandler::ReportExternalCapture(std::uint32_t a_idCode) {
        if (!m_capturing) {
            return;
        }
        m_capturedKey = a_idCode;
        m_capturing = false;
    }

    bool InputHandler::HandleKeyboardButtonEvent(RE::ButtonEvent& a_button) {
        auto* manager = HotkeyManager::GetSingleton();
        const auto& settings = manager->GetSettings();
        auto idCode = a_button.GetIDCode();

        if (m_capturing) {
            if (a_button.IsDown()) {
                m_capturedKey = idCode;
                m_capturing = false;
            }
            return false;
        }

        if (ConfigUI::IsMenuOpen()) {
            return false;
        }

        if (!settings.enabled) {
            return false;
        }

        if (settings.modifierKeyCode != 0 && idCode == settings.modifierKeyCode) {
            m_modifierHeld = a_button.IsPressed();
            return false;
        }

        auto cycleKeyCode = manager->GetProfileCycleKeyCode();
        if (cycleKeyCode != 0 && idCode == cycleKeyCode) {
            bool requiresModifier = manager->GetProfileCycleRequiresModifier();
            bool modifierSatisfied = requiresModifier ? m_modifierHeld : !m_modifierHeld;
            if (modifierSatisfied) {
                if (a_button.IsDown()) {
                    manager->CycleProfile();
                }
                return false;
            }
        }

        return HandleKeyEvent(idCode, a_button, settings);
    }

    bool InputHandler::HandleKeyEvent(std::uint32_t a_idCode, RE::ButtonEvent& a_button, const Settings& a_settings) {
        auto* manager = HotkeyManager::GetSingleton();
        auto& state = m_pressStates[a_idCode];

        if (a_button.IsDown()) {
            state.holdFired = false;
            state.modifierHeldAtPress = m_modifierHeld;
            state.isMovementBind = manager->IsMovementBind(a_idCode, state.modifierHeldAtPress);
            if (state.isMovementBind) {
                manager->TriggerMovement(a_idCode, state.modifierHeldAtPress, true);
            }
        } else if (a_button.IsPressed()) {
            if (!state.isMovementBind && !state.holdFired && a_button.HeldDuration() >= a_settings.holdThresholdSeconds) {
                state.holdFired = true;
                manager->TriggerBind(BindKey{a_idCode, state.modifierHeldAtPress, PressType::kHold});
            }
        } else {
            if (state.isMovementBind) {
                manager->TriggerMovement(a_idCode, state.modifierHeldAtPress, false);
            } else if (!state.holdFired) {
                manager->TriggerBind(BindKey{a_idCode, state.modifierHeldAtPress, PressType::kTap});
            }
        }

        bool modifierHeldForGesture = state.modifierHeldAtPress;

        if (!a_button.IsDown() && !a_button.IsPressed()) {
            m_pressStates.erase(a_idCode);
        }

        return manager->ShouldBlockVanillaForKeyAndModifier(a_idCode, modifierHeldForGesture);
    }
}
