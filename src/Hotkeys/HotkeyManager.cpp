#include "Hotkeys/HotkeyManager.h"

#include "Hotkeys/Actions.h"
#include "Hotkeys/ContinuousMovement.h"
#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/ToggleSprint.h"
#include "Hotkeys/VanillaControlSuppressor.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <fstream>

namespace Hotkeys {
    namespace {
        constexpr std::string_view kDataRelativeRoot = "Data/SKSE/Plugins/TrueHotkeys";

        void LoadSettingsFile(Settings& a_settings, const std::filesystem::path& a_path) {
            std::ifstream file(a_path);
            if (!file.is_open()) {
                return;
            }
            std::string line;
            while (std::getline(file, line)) {
                auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                auto key = line.substr(0, eq);
                auto value = line.substr(eq + 1);
                if (key == "Enabled") {
                    a_settings.enabled = (value == "1" || value == "true");
                } else if (key == "ModifierKey") {
                    if (auto code = DXScanCode::KeyNameToCode(value)) {
                        a_settings.modifierKeyCode = *code;
                    }
                } else if (key == "ModifierGamepadButton") {
                    if (auto code = GamepadButton::ButtonNameToCode(value)) {
                        a_settings.modifierGamepadCode = *code;
                    }
                } else if (key == "DefaultProfileCycleKey") {
                    if (auto code = DXScanCode::KeyNameToCode(value)) {
                        a_settings.defaultProfileCycleKeyCode = *code;
                    }
                } else if (key == "DefaultProfileCycleRequiresModifier") {
                    a_settings.defaultProfileCycleRequiresModifier = (value == "1" || value == "true");
                } else if (key == "HoldThresholdSeconds") {
                    a_settings.holdThresholdSeconds = std::strtof(value.c_str(), nullptr);
                } else if (key == "NotifyOnTrigger") {
                    a_settings.notifyOnTrigger = (value == "1" || value == "true");
                } else if (key == "ToggleUnequip") {
                    a_settings.toggleUnequip = (value == "1" || value == "true");
                } else if (key == "ActiveProfile") {
                    a_settings.activeProfileName = value;
                } else if (key == "KeyBindsKeyColumnWidth") {
                    a_settings.keyBindsKeyColumnWidth = std::strtof(value.c_str(), nullptr);
                } else if (key == "KeyBindsActionColumnWidth") {
                    a_settings.keyBindsActionColumnWidth = std::strtof(value.c_str(), nullptr);
                } else if (key == "KeyBindsBlockVanillaColumnWidth") {
                    a_settings.keyBindsBlockVanillaColumnWidth = std::strtof(value.c_str(), nullptr);
                } else if (key == "KeyBindsEnabledColumnWidth") {
                    a_settings.keyBindsEnabledColumnWidth = std::strtof(value.c_str(), nullptr);
                } else if (key == "KeyBindsSortColumn") {
                    a_settings.keyBindsSortColumn = std::atoi(value.c_str());
                } else if (key == "KeyBindsSortAscending") {
                    a_settings.keyBindsSortAscending = (value == "1" || value == "true");
                } else if (key == "AutoScrollToBottom") {
                    a_settings.autoScrollToBottom = (value == "1" || value == "true");
                } else if (key == "ConfirmSavesAndDeletes") {
                    a_settings.confirmSavesAndDeletes = (value == "1" || value == "true");
                } else if (key == "AutoSaveProfileChanges") {
                    a_settings.autoSaveProfileChanges = (value == "1" || value == "true");
                } else if (key == "AllowHotkeysInGameMenus") {
                    a_settings.allowHotkeysInGameMenus = (value == "1" || value == "true");
                } else if (key == "BlockHotkeysInInventoryMenu") {
                    a_settings.blockHotkeysInInventoryMenu = (value == "1" || value == "true");
                } else if (key == "BlockHotkeysInMagicMenu") {
                    a_settings.blockHotkeysInMagicMenu = (value == "1" || value == "true");
                } else if (key == "BlockHotkeysInMapMenu") {
                    a_settings.blockHotkeysInMapMenu = (value == "1" || value == "true");
                } else if (key == "BlockHotkeysInStatsMenu") {
                    a_settings.blockHotkeysInStatsMenu = (value == "1" || value == "true");
                } else if (key == "ModifierBlocksVanillaHotkey") {
                    a_settings.modifierBlocksVanillaHotkey = (value == "1" || value == "true");
                } else if (key == "Language") {
                    a_settings.language = value;
                }
            }
        }

