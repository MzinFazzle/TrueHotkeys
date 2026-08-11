#pragma once


#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Hotkeys::GamepadButton {
    inline constexpr std::uint32_t kBase = 0x1000;

    enum : std::uint32_t {
        kIndexUp = 0,
        kIndexDown,
        kIndexLeft,
        kIndexRight,
        kIndexStart,
        kIndexBack,
        kIndexLeftThumb,
        kIndexRightThumb,
        kIndexLeftShoulder,
        kIndexRightShoulder,
        kIndexA,
        kIndexB,
        kIndexX,
        kIndexY,
        kIndexLeftTrigger,
        kIndexRightTrigger,
    };

    inline constexpr std::uint32_t kA = kBase + kIndexA;
    inline constexpr std::uint32_t kB = kBase + kIndexB;
    inline constexpr std::uint32_t kX = kBase + kIndexX;
    inline constexpr std::uint32_t kY = kBase + kIndexY;
    inline constexpr std::uint32_t kLeftShoulder = kBase + kIndexLeftShoulder;
    inline constexpr std::uint32_t kRightShoulder = kBase + kIndexRightShoulder;
    inline constexpr std::uint32_t kLeftThumb = kBase + kIndexLeftThumb;
    inline constexpr std::uint32_t kRightThumb = kBase + kIndexRightThumb;
    inline constexpr std::uint32_t kStart = kBase + kIndexStart;
    inline constexpr std::uint32_t kBack = kBase + kIndexBack;
    inline constexpr std::uint32_t kDpadUp = kBase + kIndexUp;
    inline constexpr std::uint32_t kDpadDown = kBase + kIndexDown;
    inline constexpr std::uint32_t kDpadLeft = kBase + kIndexLeft;
    inline constexpr std::uint32_t kDpadRight = kBase + kIndexRight;
    inline constexpr std::uint32_t kLeftTrigger = kBase + kIndexLeftTrigger;
    inline constexpr std::uint32_t kRightTrigger = kBase + kIndexRightTrigger;

    [[nodiscard]] inline constexpr bool IsGamepadCode(std::uint32_t a_code) noexcept {
        return a_code >= kBase;
    }

    inline constexpr std::uint32_t kRawLeftTrigger = 0x0009;
    inline constexpr std::uint32_t kRawRightTrigger = 0x000A;

    inline constexpr float kTriggerThreshold = 0.15f;

    [[nodiscard]] inline bool IsTriggerHeld(RE::ButtonEvent& a_event) {
        return a_event.Value() > kTriggerThreshold;
    }

    namespace detail {
        [[nodiscard]] inline const std::unordered_map<std::string_view, std::uint32_t>& NameToCodeMap() {
            static const std::unordered_map<std::string_view, std::uint32_t> map{
                {"GamepadA", kA},
                {"GamepadB", kB},
                {"GamepadX", kX},
                {"GamepadY", kY},
                {"GamepadLB", kLeftShoulder},
                {"GamepadRB", kRightShoulder},
                {"GamepadLS", kLeftThumb},
                {"GamepadRS", kRightThumb},
                {"GamepadStart", kStart},
                {"GamepadBack", kBack},
                {"GamepadDUp", kDpadUp},
                {"GamepadDDown", kDpadDown},
                {"GamepadDLeft", kDpadLeft},
                {"GamepadDRight", kDpadRight},
                {"GamepadLT", kLeftTrigger},
                {"GamepadRT", kRightTrigger},
            };
            return map;
        }
    }

    [[nodiscard]] inline std::optional<std::uint32_t> ButtonNameToCode(std::string_view a_name) {
        const auto& map = detail::NameToCodeMap();
        auto it = map.find(a_name);
        if (it == map.end()) return std::nullopt;
        return it->second;
    }

    [[nodiscard]] inline std::optional<std::string> CodeToButtonName(std::uint32_t a_code) {
        for (const auto& [name, code] : detail::NameToCodeMap()) {
            if (code == a_code) return std::string(name);
        }
        return std::nullopt;
    }

    [[nodiscard]] inline std::optional<std::string> DisplayName(std::uint32_t a_code) {
        static const std::unordered_map<std::uint32_t, const char*> display{
            {kA, "A"}, {kB, "B"}, {kX, "X"}, {kY, "Y"},
            {kLeftShoulder, "LB"}, {kRightShoulder, "RB"},
            {kLeftThumb, "LS"}, {kRightThumb, "RS"},
            {kStart, "Start"}, {kBack, "Back"},
            {kDpadUp, "D-Up"}, {kDpadDown, "D-Down"},
            {kDpadLeft, "D-Left"}, {kDpadRight, "D-Right"},
            {kLeftTrigger, "LT"}, {kRightTrigger, "RT"},
        };
        auto it = display.find(a_code);
        if (it == display.end()) return std::nullopt;
        return std::string(it->second);
    }

    [[nodiscard]] inline std::optional<std::uint32_t> ToUnifiedCode(RE::ButtonEvent& a_event) {
        if (a_event.GetDevice() != RE::INPUT_DEVICE::kGamepad) {
            return std::nullopt;
        }
        switch (a_event.GetIDCode()) {
            case 0x0001: return kDpadUp;
            case 0x0002: return kDpadDown;
            case 0x0004: return kDpadLeft;
            case 0x0008: return kDpadRight;
            case 0x0010: return kStart;
            case 0x0020: return kBack;
            case 0x0040: return kLeftThumb;
            case 0x0080: return kRightThumb;
            case 0x0100: return kLeftShoulder;
            case 0x0200: return kRightShoulder;
            case 0x1000: return kA;
            case 0x2000: return kB;
            case 0x4000: return kX;
            case 0x8000: return kY;
            default: return std::nullopt;  // kRawLeftTrigger (0x0009)/kRawRightTrigger (0x000A) land here
        }
    }
}
