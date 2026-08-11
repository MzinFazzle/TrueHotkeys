#include "Hotkeys/InputDispatchHook.h"

#include "Hotkeys/ContinuousMovement.h"
#include "Hotkeys/InputHandler.h"
#include "Hotkeys/SyntheticTap.h"
#include "Hotkeys/ToggleSprint.h"

namespace Hotkeys::InputDispatchHook {
    namespace {
        struct ProcessInput {
            static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_event) {
                RE::InputEvent* head = (a_event && *a_event) ? *a_event : nullptr;

                RE::InputEvent* prev = nullptr;

                for (RE::InputEvent* event = head; event;) {
                    RE::InputEvent* next = event->next;
                    bool suppress = false;

                    if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                        if (auto* button = event->AsButtonEvent()) {
                            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                                suppress = InputHandler::GetSingleton()->HandleKeyboardButtonEvent(*button);
                            } else if (button->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                                suppress = InputHandler::GetSingleton()->HandleGamepadButtonEvent(*button);
                            }
                        }
                    }

                    if (suppress) {
                        if (prev) {
                            prev->next = next;
                        } else {
                            head = next;
                        }
                    } else {
                        prev = event;
                    }
                    event = next;
                }

                if (auto* movementEvents = ContinuousMovement::BuildFrameEvents()) {
                    if (prev) {
                        prev->next = movementEvents;
                    } else {
                        head = movementEvents;
                    }
                    prev = movementEvents;
                    while (prev->next) {
                        prev = prev->next;
                    }
                }

                SyntheticTap::CheckAutoMoveCancel(head);

                if (auto* tapEvents = SyntheticTap::BuildFrameEvents()) {
                    if (prev) {
                        prev->next = tapEvents;
                    } else {
                        head = tapEvents;
                    }
                    prev = tapEvents;
                    while (prev->next) {
                        prev = prev->next;
                    }
                }

                if (auto* sprintEvent = ToggleSprint::BuildFrameEvents()) {
                    if (prev) {
                        prev->next = sprintEvent;
                    } else {
                        head = sprintEvent;
                    }
                }

                RE::InputEvent* const filteredHead = head;
                func(a_dispatcher, &filteredHead);
            }

            static inline REL::Relocation<decltype(thunk)> func;
            static inline constexpr std::size_t size{5};
        };
    }

    void Install() {
        const std::uintptr_t address = REL::RelocationID(67315, 68617).address() + 0x7B;
        stl::write_thunk_call<ProcessInput>(address);
        SKSE::log::info("True Hotkeys: input dispatch hook installed.");
    }
}