        void SaveSettingsFile(const Settings& a_settings, const std::filesystem::path& a_path) {
            std::error_code ec;
            if (a_path.has_parent_path()) {
                std::filesystem::create_directories(a_path.parent_path(), ec);
            }
            std::ofstream file(a_path, std::ios::trunc);
            if (!file.is_open()) {
                return;
            }
            file << "Enabled=" << (a_settings.enabled ? "1" : "0") << "\n";
            auto modName = DXScanCode::CodeToKeyName(a_settings.modifierKeyCode);
            file << "ModifierKey=" << (modName ? *modName : "") << "\n";
            auto gpModName = GamepadButton::CodeToButtonName(a_settings.modifierGamepadCode);
            file << "ModifierGamepadButton=" << (gpModName ? *gpModName : "") << "\n";
            auto defaultCycleName = DXScanCode::CodeToKeyName(a_settings.defaultProfileCycleKeyCode);
            file << "DefaultProfileCycleKey=" << (defaultCycleName ? *defaultCycleName : "") << "\n";
            file << "DefaultProfileCycleRequiresModifier=" << (a_settings.defaultProfileCycleRequiresModifier ? "1" : "0") << "\n";
            file << "HoldThresholdSeconds=" << a_settings.holdThresholdSeconds << "\n";
            file << "NotifyOnTrigger=" << (a_settings.notifyOnTrigger ? "1" : "0") << "\n";
            file << "ToggleUnequip=" << (a_settings.toggleUnequip ? "1" : "0") << "\n";
            file << "ActiveProfile=" << a_settings.activeProfileName << "\n";
            file << "KeyBindsKeyColumnWidth=" << a_settings.keyBindsKeyColumnWidth << "\n";
            file << "KeyBindsActionColumnWidth=" << a_settings.keyBindsActionColumnWidth << "\n";
            file << "KeyBindsBlockVanillaColumnWidth=" << a_settings.keyBindsBlockVanillaColumnWidth << "\n";
            file << "KeyBindsEnabledColumnWidth=" << a_settings.keyBindsEnabledColumnWidth << "\n";
            file << "KeyBindsSortColumn=" << a_settings.keyBindsSortColumn << "\n";
            file << "KeyBindsSortAscending=" << (a_settings.keyBindsSortAscending ? "1" : "0") << "\n";
            file << "AutoScrollToBottom=" << (a_settings.autoScrollToBottom ? "1" : "0") << "\n";
            file << "ConfirmSavesAndDeletes=" << (a_settings.confirmSavesAndDeletes ? "1" : "0") << "\n";
            file << "AutoSaveProfileChanges=" << (a_settings.autoSaveProfileChanges ? "1" : "0") << "\n";
            file << "AllowHotkeysInGameMenus=" << (a_settings.allowHotkeysInGameMenus ? "1" : "0") << "\n";
            file << "BlockHotkeysInInventoryMenu=" << (a_settings.blockHotkeysInInventoryMenu ? "1" : "0") << "\n";
            file << "BlockHotkeysInMagicMenu=" << (a_settings.blockHotkeysInMagicMenu ? "1" : "0") << "\n";
            file << "BlockHotkeysInMapMenu=" << (a_settings.blockHotkeysInMapMenu ? "1" : "0") << "\n";
            file << "BlockHotkeysInStatsMenu=" << (a_settings.blockHotkeysInStatsMenu ? "1" : "0") << "\n";
            file << "ModifierBlocksVanillaHotkey=" << (a_settings.modifierBlocksVanillaHotkey ? "1" : "0") << "\n";
            file << "Language=" << a_settings.language << "\n";
        }
    }

    HotkeyManager* HotkeyManager::GetSingleton() {
        static HotkeyManager singleton;
        return &singleton;
    }

    std::filesystem::path HotkeyManager::ProfilesDirectory() const { return std::filesystem::path(kDataRelativeRoot) / "Profiles"; }

    std::filesystem::path HotkeyManager::SettingsPath() const { return std::filesystem::path(kDataRelativeRoot) / "Settings.ini"; }

