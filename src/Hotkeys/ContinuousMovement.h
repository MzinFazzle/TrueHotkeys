#pragma once


#include "Hotkeys/HotkeyAction.h"

namespace RE {
    class InputEvent;
}

namespace Hotkeys::ContinuousMovement {
    void SetActive(MovementDirection a_direction, bool a_active);

    [[nodiscard]] RE::InputEvent* BuildFrameEvents();

    void StopAll();
}
