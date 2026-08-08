#include "Hotkeys/Profile.h"

#include "Hotkeys/Actions.h"
#include "Hotkeys/DXScanCodes.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <string_view>
#include <unordered_map>

namespace Hotkeys {
    namespace {
        [[nodiscard]] std::string Trim(std::string_view a_str) {
            auto begin = a_str.find_first_not_of(" \t\r\n");
            if (begin == std::string_view::npos) {
                return "";
            }
            auto end = a_str.find_last_not_of(" \t\r\n");
            return std::string(a_str.substr(begin, end - begin + 1));
        }

        struct SplitBindLine {
            std::string flat;
            std::vector<std::string> actionBlocks;
        };

        [[nodiscard]] SplitBindLine SplitActionBlocks(std::string_view a_value) {
            SplitBindLine result;
            std::size_t pos = 0;
            while (pos < a_value.size()) {
                auto open = a_value.find("{{", pos);
                if (open == std::string_view::npos) {
                    result.flat.append(a_value.substr(pos));
                    break;
                }
                result.flat.append(a_value.substr(pos, open - pos));
                auto close = a_value.find("}}", open + 2);
                if (close == std::string_view::npos) {
                    break;
                }
                result.actionBlocks.push_back(std::string(a_value.substr(open + 2, close - open - 2)));
                pos = close + 2;
            }
            return result;
        }

        [[nodiscard]] std::optional<Bind> ParseBindLine(std::string_view a_value) {
            auto split = SplitActionBlocks(a_value);
            auto fields = ParseFieldString(split.flat);

            auto keyIt = fields.find("Key");
            auto modIt = fields.find("Mod");
            auto pressIt = fields.find("Press");
            if (keyIt == fields.end() || modIt == fields.end() || pressIt == fields.end()) {
                return std::nullopt;
            }

            auto code = DXScanCode::KeyNameToCode(keyIt->second);
            if (!code) {
                return std::nullopt;
            }

            Bind bind;
            bind.key.idCode = *code;
            bind.key.modifierHeld = (modIt->second == "1");
            bind.key.press = (pressIt->second == "Hold") ? PressType::kHold : PressType::kTap;

            if (!split.actionBlocks.empty()) {
                for (const auto& block : split.actionBlocks) {
                    auto actionFields = ParseFieldString(block);
                    if (auto action = DeserializeAction(actionFields)) {
                        bind.actions.push_back(std::move(action));
                    }
                }
            } else if (fields.contains("Type")) {
                if (auto action = DeserializeAction(fields)) {
                    bind.actions.push_back(std::move(action));
                } else {
                    return std::nullopt;
                }
            }

            auto enabledIt = fields.find("Enabled");
            bind.enabled = (enabledIt == fields.end()) || (enabledIt->second == "1");
            auto blockVanillaIt = fields.find("BlockVanilla");
            bind.blockVanillaKey = (blockVanillaIt != fields.end()) && (blockVanillaIt->second == "1");
            return bind;
        }

        [[nodiscard]] std::string SerializeBindLine(const Bind& a_bind) {
            auto keyName = DXScanCode::CodeToKeyName(a_bind.key.idCode);
            std::string key = keyName ? *keyName : std::format("0x{:02X}", a_bind.key.idCode);
            std::string mod = a_bind.key.modifierHeld ? "1" : "0";
            std::string press = (a_bind.key.press == PressType::kHold) ? "Hold" : "Tap";
            std::string enabled = a_bind.enabled ? "1" : "0";
            std::string blockVanilla = a_bind.blockVanillaKey ? "1" : "0";
            std::string line = std::format("Key:{}|Mod:{}|Press:{}|Enabled:{}|BlockVanilla:{}", key, mod, press, enabled, blockVanilla);
            for (const auto& action : a_bind.actions) {
                line += "|{{" + action->Serialize() + "}}";
            }
            return line;
        }
    }

    std::unordered_map<std::string, std::string> ParseFieldString(std::string_view a_str) {
        std::unordered_map<std::string, std::string> fields;
        std::size_t start = 0;
        while (start <= a_str.size()) {
            auto pipe = a_str.find('|', start);
            auto segment = (pipe == std::string_view::npos) ? a_str.substr(start) : a_str.substr(start, pipe - start);
            auto colon = segment.find(':');
            if (colon != std::string_view::npos) {
                auto key = Trim(segment.substr(0, colon));
                auto value = Trim(segment.substr(colon + 1));
                fields[key] = value;
            }
            if (pipe == std::string_view::npos) {
                break;
            }
            start = pipe + 1;
        }
        return fields;
    }

    std::optional<Profile> LoadProfile(const std::filesystem::path& a_path) {
        std::ifstream file(a_path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        Profile profile;
        profile.name = a_path.stem().string();

        std::string section;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(file, line)) {
            ++lineNumber;
            auto trimmed = Trim(line);
            if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#') {
                continue;
            }
            if (trimmed.front() == '[' && trimmed.back() == ']') {
                section = trimmed.substr(1, trimmed.size() - 2);
                continue;
            }

            auto eq = trimmed.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            auto value = Trim(std::string_view(trimmed).substr(eq + 1));

            if (section == "Meta") {
                auto key = Trim(std::string_view(trimmed).substr(0, eq));
                if (key == "Name") {
                    profile.name = value;
                } else if (key == "Updated") {
                    profile.updated = value;
                } else if (key == "ProfileCycleKey") {
                    if (auto code = DXScanCode::KeyNameToCode(value)) {
                        profile.profileCycleKeyCode = *code;
                    }
                } else if (key == "ProfileCycleRequiresModifier") {
                    profile.profileCycleRequiresModifier = (value == "1");
                }
            } else if (section == "Binds") {
                if (auto bind = ParseBindLine(value)) {
                    profile.binds.push_back(std::move(*bind));
                } else {
                    SKSE::log::warn("True Hotkeys: skipping malformed bind on line {} of profile '{}'", lineNumber, a_path.string());
                }
            }
        }

        return profile;
    }

    bool SaveProfile(const std::filesystem::path& a_path, const Profile& a_profile) {
        std::error_code ec;
        if (a_path.has_parent_path()) {
            std::filesystem::create_directories(a_path.parent_path(), ec);
        }

        std::ofstream file(a_path, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }

        file << "[Meta]\n";
        file << "Name=" << a_profile.name << "\n";
        file << "Updated=" << a_profile.updated << "\n";
        auto cycleName = DXScanCode::CodeToKeyName(a_profile.profileCycleKeyCode);
        file << "ProfileCycleKey=" << (cycleName ? *cycleName : "") << "\n";
        file << "ProfileCycleRequiresModifier=" << (a_profile.profileCycleRequiresModifier ? "1" : "0") << "\n";
        file << "\n[Binds]\n";
        for (std::size_t i = 0; i < a_profile.binds.size(); ++i) {
            file << "Bind" << (i + 1) << "=" << SerializeBindLine(a_profile.binds[i]) << "\n";
        }

        return file.good();
    }

    std::vector<std::string> ListProfiles(const std::filesystem::path& a_directory) {
        std::vector<std::string> names;
        std::error_code ec;
        if (!std::filesystem::exists(a_directory, ec)) {
            return names;
        }
        for (const auto& entry : std::filesystem::directory_iterator(a_directory, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                names.push_back(entry.path().stem().string());
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }
}