    Profile HotkeyManager::MakeBlankProfile(std::string_view a_name) const {
        Profile profile;
        profile.name = std::string(a_name);
        profile.updated = "";
        profile.profileCycleKeyCode = m_settings.defaultProfileCycleKeyCode;
        profile.profileCycleRequiresModifier = m_settings.defaultProfileCycleRequiresModifier;
        return profile;
    }

    void HotkeyManager::Initialize() {
        LoadSettingsFile(m_settings, SettingsPath());

        auto names = Hotkeys::ListProfiles(ProfilesDirectory());
        bool loaded = false;

        if (!m_settings.activeProfileName.empty() &&
            std::find(names.begin(), names.end(), m_settings.activeProfileName) != names.end()) {
            loaded = LoadProfileByName(m_settings.activeProfileName);
        }

        if (!loaded) {
            for (const auto& name : names) {
                if (LoadProfileByName(name)) {
                    loaded = true;
                    break;
                }
            }
        }

        if (!loaded) {
            m_activeProfile = MakeBlankProfile("Default");
            SaveProfile(ProfilesDirectory() / "Default.ini", m_activeProfile);
            RebuildBindLookup();
        }

        SKSE::log::info(
            "True Hotkeys: initialized ({}). Active profile '{}' with {} bind(s).",
            m_settings.enabled ? "enabled" : "disabled", m_activeProfile.name, m_activeProfile.binds.size());
    }

    void HotkeyManager::SetSettings(const Settings& a_settings) {
        m_settings = a_settings;
        SaveSettingsFile(m_settings, SettingsPath());
        VanillaControlSuppressor::Sync();
    }

    void HotkeyManager::RebuildBindLookup() {
        m_bindLookup.clear();
        m_blockVanillaClaimedKeys.clear();
        for (const auto& bind : m_activeProfile.binds) {
            if (!bind.enabled) {
                continue;
            }
            m_bindLookup[bind.key] = &bind;
            if (!bind.key.modifierHeld && bind.blockVanillaKey) {
                m_blockVanillaClaimedKeys.insert(bind.key.idCode);
            }
        }
        m_activeToggleBinds.clear();
        ContinuousMovement::StopAll();
        ToggleSprint::Stop();
        VanillaControlSuppressor::Sync();

        if (m_settings.activeProfileName != m_activeProfile.name) {
            m_settings.activeProfileName = m_activeProfile.name;
            SaveSettingsFile(m_settings, SettingsPath());
        }
    }

    bool HotkeyManager::HasBind(const BindKey& a_key) const { return m_bindLookup.contains(a_key); }

    bool HotkeyManager::HasBlockingUnmodifiedBindForKey(std::uint32_t a_idCode) const {
        return m_blockVanillaClaimedKeys.contains(a_idCode);
    }

    bool HotkeyManager::ShouldBlockVanillaForKeyAndModifier(std::uint32_t a_idCode, bool a_modifierHeld) const {
        auto tapIt = m_bindLookup.find(BindKey{a_idCode, a_modifierHeld, PressType::kTap});
        auto holdIt = m_bindLookup.find(BindKey{a_idCode, a_modifierHeld, PressType::kHold});
        bool hasMatchingBind = tapIt != m_bindLookup.end() || holdIt != m_bindLookup.end();
        if (!hasMatchingBind) {
            return false;
        }
        if (!IsSafeToAct()) {
            return false;
        }
        if (a_modifierHeld) {
            return true;
        }
        if (tapIt != m_bindLookup.end() && tapIt->second->blockVanillaKey) {
            return true;
        }
        return holdIt != m_bindLookup.end() && holdIt->second->blockVanillaKey;
    }

    bool HotkeyManager::IsSafeToAct() const {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded() || player->IsDead()) {
            return false;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return true;
        }
        if (ui->GameIsPaused()) {
            return false;
        }

        static constexpr std::array kBlockingMenus = {
            RE::Console::MENU_NAME,      RE::DialogueMenu::MENU_NAME,  RE::LoadingMenu::MENU_NAME,
            RE::MainMenu::MENU_NAME,     RE::CraftingMenu::MENU_NAME,  RE::RaceSexMenu::MENU_NAME,
            RE::SleepWaitMenu::MENU_NAME,
        };
        for (auto menuName : kBlockingMenus) {
            if (ui->IsMenuOpen(menuName)) {
                return false;
            }
        }

