#pragma once


#include <cstdint>

namespace RE {
    class InputEvent;
}

namespace Hotkeys::ToggleSprint {
    void Toggle();

    [[nodiscard]] RE::InputEvent* BuildFrameEvents();

    void Stop();
}
