#include "Hotkeys/ConfigUI.h"

#include "Hotkeys/Actions.h"
#include "Hotkeys/DXScanCodes.h"
#include "Hotkeys/FormBrowser.h"
#include "Hotkeys/GamepadButtons.h"
#include "Hotkeys/HotkeyManager.h"
#include "Hotkeys/InputHandler.h"
#include "Hotkeys/Locale.h"
#include "Hotkeys/Profile.h"

#include "SKSEMenuFramework.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Hotkeys {
    namespace {
        using namespace ImGuiMCP;

        [[nodiscard]] const char* TR(std::string_view a_key) { return Locale::GetSingleton()->T(a_key); }

        [[nodiscard]] std::string TRID(std::string_view a_key, std::string_view a_idSuffix) {
            return std::format("{}{}", TR(a_key), a_idSuffix);
        }

        enum class CaptureTarget { kNone, kModifierKey, kGamepadModifier, kProfileCycleKey, kBindKey };
        CaptureTarget s_captureTarget = CaptureTarget::kNone;

        bool s_menuOpen = false;

        std::string s_lastSelectedPlugin;

        std::string s_lastSelectedForm;

        constexpr const char* kActionTypeNames[] = {
            "Weapon",       "Ammo",      "Spell",          "Shout",         "Outfit",     "Consumable", "Panic",
            "Movement",     "ReadySheath", "Jump",          "ToggleSneak",   "ToggleSprint", "ToggleAutoMove",
            "ToggleTorch",  "TogglePOV", "ToggleFreeCam",  "ToggleFreeCamPaused", "ToggleMenus", "OpenMenu",
            "QuickSave",    "QuickLoad", "RechargeWeapon", "RechargeWeaponLeftHand"};
        constexpr const char* kActionTypeDisplayKeys[] = {
            "actiontype.weapon",       "actiontype.ammo",      "actiontype.spell",       "actiontype.shout",
            "actiontype.outfit",       "actiontype.consumable", "actiontype.unequip",     "actiontype.movement",
            "actiontype.ready_sheath", "actiontype.jump",      "actiontype.toggle_sneak",
            "actiontype.toggle_sprint", "actiontype.toggle_auto_move", "actiontype.toggle_torch", "actiontype.toggle_pov",
            "actiontype.toggle_freecam", "actiontype.toggle_freecam_paused", "actiontype.toggle_menus", "actiontype.open_menu",
            "actiontype.quick_save", "actiontype.quick_load", "actiontype.recharge_weapon", "actiontype.recharge_weapon_left_hand"};
        constexpr int kActionTypeCount = 23;

        [[nodiscard]] const char* ActionTypeDisplayName(ActionType a_type) { return TR(kActionTypeDisplayKeys[static_cast<int>(a_type)]); }

        constexpr int kOutfitModeCount = 2;

        constexpr const char* kMovementDirectionNames[] = {"Forward", "Backward", "StrafeLeft", "StrafeRight"};
        constexpr int kMovementDirectionCount = 4;
        constexpr const char* kOpenMenuTargetNames[] = {"Inventory", "Spells", "Map", "Skills", "Favorites", "Rest"};
        constexpr int kOpenMenuTargetCount = 6;


        [[nodiscard]] std::string KeyName(std::uint32_t a_idCode) {
            if (a_idCode == 0) {
                return "Not set";
            }
            if (auto name = DXScanCode::CodeToKeyName(a_idCode)) {
                return *name;
            }
            if (auto name = GamepadButton::DisplayName(a_idCode)) {
                return *name;
            }
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%02X", a_idCode);
            return buf;
        }

        [[nodiscard]] std::string KeyWithModifierLabel(std::uint32_t a_idCode, bool a_requiresModifier, std::uint32_t a_modifierKeyCode,
                                                        std::uint32_t a_modifierGamepadCode) {
            std::uint32_t effectiveModifierCode = GamepadButton::IsGamepadCode(a_idCode) ? a_modifierGamepadCode : a_modifierKeyCode;
            return (a_requiresModifier && effectiveModifierCode != 0) ? std::format("{} + {}", KeyName(effectiveModifierCode), KeyName(a_idCode))
                                                                       : KeyName(a_idCode);
        }

        [[nodiscard]] std::string BindKeyLabel(const BindKey& a_key, std::uint32_t a_modifierKeyCode, std::uint32_t a_modifierGamepadCode) {
            std::string label = KeyWithModifierLabel(a_key.idCode, a_key.modifierHeld, a_modifierKeyCode, a_modifierGamepadCode);
            label += (a_key.press == PressType::kHold) ? TR("keybinds.press_suffix_hold") : TR("keybinds.press_suffix_tap");
            return label;
        }

        [[nodiscard]] std::vector<std::string> SplitComma(std::string_view a_str) {
            std::vector<std::string> parts;
            std::size_t start = 0;
            while (start <= a_str.size()) {
                auto comma = a_str.find(',', start);
                if (comma == std::string_view::npos) {
                    parts.emplace_back(a_str.substr(start));
                    break;
                }
                parts.emplace_back(a_str.substr(start, comma - start));
                start = comma + 1;
            }
            return parts;
        }

        enum class CaptureResult { kPending, kCaptured, kCancelled };

        CaptureResult RenderCaptureButton(CaptureTarget a_target, std::uint32_t& a_outCode, bool* a_outRequiresModifier = nullptr) {
            auto* input = InputHandler::GetSingleton();

            if (s_captureTarget == a_target) {
                std::uint32_t captured = 0;
                bool requiresModifier = false;
                if (input->TryConsumeCapturedKey(captured, requiresModifier)) {
                    a_outCode = captured;
                    if (a_outRequiresModifier) {
                        *a_outRequiresModifier = requiresModifier;
                    }
                    s_captureTarget = CaptureTarget::kNone;
                    return CaptureResult::kCaptured;
                }
                Text("%s", input->IsCaptureModifierHeld() ? TR("capture.modifier_plus_ellipsis") : TR("capture.press_a_key"));
                SameLine();
                if (Button(TR("common.cancel"))) {
                    input->CancelCapture();
                    s_captureTarget = CaptureTarget::kNone;
                    return CaptureResult::kCancelled;
                }
            } else {
                if (Button(TR("capture.bind"))) {
                    bool combinesWithModifier = a_target != CaptureTarget::kModifierKey && a_target != CaptureTarget::kGamepadModifier;
                    input->BeginCapture(combinesWithModifier);
                    s_captureTarget = a_target;
                }
            }
            return CaptureResult::kPending;
        }

        bool __stdcall OnFrameworkInputEvent(RE::InputEvent* a_event) {
            if (!a_event || a_event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                return false;
            }
            auto* button = a_event->AsButtonEvent();
            if (!button || !button->IsDown()) {
                return false;
            }
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                InputHandler::GetSingleton()->ReportExternalCapture(button->GetIDCode());
                return false;
            }
            if (auto code = GamepadButton::ToUnifiedCode(*button)) {
                const bool wasCapturing = InputHandler::GetSingleton()->IsCapturing();
                InputHandler::GetSingleton()->ReportExternalCapture(*code);
                return wasCapturing;
            }
            return false;  // don't consume - just observing for capture mode
        }


        struct TypeaheadState {
            int index = -1;  // highlighted (not yet chosen) item, or -1
            char ch = '\0';  // which typed character produced that highlight
        };

        enum class FormSortKind { kGeneric, kSpell, kWeapon, kArmor };

        struct FieldPicker {
            int pluginIndex = -1;
            int formIndex = -1;
            FormBrowser::FormCatalog catalog;
            bool catalogLoaded = false;  // catalog is fetched once, lazily, not every frame

            std::vector<FormBrowser::FormChoice> flatChoices;
            bool flatMode = false;

            int formSortMode = 0;

            bool formSortModeInitialized = false;

            char formFilter[64] = "";

            char pluginFilter[64] = "";
            int pluginSortMode = 0;       // 0=A->Z, 1=Z->A, 2=Priority
            int pluginTypeaheadIndex = -1;  // index into catalog.plugins, highlighted but not chosen
            char pluginTypeaheadChar = '\0';

            TypeaheadState formTypeahead;
            TypeaheadState formSortTypeahead;
            TypeaheadState pluginSortTypeahead;
        };

        using CatalogGetter = FormBrowser::FormCatalog (*)();

        void EnsureCatalog(FieldPicker& a_picker, CatalogGetter a_catalogGetter) {
            if (!a_picker.catalogLoaded) {
                a_picker.catalog = a_catalogGetter();
                a_picker.catalogLoaded = true;

                a_picker.flatChoices.clear();
                for (const auto& plugin : a_picker.catalog.plugins) {
                    auto it = a_picker.catalog.byPlugin.find(plugin);
                    if (it != a_picker.catalog.byPlugin.end()) {
                        a_picker.flatChoices.insert(a_picker.flatChoices.end(), it->second.begin(), it->second.end());
                    }
                }
                std::sort(a_picker.flatChoices.begin(), a_picker.flatChoices.end(), [](const FormBrowser::FormChoice& a_lhs, const FormBrowser::FormChoice& a_rhs) {
                    return a_lhs.displayName < a_rhs.displayName;
                });
            }
        }

        [[nodiscard]] const std::vector<FormBrowser::FormChoice>* CurrentChoices(const FieldPicker& a_picker) {
            if (a_picker.pluginIndex < 0 || a_picker.pluginIndex >= static_cast<int>(a_picker.catalog.plugins.size())) {
                return nullptr;
            }
            auto it = a_picker.catalog.byPlugin.find(a_picker.catalog.plugins[a_picker.pluginIndex]);
            return (it != a_picker.catalog.byPlugin.end()) ? &it->second : nullptr;
        }

        void PreFillPicker(FieldPicker& a_picker, CatalogGetter a_catalogGetter, const std::string& a_formRefStr, bool a_flat = false) {
            auto parsed = FormRef::Parse(a_formRefStr);
            if (!parsed) {
                return;
            }
            EnsureCatalog(a_picker, a_catalogGetter);

            if (a_flat) {
                auto formIt = std::find_if(a_picker.flatChoices.begin(), a_picker.flatChoices.end(), [&](const FormBrowser::FormChoice& a_choice) {
                    return a_choice.ref.plugin == parsed->plugin && a_choice.ref.localFormID == parsed->localFormID;
                });
                a_picker.formIndex =
                    (formIt != a_picker.flatChoices.end()) ? static_cast<int>(std::distance(a_picker.flatChoices.begin(), formIt)) : -1;
                return;
            }

            const auto& plugins = a_picker.catalog.plugins;
            auto pluginIt = std::find(plugins.begin(), plugins.end(), parsed->plugin);
            if (pluginIt == plugins.end()) {
                return;  // this plugin has none of this action type's forms (or isn't loaded) - leave unset, user re-picks
            }
            a_picker.pluginIndex = static_cast<int>(std::distance(plugins.begin(), pluginIt));

            auto formsIt = a_picker.catalog.byPlugin.find(parsed->plugin);
            if (formsIt == a_picker.catalog.byPlugin.end()) {
                return;
            }
            auto formIt = std::find_if(formsIt->second.begin(), formsIt->second.end(), [&](const FormBrowser::FormChoice& a_choice) {
                return a_choice.ref.plugin == parsed->plugin && a_choice.ref.localFormID == parsed->localFormID;
            });
            a_picker.formIndex = (formIt != formsIt->second.end()) ? static_cast<int>(std::distance(formsIt->second.begin(), formIt)) : -1;
        }

        constexpr const char* kPluginSortNames[] = {"A -> Z", "Z -> A", "Priority"};
        constexpr int kPluginSortCount = 3;

        [[nodiscard]] char ToLowerChar(char a_ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_ch))); }

        [[nodiscard]] std::string ToLowerCopy(std::string_view a_str) {
            std::string out(a_str);
            std::transform(out.begin(), out.end(), out.begin(), ToLowerChar);
            return out;
        }

        void HelpMarker(const char* a_desc) {
            TextDisabled("(?)");
            if (IsItemHovered()) {
                SetTooltip("%s", a_desc);
            }
        }

        void DescText(const char* a_text) {
            PushTextWrapPos(0.0f);
            TextColored(ImVec4{0.7f, 0.7f, 0.7f, 1.0f}, "%s", a_text);
            PopTextWrapPos();
        }

        bool RenderTypeaheadCombo(const char* a_comboLabel, int& a_currentIndex, const char* const* a_items, int a_itemCount,
                                   TypeaheadState& a_typeahead, const char* a_emptyMessage = nullptr, int a_scrollToOnOpen = -1) {
            std::string preview = (a_currentIndex >= 0 && a_currentIndex < a_itemCount) ? a_items[a_currentIndex] : "(none)";
            bool changed = false;

            bool opened = BeginCombo(a_comboLabel, preview.c_str());
            bool scrollToRememberedOnOpen = false;
            if (opened) {
                if (IsWindowAppearing()) {
                    a_typeahead.index = -1;
                    a_typeahead.ch = '\0';
                    scrollToRememberedOnOpen = (a_scrollToOnOpen >= 0 && a_scrollToOnOpen < a_itemCount);
                }

                bool scrollToTypeahead = false;
                char typed = '\0';
                for (int digit = 0; digit < 10 && typed == '\0'; ++digit) {
                    if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + digit), false)) {
                        typed = static_cast<char>('0' + digit);
                    }
                }
                for (int letter = 0; letter < 26 && typed == '\0'; ++letter) {
                    if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + letter), false)) {
                        typed = static_cast<char>('a' + letter);
                    }
                }

                if (typed != '\0' && a_itemCount > 0) {
                    int startPos = 0;
                    if (a_typeahead.ch == typed && a_typeahead.index >= 0 && a_typeahead.index < a_itemCount) {
                        startPos = a_typeahead.index + 1;
                    }
                    int found = -1;
                    for (int pass = 0; pass < 2 && found < 0; ++pass) {
                        int from = (pass == 0) ? startPos : 0;
                        int to = (pass == 0) ? a_itemCount : startPos;
                        for (int pos = from; pos < to; ++pos) {
                            const char* name = a_items[pos];
                            if (name && name[0] != '\0' && ToLowerChar(name[0]) == typed) {
                                found = pos;
                                break;
                            }
                        }
                    }
                    if (found >= 0) {
                        a_typeahead.index = found;
                        a_typeahead.ch = typed;
                        scrollToTypeahead = true;
                    }
                }

                if (a_typeahead.index >= 0 && (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
                    a_currentIndex = a_typeahead.index;
                    changed = true;
                    CloseCurrentPopup();
                }

                for (int i = 0; i < a_itemCount; ++i) {
                    bool isCurrent = (i == a_currentIndex);
                    bool isTypeaheadTarget = (i == a_typeahead.index);

                    if (isTypeaheadTarget) {
                        PushStyleColor(ImGuiCol_Header, ImVec4{0.85f, 0.65f, 0.15f, 0.65f});
                        PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{0.85f, 0.65f, 0.15f, 0.85f});
                    }

                    PushID(i);
                    if (Selectable(a_items[i], isCurrent || isTypeaheadTarget)) {
                        a_currentIndex = i;
                        changed = true;
                        CloseCurrentPopup();
                    }
                    PopID();

                    if (isTypeaheadTarget) {
                        PopStyleColor(2);
                        if (scrollToTypeahead) {
                            SetScrollHereY();
                        }
                    } else if (scrollToRememberedOnOpen && i == a_scrollToOnOpen) {
                        SetScrollHereY();
                    }
                }

                if (a_itemCount == 0 && a_emptyMessage) {
                    DescText(a_emptyMessage);
                }

                EndCombo();
            }

            return changed;
        }

        bool RenderPluginCombo(const char* a_comboLabel, FieldPicker& a_picker) {
            const auto& plugins = a_picker.catalog.plugins;
            int& index = a_picker.pluginIndex;
            std::string preview = (index >= 0 && index < static_cast<int>(plugins.size())) ? plugins[index] : "(none)";
            bool changed = false;

            bool opened = BeginCombo(a_comboLabel, preview.c_str());

            bool scrollToLastSelectedOnOpen = false;

            if (opened) {
                if (IsWindowAppearing()) {
                    a_picker.pluginTypeaheadIndex = -1;
                    a_picker.pluginTypeaheadChar = '\0';
                    scrollToLastSelectedOnOpen = !s_lastSelectedPlugin.empty();
                }

                Text("%s", TR("picker.filter"));
                SameLine();
                SetNextItemWidth(140.0f);
                InputText("##PluginFilter", a_picker.pluginFilter, sizeof(a_picker.pluginFilter));
                bool filterFocused = IsItemActive();

                SameLine();
                Text("%s", TR("picker.sort"));
                SameLine();
                SetNextItemWidth(110.0f);
                RenderTypeaheadCombo("##PluginSort", a_picker.pluginSortMode, kPluginSortNames, kPluginSortCount, a_picker.pluginSortTypeahead);

                Separator();

                std::vector<int> shown;
                shown.reserve(plugins.size());
                std::string needle = ToLowerCopy(a_picker.pluginFilter);
                for (int i = 0; i < static_cast<int>(plugins.size()); ++i) {
                    if (needle.empty() || ToLowerCopy(plugins[i]).find(needle) != std::string::npos) {
                        shown.push_back(i);
                    }
                }
                if (a_picker.pluginSortMode == 0) {
                    std::sort(shown.begin(), shown.end(), [&](int a_lhs, int a_rhs) { return plugins[a_lhs] < plugins[a_rhs]; });
                } else if (a_picker.pluginSortMode == 1) {
                    std::sort(shown.begin(), shown.end(), [&](int a_lhs, int a_rhs) { return plugins[a_lhs] > plugins[a_rhs]; });
                } else if (a_picker.pluginSortMode == 2) {
                    std::sort(shown.begin(), shown.end(), [&](int a_lhs, int a_rhs) {
                        auto lhsIdx = FormBrowser::GetPluginLoadOrderIndex(plugins[a_lhs]).value_or(0xFFFFFFFFu);
                        auto rhsIdx = FormBrowser::GetPluginLoadOrderIndex(plugins[a_rhs]).value_or(0xFFFFFFFFu);
                        return lhsIdx < rhsIdx;
                    });
                }

                bool scrollToTypeahead = false;

                if (!filterFocused) {
                    char typed = '\0';
                    for (int digit = 0; digit < 10 && typed == '\0'; ++digit) {
                        if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + digit), false)) {
                            typed = static_cast<char>('0' + digit);
                        }
                    }
                    for (int letter = 0; letter < 26 && typed == '\0'; ++letter) {
                        if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + letter), false)) {
                            typed = static_cast<char>('a' + letter);
                        }
                    }

                    if (typed != '\0') {
                        int startPos = 0;
                        if (a_picker.pluginTypeaheadChar == typed && a_picker.pluginTypeaheadIndex >= 0) {
                            auto it = std::find(shown.begin(), shown.end(), a_picker.pluginTypeaheadIndex);
                            if (it != shown.end()) {
                                startPos = static_cast<int>(std::distance(shown.begin(), it)) + 1;
                            }
                        }

                        int found = -1;
                        for (int pass = 0; pass < 2 && found < 0; ++pass) {
                            int from = (pass == 0) ? startPos : 0;
                            int to = (pass == 0) ? static_cast<int>(shown.size()) : startPos;
                            for (int pos = from; pos < to; ++pos) {
                                const auto& name = plugins[shown[pos]];
                                if (!name.empty() && ToLowerChar(name[0]) == typed) {
                                    found = shown[pos];
                                    break;
                                }
                            }
                        }

                        if (found >= 0) {
                            a_picker.pluginTypeaheadIndex = found;
                            a_picker.pluginTypeaheadChar = typed;
                            scrollToTypeahead = true;
                        }
                    }

                    if (a_picker.pluginTypeaheadIndex >= 0 && (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
                        index = a_picker.pluginTypeaheadIndex;
                        changed = true;
                        CloseCurrentPopup();
                    }
                }

                for (int shownPlugin : shown) {
                    bool isCurrent = (shownPlugin == index);
                    bool isTypeaheadTarget = (shownPlugin == a_picker.pluginTypeaheadIndex);

                    if (isTypeaheadTarget) {
                        PushStyleColor(ImGuiCol_Header, ImVec4{0.85f, 0.65f, 0.15f, 0.65f});
                        PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{0.85f, 0.65f, 0.15f, 0.85f});
                    }

                    PushID(shownPlugin);
                    if (Selectable(plugins[shownPlugin].c_str(), isCurrent || isTypeaheadTarget)) {
                        index = shownPlugin;
                        changed = true;
                        CloseCurrentPopup();
                    }
                    PopID();

                    if (isTypeaheadTarget) {
                        PopStyleColor(2);
                        if (scrollToTypeahead) {
                            SetScrollHereY();
                        }
                    } else if (scrollToLastSelectedOnOpen && plugins[shownPlugin] == s_lastSelectedPlugin) {
                        SetScrollHereY();
                    }
                }

                if (shown.empty()) {
                    DescText(plugins.empty() ? TR("picker.no_plugins_have_forms") : TR("picker.no_plugins_match_filter"));
                }

                EndCombo();
            }

            if (changed) {
                a_picker.formIndex = -1;
                if (index >= 0 && index < static_cast<int>(plugins.size())) {
                    s_lastSelectedPlugin = plugins[index];
                }
            }
            return changed;
        }


        struct SpellGroupInfo {
            int order;
            const char* label;
        };

        [[nodiscard]] SpellGroupInfo SpellTypeGroup(RE::SpellItem* a_spell) {
            if (!a_spell) {
                return {5, "Other"};
            }
            switch (a_spell->GetSpellType()) {
                case RE::MagicSystem::SpellType::kSpell:
                    return {0, "Spell"};
                case RE::MagicSystem::SpellType::kPower:
                    return {1, "Power"};
                case RE::MagicSystem::SpellType::kLesserPower:
                    return {2, "Lesser Power"};
                case RE::MagicSystem::SpellType::kVoicePower:
                    return {3, "Voice Power"};
                case RE::MagicSystem::SpellType::kAbility:
                    return {4, "Ability"};
                default:
                    return {5, "Other"};
            }
        }

        [[nodiscard]] SpellGroupInfo SpellSchoolGroup(RE::SpellItem* a_spell) {
            if (!a_spell) {
                return {5, "Other"};
            }
            switch (a_spell->GetAssociatedSkill()) {
                case RE::ActorValue::kAlteration:
                    return {0, "Alteration"};
                case RE::ActorValue::kConjuration:
                    return {1, "Conjuration"};
                case RE::ActorValue::kDestruction:
                    return {2, "Destruction"};
                case RE::ActorValue::kIllusion:
                    return {3, "Illusion"};
                case RE::ActorValue::kRestoration:
                    return {4, "Restoration"};
                default:
                    return {5, "Other"};
            }
        }

        constexpr const char* kSpellSortNames[] = {"A -> Z", "Z -> A", "Type", "School"};
        constexpr int kSpellSortCount = 4;


        [[nodiscard]] const char* WeaponTypeGroup(RE::TESObjectWEAP* a_weapon) {
            if (!a_weapon) {
                return "Other";
            }
            switch (a_weapon->GetWeaponType()) {
                case RE::WEAPON_TYPE::kOneHandSword:
                case RE::WEAPON_TYPE::kOneHandAxe:
                case RE::WEAPON_TYPE::kOneHandMace:
                    return "One-Handed";
                case RE::WEAPON_TYPE::kOneHandDagger:
                    return "Dagger";
                case RE::WEAPON_TYPE::kTwoHandSword:
                case RE::WEAPON_TYPE::kTwoHandAxe:
                    return "Two-Handed";
                case RE::WEAPON_TYPE::kBow:
                    return "Bow";
                case RE::WEAPON_TYPE::kCrossbow:
                    return "Crossbow";
                case RE::WEAPON_TYPE::kStaff:
                    return "Staff";
                case RE::WEAPON_TYPE::kHandToHandMelee:
                    return "Hand to Hand";
                default:
                    return "Other";
            }
        }

        [[nodiscard]] const char* ArmorClassGroup(RE::TESObjectARMO* a_armor) {
            if (!a_armor) {
                return "Other";
            }
            if (a_armor->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet) ||
                a_armor->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kRing)) {
                return "Jewelry";
            }
            switch (a_armor->GetArmorType()) {
                case RE::BGSBipedObjectForm::ArmorType::kLightArmor:
                    return "Light";
                case RE::BGSBipedObjectForm::ArmorType::kHeavyArmor:
                    return "Heavy";
                case RE::BGSBipedObjectForm::ArmorType::kClothing:
                    return "Clothing";
                default:
                    return "Other";
            }
        }

        constexpr const char* kWeaponSortNames[] = {"A -> Z", "Z -> A", "Type"};
        constexpr int kWeaponSortCount = 3;
        constexpr const char* kArmorSortNames[] = {"A -> Z", "Z -> A", "Class"};
        constexpr int kArmorSortCount = 3;

        struct SortedFormChoice {
            std::size_t originalIndex;
            std::string label;
        };

        [[nodiscard]] std::vector<SortedFormChoice> BuildSortedFormChoices(const std::vector<FormBrowser::FormChoice>& a_choices, int a_sortMode,
                                                                             FormSortKind a_kind = FormSortKind::kGeneric) {
            std::vector<SortedFormChoice> out;
            out.reserve(a_choices.size());

            if (a_kind == FormSortKind::kSpell && (a_sortMode == 2 || a_sortMode == 3)) {  // Type or School
                std::vector<std::pair<SpellGroupInfo, std::size_t>> grouped;
                grouped.reserve(a_choices.size());
                for (std::size_t i = 0; i < a_choices.size(); ++i) {
                    auto* spell = a_choices[i].ref.Resolve<RE::SpellItem>();
                    grouped.push_back({(a_sortMode == 2) ? SpellTypeGroup(spell) : SpellSchoolGroup(spell), i});
                }
                std::stable_sort(grouped.begin(), grouped.end(), [&](const auto& a_lhs, const auto& a_rhs) {
                    if (a_lhs.first.order != a_rhs.first.order) {
                        return a_lhs.first.order < a_rhs.first.order;
                    }
                    return a_choices[a_lhs.second].displayName < a_choices[a_rhs.second].displayName;
                });
                for (const auto& [group, index] : grouped) {
                    out.push_back({index, std::format("[{}] {}", group.label, a_choices[index].displayName)});
                }
            } else if (a_kind == FormSortKind::kWeapon && a_sortMode == 2) {  // Type, alphabetical by group label
                std::vector<std::pair<const char*, std::size_t>> grouped;
                grouped.reserve(a_choices.size());
                for (std::size_t i = 0; i < a_choices.size(); ++i) {
                    grouped.push_back({WeaponTypeGroup(a_choices[i].ref.Resolve<RE::TESObjectWEAP>()), i});
                }
                std::stable_sort(grouped.begin(), grouped.end(), [&](const auto& a_lhs, const auto& a_rhs) {
                    std::string_view lhsLabel{a_lhs.first};
                    std::string_view rhsLabel{a_rhs.first};
                    if (lhsLabel != rhsLabel) {
                        return lhsLabel < rhsLabel;
                    }
                    return a_choices[a_lhs.second].displayName < a_choices[a_rhs.second].displayName;
                });
                for (const auto& [label, index] : grouped) {
                    out.push_back({index, std::format("[{}] {}", label, a_choices[index].displayName)});
                }
            } else if (a_kind == FormSortKind::kArmor && a_sortMode == 2) {  // Class, alphabetical by group label
                std::vector<std::pair<const char*, std::size_t>> grouped;
                grouped.reserve(a_choices.size());
                for (std::size_t i = 0; i < a_choices.size(); ++i) {
                    grouped.push_back({ArmorClassGroup(a_choices[i].ref.Resolve<RE::TESObjectARMO>()), i});
                }
                std::stable_sort(grouped.begin(), grouped.end(), [&](const auto& a_lhs, const auto& a_rhs) {
                    std::string_view lhsLabel{a_lhs.first};
                    std::string_view rhsLabel{a_rhs.first};
                    if (lhsLabel != rhsLabel) {
                        return lhsLabel < rhsLabel;
                    }
                    return a_choices[a_lhs.second].displayName < a_choices[a_rhs.second].displayName;
                });
                for (const auto& [label, index] : grouped) {
                    out.push_back({index, std::format("[{}] {}", label, a_choices[index].displayName)});
                }
            } else {
                for (std::size_t i = 0; i < a_choices.size(); ++i) {
                    out.push_back({i, a_choices[i].displayName});
                }
                if (a_sortMode == 1) {
                    std::reverse(out.begin(), out.end());
                }
            }

            return out;
        }

        void RenderSortedFormCombo(FieldPicker& a_picker, const std::vector<FormBrowser::FormChoice>& a_choices, const char* a_formLabel,
                                    const char* a_emptyMessage, FormSortKind a_kind = FormSortKind::kGeneric) {
            if (a_choices.empty()) {
                DescText(a_emptyMessage);
                return;
            }

            auto sorted = BuildSortedFormChoices(a_choices, a_picker.formSortMode, a_kind);

            std::vector<const char*> formItems;
            formItems.reserve(sorted.size());
            for (const auto& entry : sorted) {
                formItems.push_back(entry.label.c_str());
            }

            int displayIndex = -1;
            int lastSelectedDisplayIndex = -1;
            for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
                if (static_cast<int>(sorted[i].originalIndex) == a_picker.formIndex) {
                    displayIndex = i;
                }
                if (!s_lastSelectedForm.empty() && a_choices[sorted[i].originalIndex].ref.ToString() == s_lastSelectedForm) {
                    lastSelectedDisplayIndex = i;
                }
            }
            if (RenderTypeaheadCombo(a_formLabel, displayIndex, formItems.data(), static_cast<int>(formItems.size()), a_picker.formTypeahead,
                                      nullptr, lastSelectedDisplayIndex) &&
                displayIndex >= 0) {
                a_picker.formIndex = static_cast<int>(sorted[displayIndex].originalIndex);
                s_lastSelectedForm = a_choices[sorted[displayIndex].originalIndex].ref.ToString();
            }
        }

        void RenderFilterableFormCombo(FieldPicker& a_picker, const std::vector<FormBrowser::FormChoice>& a_choices, const char* a_formLabel,
                                        const char* a_emptyMessage, const char* const* a_sortNames, int a_sortCount, FormSortKind a_kind) {
            if (a_choices.empty()) {
                DescText(a_emptyMessage);
                return;
            }

            auto sorted = BuildSortedFormChoices(a_choices, a_picker.formSortMode, a_kind);

            int currentDisplayIndex = -1;
            int lastSelectedDisplayIndex = -1;
            for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
                if (static_cast<int>(sorted[i].originalIndex) == a_picker.formIndex) {
                    currentDisplayIndex = i;
                }
                if (!s_lastSelectedForm.empty() && a_choices[sorted[i].originalIndex].ref.ToString() == s_lastSelectedForm) {
                    lastSelectedDisplayIndex = i;
                }
            }

            std::string preview = (currentDisplayIndex >= 0) ? sorted[currentDisplayIndex].label : "(none)";
            bool changed = false;

            bool opened = BeginCombo(a_formLabel, preview.c_str());
            bool scrollToLastSelectedOnOpen = false;

            if (opened) {
                if (IsWindowAppearing()) {
                    a_picker.formTypeahead.index = -1;
                    a_picker.formTypeahead.ch = '\0';
                    scrollToLastSelectedOnOpen = (lastSelectedDisplayIndex >= 0);
                }

                Text("%s", TR("picker.filter"));
                SameLine();
                SetNextItemWidth(140.0f);
                InputText("##FormFilter", a_picker.formFilter, sizeof(a_picker.formFilter));
                bool filterFocused = IsItemActive();

                SameLine();
                Text("%s", TR("picker.sort"));
                SameLine();
                SetNextItemWidth(110.0f);
                RenderTypeaheadCombo("##FormSort", a_picker.formSortMode, a_sortNames, a_sortCount, a_picker.formSortTypeahead);

                Separator();

                std::vector<int> shown;
                shown.reserve(sorted.size());
                std::string needle = ToLowerCopy(a_picker.formFilter);
                for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
                    if (needle.empty() || ToLowerCopy(sorted[i].label).find(needle) != std::string::npos) {
                        shown.push_back(i);
                    }
                }

                bool scrollToTypeahead = false;

                if (!filterFocused) {
                    char typed = '\0';
                    for (int digit = 0; digit < 10 && typed == '\0'; ++digit) {
                        if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + digit), false)) {
                            typed = static_cast<char>('0' + digit);
                        }
                    }
                    for (int letter = 0; letter < 26 && typed == '\0'; ++letter) {
                        if (IsKeyPressed(static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + letter), false)) {
                            typed = static_cast<char>('a' + letter);
                        }
                    }

                    if (typed != '\0') {
                        int startPos = 0;
                        if (a_picker.formTypeahead.ch == typed && a_picker.formTypeahead.index >= 0) {
                            auto it = std::find(shown.begin(), shown.end(), a_picker.formTypeahead.index);
                            if (it != shown.end()) {
                                startPos = static_cast<int>(std::distance(shown.begin(), it)) + 1;
                            }
                        }
                        int found = -1;
                        for (int pass = 0; pass < 2 && found < 0; ++pass) {
                            int from = (pass == 0) ? startPos : 0;
                            int to = (pass == 0) ? static_cast<int>(shown.size()) : startPos;
                            for (int pos = from; pos < to; ++pos) {
                                const auto& label = sorted[shown[pos]].label;
                                if (!label.empty() && ToLowerChar(label[0]) == typed) {
                                    found = shown[pos];
                                    break;
                                }
                            }
                        }
                        if (found >= 0) {
                            a_picker.formTypeahead.index = found;
                            a_picker.formTypeahead.ch = typed;
                            scrollToTypeahead = true;
                        }
                    }

                    if (a_picker.formTypeahead.index >= 0 && (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
                        currentDisplayIndex = a_picker.formTypeahead.index;
                        changed = true;
                        CloseCurrentPopup();
                    }
                }

                for (int displayPos : shown) {
                    bool isCurrent = (displayPos == currentDisplayIndex);
                    bool isTypeaheadTarget = (displayPos == a_picker.formTypeahead.index);

                    if (isTypeaheadTarget) {
                        PushStyleColor(ImGuiCol_Header, ImVec4{0.85f, 0.65f, 0.15f, 0.65f});
                        PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{0.85f, 0.65f, 0.15f, 0.85f});
                    }

                    PushID(displayPos);
                    if (Selectable(sorted[displayPos].label.c_str(), isCurrent || isTypeaheadTarget)) {
                        currentDisplayIndex = displayPos;
                        changed = true;
                        CloseCurrentPopup();
                    }
                    PopID();

                    if (isTypeaheadTarget) {
                        PopStyleColor(2);
                        if (scrollToTypeahead) {
                            SetScrollHereY();
                        }
                    } else if (scrollToLastSelectedOnOpen && displayPos == lastSelectedDisplayIndex) {
                        SetScrollHereY();
                    }
                }

                if (shown.empty()) {
                    DescText(sorted.empty() ? a_emptyMessage : TR("picker.no_forms_match_filter"));
                }

                EndCombo();
            }

            if (changed && currentDisplayIndex >= 0) {
                a_picker.formIndex = static_cast<int>(sorted[currentDisplayIndex].originalIndex);
                s_lastSelectedForm = a_choices[sorted[currentDisplayIndex].originalIndex].ref.ToString();
            }
        }

        void RenderFieldPicker(const char* a_label, FieldPicker& a_picker, CatalogGetter a_catalogGetter, const char* a_formLabel = "Form",
                                bool a_flat = false, FormSortKind a_sortKind = FormSortKind::kGeneric, float a_widthScale = 1.0f) {
            EnsureCatalog(a_picker, a_catalogGetter);
            a_picker.flatMode = a_flat;

            if (!a_picker.formSortModeInitialized) {
                a_picker.formSortModeInitialized = true;
                if (a_sortKind == FormSortKind::kSpell || a_sortKind == FormSortKind::kWeapon || a_sortKind == FormSortKind::kArmor) {
                    a_picker.formSortMode = 2;
                }
            }

            PushID(a_label);

            if (a_sortKind == FormSortKind::kSpell) {
                SetNextItemWidth(130.0f);
                RenderTypeaheadCombo(TR("picker.spell_sort"), a_picker.formSortMode, kSpellSortNames, kSpellSortCount, a_picker.formSortTypeahead);
            }

            auto renderForm = [&](const std::vector<FormBrowser::FormChoice>& a_choices, const char* a_empty) {
                if (a_widthScale != 1.0f) {
                    SetNextItemWidth(CalcItemWidth() * a_widthScale);
                }
                if (a_sortKind == FormSortKind::kWeapon) {
                    RenderFilterableFormCombo(a_picker, a_choices, a_formLabel, a_empty, kWeaponSortNames, kWeaponSortCount, FormSortKind::kWeapon);
                } else if (a_sortKind == FormSortKind::kArmor) {
                    RenderFilterableFormCombo(a_picker, a_choices, a_formLabel, a_empty, kArmorSortNames, kArmorSortCount, FormSortKind::kArmor);
                } else {
                    RenderSortedFormCombo(a_picker, a_choices, a_formLabel, a_empty, a_sortKind);
                }
            };

            if (a_flat) {
                renderForm(a_picker.flatChoices, TR("picker.nothing_matching_inventory"));
            } else {
                if (a_widthScale != 1.0f) {
                    SetNextItemWidth(CalcItemWidth() * a_widthScale);
                }
                RenderPluginCombo(TR("picker.plugin"), a_picker);

                if (a_picker.pluginIndex >= 0) {
                    const auto* choices = CurrentChoices(a_picker);
                    if (choices) {
                        renderForm(*choices, TR("picker.no_matching_forms_in_plugin"));
                    } else {
                        DescText(TR("picker.no_matching_forms_in_plugin"));
                    }
                }
            }
            PopID();
        }

        [[nodiscard]] std::optional<FormRef> PickedFormRef(const FieldPicker& a_picker) {
            if (a_picker.flatMode) {
                if (a_picker.formIndex < 0 || a_picker.formIndex >= static_cast<int>(a_picker.flatChoices.size())) {
                    return std::nullopt;
                }
                return a_picker.flatChoices[a_picker.formIndex].ref;
            }
            const auto* choices = CurrentChoices(a_picker);
            if (!choices || a_picker.formIndex < 0 || a_picker.formIndex >= static_cast<int>(choices->size())) {
                return std::nullopt;
            }
            return (*choices)[a_picker.formIndex].ref;
        }

        [[nodiscard]] bool IsTwoHandedWeapon(const std::optional<FormRef>& a_ref) {
            if (!a_ref) {
                return false;
            }
            auto* weapon = a_ref->Resolve<RE::TESObjectWEAP>();
            if (!weapon) {
                return false;
            }
            return weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
        }

        [[nodiscard]] std::string JoinFormRefs(const std::vector<FormRef>& a_refs) {
            std::string out;
            for (std::size_t i = 0; i < a_refs.size(); ++i) {
                if (i > 0) {
                    out += ',';
                }
                out += a_refs[i].ToString();
            }
            return out;
        }


        struct KeyEditState {
            bool active = false;
            std::optional<BindKey> key;
            bool requiresModifier = false;
            int pressIndex = 0;  // 0 = Tap, 1 = Hold
            bool isNewKey = false;     // true while waiting on the very first capture (no row yet)
            bool isRemapping = false;  // true while waiting on a capture that replaces an existing key
            std::optional<std::uint32_t> pendingCode;
        };
        KeyEditState s_keyEditor;

        struct ActionEditState {
            bool active = false;
            BindKey targetKey;  // which row this Add/Edit session targets - fixed for the whole session
            bool fromPlugins = true;
            bool addIfMissing = false;
            ActionType actionType = ActionType::kWeaponSet;
            std::string errorMessage;

            bool editingExisting = false;
            std::optional<ActionType> editingOriginalType;

            FieldPicker weaponRight;
            FieldPicker weaponLeft;
            bool useShield = false;  // Left Hand picker shows shields (biped slot 39) instead of one-handed weapons
            FieldPicker weaponAmmo;
            int outfitModeIndex = 0;  // 0 = individual items, 1 = native Outfit record
            std::vector<FormRef> outfitItems;
            char outfitName[64] = "";
            FieldPicker outfitPicker;
            FieldPicker outfitFormPicker;
            bool outfitUnequipEverythingElse = false;
            FieldPicker spellRight;
            FieldPicker spellLeft;
            FieldPicker shoutPicker;
            FieldPicker consumablePicker;
            FieldPicker ammoPicker;
            FieldPicker torchPicker;
            bool panicWeapons = false;
            bool panicSpells = false;
            bool panicArmor = false;
            bool panicShouts = false;
            bool panicAmmo = false;  // added 2026-08-08, per Josh's explicit request
            FieldPicker unequipWornPicker;
            std::vector<FormRef> unequipItems;
            int movementDirectionIndex = 0;
            int openMenuTargetIndex = 0;
            bool rechargePreferSmaller = true;
            int rechargeMaxSize = 5;
            bool rechargeNotify = false;
            int ammoAddCount = 1;
            bool consumeRandom = false;
            int consumeRandomKindIndex = 0;
        };
        ActionEditState s_actionEditor;

        bool s_keyEditorWasActive = false;
        bool s_actionEditorWasActive = false;

        struct CopyToState {
            bool active = false;
            BindKey sourceKey;
            ActionType sourceType = ActionType::kWeaponSet;
            int selectedTargetIndex = 0;
            TypeaheadState typeahead;
        };
        CopyToState s_copyTo;

        struct PendingConfirm {
            std::string message;
            std::string confirmLabel = "Confirm";  // e.g. "Delete" or "Overwrite" - button text, not just generic
            std::function<void()> onConfirm;       // captures everything it needs by value - runs once, on Confirm
        };
        PendingConfirm s_pendingConfirm;
        bool s_openConfirm = false;

        void RequestConfirm(std::string a_message, std::string a_confirmLabel, std::function<void()> a_onConfirm) {
            if (!HotkeyManager::GetSingleton()->GetSettings().confirmSavesAndDeletes) {
                if (a_onConfirm) {
                    a_onConfirm();
                }
                return;
            }
            s_pendingConfirm = PendingConfirm{std::move(a_message), std::move(a_confirmLabel), std::move(a_onConfirm)};
            s_openConfirm = true;
        }

        void MaybeSaveActiveProfile(HotkeyManager* a_manager) {
            if (a_manager->GetSettings().autoSaveProfileChanges) {
                a_manager->SaveActiveProfile();
            }
        }

        bool ProfileCycleKeyCollidesWithBind(HotkeyManager* a_manager, std::uint32_t a_idCode, bool a_requiresModifier) {
            return a_manager->HasBind(BindKey{a_idCode, a_requiresModifier, PressType::kTap}) ||
                   a_manager->HasBind(BindKey{a_idCode, a_requiresModifier, PressType::kHold});
        }

        void ApplyProfileCycleKeyChange(HotkeyManager* a_manager, std::uint32_t a_idCode, bool a_requiresModifier) {
            a_manager->SetProfileCycleKey(a_idCode, a_requiresModifier);
            MaybeSaveActiveProfile(a_manager);
        }

        void SetProfileCycleKeyWithConfirm(HotkeyManager* a_manager, std::uint32_t a_idCode, bool a_requiresModifier) {
            if (a_idCode != 0 && ProfileCycleKeyCollidesWithBind(a_manager, a_idCode, a_requiresModifier)) {
                RequestConfirm(
                    TR("keybinds.profile_cycle_collision_confirm"), TR("common.continue"),
                    [a_manager, a_idCode, a_requiresModifier]() { ApplyProfileCycleKeyChange(a_manager, a_idCode, a_requiresModifier); });
            } else {
                ApplyProfileCycleKeyChange(a_manager, a_idCode, a_requiresModifier);
            }
        }

        void RenderConfirmModal() {
            std::string modalId = TRID("confirm_modal.title", "##ConfirmModal");
            if (s_openConfirm) {
                OpenPopup(modalId.c_str());
                s_openConfirm = false;
            }
            if (BeginPopupModal(modalId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                TextWrapped("%s", s_pendingConfirm.message.c_str());
                Spacing();
                std::string confirmButtonId = s_pendingConfirm.confirmLabel + "##ConfirmModalYes";
                if (SmallButton(confirmButtonId.c_str())) {
                    if (s_pendingConfirm.onConfirm) {
                        s_pendingConfirm.onConfirm();
                    }
                    s_pendingConfirm = PendingConfirm{};
                    CloseCurrentPopup();
                }
                SameLine();
                if (SmallButton(TRID("common.cancel", "##ConfirmModalNo").c_str())) {
                    s_pendingConfirm = PendingConfirm{};
                    CloseCurrentPopup();
                }
                EndPopup();
            }
        }

        void CloseEditors() {
            if (s_captureTarget == CaptureTarget::kBindKey) {
                InputHandler::GetSingleton()->CancelCapture();
                s_captureTarget = CaptureTarget::kNone;
            }
            s_keyEditor = KeyEditState{};
            s_actionEditor = ActionEditState{};
            s_copyTo = CopyToState{};
        }

        void OpenCopyTo(const BindKey& a_sourceKey, ActionType a_sourceType) {
            CloseEditors();
            s_copyTo = CopyToState{
                .active = true,
                .sourceKey = a_sourceKey,
                .sourceType = a_sourceType,
                .selectedTargetIndex = 0,
            };
        }

        void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_eventType) {
            if (a_eventType == SKSEMenuFramework::Model::EventType::kOpenMenu) {
                s_menuOpen = true;
                return;
            }
            if (a_eventType != SKSEMenuFramework::Model::EventType::kCloseMenu) {
                return;
            }
            s_menuOpen = false;
            CloseEditors();
        }

        void OpenKeyEditorForRow(const BindSummary& a_summary) {
            CloseEditors();
            s_keyEditor = KeyEditState{
                .active = true,
                .key = a_summary.key,
                .requiresModifier = a_summary.key.modifierHeld,
                .pressIndex = (a_summary.key.press == PressType::kHold) ? 1 : 0,
            };
        }

        void ApplyKeyChangeLiveWithConfirm(int a_pressIndex, bool a_requiresModifier) {
            if (!s_keyEditor.key) {
                s_keyEditor.pressIndex = a_pressIndex;
                s_keyEditor.requiresModifier = a_requiresModifier;
                return;
            }
            BindKey newKey{s_keyEditor.key->idCode, a_requiresModifier, a_pressIndex == 1 ? PressType::kHold : PressType::kTap};
            if (newKey == *s_keyEditor.key) {
                return;
            }
            auto* manager = HotkeyManager::GetSingleton();
            BindKey oldKey = *s_keyEditor.key;
            auto applyChange = [manager, oldKey, newKey, a_pressIndex, a_requiresModifier]() {
                manager->RekeyBind(oldKey, newKey);
                MaybeSaveActiveProfile(manager);
                s_keyEditor.key = newKey;
                s_keyEditor.pressIndex = a_pressIndex;
                s_keyEditor.requiresModifier = a_requiresModifier;
            };
            if (manager->HasBind(newKey)) {
                RequestConfirm(TR("keybinds.rekey_collision_confirm"), TR("common.continue"), applyChange);
            } else {
                applyChange();
            }
        }


        void ResetActionPickers(ActionEditState& a_state) {
            a_state.weaponRight = FieldPicker{};
            a_state.weaponLeft = FieldPicker{};
            a_state.weaponAmmo = FieldPicker{};
            a_state.outfitPicker = FieldPicker{};
            a_state.outfitFormPicker = FieldPicker{};
            a_state.spellRight = FieldPicker{};
            a_state.spellLeft = FieldPicker{};
            a_state.shoutPicker = FieldPicker{};
            a_state.consumablePicker = FieldPicker{};
            a_state.ammoPicker = FieldPicker{};
            a_state.torchPicker = FieldPicker{};
        }

        CatalogGetter WeaponCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetWeaponCatalog : FormBrowser::GetInventoryWeaponCatalog;
        }
        CatalogGetter LeftHandCatalogFor(bool a_fromPlugins, bool a_useShield) {
            if (a_useShield) {
                return a_fromPlugins ? FormBrowser::GetShieldCatalog : FormBrowser::GetInventoryShieldCatalog;
            }
            return a_fromPlugins ? FormBrowser::GetOneHandedWeaponCatalog : FormBrowser::GetInventoryOneHandedWeaponCatalog;
        }
        CatalogGetter AmmoCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetAmmoCatalog : FormBrowser::GetInventoryAmmoCatalog;
        }
        CatalogGetter SpellCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetSpellCatalog : FormBrowser::GetKnownSpellCatalog;
        }
        CatalogGetter ShoutCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetShoutCatalog : FormBrowser::GetKnownShoutCatalog;
        }
        CatalogGetter ConsumableCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetConsumableCatalog : FormBrowser::GetInventoryConsumableCatalog;
        }
        CatalogGetter ArmorCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetArmorCatalog : FormBrowser::GetInventoryArmorCatalog;
        }
        CatalogGetter TorchCatalogFor(bool a_fromPlugins) {
            return a_fromPlugins ? FormBrowser::GetTorchCatalog : FormBrowser::GetInventoryTorchCatalog;
        }

        void AddWornArmorItems(std::vector<FormRef>& a_outfitItems) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            auto inventory = player->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::Armor); });
            for (auto& [item, entry] : inventory) {
                auto& [count, data] = entry;
                if (!item || !data || !data->IsWorn()) {
                    continue;
                }
                auto* file = item->GetFile(0);
                if (!file) {
                    continue;
                }

                FormRef ref{.plugin = std::string(file->GetFilename()), .localFormID = item->GetLocalFormID()};
                bool alreadyPresent = std::any_of(a_outfitItems.begin(), a_outfitItems.end(), [&](const FormRef& a_existing) {
                    return a_existing.plugin == ref.plugin && a_existing.localFormID == ref.localFormID;
                });
                if (!alreadyPresent) {
                    a_outfitItems.push_back(ref);
                }
            }
        }

        [[nodiscard]] std::optional<FormRef> FormRefFor(RE::TESForm* a_form) {
            if (!a_form) {
                return std::nullopt;
            }
            auto* file = a_form->GetFile(0);
            if (!file) {
                return std::nullopt;
            }
            return FormRef{.plugin = std::string(file->GetFilename()), .localFormID = a_form->GetLocalFormID()};
        }

        void AddEquippedWeapons(ActionEditState& a_state) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            bool flat = !a_state.fromPlugins;

            if (auto* rightForm = player->GetEquippedObject(false); rightForm && rightForm->As<RE::TESObjectWEAP>()) {
                if (auto ref = FormRefFor(rightForm)) {
                    PreFillPicker(a_state.weaponRight, WeaponCatalogFor(a_state.fromPlugins), ref->ToString(), flat);
                }
            }

            if (auto* leftForm = player->GetEquippedObject(true)) {
                bool isWeapon = leftForm->As<RE::TESObjectWEAP>() != nullptr;
                auto* leftArmor = leftForm->As<RE::TESObjectARMO>();
                bool isShield = leftArmor && leftArmor->IsShield();
                if (isWeapon || isShield) {
                    a_state.useShield = isShield;
                    a_state.weaponLeft = FieldPicker{};
                    if (auto ref = FormRefFor(leftForm)) {
                        PreFillPicker(a_state.weaponLeft, LeftHandCatalogFor(a_state.fromPlugins, a_state.useShield), ref->ToString(), flat);
                    }
                }
            }

            if (auto* ammo = player->GetCurrentAmmo()) {
                if (auto ref = FormRefFor(ammo)) {
                    PreFillPicker(a_state.weaponAmmo, AmmoCatalogFor(a_state.fromPlugins), ref->ToString(), flat);
                }
            }
        }

        void AddEquippedSpells(ActionEditState& a_state) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            bool flat = !a_state.fromPlugins;

            if (auto* rightForm = player->GetEquippedObject(false); rightForm && rightForm->As<RE::SpellItem>()) {
                if (auto ref = FormRefFor(rightForm)) {
                    PreFillPicker(a_state.spellRight, SpellCatalogFor(a_state.fromPlugins), ref->ToString(), flat);
                }
            }

            if (auto* leftForm = player->GetEquippedObject(true); leftForm && leftForm->As<RE::SpellItem>()) {
                if (auto ref = FormRefFor(leftForm)) {
                    PreFillPicker(a_state.spellLeft, SpellCatalogFor(a_state.fromPlugins), ref->ToString(), flat);
                }
            }
        }

        void AddEquippedShouts(ActionEditState& a_state) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return;
            }

            auto* power = player->GetActorRuntimeData().selectedPower;
            if (auto* shout = power ? power->As<RE::TESShout>() : nullptr) {
                if (auto ref = FormRefFor(shout)) {
                    PreFillPicker(a_state.shoutPicker, ShoutCatalogFor(a_state.fromPlugins), ref->ToString(), !a_state.fromPlugins);
                }
            }
        }

        [[nodiscard]] std::vector<ActionType> AllowedNewActionTypes(const std::vector<ActionType>& a_existingTypes) {
            bool hasWeapon = std::find(a_existingTypes.begin(), a_existingTypes.end(), ActionType::kWeaponSet) != a_existingTypes.end();
            bool hasAmmo = std::find(a_existingTypes.begin(), a_existingTypes.end(), ActionType::kAmmoSwap) != a_existingTypes.end();
            bool hasMovement = std::find(a_existingTypes.begin(), a_existingTypes.end(), ActionType::kMovement) != a_existingTypes.end();
            bool hasOpenMenu = std::find(a_existingTypes.begin(), a_existingTypes.end(), ActionType::kOpenMenu) != a_existingTypes.end();

            std::vector<ActionType> allowed;
            for (int i = 0; i < kActionTypeCount; ++i) {
                auto type = static_cast<ActionType>(i);
                if (std::find(a_existingTypes.begin(), a_existingTypes.end(), type) != a_existingTypes.end()) {
                    continue;  // already present - no duplicates
                }
                if (type == ActionType::kWeaponSet && hasAmmo) {
                    continue;  // Ammo Swap present - Weapon's own Ammo field already covers this
                }
                if (type == ActionType::kAmmoSwap && hasWeapon) {
                    continue;  // Weapon present - use its own Ammo field instead
                }
                if (type == ActionType::kMovement && !a_existingTypes.empty()) {
                    continue;  // bind already has something else - Movement can't join it
                }
                if (hasMovement && type != ActionType::kMovement) {
                    continue;  // bind already has Movement - nothing else can join it
                }
                if (type == ActionType::kOpenMenu && !a_existingTypes.empty()) {
                    continue;  // bind already has something else - Open Menu can't join it
                }
                if (hasOpenMenu && type != ActionType::kOpenMenu) {
                    continue;  // bind already has Open Menu - nothing else can join it
                }
                allowed.push_back(type);
            }
            return allowed;
        }

        void RenderActionTypeCombo(ActionType& a_type, const std::vector<ActionType>& a_allowedTypes) {
            if (a_allowedTypes.empty()) {
                DescText(TR("actioneditor.no_action_types_available"));
                return;
            }

            std::vector<const char*> labels;
            labels.reserve(a_allowedTypes.size());
            for (auto type : a_allowedTypes) {
                labels.push_back(ActionTypeDisplayName(type));
            }

            int currentIndex = -1;
            for (int i = 0; i < static_cast<int>(a_allowedTypes.size()); ++i) {
                if (a_allowedTypes[i] == a_type) {
                    currentIndex = i;
                    break;
                }
            }
            if (currentIndex < 0) {
                a_type = a_allowedTypes[0];
                currentIndex = 0;
            }

            static TypeaheadState typeahead;

            Text("%s", TR("actioneditor.action_type"));
            SameLine();
            SetNextItemWidth(180.0f);
            if (RenderTypeaheadCombo("##ActionType", currentIndex, labels.data(), static_cast<int>(labels.size()), typeahead)) {
                a_type = a_allowedTypes[currentIndex];
            }
        }

        void PreFillActionFromExisting(const ActionSummary& a_action) {
            s_actionEditor.actionType = a_action.type;

            auto fields = ParseFieldString(a_action.serialized);

            switch (a_action.type) {
                case ActionType::kWeaponSet:
                    if (auto it = fields.find("Right"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.weaponRight, FormBrowser::GetWeaponCatalog, it->second);
                    }
                    if (auto it = fields.find("Left"); it != fields.end()) {
                        if (auto leftRef = FormRef::Parse(it->second)) {
                            s_actionEditor.useShield = leftRef->Resolve<RE::TESObjectARMO>() != nullptr;
                        }
                        PreFillPicker(s_actionEditor.weaponLeft,
                                      s_actionEditor.useShield ? FormBrowser::GetShieldCatalog : FormBrowser::GetOneHandedWeaponCatalog,
                                      it->second);
                    }
                    if (auto it = fields.find("Ammo"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.weaponAmmo, FormBrowser::GetAmmoCatalog, it->second);
                    }
                    if (auto it = fields.find("AddIfMissing"); it != fields.end()) {
                        s_actionEditor.addIfMissing = (it->second == "1");
                    }
                    break;
                case ActionType::kOutfit:
                    if (auto it = fields.find("Outfit"); it != fields.end()) {
                        s_actionEditor.outfitModeIndex = 1;
                        PreFillPicker(s_actionEditor.outfitFormPicker, FormBrowser::GetOutfitCatalog, it->second);
                    } else if (auto it2 = fields.find("Items"); it2 != fields.end()) {
                        s_actionEditor.outfitModeIndex = 0;
                        for (const auto& part : SplitComma(it2->second)) {
                            if (auto ref = FormRef::Parse(part)) {
                                s_actionEditor.outfitItems.push_back(*ref);
                            }
                        }
                    }
                    if (auto it3 = fields.find("UnequipAll"); it3 != fields.end()) {
                        s_actionEditor.outfitUnequipEverythingElse = (it3->second == "1");
                    }
                    if (auto it4 = fields.find("AddIfMissing"); it4 != fields.end()) {
                        s_actionEditor.addIfMissing = (it4->second == "1");
                    }
                    if (auto it5 = fields.find("OutfitName"); it5 != fields.end()) {
                        std::snprintf(s_actionEditor.outfitName, sizeof(s_actionEditor.outfitName), "%s", it5->second.c_str());
                    }
                    break;
                case ActionType::kSpell:
                    if (auto it = fields.find("Right"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.spellRight, FormBrowser::GetSpellCatalog, it->second);
                    }
                    if (auto it = fields.find("Left"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.spellLeft, FormBrowser::GetSpellCatalog, it->second);
                    }
                    if (!fields.contains("Right") && !fields.contains("Left")) {
                        auto formIt = fields.find("Form");
                        auto handIt = fields.find("Hand");
                        if (formIt != fields.end() && handIt != fields.end()) {
                            if (handIt->second == "Left") {
                                PreFillPicker(s_actionEditor.spellLeft, FormBrowser::GetSpellCatalog, formIt->second);
                            } else if (handIt->second == "BothInstant") {
                                PreFillPicker(s_actionEditor.spellRight, FormBrowser::GetSpellCatalog, formIt->second);
                                PreFillPicker(s_actionEditor.spellLeft, FormBrowser::GetSpellCatalog, formIt->second);
                            } else {
                                PreFillPicker(s_actionEditor.spellRight, FormBrowser::GetSpellCatalog, formIt->second);
                            }
                        }
                    }
                    if (auto it2 = fields.find("AddIfMissing"); it2 != fields.end()) {
                        s_actionEditor.addIfMissing = (it2->second == "1");
                    }
                    break;
                case ActionType::kShout:
                    if (auto it = fields.find("Form"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.shoutPicker, FormBrowser::GetShoutCatalog, it->second);
                    }
                    if (auto it2 = fields.find("AddIfMissing"); it2 != fields.end()) {
                        s_actionEditor.addIfMissing = (it2->second == "1");
                    }
                    break;
                case ActionType::kConsumable:
                    if (auto it0 = fields.find("ConsumeRandom"); it0 != fields.end() && it0->second == "1") {
                        s_actionEditor.consumeRandom = true;
                        s_actionEditor.consumeRandomKindIndex = 0;
                        if (auto itKind = fields.find("RandomKind"); itKind != fields.end() && itKind->second == "Drink") {
                            s_actionEditor.consumeRandomKindIndex = 1;
                        }
                        break;
                    }
                    s_actionEditor.consumeRandom = false;
                    if (auto it = fields.find("Form"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.consumablePicker, FormBrowser::GetConsumableCatalog, it->second);
                    }
                    if (auto it2 = fields.find("AddIfMissing"); it2 != fields.end()) {
                        s_actionEditor.addIfMissing = (it2->second == "1");
                    }
                    break;
                case ActionType::kAmmoSwap:
                    if (auto it = fields.find("Form"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.ammoPicker, FormBrowser::GetAmmoCatalog, it->second);
                    }
                    if (auto it2 = fields.find("AddIfMissing"); it2 != fields.end()) {
                        s_actionEditor.addIfMissing = (it2->second == "1");
                    }
                    s_actionEditor.ammoAddCount = 1;
                    if (auto it3 = fields.find("AddCount"); it3 != fields.end()) {
                        s_actionEditor.ammoAddCount = std::atoi(it3->second.c_str());
                        if (s_actionEditor.ammoAddCount < 1) {
                            s_actionEditor.ammoAddCount = 1;
                        }
                    }
                    break;
                case ActionType::kToggleTorch:
                    if (auto it = fields.find("Form"); it != fields.end()) {
                        PreFillPicker(s_actionEditor.torchPicker, FormBrowser::GetTorchCatalog, it->second);
                    }
                    if (auto it2 = fields.find("AddIfMissing"); it2 != fields.end()) {
                        s_actionEditor.addIfMissing = (it2->second == "1");
                    }
                    break;
                case ActionType::kTogglePOV:
                    break;
                case ActionType::kReadySheath:
                case ActionType::kToggleSneak:
                case ActionType::kToggleAutoMove:
                case ActionType::kJump:
                case ActionType::kToggleFreeCam:
                case ActionType::kToggleFreeCamPaused:
                case ActionType::kToggleSprint:
                case ActionType::kQuickSave:
                case ActionType::kQuickLoad:
                case ActionType::kToggleMenus:
                    break;
                case ActionType::kRechargeWeapon:
                case ActionType::kRechargeWeaponLeftHand:
                    if (auto it = fields.find("PreferSmaller"); it != fields.end()) {
                        s_actionEditor.rechargePreferSmaller = (it->second == "1");
                    }
                    if (auto it = fields.find("MaxSize"); it != fields.end()) {
                        auto parsed = std::atoi(it->second.c_str());
                        if (parsed >= 0 && parsed <= 5) {
                            s_actionEditor.rechargeMaxSize = parsed;
                        }
                    }
                    if (auto it = fields.find("Notify"); it != fields.end()) {
                        s_actionEditor.rechargeNotify = (it->second == "1");
                    }
                    break;
                case ActionType::kMovement:
                    if (auto it = fields.find("Direction"); it != fields.end()) {
                        for (int i = 0; i < kMovementDirectionCount; ++i) {
                            if (it->second == kMovementDirectionNames[i]) {
                                s_actionEditor.movementDirectionIndex = i;
                                break;
                            }
                        }
                    }
                    break;
                case ActionType::kOpenMenu:
                    if (auto it = fields.find("Target"); it != fields.end()) {
                        std::string_view target = it->second == "Wait" ? "Rest" : std::string_view{it->second};
                        for (int i = 0; i < kOpenMenuTargetCount; ++i) {
                            if (target == kOpenMenuTargetNames[i]) {
                                s_actionEditor.openMenuTargetIndex = i;
                                break;
                            }
                        }
                    }
                    break;
                case ActionType::kPanic:
                    if (auto it = fields.find("Items"); it != fields.end()) {
                        for (const auto& part : SplitComma(it->second)) {
                            if (auto ref = FormRef::Parse(part)) {
                                s_actionEditor.unequipItems.push_back(*ref);
                            }
                        }
                    } else if (auto it2 = fields.find("Categories"); it2 != fields.end()) {
                        for (const auto& part : SplitComma(it2->second)) {
                            if (part == "Weapons") s_actionEditor.panicWeapons = true;
                            else if (part == "Spells") s_actionEditor.panicSpells = true;
                            else if (part == "Armor") s_actionEditor.panicArmor = true;
                            else if (part == "Shouts") s_actionEditor.panicShouts = true;
                            else if (part == "Ammo") s_actionEditor.panicAmmo = true;
                        }
                    }
                    break;
            }
        }

        void OpenActionEditorForAdd(const BindSummary& a_summary) {
            CloseEditors();
            s_actionEditor = ActionEditState{
                .active = true,
                .targetKey = a_summary.key,
                .fromPlugins = false,
                .editingExisting = false,
            };
            s_actionEditor.ammoAddCount = HotkeyManager::GetSingleton()->GetSettings().lastAmmoAddCount;
        }

        void OpenActionEditorForEdit(const BindSummary& a_summary, const ActionSummary& a_action) {
            CloseEditors();
            s_actionEditor = ActionEditState{
                .active = true,
                .targetKey = a_summary.key,
                .fromPlugins = true,
                .editingExisting = true,
                .editingOriginalType = a_action.type,
            };
            PreFillActionFromExisting(a_action);
        }

        void RenderKeyEditor() {
            const auto& settings = HotkeyManager::GetSingleton()->GetSettings();

            Separator();
            TextColored(ImVec4{0.9f, 0.9f, 0.3f, 1.0f}, "%s", TR("keybinds.editing_bind"));

            if (!s_keyEditor.isNewKey && !s_keyEditor.isRemapping) {
                SameLine();
                if (SmallButton(TR("keybinds.remap"))) {
                    s_keyEditor.isRemapping = true;
                    InputHandler::GetSingleton()->BeginCapture();
                    s_captureTarget = CaptureTarget::kBindKey;
                }
            }

            if (s_keyEditor.isNewKey || s_keyEditor.isRemapping) {
                if (s_keyEditor.pendingCode) {
                    Text(TR("keybinds.key_label"),
                         KeyWithModifierLabel(*s_keyEditor.pendingCode, s_keyEditor.requiresModifier, settings.modifierKeyCode,
                                               settings.modifierGamepadCode)
                             .c_str());
                    SameLine();
                    if (SmallButton(TR("common.ok"))) {
                        BindKey newKey{*s_keyEditor.pendingCode, s_keyEditor.requiresModifier,
                                       s_keyEditor.pressIndex == 1 ? PressType::kHold : PressType::kTap};
                        auto* manager = HotkeyManager::GetSingleton();
                        bool isNewKey = s_keyEditor.isNewKey;
                        std::optional<BindKey> oldKey = s_keyEditor.key;

                        auto applyAndClose = [manager, isNewKey, oldKey, newKey]() {
                            if (isNewKey) {
                                manager->RemoveBind(newKey);
                                manager->CreateBind(newKey);
                                MaybeSaveActiveProfile(manager);
                            } else if (oldKey && *oldKey != newKey) {
                                manager->RekeyBind(*oldKey, newKey);
                                MaybeSaveActiveProfile(manager);
                            }
                            CloseEditors();
                        };

                        bool wouldOverwriteOther = manager->HasBind(newKey) && (isNewKey || !oldKey || *oldKey != newKey);
                        if (wouldOverwriteOther) {
                            RequestConfirm(TR("keybinds.key_collision_confirm"), TR("common.continue"), applyAndClose);
                        } else {
                            applyAndClose();
                        }
                        return;
                    }
                    SameLine();
                    if (SmallButton(TR("common.cancel"))) {
                        s_keyEditor.pendingCode.reset();
                        if (s_keyEditor.isNewKey) {
                            CloseEditors();
                            return;
                        }
                        s_keyEditor.isRemapping = false;
                    }
                } else {
                    Text("%s", TR("keybinds.key_capture_label"));
                    SameLine();
                    PushID("BindKeyCapture");
                    std::uint32_t captured = 0;
                    bool capturedRequiresModifier = false;
                    auto result = RenderCaptureButton(CaptureTarget::kBindKey, captured, &capturedRequiresModifier);
                    PopID();
                    if (result == CaptureResult::kCaptured) {
                        s_keyEditor.pendingCode = captured;
                        s_keyEditor.requiresModifier = capturedRequiresModifier;
                    } else if (result == CaptureResult::kCancelled) {
                        if (s_keyEditor.isNewKey) {
                            CloseEditors();
                            return;
                        }
                        s_keyEditor.isRemapping = false;
                    }
                }
            } else {
                Text(TR("keybinds.key_label"), s_keyEditor.key
                                     ? KeyWithModifierLabel(s_keyEditor.key->idCode, s_keyEditor.requiresModifier, settings.modifierKeyCode,
                                                             settings.modifierGamepadCode)
                                           .c_str()
                                     : TR("keybinds.key_label_none"));
            }

            static TypeaheadState pressTypeahead;
            SetNextItemWidth(90.0f);
            int pendingPressIndex = s_keyEditor.pressIndex;
            const char* pressNames[] = {TR("keybinds.press_tap"), TR("keybinds.press_hold")};
            bool pressChanged = RenderTypeaheadCombo(TR("keybinds.press"), pendingPressIndex, pressNames, 2, pressTypeahead);
            SameLine();
            bool pendingRequiresModifier = s_keyEditor.requiresModifier;
            bool modifierChanged = Checkbox(TR("keybinds.requires_modifier"), &pendingRequiresModifier);
            if (pressChanged || modifierChanged) {
                ApplyKeyChangeLiveWithConfirm(pendingPressIndex, pendingRequiresModifier);
            }
        }


        void RenderActionEditor() {
            Separator();
            TextColored(ImVec4{0.9f, 0.9f, 0.3f, 1.0f}, "%s", TR("actioneditor.editing_action"));
            Text(TR("keybinds.key_label"), BindKeyLabel(s_actionEditor.targetKey, HotkeyManager::GetSingleton()->GetSettings().modifierKeyCode, HotkeyManager::GetSingleton()->GetSettings().modifierGamepadCode).c_str());

            if (s_actionEditor.actionType != ActionType::kPanic && s_actionEditor.actionType != ActionType::kTogglePOV &&
                s_actionEditor.actionType != ActionType::kMovement && s_actionEditor.actionType != ActionType::kOpenMenu &&
                s_actionEditor.actionType != ActionType::kReadySheath &&
                s_actionEditor.actionType != ActionType::kToggleSneak &&
                s_actionEditor.actionType != ActionType::kToggleAutoMove &&
                s_actionEditor.actionType != ActionType::kJump && s_actionEditor.actionType != ActionType::kToggleFreeCam &&
                s_actionEditor.actionType != ActionType::kToggleFreeCamPaused &&
                s_actionEditor.actionType != ActionType::kToggleSprint &&
                s_actionEditor.actionType != ActionType::kQuickSave && s_actionEditor.actionType != ActionType::kQuickLoad &&
                s_actionEditor.actionType != ActionType::kToggleMenus && s_actionEditor.actionType != ActionType::kRechargeWeapon &&
                s_actionEditor.actionType != ActionType::kRechargeWeaponLeftHand) {
                if (Checkbox(TR("actioneditor.from_plugins"), &s_actionEditor.fromPlugins)) {
                    ResetActionPickers(s_actionEditor);
                }
            }

            if (s_actionEditor.actionType != ActionType::kPanic && s_actionEditor.actionType != ActionType::kTogglePOV &&
                s_actionEditor.actionType != ActionType::kMovement && s_actionEditor.actionType != ActionType::kOpenMenu &&
                s_actionEditor.actionType != ActionType::kReadySheath &&
                s_actionEditor.actionType != ActionType::kToggleSneak &&
                s_actionEditor.actionType != ActionType::kToggleAutoMove &&
                s_actionEditor.actionType != ActionType::kJump && s_actionEditor.actionType != ActionType::kToggleFreeCam &&
                s_actionEditor.actionType != ActionType::kToggleFreeCamPaused &&
                s_actionEditor.actionType != ActionType::kToggleSprint &&
                s_actionEditor.actionType != ActionType::kQuickSave && s_actionEditor.actionType != ActionType::kQuickLoad &&
                s_actionEditor.actionType != ActionType::kToggleMenus && s_actionEditor.actionType != ActionType::kRechargeWeapon &&
                s_actionEditor.actionType != ActionType::kRechargeWeaponLeftHand &&
                !(s_actionEditor.actionType == ActionType::kConsumable && s_actionEditor.consumeRandom)) {
                SameLine();
                Checkbox(TR("actioneditor.add_if_missing"), &s_actionEditor.addIfMissing);
                if (s_actionEditor.actionType == ActionType::kAmmoSwap && s_actionEditor.addIfMissing) {
                    SameLine();
                    SetNextItemWidth(140.0f);
                    InputInt(TR("actioneditor.ammo.add_count"), &s_actionEditor.ammoAddCount);
                    if (s_actionEditor.ammoAddCount < 1) {
                        s_actionEditor.ammoAddCount = 1;
                    }
                }
            }

            std::vector<ActionType> existingTypes;
            {
                auto* manager = HotkeyManager::GetSingleton();
                for (const auto& summary : manager->GetBindSummaries()) {
                    if (summary.key != s_actionEditor.targetKey) {
                        continue;
                    }
                    for (const auto& action : summary.actions) {
                        if (s_actionEditor.editingExisting && s_actionEditor.editingOriginalType && action.type == *s_actionEditor.editingOriginalType) {
                            continue;  // exclude the action being edited itself
                        }
                        existingTypes.push_back(action.type);
                    }
                    break;
                }
            }
            auto allowedTypes = AllowedNewActionTypes(existingTypes);

            RenderActionTypeCombo(s_actionEditor.actionType, allowedTypes);
            auto selectedType = s_actionEditor.actionType;

            bool disableLeftHand = selectedType == ActionType::kWeaponSet && IsTwoHandedWeapon(PickedFormRef(s_actionEditor.weaponRight));

            if (selectedType == ActionType::kWeaponSet) {
                SameLine();
                BeginDisabled(disableLeftHand);
                if (Checkbox(TR("actioneditor.use_shield"), &s_actionEditor.useShield)) {
                    s_actionEditor.weaponLeft = FieldPicker{};
                }
                EndDisabled();
                SameLine();
                if (SmallButton(TR("actioneditor.add_equipped_weapons"))) {
                    AddEquippedWeapons(s_actionEditor);
                }
            } else if (selectedType == ActionType::kSpell) {
                SameLine();
                if (SmallButton(TR("actioneditor.add_equipped_spells"))) {
                    AddEquippedSpells(s_actionEditor);
                }
            } else if (selectedType == ActionType::kShout) {
                SameLine();
                if (SmallButton(TR("actioneditor.add_equipped_shouts"))) {
                    AddEquippedShouts(s_actionEditor);
                }
            } else if (selectedType == ActionType::kOutfit) {
                SameLine();
                Checkbox(TR("actioneditor.outfit_unequip_everything_else"), &s_actionEditor.outfitUnequipEverythingElse);
            }

            Separator();

            bool flat = !s_actionEditor.fromPlugins;

            switch (selectedType) {
                case ActionType::kWeaponSet: {
                    Text("%s", TR("actioneditor.right_hand_optional"));
                    RenderFieldPicker("WeaponRight", s_actionEditor.weaponRight, WeaponCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kWeapon, 0.75f);

                    Text("%s", TR("actioneditor.left_hand_optional"));
                    if (disableLeftHand) {
                        DescText(TR("actioneditor.weaponset.left_disabled_two_handed"));
                    }
                    BeginDisabled(disableLeftHand);
                    RenderFieldPicker(
                        "WeaponLeft", s_actionEditor.weaponLeft, LeftHandCatalogFor(s_actionEditor.fromPlugins, s_actionEditor.useShield),
                        s_actionEditor.useShield ? "Shield" : ActionTypeDisplayName(selectedType), flat, FormSortKind::kWeapon,
                        0.75f);
                    EndDisabled();

                    Text("%s", TR("actioneditor.ammo_optional"));
                    RenderFieldPicker("WeaponAmmo", s_actionEditor.weaponAmmo, AmmoCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                    break;
                }
                case ActionType::kOutfit: {
                    static TypeaheadState outfitSourceTypeahead;
                    if (s_actionEditor.fromPlugins) {
                        SetNextItemWidth(CalcItemWidth() * 0.75f);
                        const char* outfitModeNames[] = {TR("actioneditor.outfit.mode_individual_items"), TR("actioneditor.outfit.mode_outfit_record")};
                        RenderTypeaheadCombo(TR("actioneditor.outfit.source"), s_actionEditor.outfitModeIndex, outfitModeNames, kOutfitModeCount, outfitSourceTypeahead);
                    } else {
                        s_actionEditor.outfitModeIndex = 0;
                    }
                    if (s_actionEditor.fromPlugins && s_actionEditor.outfitModeIndex == 1) {
                        Text("%s", TR("actioneditor.outfit.outfit_record"));
                        RenderFieldPicker("OutfitForm", s_actionEditor.outfitFormPicker, FormBrowser::GetOutfitCatalog,
                                          ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                        DescText(TR("actioneditor.outfit.record_tooltip"));
                    } else {
                        Text("%s", TR("actioneditor.outfit.armor_item"));
                        RenderFieldPicker("OutfitItem", s_actionEditor.outfitPicker, ArmorCatalogFor(s_actionEditor.fromPlugins),
                                          TR("actioneditor.outfit.armor_clothing"), flat, FormSortKind::kArmor, 0.75f);
                        if (Button(TR("actioneditor.outfit.add_item"))) {
                            if (auto ref = PickedFormRef(s_actionEditor.outfitPicker)) {
                                s_actionEditor.outfitItems.push_back(*ref);
                            }
                        }
                        SameLine();
                        if (Button(TR("actioneditor.outfit.add_worn_items"))) {
                            AddWornArmorItems(s_actionEditor.outfitItems);
                        }
                        Text("%s", TR("actioneditor.outfit.name_label"));
                        SameLine();
                        SetNextItemWidth(CalcItemWidth() * 0.33f);
                        InputText("##OutfitName", s_actionEditor.outfitName, sizeof(s_actionEditor.outfitName));
                        Text("%s", TR("actioneditor.outfit.items_in_outfit"));
                        for (std::size_t i = 0; i < s_actionEditor.outfitItems.size();) {
                            PushID(static_cast<int>(i));
                            Text("  %s", s_actionEditor.outfitItems[i].ToDisplayString().c_str());
                            SameLine();
                            if (SmallButton(TR("common.remove"))) {
                                s_actionEditor.outfitItems.erase(s_actionEditor.outfitItems.begin() + static_cast<std::ptrdiff_t>(i));
                                PopID();
                                continue;  // don't advance i - the next item shifted into this slot
                            }
                            PopID();
                            ++i;
                        }
                    }
                    break;
                }
                case ActionType::kSpell:
                    Text("%s", TR("actioneditor.right_hand_optional"));
                    RenderFieldPicker("SpellRight", s_actionEditor.spellRight, SpellCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kSpell, 0.75f);
                    Text("%s", TR("actioneditor.left_hand_optional"));
                    RenderFieldPicker("SpellLeft", s_actionEditor.spellLeft, SpellCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kSpell, 0.75f);
                    DescText(TR("actioneditor.spell.dual_cast_tooltip"));
                    break;
                case ActionType::kShout:
                    Text("%s", TR("actioneditor.shout.label"));
                    RenderFieldPicker("ShoutForm", s_actionEditor.shoutPicker, ShoutCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                    break;
                case ActionType::kConsumable:
                    Text("%s", TR("actioneditor.consumable.label"));
                    BeginDisabled(s_actionEditor.consumeRandom);
                    RenderFieldPicker("ConsumableForm", s_actionEditor.consumablePicker, ConsumableCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                    EndDisabled();
                    Checkbox(TR("actioneditor.consumable.consume_random"), &s_actionEditor.consumeRandom);
                    if (s_actionEditor.consumeRandom) {
                        const char* consumeRandomKindNames[] = {TR("actioneditor.consumable.food_option"),
                                                                 TR("actioneditor.consumable.drink_option")};
                        for (int i = 0; i < 2; ++i) {
                            RadioButton(consumeRandomKindNames[i], &s_actionEditor.consumeRandomKindIndex, i);
                        }
                        DescText(TR("actioneditor.consumable.consume_random_tooltip"));
                    }
                    break;
                case ActionType::kAmmoSwap:
                    Text("%s", TR("actioneditor.ammoswap.label"));
                    RenderFieldPicker("AmmoForm", s_actionEditor.ammoPicker, AmmoCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                    break;
                case ActionType::kToggleTorch:
                    Text("%s", TR("actioneditor.toggletorch.label"));
                    RenderFieldPicker("TorchForm", s_actionEditor.torchPicker, TorchCatalogFor(s_actionEditor.fromPlugins),
                                      ActionTypeDisplayName(selectedType), flat, FormSortKind::kGeneric, 0.75f);
                    break;
                case ActionType::kTogglePOV:
                    DescText(TR("actioneditor.togglepov.desc"));
                    break;
                case ActionType::kReadySheath:
                    DescText(TR("actioneditor.readysheath.desc"));
                    break;
                case ActionType::kToggleSneak:
                    DescText(TR("actioneditor.togglesneak.desc"));
                    break;
                case ActionType::kToggleAutoMove:
                    DescText(TR("actioneditor.toggleautomove.desc"));
                    break;
                case ActionType::kJump:
                    DescText(TR("actioneditor.jump.desc"));
                    break;
                case ActionType::kToggleFreeCam:
                    DescText(TR("actioneditor.togglefreecam.desc"));
                    break;
                case ActionType::kToggleFreeCamPaused:
                    DescText(TR("actioneditor.togglefreecampaused.desc"));
                    break;
                case ActionType::kToggleSprint:
                    DescText(TR("actioneditor.togglesprint.desc"));
                    break;
                case ActionType::kQuickSave:
                    DescText(TR("actioneditor.quicksave.desc"));
                    break;
                case ActionType::kQuickLoad:
                    DescText(TR("actioneditor.quickload.desc"));
                    break;
                case ActionType::kToggleMenus:
                    DescText(TR("actioneditor.togglemenus.desc"));
                    break;
                case ActionType::kRechargeWeapon:
                case ActionType::kRechargeWeaponLeftHand: {
                    bool isLeftHand = s_actionEditor.actionType == ActionType::kRechargeWeaponLeftHand;
                    DescText(isLeftHand ? TR("actioneditor.recharge.desc_left") : TR("actioneditor.recharge.desc_right"));
                    Checkbox(TR("actioneditor.recharge.prefer_smaller"), &s_actionEditor.rechargePreferSmaller);
                    DescText(TR("actioneditor.recharge.prefer_smaller_tooltip"));
                    {
                        const char* soulGemSizeNames[] = {TR("actioneditor.recharge.soul_petty"), TR("actioneditor.recharge.soul_lesser"),
                                                           TR("actioneditor.recharge.soul_common"), TR("actioneditor.recharge.soul_greater"),
                                                           TR("actioneditor.recharge.soul_grand")};
                        Text("%s", TR("actioneditor.recharge.never_use_above"));
                        for (int i = 1; i <= 5; ++i) {
                            SameLine();
                            PushID(i);
                            RadioButton(soulGemSizeNames[i - 1], &s_actionEditor.rechargeMaxSize, i);
                            PopID();
                        }
                    }
                    DescText(TR("actioneditor.recharge.contained_soul_tooltip"));

                    Checkbox(TR("actioneditor.recharge.notify"), &s_actionEditor.rechargeNotify);
                    break;
                }
                case ActionType::kMovement: {
                    static TypeaheadState movementDirectionTypeahead;
                    Text("%s", TR("actioneditor.movement.direction"));
                    SameLine();
                    SetNextItemWidth(180.0f);
                    const char* movementDirectionNames[] = {TR("actioneditor.movement.forward"), TR("actioneditor.movement.backward"),
                                                             TR("actioneditor.movement.strafe_left"), TR("actioneditor.movement.strafe_right")};
                    RenderTypeaheadCombo("##MovementDirection", s_actionEditor.movementDirectionIndex, movementDirectionNames,
                                          kMovementDirectionCount, movementDirectionTypeahead);
                    DescText(TR("actioneditor.movement.tooltip"));
                    break;
                }
                case ActionType::kOpenMenu: {
                    const char* openMenuTargetNames[] = {TR("actioneditor.openmenu.inventory"), TR("actioneditor.openmenu.spells"),
                                                          TR("actioneditor.openmenu.map"),       TR("actioneditor.openmenu.skills"),
                                                          TR("actioneditor.openmenu.favorites"),  TR("actioneditor.openmenu.wait_rest")};
                    for (int i = 0; i < kOpenMenuTargetCount; ++i) {
                        RadioButton(openMenuTargetNames[i], &s_actionEditor.openMenuTargetIndex, i);
                    }
                    DescText(TR("actioneditor.openmenu.tooltip"));
                    break;
                }
                case ActionType::kPanic: {
                    bool hasSpecificItems = !s_actionEditor.unequipItems.empty();
                    BeginDisabled(hasSpecificItems);
                    Checkbox(TR("actioneditor.panic.unequip_weapons"), &s_actionEditor.panicWeapons);
                    Checkbox(TR("actioneditor.panic.unequip_spells"), &s_actionEditor.panicSpells);
                    Checkbox(TR("actioneditor.panic.unequip_armor"), &s_actionEditor.panicArmor);
                    Checkbox(TR("actioneditor.panic.unequip_shouts"), &s_actionEditor.panicShouts);
                    Checkbox(TR("actioneditor.panic.unequip_ammo"), &s_actionEditor.panicAmmo);
                    EndDisabled();
                    if (hasSpecificItems) {
                        DescText(TR("actioneditor.panic.disabled_tooltip"));
                    }

                    Separator();
                    EnsureCatalog(s_actionEditor.unequipWornPicker, FormBrowser::GetWornArmorCatalog);
                    s_actionEditor.unequipWornPicker.flatMode = true;
                    PushID("UnequipWornForm");
                    Text("%s", TR("actioneditor.panic.unequip_worn_item"));
                    SameLine();
                    SetNextItemWidth(CalcItemWidth() * 0.5f);
                    RenderSortedFormCombo(s_actionEditor.unequipWornPicker, s_actionEditor.unequipWornPicker.flatChoices, "##UnequipWornForm",
                                          TR("picker.nothing_matching_inventory"));
                    PopID();
                    SameLine();
                    if (Button(TR("common.add"))) {
                        if (auto ref = PickedFormRef(s_actionEditor.unequipWornPicker)) {
                            bool alreadyPresent = std::any_of(
                                s_actionEditor.unequipItems.begin(), s_actionEditor.unequipItems.end(), [&](const FormRef& a_existing) {
                                    return a_existing.plugin == ref->plugin && a_existing.localFormID == ref->localFormID;
                                });
                            if (!alreadyPresent) {
                                s_actionEditor.unequipItems.push_back(*ref);
                            }
                        }
                    }
                    Text("%s", TR("actioneditor.panic.items_to_unequip"));
                    for (std::size_t i = 0; i < s_actionEditor.unequipItems.size();) {
                        PushID(static_cast<int>(i));
                        Text("  %s", s_actionEditor.unequipItems[i].ToString().c_str());
                        SameLine();
                        if (SmallButton(TR("common.remove"))) {
                            s_actionEditor.unequipItems.erase(s_actionEditor.unequipItems.begin() + static_cast<std::ptrdiff_t>(i));
                            PopID();
                            continue;  // don't advance i - the next item shifted into this slot
                        }
                        PopID();
                        ++i;
                    }
                    break;
                }
            }

            if (!s_actionEditor.errorMessage.empty()) {
                TextColored(ImVec4{0.9f, 0.3f, 0.3f, 1.0f}, "%s", s_actionEditor.errorMessage.c_str());
            }

            Separator();

            if (Button(TR("common.save_bind"))) {
                std::unordered_map<std::string, std::string> fields;
                fields["Type"] = kActionTypeNames[static_cast<int>(selectedType)];
                bool valid = true;

                switch (selectedType) {
                    case ActionType::kWeaponSet: {
                        auto right = PickedFormRef(s_actionEditor.weaponRight);
                        std::optional<FormRef> left;
                        if (!IsTwoHandedWeapon(right)) {
                            left = PickedFormRef(s_actionEditor.weaponLeft);
                        }
                        if (!right && !left) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_right_or_left");
                            valid = false;
                            break;
                        }
                        if (right) {
                            fields["Right"] = right->ToString();
                        }
                        if (left) {
                            fields["Left"] = left->ToString();
                        }
                        if (auto ammo = PickedFormRef(s_actionEditor.weaponAmmo)) {
                            fields["Ammo"] = ammo->ToString();
                        }
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    }
                    case ActionType::kOutfit:
                        if (s_actionEditor.outfitModeIndex == 1) {
                            auto outfit = PickedFormRef(s_actionEditor.outfitFormPicker);
                            if (!outfit) {
                                s_actionEditor.errorMessage = TR("actioneditor.error.pick_outfit_record");
                                valid = false;
                                break;
                            }
                            fields["Outfit"] = outfit->ToString();
                        } else {
                            if (s_actionEditor.outfitItems.empty()) {
                                s_actionEditor.errorMessage = TR("actioneditor.error.add_armor_item");
                                valid = false;
                                break;
                            }
                            fields["Items"] = JoinFormRefs(s_actionEditor.outfitItems);
                            std::string outfitName = s_actionEditor.outfitName;
                            outfitName.erase(std::remove(outfitName.begin(), outfitName.end(), '|'), outfitName.end());
                            if (!outfitName.empty()) {
                                fields["OutfitName"] = outfitName;
                            }
                        }
                        if (s_actionEditor.outfitUnequipEverythingElse) {
                            fields["UnequipAll"] = "1";
                        }
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    case ActionType::kSpell: {
                        auto right = PickedFormRef(s_actionEditor.spellRight);
                        auto left = PickedFormRef(s_actionEditor.spellLeft);
                        if (!right && !left) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_spell");
                            valid = false;
                            break;
                        }
                        if (right) {
                            fields["Right"] = right->ToString();
                        }
                        if (left) {
                            fields["Left"] = left->ToString();
                        }
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    }
                    case ActionType::kShout: {
                        auto shout = PickedFormRef(s_actionEditor.shoutPicker);
                        if (!shout) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_shout");
                            valid = false;
                            break;
                        }
                        fields["Form"] = shout->ToString();
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    }
                    case ActionType::kConsumable: {
                        if (s_actionEditor.consumeRandom) {
                            fields["ConsumeRandom"] = "1";
                            fields["RandomKind"] =
                                std::string(ToString(s_actionEditor.consumeRandomKindIndex == 1 ? ConsumableRandomKind::kDrink
                                                                                                 : ConsumableRandomKind::kFood));
                            break;
                        }
                        auto item = PickedFormRef(s_actionEditor.consumablePicker);
                        if (!item) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_item");
                            valid = false;
                            break;
                        }
                        fields["Form"] = item->ToString();
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    }
                    case ActionType::kAmmoSwap: {
                        auto ammo = PickedFormRef(s_actionEditor.ammoPicker);
                        if (!ammo) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_ammo");
                            valid = false;
                            break;
                        }
                        fields["Form"] = ammo->ToString();
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                            fields["AddCount"] = std::to_string(s_actionEditor.ammoAddCount);
                        }
                        break;
                    }
                    case ActionType::kToggleTorch: {
                        auto torch = PickedFormRef(s_actionEditor.torchPicker);
                        if (!torch) {
                            s_actionEditor.errorMessage = TR("actioneditor.error.pick_torch");
                            valid = false;
                            break;
                        }
                        fields["Form"] = torch->ToString();
                        if (s_actionEditor.addIfMissing) {
                            fields["AddIfMissing"] = "1";
                        }
                        break;
                    }
                    case ActionType::kTogglePOV:
                        break;
                    case ActionType::kReadySheath:
                    case ActionType::kToggleSneak:
                    case ActionType::kToggleAutoMove:
                    case ActionType::kJump:
                    case ActionType::kToggleFreeCam:
                    case ActionType::kToggleFreeCamPaused:
                    case ActionType::kToggleSprint:
                    case ActionType::kQuickSave:
                    case ActionType::kQuickLoad:
                    case ActionType::kToggleMenus:
                        break;
                    case ActionType::kRechargeWeapon:
                    case ActionType::kRechargeWeaponLeftHand:
                        fields["PreferSmaller"] = s_actionEditor.rechargePreferSmaller ? "1" : "0";
                        fields["MaxSize"] = std::to_string(s_actionEditor.rechargeMaxSize);
                        fields["Notify"] = s_actionEditor.rechargeNotify ? "1" : "0";
                        break;
                    case ActionType::kMovement:
                        fields["Direction"] = kMovementDirectionNames[s_actionEditor.movementDirectionIndex];
                        break;
                    case ActionType::kOpenMenu:
                        fields["Target"] = kOpenMenuTargetNames[s_actionEditor.openMenuTargetIndex];
                        break;
                    case ActionType::kPanic: {
                        if (!s_actionEditor.unequipItems.empty()) {
                            fields["Items"] = JoinFormRefs(s_actionEditor.unequipItems);
                        } else {
                            std::string categories;
                            if (s_actionEditor.panicWeapons) categories += "Weapons,";
                            if (s_actionEditor.panicSpells) categories += "Spells,";
                            if (s_actionEditor.panicArmor) categories += "Armor,";
                            if (s_actionEditor.panicShouts) categories += "Shouts,";
                            if (s_actionEditor.panicAmmo) categories += "Ammo,";
                            if (!categories.empty()) {
                                categories.pop_back();
                            }
                            fields["Categories"] = categories;
                        }
                        break;
                    }
                }

                if (valid) {
                    auto action = DeserializeAction(fields);
                    if (!action) {
                        s_actionEditor.errorMessage = TR("actioneditor.error.couldnt_build_action");
                    } else {
                        auto* manager = HotkeyManager::GetSingleton();
                        if (s_actionEditor.editingExisting && s_actionEditor.editingOriginalType && *s_actionEditor.editingOriginalType != selectedType) {
                            manager->ClearBindAction(s_actionEditor.targetKey, *s_actionEditor.editingOriginalType);
                        }
                        manager->AddOrUpdateAction(s_actionEditor.targetKey, std::move(action));
                        MaybeSaveActiveProfile(manager);
                        if (selectedType == ActionType::kAmmoSwap && s_actionEditor.addIfMissing) {
                            auto settings = manager->GetSettings();
                            if (settings.lastAmmoAddCount != s_actionEditor.ammoAddCount) {
                                settings.lastAmmoAddCount = s_actionEditor.ammoAddCount;
                                manager->SetSettings(settings);
                            }
                        }
                        CloseEditors();
                        return;
                    }
                }
            }
            SameLine();
            if (Button(TR("common.cancel_edit"))) {
                CloseEditors();
            }
        }

        void __stdcall RenderSettingsTab() {
            auto* manager = HotkeyManager::GetSingleton();
            Settings settings = manager->GetSettings();
            bool settingsDirty = false;

            bool enabled = settings.enabled;
            if (ImGuiMCPComponents::ToggleButton(TRID("settings.enabled.label", "##GlobalEnable").c_str(), &enabled)) {
                settings.enabled = enabled;
                settingsDirty = true;
            }
            DescText(TR("settings.enabled.tooltip"));

            Separator();

            if (Checkbox(TR("settings.toggle_unequip"), &settings.toggleUnequip)) {
                settingsDirty = true;
            }
            if (Checkbox(TR("settings.notify_on_trigger"), &settings.notifyOnTrigger)) {
                settingsDirty = true;
            }
            if (Checkbox(TR("settings.auto_scroll_to_bottom"), &settings.autoScrollToBottom)) {
                settingsDirty = true;
            }
            DescText(TR("settings.auto_scroll_to_bottom.tooltip"));
            if (Checkbox(TR("settings.confirm_saves_and_deletes"), &settings.confirmSavesAndDeletes)) {
                settingsDirty = true;
            }
            DescText(TR("settings.confirm_saves_and_deletes.tooltip"));
            if (Checkbox(TR("settings.auto_save_profile_changes"), &settings.autoSaveProfileChanges)) {
                settingsDirty = true;
            }
            DescText(TR("settings.auto_save_profile_changes.tooltip"));

            if (Checkbox(TR("settings.allow_hotkeys_in_game_menus"), &settings.allowHotkeysInGameMenus)) {
                settingsDirty = true;
            }
            DescText(TR("settings.allow_hotkeys_in_game_menus.tooltip"));

            DescText(TR("settings.block_menus.header"));
            if (Checkbox(TRID("settings.block_menus.inventory", "##BlockMenu").c_str(), &settings.blockHotkeysInInventoryMenu)) {
                settingsDirty = true;
            }
            SameLine();
            if (Checkbox(TRID("settings.block_menus.spells", "##BlockMenu").c_str(), &settings.blockHotkeysInMagicMenu)) {
                settingsDirty = true;
            }
            SameLine();
            if (Checkbox(TRID("settings.block_menus.map", "##BlockMenu").c_str(), &settings.blockHotkeysInMapMenu)) {
                settingsDirty = true;
            }
            SameLine();
            if (Checkbox(TRID("settings.block_menus.skills", "##BlockMenu").c_str(), &settings.blockHotkeysInStatsMenu)) {
                settingsDirty = true;
            }
            DescText(TR("settings.block_menus.tooltip"));


            SeparatorText(TR("settings.modifier_key.header"));
            if (Checkbox(TR("settings.modifier_blocks_vanilla_hotkey"), &settings.modifierBlocksVanillaHotkey)) {
                settingsDirty = true;
            }
            DescText(TR("settings.modifier_blocks_vanilla_hotkey.tooltip"));

            SeparatorText(TR("settings.gamepad_plus_plus.header"));
            if (Checkbox(TR("settings.gamepad_plus_plus_compat"), &settings.gamepadPlusPlusCompat)) {
                settingsDirty = true;
            }
            DescText(TR("settings.gamepad_plus_plus_compat.tooltip"));

            auto* locale = Locale::GetSingleton();
            SeparatorText(TR("settings.language.label"));
            if (BeginCombo("##Language", locale->GetCurrentLanguage().c_str())) {
                for (const auto& language : locale->ListLanguages()) {
                    bool isCurrent = language == locale->GetCurrentLanguage();
                    if (Selectable(language.c_str(), isCurrent) && !isCurrent) {
                        if (locale->SetLanguage(language)) {
                            settings.language = language;
                            settingsDirty = true;
                        }
                    }
                }
                EndCombo();
            }
            DescText(TR("settings.language.tooltip"));

            if (settingsDirty) {
                manager->SetSettings(settings);
            }
        }


        void __stdcall RenderKeyBindsTab() {
            auto* manager = HotkeyManager::GetSingleton();
            Settings settings = manager->GetSettings();
            bool settingsDirty = false;

            Text(TR("keybinds.modifier_key"), KeyName(settings.modifierKeyCode).c_str());
            SameLine();
            PushID("ModifierKeyCapture");
            if (RenderCaptureButton(CaptureTarget::kModifierKey, settings.modifierKeyCode) == CaptureResult::kCaptured) {
                settingsDirty = true;
            }
            PopID();

            Text(TR("keybinds.gamepad_modifier"), KeyName(settings.modifierGamepadCode).c_str());
            SameLine();
            PushID("GamepadModifierCapture");
            if (RenderCaptureButton(CaptureTarget::kGamepadModifier, settings.modifierGamepadCode) == CaptureResult::kCaptured) {
                settingsDirty = true;
            }
            PopID();

            if (SliderFloat(TR("keybinds.hold_threshold"), &settings.holdThresholdSeconds, 0.1f, 2.0f, "%.2f")) {
                settingsDirty = true;
            }
            float boundTableWidth = GetItemRectSize().x;
            SameLine();
            HelpMarker(TR("keybinds.hold_threshold.tooltip"));

            if (settingsDirty) {
                manager->SetSettings(settings);
            }

            Separator();
            Spacing();
            Spacing();

            auto summaries = manager->GetBindSummaries();

            float capturedKeyWidth = settings.keyBindsKeyColumnWidth;
            float capturedActionWidth = settings.keyBindsActionColumnWidth;
            float capturedBlockVanillaWidth = settings.keyBindsBlockVanillaColumnWidth;
            float capturedEnabledWidth = settings.keyBindsEnabledColumnWidth;

            int keyColumnFlags = ImGuiTableColumnFlags_None;
            int actionColumnFlags = ImGuiTableColumnFlags_None;
            if (settings.keyBindsSortColumn == 0) {
                keyColumnFlags |= ImGuiTableColumnFlags_DefaultSort;
                if (!settings.keyBindsSortAscending) {
                    keyColumnFlags |= ImGuiTableColumnFlags_PreferSortDescending;
                }
            } else if (settings.keyBindsSortColumn == 1) {
                actionColumnFlags |= ImGuiTableColumnFlags_DefaultSort;
                if (!settings.keyBindsSortAscending) {
                    actionColumnFlags |= ImGuiTableColumnFlags_PreferSortDescending;
                }
            }

            if (BeginTable("KeyBindsTable", 4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit |
                                ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate,
                            ImVec2{boundTableWidth, 0.0f})) {
                TableSetupColumn(TR("keybinds.table.key_column"), keyColumnFlags, settings.keyBindsKeyColumnWidth);
                TableSetupColumn(TR("keybinds.table.action_column"), actionColumnFlags,
                                  settings.keyBindsActionColumnWidth > 0.0f ? settings.keyBindsActionColumnWidth : 260.0f);
                TableSetupColumn(TR("keybinds.table.block_vanilla_column"), ImGuiTableColumnFlags_NoSort,
                                  settings.keyBindsBlockVanillaColumnWidth);
                TableSetupColumn(TR("keybinds.table.enabled_column"), ImGuiTableColumnFlags_NoSort, settings.keyBindsEnabledColumnWidth);
                TableHeadersRow();

                auto* sortSpecs = TableGetSortSpecs();

                if (sortSpecs && sortSpecs->SpecsDirty) {
                    int newColumn = (sortSpecs->SpecsCount > 0) ? static_cast<int>(sortSpecs->Specs[0].ColumnIndex) : -1;
                    bool newAscending = (sortSpecs->SpecsCount == 0) || (sortSpecs->Specs[0].SortDirection != ImGuiSortDirection_Descending);
                    if (newColumn != settings.keyBindsSortColumn || newAscending != settings.keyBindsSortAscending) {
                        settings.keyBindsSortColumn = newColumn;
                        settings.keyBindsSortAscending = newAscending;
                        manager->SetSettings(settings);
                    }
                    sortSpecs->SpecsDirty = false;
                }

                if (sortSpecs && sortSpecs->SpecsCount > 0) {
                    const auto& spec = sortSpecs->Specs[0];
                    bool ascending = spec.SortDirection != ImGuiSortDirection_Descending;
                    if (spec.ColumnIndex == 0) {
                        auto keyLess = [](const BindKey& a_lhs, const BindKey& a_rhs) {
                            if (a_lhs.idCode != a_rhs.idCode) return a_lhs.idCode < a_rhs.idCode;
                            if (a_lhs.modifierHeld != a_rhs.modifierHeld) return !a_lhs.modifierHeld;
                            return a_lhs.press < a_rhs.press;
                        };
                        std::sort(summaries.begin(), summaries.end(), [&](const BindSummary& a_lhs, const BindSummary& a_rhs) {
                            return ascending ? keyLess(a_lhs.key, a_rhs.key) : keyLess(a_rhs.key, a_lhs.key);
                        });
                    } else if (spec.ColumnIndex == 1) {
                        auto actionKey = [](const BindSummary& a_summary) -> std::string_view {
                            return a_summary.actions.empty() ? std::string_view{}
                                                              : std::string_view{ActionTypeDisplayName(a_summary.actions.front().type)};
                        };
                        std::sort(summaries.begin(), summaries.end(), [&](const BindSummary& a_lhs, const BindSummary& a_rhs) {
                            return ascending ? actionKey(a_lhs) < actionKey(a_rhs) : actionKey(a_rhs) < actionKey(a_lhs);
                        });
                    }
                }

                for (const auto& summary : summaries) {
                    TableNextRow();
                    PushID(static_cast<int>(summary.key.idCode * 4 + (summary.key.modifierHeld ? 2u : 0u) + (summary.key.press == PressType::kHold ? 1u : 0u)));

                    TableNextColumn();
                    Text("%s", BindKeyLabel(summary.key, settings.modifierKeyCode, settings.modifierGamepadCode).c_str());
                    bool isThisRowEditing = s_keyEditor.active && s_keyEditor.key && *s_keyEditor.key == summary.key;
                    if (SmallButton(isThisRowEditing ? TR("keybinds.row.cancel_edit") : TR("common.edit"))) {
                        if (isThisRowEditing) {
                            CloseEditors();
                        } else {
                            OpenKeyEditorForRow(summary);
                        }
                    }
                    SameLine();
                    if (SmallButton(TR("common.delete"))) {
                        bool wasEditingThisRow = (s_keyEditor.active && s_keyEditor.key && *s_keyEditor.key == summary.key) ||
                                                  (s_actionEditor.active && s_actionEditor.targetKey == summary.key);
                        BindKey deleteKey = summary.key;
                        RequestConfirm(TF("keybinds.delete_bind_confirm",
                                           BindKeyLabel(summary.key, settings.modifierKeyCode, settings.modifierGamepadCode)),
                                       TR("common.delete"), [manager, deleteKey, wasEditingThisRow]() {
                                           manager->RemoveBind(deleteKey);
                                           MaybeSaveActiveProfile(manager);
                                           if (wasEditingThisRow) {
                                               CloseEditors();
                                           }
                                       });
                    }

                    TableNextColumn();

                    bool anyCopyToActiveOnThisRow = s_copyTo.active && s_copyTo.sourceKey == summary.key;
                    std::vector<ActionType> rowExistingTypes;
                    rowExistingTypes.reserve(summary.actions.size());
                    for (const auto& action : summary.actions) {
                        rowExistingTypes.push_back(action.type);
                    }
                    bool canAddMore = !AllowedNewActionTypes(rowExistingTypes).empty();
                    bool isAddingToThisRow = s_actionEditor.active && s_actionEditor.targetKey == summary.key && !s_actionEditor.editingExisting;
                    bool showAdd = !anyCopyToActiveOnThisRow && (canAddMore || isAddingToThisRow);
                    if (showAdd) {
                        if (SmallButton(isAddingToThisRow ? TR("keybinds.row.cancel_add") : TR("keybinds.row.add"))) {
                            if (isAddingToThisRow) {
                                CloseEditors();
                            } else {
                                OpenActionEditorForAdd(summary);
                            }
                        }
                    }

                    if (summary.actions.empty()) {
                        DescText(TR("keybinds.row.no_actions"));
                    } else {
                        for (std::size_t actionIndex = 0; actionIndex < summary.actions.size(); ++actionIndex) {
                            const auto& action = summary.actions[actionIndex];
                            PushID(static_cast<int>(action.type));
                            TextWrapped("%s", action.displayName.c_str());
                            bool isThisActionEditing = s_actionEditor.active && s_actionEditor.targetKey == summary.key &&
                                                        s_actionEditor.editingExisting && s_actionEditor.editingOriginalType == action.type;
                            bool isThisActionCopying = s_copyTo.active && s_copyTo.sourceKey == summary.key &&
                                                        s_copyTo.sourceType == action.type;

                            if (isThisActionCopying) {
                                std::vector<BindKey> copyTargets;
                                std::vector<std::string> copyTargetLabels;
                                for (const auto& other : summaries) {
                                    if (other.key == summary.key) {
                                        continue;
                                    }
                                    copyTargets.push_back(other.key);
                                    copyTargetLabels.push_back(BindKeyLabel(other.key, settings.modifierKeyCode, settings.modifierGamepadCode));
                                }

                                if (copyTargets.empty()) {
                                    DescText(TR("keybinds.row.no_other_binds"));
                                    SameLine();
                                    if (SmallButton(TR("common.cancel"))) {
                                        CloseEditors();
                                    }
                                } else {
                                    if (s_copyTo.selectedTargetIndex < 0 ||
                                        s_copyTo.selectedTargetIndex >= static_cast<int>(copyTargets.size())) {
                                        s_copyTo.selectedTargetIndex = 0;
                                    }
                                    std::vector<const char*> copyTargetItems;
                                    copyTargetItems.reserve(copyTargetLabels.size());
                                    for (const auto& label : copyTargetLabels) {
                                        copyTargetItems.push_back(label.c_str());
                                    }
                                    SetNextItemWidth(160.0f);
                                    RenderTypeaheadCombo(TR("keybinds.row.copy_to"), s_copyTo.selectedTargetIndex, copyTargetItems.data(),
                                                          static_cast<int>(copyTargetItems.size()), s_copyTo.typeahead);
                                    SameLine();
                                    if (SmallButton(TR("common.ok"))) {
                                        auto fields = ParseFieldString(action.serialized);
                                        if (auto cloned = DeserializeAction(fields)) {
                                            manager->AddOrUpdateAction(copyTargets[s_copyTo.selectedTargetIndex], std::move(cloned));
                                            MaybeSaveActiveProfile(manager);
                                        }
                                        CloseEditors();
                                    }
                                    SameLine();
                                    if (SmallButton(TR("common.cancel"))) {
                                        CloseEditors();
                                    }
                                }
                            } else {
                                if (SmallButton(TR("common.edit"))) {
                                    if (isThisActionEditing) {
                                        CloseEditors();
                                    } else {
                                        OpenActionEditorForEdit(summary, action);
                                    }
                                }
                                SameLine();
                                if (SmallButton(TR("common.delete"))) {
                                    CloseEditors();
                                    BindKey deleteKey = summary.key;
                                    ActionType deleteType = action.type;
                                    RequestConfirm(TF("keybinds.delete_action_confirm", action.displayName,
                                                       BindKeyLabel(summary.key, settings.modifierKeyCode, settings.modifierGamepadCode)),
                                                   TR("common.delete"), [manager, deleteKey, deleteType]() {
                                                       manager->ClearBindAction(deleteKey, deleteType);
                                                       MaybeSaveActiveProfile(manager);
                                                   });
                                }
                                SameLine();
                                if (SmallButton(TR("keybinds.row.copy_to"))) {
                                    OpenCopyTo(summary.key, action.type);
                                }
                                if (summary.actions.size() > 1) {
                                    SameLine();
                                    BeginDisabled(actionIndex == 0);
                                    if (SmallButton(TR("common.up"))) {
                                        manager->MoveAction(summary.key, action.type, true);
                                        MaybeSaveActiveProfile(manager);
                                    }
                                    EndDisabled();
                                    SameLine();
                                    BeginDisabled(actionIndex + 1 == summary.actions.size());
                                    if (SmallButton(TR("common.down"))) {
                                        manager->MoveAction(summary.key, action.type, false);
                                        MaybeSaveActiveProfile(manager);
                                    }
                                    EndDisabled();
                                }
                            }
                            PopID();
                        }
                    }

                    TableNextColumn();
                    bool modifierForced = summary.key.modifierHeld;
                    bool rowBlockVanilla = modifierForced ? true : summary.blockVanillaKey;

                    ImVec2 toggleMin = GetCursorScreenPos();
                    float toggleHeight = GetFrameHeight();
                    ImVec2 toggleMax{toggleMin.x + toggleHeight * 1.8f, toggleMin.y + toggleHeight};

                    BeginDisabled(modifierForced);
                    if (ImGuiMCPComponents::ToggleButton("##RowBlockVanilla", &rowBlockVanilla)) {
                        manager->SetBindBlockVanillaKey(summary.key, rowBlockVanilla);
                        MaybeSaveActiveProfile(manager);
                    }
                    EndDisabled();

                    if (modifierForced) {
                        ImDrawListManager::AddRectFilled(GetWindowDrawList(), toggleMin, toggleMax, IM_COL32(30, 30, 30, 150),
                                                          toggleHeight * 0.5f, 0);
                        if (IsMouseHoveringRect(toggleMin, toggleMax)) {
                            SetTooltip("%s", TR("keybinds.row.modifier_locked_tooltip"));
                        }
                    }

                    TableNextColumn();
                    bool rowEnabled = summary.enabled;
                    if (ImGuiMCPComponents::ToggleButton("##RowEnabled", &rowEnabled)) {
                        manager->SetBindEnabled(summary.key, rowEnabled);
                        MaybeSaveActiveProfile(manager);
                    }

                    PopID();
                }

                if (auto* currentTable = GetCurrentTable()) {
                    if (currentTable->Columns.Data && (currentTable->Columns.DataEnd - currentTable->Columns.Data) >= 4) {
                        capturedKeyWidth = currentTable->Columns.Data[0].WidthGiven;
                        capturedActionWidth = currentTable->Columns.Data[1].WidthGiven;
                        capturedBlockVanillaWidth = currentTable->Columns.Data[2].WidthGiven;
                        capturedEnabledWidth = currentTable->Columns.Data[3].WidthGiven;
                    }
                }

                EndTable();
            }

            if (IsMouseReleased(ImGuiMouseButton_Left)) {
                constexpr float kEpsilon = 0.5f;
                bool widthsChanged = std::abs(capturedKeyWidth - settings.keyBindsKeyColumnWidth) > kEpsilon ||
                                      std::abs(capturedActionWidth - settings.keyBindsActionColumnWidth) > kEpsilon ||
                                      std::abs(capturedBlockVanillaWidth - settings.keyBindsBlockVanillaColumnWidth) > kEpsilon ||
                                      std::abs(capturedEnabledWidth - settings.keyBindsEnabledColumnWidth) > kEpsilon;
                if (widthsChanged) {
                    settings.keyBindsKeyColumnWidth = capturedKeyWidth;
                    settings.keyBindsActionColumnWidth = capturedActionWidth;
                    settings.keyBindsBlockVanillaColumnWidth = capturedBlockVanillaWidth;
                    settings.keyBindsEnabledColumnWidth = capturedEnabledWidth;
                    manager->SetSettings(settings);
                }
            }

            RenderConfirmModal();

            Separator();

            bool keyEditorJustOpened = settings.autoScrollToBottom && s_keyEditor.active && !s_keyEditorWasActive;
            bool actionEditorJustOpened = settings.autoScrollToBottom && s_actionEditor.active && !s_actionEditorWasActive;

            if (!s_keyEditor.active && !s_actionEditor.active) {
                if (Button(TR("keybinds.new_key_bind"))) {
                    CloseEditors();
                    s_keyEditor = KeyEditState{
                        .active = true,
                        .isNewKey = true,
                    };
                    InputHandler::GetSingleton()->BeginCapture();
                    s_captureTarget = CaptureTarget::kBindKey;
                }
            } else if (s_keyEditor.active) {
                RenderKeyEditor();
                if (keyEditorJustOpened) {
                    SetScrollHereY(1.0f);
                }
            } else {
                RenderActionEditor();
                if (actionEditorJustOpened) {
                    SetScrollHereY(1.0f);
                }
            }

            s_keyEditorWasActive = s_keyEditor.active;
            s_actionEditorWasActive = s_actionEditor.active;
        }


        void __stdcall RenderProfilesTab() {
            auto* manager = HotkeyManager::GetSingleton();
            auto profiles = manager->ListProfiles();
            std::string active = manager->GetActiveProfileName();

            Text(TR("profiles.active_profile"), active.c_str());
            Separator();

            float profileListWidth = GetContentRegionAvail().x * 0.5f;
            for (const auto& name : profiles) {
                PushID(name.c_str());
                if (Selectable(name.c_str(), name == active, 0, ImVec2{profileListWidth, 0.0f})) {
                    manager->LoadProfileByName(name);
                }
                PopID();
            }
            if (profiles.empty()) {
                DescText(TR("profiles.no_profiles_yet"));
            }

            Separator();

            if (Button(TR("common.save"))) {
                RequestConfirm(TF("profiles.overwrite_confirm", active), TR("common.overwrite"), [manager]() { manager->SaveActiveProfile(); });
            }
            SameLine();
            if (Button(TR("common.delete"))) {
                manager->DeleteProfile(active);
            }

            Separator();

            static char newProfileName[64] = "";
            SetNextItemWidth(CalcItemWidth() * 0.5f);
            InputText(TR("profiles.new_profile_name"), newProfileName, sizeof(newProfileName));
            SameLine();
            if (Button(TR("profiles.save_as_new")) && newProfileName[0] != '\0') {
                std::string requestedName = newProfileName;
                bool alreadyExists = std::find(profiles.begin(), profiles.end(), requestedName) != profiles.end();
                if (alreadyExists) {
                    RequestConfirm(TF("profiles.save_as_new_overwrite_confirm", requestedName), TR("common.overwrite"),
                                   [manager, requestedName]() { manager->SaveActiveProfileAs(requestedName); });
                } else {
                    manager->SaveActiveProfileAs(requestedName);
                }
                newProfileName[0] = '\0';
            }

            RenderConfirmModal();

            Separator();

            std::uint32_t profileCycleKeyCode = manager->GetProfileCycleKeyCode();
            bool profileCycleRequiresModifier = manager->GetProfileCycleRequiresModifier();
            std::string profileCycleKeyLabel = KeyName(profileCycleKeyCode);
            if (profileCycleKeyCode != 0 && profileCycleRequiresModifier) {
                std::uint32_t globalModifierKeyCode = manager->GetSettings().modifierKeyCode;
                if (globalModifierKeyCode != 0) {
                    profileCycleKeyLabel = std::format("{} + {}", KeyName(globalModifierKeyCode), profileCycleKeyLabel);
                }
            }
            Text(TR("profiles.profile_cycle_key"), profileCycleKeyLabel.c_str());
            SameLine();
            PushID("ProfileCycleKeyCapture");
            std::uint32_t capturedCycleKey = profileCycleKeyCode;
            bool capturedCycleRequiresModifier = profileCycleRequiresModifier;
            if (RenderCaptureButton(CaptureTarget::kProfileCycleKey, capturedCycleKey, &capturedCycleRequiresModifier) == CaptureResult::kCaptured) {
                SetProfileCycleKeyWithConfirm(manager, capturedCycleKey, capturedCycleRequiresModifier);
            }
            if (profileCycleKeyCode != 0 && s_captureTarget != CaptureTarget::kProfileCycleKey) {
                SameLine();
                if (Button(TR("common.unbind"))) {
                    manager->SetProfileCycleKey(0, false);
                    MaybeSaveActiveProfile(manager);
                }
            }
            PopID();
            if (Checkbox(TR("keybinds.requires_modifier"), &profileCycleRequiresModifier)) {
                SetProfileCycleKeyWithConfirm(manager, profileCycleKeyCode, profileCycleRequiresModifier);
            }
            SameLine();
            Settings globalSettings = manager->GetSettings();
            bool isDefault = profileCycleKeyCode != 0 && globalSettings.defaultProfileCycleKeyCode == profileCycleKeyCode &&
                              globalSettings.defaultProfileCycleRequiresModifier == profileCycleRequiresModifier;
            if (Checkbox(TR("profiles.make_default"), &isDefault)) {
                if (isDefault) {
                    globalSettings.defaultProfileCycleKeyCode = profileCycleKeyCode;
                    globalSettings.defaultProfileCycleRequiresModifier = profileCycleRequiresModifier;
                } else {
                    globalSettings.defaultProfileCycleKeyCode = 0;
                    globalSettings.defaultProfileCycleRequiresModifier = false;
                }
                manager->SetSettings(globalSettings);
            }
            DescText(TR("profiles.cycle_tooltip"));
            DescText(TR("profiles.make_default_tooltip"));
        }
    }

    void ConfigUI::Install() {
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("True Hotkeys: SKSE Menu Framework not installed - profiles can still be hand-edited as INI files.");
            return;
        }

        SKSEMenuFramework::SetSection("True Hotkeys");
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettingsTab);
        SKSEMenuFramework::AddSectionItem("Key Binds", RenderKeyBindsTab);
        SKSEMenuFramework::AddSectionItem("Profiles", RenderProfilesTab);

        SKSEMenuFramework::AddInputEvent(OnFrameworkInputEvent);

        SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0f);

        SKSE::log::info("True Hotkeys: config UI registered with SKSE Menu Framework.");
    }

    bool ConfigUI::IsMenuOpen() { return s_menuOpen; }
}
