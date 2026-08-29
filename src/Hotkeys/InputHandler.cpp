#include "Hotkeys/InputHandler.h"

#include "Hotkeys/ConfigUI.h"
#include "Hotkeys/FormRef.h"
#include "Hotkeys/GamepadButtons.h"

#include <array>

namespace Hotkeys {
    namespace {
        constexpr std::chrono::milliseconds kGamepadCaptureDebounce{200};

        [[nodiscard]] const std::array<RE::TESGlobal*, 4>& GamepadPlusPlusComboGlobals() {
            static const std::array<RE::TESGlobal*, 4> cached = [] {
                constexpr std::array<std::uint32_t, 4> kLocalFormIDs = {0x801, 0x802, 0x803, 0x804};
                std::array<RE::TESGlobal*, 4> result{};
                for (std::size_t i = 0; i < kLocalFormIDs.size(); ++i) {
                    result[i] = FormRef{"Gamepad++.esp", kLocalFormIDs[i]}.Resolve<RE::TESGlobal>();
                }
                return result;
            }();
            return cached;
        }

        [[nodiscard]] std::optional<std::uint32_t> CkGamepadCodeToUnifiedCode(int a_ckCode) {
            constexpr int kFirstCkCode = 266;
            constexpr int kLastCkCode = 281;
            if (a_ckCode < kFirstCkCode || a_ckCode > kLastCkCode) {
                return std::nullopt;
            }
            return GamepadButton::kBase + static_cast<std::uint32_t>(a_ckCode - kFirstCkCode);
        }
    }

    InputHandler* InputHandler::GetSingleton() {
        static InputHandler singleton;
        return &singleton;
    }

    void InputHandler::BeginCapture(bool a_combinesWithModifier) {
        m_capturing = true;
        m_capturedKey.reset();
        m_captureModifierHeld = false;
        m_captureRequiresModifier = false;
        m_captureCombinesWithModifier = a_combinesWithModifier;
        m_gamepadCaptureStartTime = std::chrono::steady_clock::now();
    }

    void InputHandler::CancelCapture() {
        m_capturing = false;
        m_capturedKey.reset();
    }

    bool InputHandler::TryConsumeCapturedKey(std::uint32_t& a_outIdCode, bool& a_outRequiresModifier) {
        if (!m_capturedKey) {
            return false;
        }
        a_outIdCode = *m_capturedKey;
        a_outRequiresModifier = m_captureRequiresModifier;
        m_capturedKey.reset();
        return true;
    }

    bool InputHandler::IsGamepadCaptureDebounced() const {
        return (std::chrono::steady_clock::now() - m_gamepadCaptureStartTime) < kGamepadCaptureDebounce;
    }

    bool InputHandler::IsGamepadPlusPlusAnchorHeld(std::uint32_t a_excludeIdCode) const {
        for (auto* global : GamepadPlusPlusComboGlobals()) {
            if (!global) {
                continue;  // Gamepad++.esp not installed, or this specific global doesn't exist
            }
            auto ckCode = static_cast<int>(global->value);
            auto unifiedCode = CkGamepadCodeToUnifiedCode(ckCode);
            if (!unifiedCode || *unifiedCode == a_excludeIdCode) {
                continue;
            }
            if (m_pressStates.contains(*unifiedCode)) {
                return true;
            }
        }
        return false;
    }

    void InputHandler::ReportExternalCapture(std::uint32_t a_idCode) {
        if (!m_capturing) {
            return;
        }
        const auto& settings = HotkeyManager::GetSingleton()->GetSettings();
        bool isGamepad = GamepadButton::IsGamepadCode(a_idCode);

        if (m_captureCombinesWithModifier && !isGamepad && settings.modifierKeyCode != 0 && a_idCode == settings.modifierKeyCode) {
            m_captureModifierHeld = true;
            return;
        }

        if (isGamepad && IsGamepadCaptureDebounced()) {
            return;
        }

        if (m_captureCombinesWithModifier && isGamepad && settings.modifierGamepadCode != 0 && a_idCode == settings.modifierGamepadCode) {
            m_captureModifierHeld = !m_captureModifierHeld;
            return;
        }

        m_capturedKey = a_idCode;
        m_captureRequiresModifier = m_captureModifierHeld;
        m_capturing = false;
    }

