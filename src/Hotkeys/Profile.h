#pragma once


#include "Hotkeys/BindKey.h"
#include "Hotkeys/HotkeyAction.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Hotkeys {
    struct Bind {
        BindKey key;
        std::vector<std::unique_ptr<IHotkeyAction>> actions;
        bool enabled = true;  // per-bind on/off switch, independent of the global mod enable
        bool blockVanillaKey = false;
    };

    struct Profile {
        std::string name;
        std::string updated;
        std::vector<Bind> binds;
        std::uint32_t profileCycleKeyCode = 0;
        bool profileCycleRequiresModifier = false;
    };

    [[nodiscard]] std::optional<Profile> LoadProfile(const std::filesystem::path& a_path);

    bool SaveProfile(const std::filesystem::path& a_path, const Profile& a_profile);

    [[nodiscard]] std::vector<std::string> ListProfiles(const std::filesystem::path& a_directory);

    [[nodiscard]] std::unordered_map<std::string, std::string> ParseFieldString(std::string_view a_str);
}
