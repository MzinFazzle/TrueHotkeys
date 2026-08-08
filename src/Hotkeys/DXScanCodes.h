#pragma once


#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Hotkeys::DXScanCode {
    inline constexpr std::uint32_t kEscape = 0x01;
    inline constexpr std::uint32_t k1 = 0x02;
    inline constexpr std::uint32_t k2 = 0x03;
    inline constexpr std::uint32_t k3 = 0x04;
    inline constexpr std::uint32_t k4 = 0x05;
    inline constexpr std::uint32_t k5 = 0x06;
    inline constexpr std::uint32_t k6 = 0x07;
    inline constexpr std::uint32_t k7 = 0x08;
    inline constexpr std::uint32_t k8 = 0x09;
    inline constexpr std::uint32_t k9 = 0x0A;
    inline constexpr std::uint32_t k0 = 0x0B;
    inline constexpr std::uint32_t kMinus = 0x0C;
    inline constexpr std::uint32_t kEquals = 0x0D;
    inline constexpr std::uint32_t kBackspace = 0x0E;
    inline constexpr std::uint32_t kTab = 0x0F;
    inline constexpr std::uint32_t kQ = 0x10;
    inline constexpr std::uint32_t kW = 0x11;
    inline constexpr std::uint32_t kE = 0x12;
    inline constexpr std::uint32_t kR = 0x13;
    inline constexpr std::uint32_t kT = 0x14;
    inline constexpr std::uint32_t kY = 0x15;
    inline constexpr std::uint32_t kU = 0x16;
    inline constexpr std::uint32_t kI = 0x17;
    inline constexpr std::uint32_t kO = 0x18;
    inline constexpr std::uint32_t kP = 0x19;
    inline constexpr std::uint32_t kLeftBracket = 0x1A;
    inline constexpr std::uint32_t kRightBracket = 0x1B;
    inline constexpr std::uint32_t kEnter = 0x1C;
    inline constexpr std::uint32_t kLeftControl = 0x1D;
    inline constexpr std::uint32_t kA = 0x1E;
    inline constexpr std::uint32_t kS = 0x1F;
    inline constexpr std::uint32_t kD = 0x20;
    inline constexpr std::uint32_t kF = 0x21;
    inline constexpr std::uint32_t kG = 0x22;
    inline constexpr std::uint32_t kH = 0x23;
    inline constexpr std::uint32_t kJ = 0x24;
    inline constexpr std::uint32_t kK = 0x25;
    inline constexpr std::uint32_t kL = 0x26;
    inline constexpr std::uint32_t kSemicolon = 0x27;
    inline constexpr std::uint32_t kApostrophe = 0x28;
    inline constexpr std::uint32_t kGrave = 0x29;
    inline constexpr std::uint32_t kLeftShift = 0x2A;
    inline constexpr std::uint32_t kBackslash = 0x2B;
    inline constexpr std::uint32_t kZ = 0x2C;
    inline constexpr std::uint32_t kX = 0x2D;
    inline constexpr std::uint32_t kC = 0x2E;
    inline constexpr std::uint32_t kV = 0x2F;
    inline constexpr std::uint32_t kB = 0x30;
    inline constexpr std::uint32_t kN = 0x31;
    inline constexpr std::uint32_t kM = 0x32;
    inline constexpr std::uint32_t kComma = 0x33;
    inline constexpr std::uint32_t kPeriod = 0x34;
    inline constexpr std::uint32_t kSlash = 0x35;
    inline constexpr std::uint32_t kRightShift = 0x36;
    inline constexpr std::uint32_t kNumpadMultiply = 0x37;
    inline constexpr std::uint32_t kLeftAlt = 0x38;
    inline constexpr std::uint32_t kSpace = 0x39;
    inline constexpr std::uint32_t kCapsLock = 0x3A;
    inline constexpr std::uint32_t kF1 = 0x3B;
    inline constexpr std::uint32_t kF2 = 0x3C;
    inline constexpr std::uint32_t kF3 = 0x3D;
    inline constexpr std::uint32_t kF4 = 0x3E;
    inline constexpr std::uint32_t kF5 = 0x3F;
    inline constexpr std::uint32_t kF6 = 0x40;
    inline constexpr std::uint32_t kF7 = 0x41;
    inline constexpr std::uint32_t kF8 = 0x42;
    inline constexpr std::uint32_t kF9 = 0x43;
    inline constexpr std::uint32_t kF10 = 0x44;
    inline constexpr std::uint32_t kNumLock = 0x45;
    inline constexpr std::uint32_t kScrollLock = 0x46;
    inline constexpr std::uint32_t kNumpad7 = 0x47;
    inline constexpr std::uint32_t kNumpad8 = 0x48;
    inline constexpr std::uint32_t kNumpad9 = 0x49;
    inline constexpr std::uint32_t kNumpadMinus = 0x4A;
    inline constexpr std::uint32_t kNumpad4 = 0x4B;
    inline constexpr std::uint32_t kNumpad5 = 0x4C;
    inline constexpr std::uint32_t kNumpad6 = 0x4D;
    inline constexpr std::uint32_t kNumpadPlus = 0x4E;
    inline constexpr std::uint32_t kNumpad1 = 0x4F;
    inline constexpr std::uint32_t kNumpad2 = 0x50;
    inline constexpr std::uint32_t kNumpad3 = 0x51;
    inline constexpr std::uint32_t kNumpad0 = 0x52;
    inline constexpr std::uint32_t kNumpadPeriod = 0x53;
    inline constexpr std::uint32_t kF11 = 0x57;
    inline constexpr std::uint32_t kF12 = 0x58;
    inline constexpr std::uint32_t kNumpadEnter = 0x9C;
    inline constexpr std::uint32_t kRightControl = 0x9D;
    inline constexpr std::uint32_t kNumpadDivide = 0xB5;
    inline constexpr std::uint32_t kRightAlt = 0xB8;
    inline constexpr std::uint32_t kHome = 0xC7;
    inline constexpr std::uint32_t kUp = 0xC8;
    inline constexpr std::uint32_t kPageUp = 0xC9;
    inline constexpr std::uint32_t kLeft = 0xCB;
    inline constexpr std::uint32_t kRight = 0xCD;
    inline constexpr std::uint32_t kEnd = 0xCF;
    inline constexpr std::uint32_t kDown = 0xD0;
    inline constexpr std::uint32_t kPageDown = 0xD1;
    inline constexpr std::uint32_t kInsert = 0xD2;
    inline constexpr std::uint32_t kDelete = 0xD3;

    namespace detail {
        inline const std::unordered_map<std::string_view, std::uint32_t>& NameToCodeMap() {
            static const std::unordered_map<std::string_view, std::uint32_t> map = {
                {"Escape", kEscape}, {"1", k1}, {"2", k2}, {"3", k3}, {"4", k4}, {"5", k5}, {"6", k6},
                {"7", k7}, {"8", k8}, {"9", k9}, {"0", k0}, {"Minus", kMinus}, {"Equals", kEquals},
                {"Backspace", kBackspace}, {"Tab", kTab}, {"Q", kQ}, {"W", kW}, {"E", kE}, {"R", kR},
                {"T", kT}, {"Y", kY}, {"U", kU}, {"I", kI}, {"O", kO}, {"P", kP},
                {"LeftBracket", kLeftBracket}, {"RightBracket", kRightBracket}, {"Enter", kEnter},
                {"LeftControl", kLeftControl}, {"A", kA}, {"S", kS}, {"D", kD}, {"F", kF}, {"G", kG},
                {"H", kH}, {"J", kJ}, {"K", kK}, {"L", kL}, {"Semicolon", kSemicolon},
                {"Apostrophe", kApostrophe}, {"Grave", kGrave}, {"LeftShift", kLeftShift},
                {"Backslash", kBackslash}, {"Z", kZ}, {"X", kX}, {"C", kC}, {"V", kV}, {"B", kB},
                {"N", kN}, {"M", kM}, {"Comma", kComma}, {"Period", kPeriod}, {"Slash", kSlash},
                {"RightShift", kRightShift}, {"NumpadMultiply", kNumpadMultiply}, {"LeftAlt", kLeftAlt},
                {"Space", kSpace}, {"CapsLock", kCapsLock}, {"F1", kF1}, {"F2", kF2}, {"F3", kF3},
                {"F4", kF4}, {"F5", kF5}, {"F6", kF6}, {"F7", kF7}, {"F8", kF8}, {"F9", kF9},
                {"F10", kF10}, {"NumLock", kNumLock}, {"ScrollLock", kScrollLock},
                {"Numpad7", kNumpad7}, {"Numpad8", kNumpad8}, {"Numpad9", kNumpad9},
                {"NumpadMinus", kNumpadMinus}, {"Numpad4", kNumpad4}, {"Numpad5", kNumpad5},
                {"Numpad6", kNumpad6}, {"NumpadPlus", kNumpadPlus}, {"Numpad1", kNumpad1},
                {"Numpad2", kNumpad2}, {"Numpad3", kNumpad3}, {"Numpad0", kNumpad0},
                {"NumpadPeriod", kNumpadPeriod}, {"F11", kF11}, {"F12", kF12},
                {"NumpadEnter", kNumpadEnter}, {"RightControl", kRightControl},
                {"NumpadDivide", kNumpadDivide}, {"RightAlt", kRightAlt}, {"Home", kHome},
                {"Up", kUp}, {"PageUp", kPageUp}, {"Left", kLeft}, {"Right", kRight}, {"End", kEnd},
                {"Down", kDown}, {"PageDown", kPageDown}, {"Insert", kInsert}, {"Delete", kDelete},
            };
            return map;
        }
    }

    [[nodiscard]] inline std::optional<std::uint32_t> KeyNameToCode(std::string_view a_name) {
        const auto& map = detail::NameToCodeMap();
        auto it = map.find(a_name);
        if (it == map.end()) return std::nullopt;
        return it->second;
    }

    [[nodiscard]] inline std::optional<std::string> CodeToKeyName(std::uint32_t a_code) {
        for (const auto& [name, code] : detail::NameToCodeMap()) {
            if (code == a_code) return std::string(name);
        }
        return std::nullopt;
    }
}
