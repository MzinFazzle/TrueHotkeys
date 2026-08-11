#pragma once


#include "Hotkeys/BindKey.h"
#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/GamepadButtons.h"
#include "Hotkeys/HotkeyAction.h"
#include "Hotkeys/Profile.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Hotkeys {
    struct Settings {
        bool enabled = true;  // master on/off switch for the whole mod
        std::uint32_t modifierKeyCode = DXScanCode::kLeftShift;
        std::uint32_t modifierGamepadCode = GamepadButton::kRightShoulder;
        std::uint32_t defaultProfileCycleKeyCode = 0;
        bool defaultProfileCycleRequiresModifier = false;
        float holdThresholdSeconds = 0.5f;  // global, not per-bind (see DESIGN.md)
        bool notifyOnTrigger = false;
        bool toggleUnequip = false;
        std::string activeProfileName = "Default";

        float keyBindsKeyColumnWidth = 211.0f;
        float keyBindsActionColumnWidth = 345.0f;
        float keyBindsBlockVanillaColumnWidth = 165.0f;
        float keyBindsEnabledColumnWidth = 81.0f;

        int keyBindsSortColumn = -1;
        bool keyBindsSortAscending = true;

        bool autoScrollToBottom = true;
        bool confirmSavesAndDeletes = true;
        bool autoSaveProfileChanges = true;
        bool allowHotkeysInGameMenus = false;

        bool blockHotkeysInInventoryMenu = true;
        bool blockHotkeysInMagicMenu = true;
        bool blockHotkeysInMapMenu = true;
        bool blockHotkeysInStatsMenu = false;

    };

    struct ActionSummary {
        ActionType type;
        std::string displayName;
        std::string serialized;  // action->Serialize() - lets the UI pre-fill an edit from the existing action
    };

    struct BindSummary {
        BindKey key;
        std::vector<ActionSummary> actions;
        bool enabled;
        bool blockVanillaKey;
    };

    class HotkeyManager {
    public:
        static HotkeyManager* GetSingleton();

        void Initialize();

        [[nodiscard]] const Settings& GetSettings() const noexcept { return m_settings; }
        void SetSettings(const Settings& a_settings);

        [[nodiscard]] bool HasBind(const BindKey& a_key) const;

        [[nodiscard]] bool HasBlockingUnmodifiedBindForKey(std::uint32_t a_idCode) const;

        [[nodiscard]] bool ShouldBlockVanillaForKeyAndModifier(std::uint32_t a_idCode, bool a_modifierHeld) const;

        void TriggerBind(const BindKey& a_key);

        [[nodiscard]] bool IsMovementBind(std::uint32_t a_idCode, bool a_modifierHeld) const;

        void TriggerMovement(std::uint32_t a_idCode, bool a_modifierHeld, bool a_start);

        [[nodiscard]] bool IsSafeToAct() const;

        [[nodiscard]] const std::string& GetActiveProfileName() const noexcept { return m_activeProfile.name; }
        [[nodiscard]] std::vector<std::string> ListProfiles() const;
        bool LoadProfileByName(std::string_view a_name);
        void CycleProfile();

        [[nodiscard]] std::uint32_t GetProfileCycleKeyCode() const noexcept { return m_activeProfile.profileCycleKeyCode; }
        [[nodiscard]] bool GetProfileCycleRequiresModifier() const noexcept { return m_activeProfile.profileCycleRequiresModifier; }

        void SetProfileCycleKey(std::uint32_t a_idCode, bool a_requiresModifier);

        bool CreateProfile(std::string_view a_name);

        bool DeleteProfile(std::string_view a_name);

        bool SaveActiveProfile();

        bool SaveActiveProfileAs(std::string_view a_name);

        [[nodiscard]] std::vector<BindSummary> GetBindSummaries() const;

        void CreateBind(const BindKey& a_key);

        void AddOrUpdateAction(const BindKey& a_key, std::unique_ptr<IHotkeyAction> a_action);

        void RemoveBind(const BindKey& a_key);

        void ClearBindAction(const BindKey& a_key, ActionType a_type);

        void MoveAction(const BindKey& a_key, ActionType a_type, bool a_moveUp);

        bool RekeyBind(const BindKey& a_oldKey, const BindKey& a_newKey);

        void SetBindEnabled(const BindKey& a_key, bool a_enabled);

        void SetBindBlockVanillaKey(const BindKey& a_key, bool a_blockVanillaKey);

    private:
        HotkeyManager() = default;
        HotkeyManager(const HotkeyManager&) = delete;
        HotkeyManager(HotkeyManager&&) = delete;

        [[nodiscard]] std::filesystem::path ProfilesDirectory() const;
        [[nodiscard]] std::filesystem::path SettingsPath() const;
        void RebuildBindLookup();
        void Notify(std::string_view a_message) const;

        [[nodiscard]] Profile MakeBlankProfile(std::string_view a_name) const;

        Settings m_settings;
        Profile m_activeProfile;
        std::unordered_map<BindKey, const Bind*> m_bindLookup;
        std::unordered_set<std::uint32_t> m_blockVanillaClaimedKeys;

        std::unordered_set<BindKey> m_activeToggleBinds;
    };
}