        if (m_settings.blockHotkeysInInventoryMenu && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            return false;
        }
        if (m_settings.blockHotkeysInMagicMenu && ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
            return false;
        }
        if (m_settings.blockHotkeysInMapMenu && ui->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
            return false;
        }
        if (m_settings.blockHotkeysInStatsMenu && ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)) {
            return false;
        }

        if (!m_settings.allowHotkeysInGameMenus) {
            static constexpr std::array kPausingMenus = {
                RE::FavoritesMenu::MENU_NAME,
                RE::ContainerMenu::MENU_NAME,
                RE::BarterMenu::MENU_NAME,
                RE::JournalMenu::MENU_NAME,
                RE::BookMenu::MENU_NAME,
            };
            for (auto menuName : kPausingMenus) {
                if (ui->IsMenuOpen(menuName)) {
                    return false;
                }
            }
        }

        return true;
    }

    void HotkeyManager::Notify(std::string_view a_message) const { RE::DebugNotification(std::string(a_message).c_str()); }

    void HotkeyManager::TriggerBind(const BindKey& a_key) {
        auto it = m_bindLookup.find(a_key);
        if (it == m_bindLookup.end()) {
            return;
        }
        if (!IsSafeToAct()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        if (it->second->actions.empty()) {
            return;
        }

        std::vector<IHotkeyAction*> actions;
        actions.reserve(it->second->actions.size());
        for (const auto& action : it->second->actions) {
            actions.push_back(action.get());
        }

        for (auto it2 = m_activeToggleBinds.begin(); it2 != m_activeToggleBinds.end();) {
            if (*it2 == a_key) {
                ++it2;
            } else {
                it2 = m_activeToggleBinds.erase(it2);
            }
        }

        bool canToggle = m_settings.toggleUnequip &&
                         std::any_of(actions.begin(), actions.end(), [](IHotkeyAction* a_action) { return a_action->SupportsUndo(); });
        bool isToggleOff = canToggle && m_activeToggleBinds.contains(a_key);
        if (isToggleOff) {
            m_activeToggleBinds.erase(a_key);
        } else if (canToggle) {
            m_activeToggleBinds.insert(a_key);
        }

        std::string displayName;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) {
                displayName += "; ";
            }
            displayName += actions[i]->GetDisplayName();
        }
        std::string message = isToggleOff ? std::format("Unequipped: {}", displayName) : displayName;
        SKSE::log::info("True Hotkeys: triggered '{}'", message);
        if (m_settings.notifyOnTrigger) {
            Notify(message);
        }

        SKSE::GetTaskInterface()->AddTask([actions, isToggleOff]() {
            if (auto* actor = RE::PlayerCharacter::GetSingleton()) {
                for (auto* action : actions) {
                    if (isToggleOff) {
                        action->Undo(actor);
                    } else {
                        action->Execute(actor);
                    }
                }
            }
        });
    }

    bool HotkeyManager::IsMovementBind(std::uint32_t a_idCode, bool a_modifierHeld) const {
        for (auto press : {PressType::kTap, PressType::kHold}) {
            auto it = m_bindLookup.find(BindKey{a_idCode, a_modifierHeld, press});
            if (it == m_bindLookup.end()) {
                continue;
            }
            if (std::any_of(it->second->actions.begin(), it->second->actions.end(),
                             [](const std::unique_ptr<IHotkeyAction>& a_action) { return a_action->GetType() == ActionType::kMovement; })) {
                return true;
            }
        }
        return false;
    }

    void HotkeyManager::TriggerMovement(std::uint32_t a_idCode, bool a_modifierHeld, bool a_start) {
        if (a_start && !IsSafeToAct()) {
            return;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        for (auto press : {PressType::kTap, PressType::kHold}) {
            auto it = m_bindLookup.find(BindKey{a_idCode, a_modifierHeld, press});
            if (it == m_bindLookup.end()) {
                continue;
            }
            for (const auto& action : it->second->actions) {
                if (action->GetType() != ActionType::kMovement) {
                    continue;
                }
                if (a_start) {
                    action->Execute(player);
                } else {
                    action->Undo(player);
                }
            }
        }
    }

    std::vector<std::string> HotkeyManager::ListProfiles() const { return Hotkeys::ListProfiles(ProfilesDirectory()); }

    bool HotkeyManager::LoadProfileByName(std::string_view a_name) {
        auto path = ProfilesDirectory() / std::format("{}.ini", a_name);
        auto profile = Hotkeys::LoadProfile(path);
        if (!profile) {
            return false;
        }
        m_activeProfile = std::move(*profile);
        RebuildBindLookup();
        return true;
    }

    void HotkeyManager::SetProfileCycleKey(std::uint32_t a_idCode, bool a_requiresModifier) {
        m_activeProfile.profileCycleKeyCode = a_idCode;
        m_activeProfile.profileCycleRequiresModifier = a_requiresModifier;
    }

    void HotkeyManager::CycleProfile() {
        auto names = ListProfiles();
        if (names.empty()) {
            return;
        }

        auto it = std::find(names.begin(), names.end(), m_activeProfile.name);
        std::size_t nextIndex = 0;
        if (it != names.end()) {
            nextIndex = (static_cast<std::size_t>(std::distance(names.begin(), it)) + 1) % names.size();
        }

        if (LoadProfileByName(names[nextIndex]) && m_settings.notifyOnTrigger) {
            Notify(std::format("True Hotkeys: switched to profile '{}'", m_activeProfile.name));
        }
    }

    bool HotkeyManager::CreateProfile(std::string_view a_name) {
        auto path = ProfilesDirectory() / std::format("{}.ini", a_name);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            return false;
        }

        Profile profile = MakeBlankProfile(a_name);
        if (!SaveProfile(path, profile)) {
            return false;
        }

        m_activeProfile = std::move(profile);
        RebuildBindLookup();
        return true;
    }

    bool HotkeyManager::DeleteProfile(std::string_view a_name) {
        auto path = ProfilesDirectory() / std::format("{}.ini", a_name);
        std::error_code ec;
        if (!std::filesystem::remove(path, ec)) {
            return false;
        }

        if (m_activeProfile.name == a_name) {
            auto remaining = ListProfiles();
            if (!remaining.empty()) {
                LoadProfileByName(remaining.front());
            } else {
                m_activeProfile = MakeBlankProfile("Default");
                RebuildBindLookup();
            }
        }

        return true;
    }

    bool HotkeyManager::SaveActiveProfile() {
        auto path = ProfilesDirectory() / std::format("{}.ini", m_activeProfile.name);
        return SaveProfile(path, m_activeProfile);
    }

    bool HotkeyManager::SaveActiveProfileAs(std::string_view a_name) {
        Profile clone;
        clone.name = std::string(a_name);
        clone.updated = "";
        if (m_settings.defaultProfileCycleKeyCode != 0) {
            clone.profileCycleKeyCode = m_settings.defaultProfileCycleKeyCode;
            clone.profileCycleRequiresModifier = m_settings.defaultProfileCycleRequiresModifier;
        } else {
            clone.profileCycleKeyCode = m_activeProfile.profileCycleKeyCode;
            clone.profileCycleRequiresModifier = m_activeProfile.profileCycleRequiresModifier;
        }
        clone.binds.reserve(m_activeProfile.binds.size());
        for (const auto& bind : m_activeProfile.binds) {
            Bind clonedBind;
            clonedBind.key = bind.key;
            clonedBind.enabled = bind.enabled;
            clonedBind.blockVanillaKey = bind.blockVanillaKey;
            clonedBind.actions.reserve(bind.actions.size());
            for (const auto& action : bind.actions) {
                auto fields = ParseFieldString(action->Serialize());
                if (auto clonedAction = DeserializeAction(fields)) {
                    clonedBind.actions.push_back(std::move(clonedAction));
                }
            }
            clone.binds.push_back(std::move(clonedBind));
        }

        auto path = ProfilesDirectory() / std::format("{}.ini", a_name);
        if (!SaveProfile(path, clone)) {
            return false;
        }

        m_activeProfile = std::move(clone);
        RebuildBindLookup();
        return true;
    }

    std::vector<BindSummary> HotkeyManager::GetBindSummaries() const {
        std::vector<BindSummary> summaries;
        summaries.reserve(m_activeProfile.binds.size());
        for (const auto& bind : m_activeProfile.binds) {
            BindSummary summary{
                .key = bind.key,
                .actions = {},
                .enabled = bind.enabled,
                .blockVanillaKey = bind.blockVanillaKey,
            };
            summary.actions.reserve(bind.actions.size());
            for (const auto& action : bind.actions) {
                summary.actions.push_back(ActionSummary{
                    .type = action->GetType(),
                    .displayName = action->GetDisplayName(),
                    .serialized = action->Serialize(),
                });
            }
            summaries.push_back(std::move(summary));
        }
        return summaries;
    }

    void HotkeyManager::CreateBind(const BindKey& a_key) {
        for (const auto& bind : m_activeProfile.binds) {
            if (bind.key == a_key) {
                return;
            }
        }
        m_activeProfile.binds.push_back(Bind{.key = a_key, .actions = {}, .enabled = true});
        RebuildBindLookup();
    }

    void HotkeyManager::AddOrUpdateAction(const BindKey& a_key, std::unique_ptr<IHotkeyAction> a_action) {
        if (!a_action) {
            return;
        }
        ActionType type = a_action->GetType();
        for (auto& bind : m_activeProfile.binds) {
            if (bind.key == a_key) {
                for (auto& existing : bind.actions) {
                    if (existing->GetType() == type) {
                        existing = std::move(a_action);
                        RebuildBindLookup();
                        return;
                    }
                }
                bind.actions.push_back(std::move(a_action));
                RebuildBindLookup();
                return;
            }
        }
        Bind bind{.key = a_key, .actions = {}, .enabled = true};
        bind.actions.push_back(std::move(a_action));
        m_activeProfile.binds.push_back(std::move(bind));
        RebuildBindLookup();
    }

    void HotkeyManager::RemoveBind(const BindKey& a_key) {
        auto& binds = m_activeProfile.binds;
        std::erase_if(binds, [&a_key](const Bind& a_bind) { return a_bind.key == a_key; });
        RebuildBindLookup();
    }

    void HotkeyManager::ClearBindAction(const BindKey& a_key, ActionType a_type) {
        for (auto& bind : m_activeProfile.binds) {
            if (bind.key == a_key) {
                std::erase_if(bind.actions, [a_type](const std::unique_ptr<IHotkeyAction>& a_action) { return a_action->GetType() == a_type; });
                RebuildBindLookup();
                return;
            }
        }
    }

    void HotkeyManager::MoveAction(const BindKey& a_key, ActionType a_type, bool a_moveUp) {
        for (auto& bind : m_activeProfile.binds) {
            if (bind.key != a_key) {
                continue;
            }
            auto& actions = bind.actions;
            auto it = std::find_if(actions.begin(), actions.end(),
                                    [a_type](const std::unique_ptr<IHotkeyAction>& a_action) { return a_action->GetType() == a_type; });
            if (it == actions.end()) {
                return;
            }
            auto index = std::distance(actions.begin(), it);
            auto otherIndex = a_moveUp ? index - 1 : index + 1;
            if (otherIndex < 0 || static_cast<std::size_t>(otherIndex) >= actions.size()) {
                return;  // already at that end of the list
            }
            std::swap(actions[static_cast<std::size_t>(index)], actions[static_cast<std::size_t>(otherIndex)]);
            RebuildBindLookup();
            return;
        }
    }

    bool HotkeyManager::RekeyBind(const BindKey& a_oldKey, const BindKey& a_newKey) {
        if (a_oldKey == a_newKey) {
            return true;
        }
        auto& binds = m_activeProfile.binds;
        auto oldIt = std::find_if(binds.begin(), binds.end(), [&a_oldKey](const Bind& a_bind) { return a_bind.key == a_oldKey; });
        if (oldIt == binds.end()) {
            return false;
        }
        std::erase_if(binds, [&a_newKey](const Bind& a_bind) { return a_bind.key == a_newKey; });
        oldIt = std::find_if(binds.begin(), binds.end(), [&a_oldKey](const Bind& a_bind) { return a_bind.key == a_oldKey; });
        if (oldIt == binds.end()) {
            return false;
        }
        oldIt->key = a_newKey;
        RebuildBindLookup();
        return true;
    }

    void HotkeyManager::SetBindEnabled(const BindKey& a_key, bool a_enabled) {
        for (auto& bind : m_activeProfile.binds) {
            if (bind.key == a_key) {
                bind.enabled = a_enabled;
                RebuildBindLookup();
                return;
            }
        }
    }

    void HotkeyManager::SetBindBlockVanillaKey(const BindKey& a_key, bool a_blockVanillaKey) {
        for (auto& bind : m_activeProfile.binds) {
            if (bind.key == a_key) {
                bind.blockVanillaKey = a_blockVanillaKey;
                RebuildBindLookup();
                return;
            }
        }
    }
}
