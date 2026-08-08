#pragma once


namespace Hotkeys {
    class ConfigUI {
    public:
        static void Install();

        [[nodiscard]] static bool IsMenuOpen();
    };
}
