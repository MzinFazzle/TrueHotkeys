#pragma once


#include <cstddef>
#include <cstdint>
#include <functional>

namespace Hotkeys {
    enum class PressType : std::uint8_t {
        kTap,
        kHold,
    };

    struct BindKey {
        std::uint32_t idCode = 0;
        bool modifierHeld = false;
        PressType press = PressType::kTap;

        [[nodiscard]] bool operator==(const BindKey&) const noexcept = default;
    };
}

template <>
struct std::hash<Hotkeys::BindKey> {
    std::size_t operator()(const Hotkeys::BindKey& a_key) const noexcept {
        std::size_t packed = a_key.idCode;
        packed |= (a_key.modifierHeld ? 1ULL : 0ULL) << 32;
        packed |= (static_cast<std::size_t>(a_key.press)) << 33;
        return std::hash<std::size_t>{}(packed);
    }
};
