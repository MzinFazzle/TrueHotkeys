#include "Hotkeys/SyntheticTap.h"

#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/HotkeyManager.h"

#include <array>
#include <chrono>

namespace Hotkeys::SyntheticTap {
    namespace {
        enum class Phase : std::uint8_t {
            kIdle,
            kPendingDown,
            kPendingUp,
        };

        struct TapState {
            Phase phase = Phase::kIdle;
            std::chrono::steady_clock::time_point downAt;
        };

        constexpr std::size_t kKindCount = 5;
        std::array<TapState, kKindCount> g_state;

        [[nodiscard]] const RE::BSFixedString& UserEventFor(Kind a_kind) {
            auto* events = RE::UserEvents::GetSingleton();
            switch (a_kind) {
                case Kind::kSneak:
                    return events->sneak;
                case Kind::kAutoMove:
                    return events->autoMove;
                case Kind::kQuickSave:
                    return events->quicksave;
                case Kind::kQuickLoad:
                    return events->quickload;
                case Kind::kJump:
                default:
                    return events->jump;
            }
        }

        [[nodiscard]] std::uint32_t CanonicalIdCodeFor(Kind a_kind) {
            switch (a_kind) {
                case Kind::kSneak:
                    return DXScanCode::kLeftControl;
                case Kind::kAutoMove:
                    return DXScanCode::kNumpadEnter;
                case Kind::kQuickSave:
                    return DXScanCode::kF5;
                case Kind::kQuickLoad:
                    return DXScanCode::kF9;
                case Kind::kJump:
                default:
                    return DXScanCode::kSpace;
            }
        }

        [[nodiscard]] RE::InputEvent* MakeEvent(Kind a_kind, float a_value, float a_heldDownSecs) {
            return RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard, UserEventFor(a_kind), CanonicalIdCodeFor(a_kind), a_value, a_heldDownSecs);
        }

        bool g_autoMoveWasOff = true;
        std::chrono::steady_clock::time_point g_autoMoveTurnedOnAt;

        [[nodiscard]] bool IsRealMovementInput(const RE::InputEvent& a_event) {
            if (a_event.GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
                const auto* stick = a_event.AsThumbstickEvent();
                if (!stick || !stick->IsLeft()) {
                    return false;
                }
                constexpr float kDeadzone = 0.1f;
                return (stick->xValue * stick->xValue + stick->yValue * stick->yValue) > (kDeadzone * kDeadzone);
            }
            if (a_event.GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                const auto* button = a_event.AsButtonEvent();
                if (!button || !button->IsPressed()) {
                    return false;  // IsPressed() covers both a fresh down and every subsequent held/repeat frame
                }
                auto* events = RE::UserEvents::GetSingleton();
                const auto& userEvent = button->QUserEvent();
                return userEvent == events->forward || userEvent == events->back || userEvent == events->strafeLeft ||
                       userEvent == events->strafeRight;
            }
            return false;
        }
    }

    void CheckAutoMoveCancel(RE::InputEvent* a_headEvent) {
        auto* controls = RE::PlayerControls::GetSingleton();
        if (!controls) {
            return;
        }

        const bool autoMoveOn = controls->data.autoMove;
        if (autoMoveOn && g_autoMoveWasOff) {
            g_autoMoveTurnedOnAt = std::chrono::steady_clock::now();
        }
        g_autoMoveWasOff = !autoMoveOn;

        if (!autoMoveOn) {
            return;
        }

        constexpr float kGraceSeconds = 0.5f;
        const float sinceOn = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_autoMoveTurnedOnAt).count();
        if (sinceOn < kGraceSeconds) {
            return;
        }

        for (RE::InputEvent* event = a_headEvent; event; event = event->next) {
            if (IsRealMovementInput(*event)) {
                SKSE::log::info("True Hotkeys: Toggle Auto Move auto-cancel triggered (real movement InputEvent detected).");
                Queue(Kind::kAutoMove);
                break;
            }
        }
    }

    void Queue(Kind a_kind) {
        auto& state = g_state[static_cast<std::size_t>(a_kind)];
        if (state.phase != Phase::kIdle) {
            return;  // already in flight - see this function's own header comment
        }
        state.phase = Phase::kPendingDown;
    }

    RE::InputEvent* BuildFrameEvents() {
        RE::InputEvent* head = nullptr;
        RE::InputEvent* tail = nullptr;
        auto append = [&](RE::InputEvent* a_event) {
            if (!a_event) {
                return;
            }
            if (!head) {
                head = tail = a_event;
            } else {
                tail->next = a_event;
                tail = a_event;
            }
        };

        const bool safeToAct = HotkeyManager::GetSingleton()->IsSafeToAct();

        for (std::size_t i = 0; i < kKindCount; ++i) {
            auto& state = g_state[i];
            auto kind = static_cast<Kind>(i);
            switch (state.phase) {
                case Phase::kIdle:
                    break;
                case Phase::kPendingDown:
                    if (!safeToAct) {
                        break;
                    }
                    append(MakeEvent(kind, 1.0f, 0.0f));
                    state.downAt = std::chrono::steady_clock::now();
                    state.phase = Phase::kPendingUp;
                    break;
                case Phase::kPendingUp: {
                    float heldDownSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - state.downAt).count();
                    if (heldDownSecs <= 0.0f) {
                        heldDownSecs = 0.001f;
                    }
                    append(MakeEvent(kind, 0.0f, heldDownSecs));
                    state.phase = Phase::kIdle;
                    break;
                }
            }
        }

        return head;
    }
}