    bool InputHandler::HandleKeyboardButtonEvent(RE::ButtonEvent& a_button) {
        auto* manager = HotkeyManager::GetSingleton();
        const auto& settings = manager->GetSettings();
        auto idCode = a_button.GetIDCode();

        if (m_capturing) {
            if (m_captureCombinesWithModifier && settings.modifierKeyCode != 0 && idCode == settings.modifierKeyCode) {
                m_captureModifierHeld = a_button.IsPressed();
                return false;
            }
            if (a_button.IsDown()) {
                m_capturedKey = idCode;
                m_captureRequiresModifier = m_captureModifierHeld;
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
            return settings.modifierBlocksVanillaHotkey && manager->IsSafeToAct();
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

        return HandleKeyEvent(idCode, a_button.IsDown(), a_button.IsPressed(), a_button.HeldDuration(), settings, m_modifierHeld);
    }

    bool InputHandler::HandleGamepadButtonEvent(RE::ButtonEvent& a_button) {
        auto* manager = HotkeyManager::GetSingleton();
        const auto& settings = manager->GetSettings();
        auto rawCode = a_button.GetIDCode();

        if (m_capturing) {
            if (m_captureCombinesWithModifier) {
                if (settings.modifierGamepadCode == GamepadButton::kLeftTrigger && rawCode == GamepadButton::kRawLeftTrigger) {
                    m_captureModifierHeld = GamepadButton::IsTriggerHeld(a_button);
                    return false;
                }
                if (settings.modifierGamepadCode == GamepadButton::kRightTrigger && rawCode == GamepadButton::kRawRightTrigger) {
                    m_captureModifierHeld = GamepadButton::IsTriggerHeld(a_button);
                    return false;
                }
                if (auto code = GamepadButton::ToUnifiedCode(a_button); code && *code == settings.modifierGamepadCode) {
                    return false;
                }
            }

            if (a_button.IsDown() && !IsGamepadCaptureDebounced()) {
                if (auto code = GamepadButton::ToUnifiedCode(a_button)) {
                    m_capturedKey = *code;
                    m_captureRequiresModifier = m_captureModifierHeld;
                    m_capturing = false;
                } else if (rawCode == GamepadButton::kRawLeftTrigger && GamepadButton::IsTriggerHeld(a_button)) {
                    m_capturedKey = GamepadButton::kLeftTrigger;
                    m_captureRequiresModifier = m_captureModifierHeld;
                    m_capturing = false;
                } else if (rawCode == GamepadButton::kRawRightTrigger && GamepadButton::IsTriggerHeld(a_button)) {
                    m_capturedKey = GamepadButton::kRightTrigger;
                    m_captureRequiresModifier = m_captureModifierHeld;
                    m_capturing = false;
                }
            }
            return false;
        }

        if (ConfigUI::IsMenuOpen()) {
            return false;
        }
        if (!settings.enabled) {
            return false;
        }

        if (settings.modifierGamepadCode == GamepadButton::kLeftTrigger && rawCode == GamepadButton::kRawLeftTrigger) {
            m_gamepadModifierHeld = GamepadButton::IsTriggerHeld(a_button);
            return settings.modifierBlocksVanillaHotkey && manager->IsSafeToAct();
        }
        if (settings.modifierGamepadCode == GamepadButton::kRightTrigger && rawCode == GamepadButton::kRawRightTrigger) {
            m_gamepadModifierHeld = GamepadButton::IsTriggerHeld(a_button);
            return settings.modifierBlocksVanillaHotkey && manager->IsSafeToAct();
        }
        if (auto code = GamepadButton::ToUnifiedCode(a_button); code && *code == settings.modifierGamepadCode) {
            m_gamepadModifierHeld = a_button.IsPressed();
            return settings.modifierBlocksVanillaHotkey && manager->IsSafeToAct();
        }

        if (auto code = GamepadButton::ToUnifiedCode(a_button)) {
            return HandleKeyEvent(*code, a_button.IsDown(), a_button.IsPressed(), a_button.HeldDuration(), settings, m_gamepadModifierHeld);
        }
        if (rawCode == GamepadButton::kRawLeftTrigger) {
            return DispatchGamepadTrigger(GamepadButton::kLeftTrigger, m_leftTriggerHeldPastThreshold, a_button, settings);
        }
        if (rawCode == GamepadButton::kRawRightTrigger) {
            return DispatchGamepadTrigger(GamepadButton::kRightTrigger, m_rightTriggerHeldPastThreshold, a_button, settings);
        }
        return false;
    }

    bool InputHandler::DispatchGamepadTrigger(std::uint32_t a_triggerCode, bool& a_heldPastThreshold, RE::ButtonEvent& a_button,
                                               const Settings& a_settings) {
        bool nowPastThreshold = GamepadButton::IsTriggerHeld(a_button);
        if (!a_heldPastThreshold && !nowPastThreshold) {
            return false;  // still below threshold - nothing crossed, nothing to do
        }

        bool isDown = !a_heldPastThreshold && nowPastThreshold;
        bool isPressed = nowPastThreshold;
        a_heldPastThreshold = nowPastThreshold;

        return HandleKeyEvent(a_triggerCode, isDown, isPressed, a_button.HeldDuration(), a_settings, m_gamepadModifierHeld);
    }

    bool InputHandler::HandleKeyEvent(std::uint32_t a_idCode, bool a_isDown, bool a_isPressed, float a_heldDuration, const Settings& a_settings,
                                       bool a_modifierHeld) {
        auto* manager = HotkeyManager::GetSingleton();
        auto& state = m_pressStates[a_idCode];

        if (a_isDown) {
            state.holdFired = false;
            state.modifierHeldAtPress = a_modifierHeld;
            state.gamepadPlusPlusStandDown =
                a_settings.gamepadPlusPlusCompat && GamepadButton::IsGamepadCode(a_idCode) && IsGamepadPlusPlusAnchorHeld(a_idCode);
            state.isMovementBind = manager->IsMovementBind(a_idCode, state.modifierHeldAtPress);
            if (state.isMovementBind && !state.gamepadPlusPlusStandDown) {
                manager->TriggerMovement(a_idCode, state.modifierHeldAtPress, true);
            }
        } else if (a_isPressed) {
            if (!state.isMovementBind && !state.holdFired && !state.gamepadPlusPlusStandDown && a_heldDuration >= a_settings.holdThresholdSeconds &&
                manager->HasBind(BindKey{a_idCode, state.modifierHeldAtPress, PressType::kHold})) {
                state.holdFired = true;
                manager->TriggerBind(BindKey{a_idCode, state.modifierHeldAtPress, PressType::kHold});
            }
        } else {
            if (state.isMovementBind) {
                if (!state.gamepadPlusPlusStandDown) {
                    manager->TriggerMovement(a_idCode, state.modifierHeldAtPress, false);
                }
            } else if (!state.holdFired && !state.gamepadPlusPlusStandDown) {
                manager->TriggerBind(BindKey{a_idCode, state.modifierHeldAtPress, PressType::kTap});
            }
        }

        bool modifierHeldForGesture = state.modifierHeldAtPress;
        bool standDownForGesture = state.gamepadPlusPlusStandDown;

        if (!a_isDown && !a_isPressed) {
            m_pressStates.erase(a_idCode);
        }

        if (standDownForGesture) {
            return false;
        }

        return manager->ShouldBlockVanillaForKeyAndModifier(a_idCode, modifierHeldForGesture);
    }
}
