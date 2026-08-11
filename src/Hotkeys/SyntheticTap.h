#pragma once


#include "Hotkeys/HotkeyAction.h"

namespace RE {
    class InputEvent;
}

namespace Hotkeys::SyntheticTap {
    enum class Kind : std::uint8_t {
        kSneak,
        kAutoMove,
        kJump,

        kQuickSave,
        kQuickLoad,
    };

    void CheckAutoMoveCancel(RE::InputEvent* a_headEvent);

    void Queue(Kind a_kind);

    [[nodiscard]] RE::InputEvent* BuildFrameEvents();
}
