#include "Hotkeys/ContinuousMovement.h"

#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/HotkeyManager.h"

#include <array>
#include <chrono>
#include <vector>

namespace Hotkeys::ContinuousMovement {
    namespace {
        struct DirectionState {
            bool active = false;
            bool firstTick = false;
            std::chrono::steady_clock::time_point activatedAt;
        };

        constexpr std::size_t kDirectionCount = 4;
        std::array<DirectionState, kDirectionCount> g_state;

        std::vector<MovementDirection> g_pendingStops;

        [[nodiscard]] const RE::BSFixedString& UserEventFor(MovementDirection a_direction) {
            auto* events = RE::UserEvents::GetSingleton();
            switch (a_direction) {
                case MovementDirection::kForward:
                    return events->forward;
                case MovementDirection::kBackward:
                    return events->back;
                case MovementDirection::kStrafeLeft:
                    return events->strafeLeft;
                case MovementDirection::kStrafeRight:
                default:
                    return events->strafeRight;
            }
        }

        [[nodiscard]] std::uint32_t CanonicalIdCodeFor(MovementDirection a_direction) {
            switch (a_direction) {
                case MovementDirection::kForward:
                    return DXScanCode::kW;
                case MovementDirection::kBackward:
                    return DXScanCode::kS;
                case MovementDirection::kStrafeLeft:
                    return DXScanCode::kA;
                case MovementDirection::kStrafeRight:
                default:
                    return DXScanCode::kD;
            }
        }

        [[nodiscard]] RE::InputEvent* MakeEvent(MovementDirection a_direction, float a_value, float a_heldDownSecs) {
            auto* event =
                RE::ButtonEvent::Create(RE::INPUT_DEVICE::kKeyboard, UserEventFor(a_direction), CanonicalIdCodeFor(a_direction), a_value, a_heldDownSecs);
            return event;
        }
    }

    void SetActive(MovementDirection a_direction, bool a_active) {
        auto& state = g_state[static_cast<std::size_t>(a_direction)];
        if (a_active) {
            if (state.active) {
                return;
            }
            state.active = true;
            state.firstTick = true;
            state.activatedAt = std::chrono::steady_clock::now();
        } else {
            if (!state.active) {
                return;
            }
            state.active = false;
            g_pendingStops.push_back(a_direction);
        }
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

        for (auto direction : g_pendingStops) {
            auto& state = g_state[static_cast<std::size_t>(direction)];
            float heldDownSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - state.activatedAt).count();
            if (heldDownSecs <= 0.0f) {
                heldDownSecs = 0.001f;
            }
            append(MakeEvent(direction, 0.0f, heldDownSecs));
        }
        g_pendingStops.clear();

        bool freeCameraActive = false;
        if (auto* camera = RE::PlayerCamera::GetSingleton()) {
            freeCameraActive = camera->IsInFreeCameraMode();
        }

        if (!freeCameraActive && HotkeyManager::GetSingleton()->IsSafeToAct()) {
            for (std::size_t i = 0; i < kDirectionCount; ++i) {
                auto& state = g_state[i];
                if (!state.active) {
                    continue;
                }
                auto direction = static_cast<MovementDirection>(i);
                if (state.firstTick) {
                    state.firstTick = false;
                    append(MakeEvent(direction, 1.0f, 0.0f));
                } else {
                    float heldDownSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - state.activatedAt).count();
                    append(MakeEvent(direction, 1.0f, heldDownSecs));
                }
            }
        }

        return head;
    }

    void StopAll() {
        for (std::size_t i = 0; i < kDirectionCount; ++i) {
            SetActive(static_cast<MovementDirection>(i), false);
        }
    }
}
