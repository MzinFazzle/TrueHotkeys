#include "Hotkeys/Locale.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <utility>

namespace Hotkeys {
    namespace {
        constexpr std::string_view kDataRelativeRoot = "Data/SKSE/Plugins/TrueHotkeys";

        constexpr std::pair<std::string_view, std::string_view> kDefaultStrings[] = {
            {"settings.enabled.label", "Enabled"},
            {"settings.enabled.tooltip", "Master switch. Turn off to completely disable the mod."},
            {"settings.toggle_unequip", "Toggle unequip (press bind again to unequip)"},
            {"settings.notify_on_trigger", "Notify on activation"},
            {"settings.auto_scroll_to_bottom", "Auto Scroll to bottom"},
            {"settings.auto_scroll_to_bottom.tooltip", "Scrolls the Editing Bind/Action panel into view the moment it opens."},
            {"settings.confirm_saves_and_deletes", "Confirm saves and deletes"},
            {"settings.confirm_saves_and_deletes.tooltip",
             "Shows a confirmation dialog before deleting a bind/action or overwriting a saved profile."},
            {"settings.auto_save_profile_changes", "Automatically save profile changes"},
            {"settings.auto_save_profile_changes.tooltip",
             "Off leaves bind/action changes in memory only, until you click Save on the Profiles tab."},
            {"settings.allow_hotkeys_in_game_menus", "Allow hotkeys to function in game menus"},
            {"settings.allow_hotkeys_in_game_menus.tooltip",
             "Only affects Favorites, Container, Barter, Journal, and Book menus - other menus always block hotkeys."},
            {"settings.block_menus.header", "Block hotkeys while these menus are open:"},
            {"settings.block_menus.inventory", "Inventory"},
            {"settings.block_menus.spells", "Spells"},
            {"settings.block_menus.map", "Map"},
            {"settings.block_menus.skills", "Skills"},
            {"settings.block_menus.tooltip",
             "Also only matters with something like Skyrim Souls - lets an Open Menu bind switch to/from whichever of these "
             "you leave unchecked while a different one is already open."},
            {"settings.modifier_key.header", "Modifier Key"},
            {"settings.modifier_blocks_vanilla_hotkey", "Modifier blocks vanilla hotkey"},
            {"settings.modifier_blocks_vanilla_hotkey.tooltip",
             "When checked, pressing your modifier key/button on its own blocks whatever vanilla action (or other mod's "
             "menu) it's normally bound to, so it never leaks through while being held as a modifier here. Still lets menus "
             "that want the button for themselves (like the Wait/Sleep duration slider on RB/LB) use it normally."},
            {"settings.language.label", "Language"},
            {"settings.language.tooltip", "Changes the language of this config UI. Drop a new Languages/<Name>.ini file next "
                                           "to English.ini to add one - see English.ini itself for the format."},

            {"common.cancel", "Cancel"},
            {"common.ok", "Ok"},
            {"common.edit", "Edit"},
            {"common.delete", "Delete"},
            {"common.up", "Up"},
            {"common.down", "Down"},
            {"common.continue", "Continue"},

            {"keybinds.modifier_key", "Modifier Key: %s"},
            {"keybinds.gamepad_modifier", "Gamepad Modifier: %s"},
            {"keybinds.hold_threshold", "Hold Threshold (seconds)"},
            {"keybinds.hold_threshold.tooltip", "Hold a key for this long before the action(s) activate."},
            {"keybinds.table.key_column", "Key"},
            {"keybinds.table.action_column", "Action"},
            {"keybinds.table.block_vanilla_column", "Block vanilla hotkey"},
            {"keybinds.table.enabled_column", "Enabled"},
            {"keybinds.row.cancel_edit", "Cancel Edit"},
            {"keybinds.row.add", "Add"},
            {"keybinds.row.cancel_add", "Cancel Add"},
            {"keybinds.row.no_actions", "(no actions)"},
            {"keybinds.row.no_other_binds", "No other key binds to copy to."},
            {"keybinds.row.copy_to", "Copy To"},
            {"keybinds.row.modifier_locked_tooltip", "Only toggleable if not using a modifier key."},
            {"keybinds.new_key_bind", "+ New Key Bind"},
            {"keybinds.editing_bind", "Editing Bind"},
            {"keybinds.remap", "Remap"},
            {"keybinds.key_label", "Key: %s"},
            {"keybinds.key_label_none", "(none)"},
            {"keybinds.key_capture_label", "Key:"},
            {"keybinds.key_collision_confirm",
             "Key bind already exists. Proceeding will clear the actions from this key bind. Continue?"},
            {"keybinds.press", "Press"},
            {"keybinds.press_tap", "Tap"},
            {"keybinds.press_hold", "Hold"},
            {"keybinds.requires_modifier", "Requires modifier"},

            {"confirm_modal.title", "Confirm?"},

            {"picker.plugin", "Plugin"},
            {"picker.spell_sort", "Spell Sort"},
            {"picker.nothing_matching_inventory", "Nothing matching in your inventory."},
            {"picker.no_matching_forms_in_plugin", "No matching forms in this plugin."},
            {"picker.no_plugins_have_forms", "No plugins have any forms of this type."},
            {"picker.no_plugins_match_filter", "No plugins match this filter."},
            {"picker.no_forms_match_filter", "No forms match this filter."},

            {"common.remove", "Remove"},
            {"common.save_bind", "Save Bind"},
            {"common.cancel_edit", "Cancel Edit"},
            {"common.add", "Add"},

            {"actioneditor.editing_action", "Editing Action"},
            {"actioneditor.from_plugins", "From Plugins"},
            {"actioneditor.add_if_missing", "Add if missing"},
            {"actioneditor.use_shield", "Use Shield"},
            {"actioneditor.add_equipped_weapons", "Add Equipped Weapons"},
            {"actioneditor.add_equipped_spells", "Add Equipped Spells"},
            {"actioneditor.add_equipped_shouts", "Add Equipped Shouts"},
            {"actioneditor.outfit_unequip_everything_else", "Unequip everything else"},
            {"actioneditor.no_action_types_available", "No action types available - this bind already has one of everything it can."},
            {"actioneditor.right_hand_optional", "Right Hand (optional)"},
            {"actioneditor.left_hand_optional", "Left Hand (optional)"},
            {"actioneditor.ammo_optional", "Ammo (optional)"},
            {"actioneditor.weaponset.left_disabled_two_handed", "(disabled - Right Hand is two-handed)"},

            {"actioneditor.outfit.source", "Source"},
            {"actioneditor.outfit.mode_individual_items", "Individual Items"},
            {"actioneditor.outfit.mode_outfit_record", "Outfit Record"},
            {"actioneditor.outfit.outfit_record", "Outfit Record"},
            {"actioneditor.outfit.record_tooltip", "Equips everything the outfit record contains."},
            {"actioneditor.outfit.armor_item", "Armor Item"},
            {"actioneditor.outfit.armor_clothing", "Armor/Clothing"},
            {"actioneditor.outfit.add_item", "Add Item"},
            {"actioneditor.outfit.add_worn_items", "Add Worn Items"},
            {"actioneditor.outfit.name_label", "Outfit name:"},
            {"actioneditor.outfit.items_in_outfit", "Items in this outfit:"},

            {"actioneditor.spell.dual_cast_tooltip", "Pick the same spell for both hands to dual-cast it."},
            {"actioneditor.shout.label", "Shout"},
            {"actioneditor.consumable.label", "Item"},
            {"actioneditor.ammoswap.label", "Ammo"},
            {"actioneditor.toggletorch.label", "Torch"},

            {"actioneditor.togglepov.desc", "Switches between 1st and 3rd person view."},
            {"actioneditor.readysheath.desc", "Draws or sheathes your weapon/magic, whichever you aren't currently doing."},
            {"actioneditor.togglesneak.desc", "Toggles sneaking, the same as tapping your Sneak key."},
            {"actioneditor.toggleautomove.desc", "Toggles auto-move, the same as tapping your Auto-Move key."},
            {"actioneditor.jump.desc", "Makes you jump, the same as tapping your Jump key. Doesn't toggle."},
            {"actioneditor.togglefreecam.desc", "Toggles free-flying camera, the same as the \"tfc\" console command."},
            {"actioneditor.togglefreecampaused.desc",
             "Toggles free-flying camera and freezes time, the same as the \"tfc 1\" console command."},
            {"actioneditor.togglesprint.desc",
             "Toggles sprinting on/off persistently instead of requiring it held - handy for gamepads."},
            {"actioneditor.quicksave.desc", "Quick-saves, the same as tapping your Quicksave key. Doesn't toggle."},
            {"actioneditor.quickload.desc", "Quick-loads, the same as tapping your Quickload key. Doesn't toggle."},
            {"actioneditor.togglemenus.desc",
             "Toggles HUD/menu visibility, the same as the \"tm\" console command. Doesn't close any open menu."},

            {"actioneditor.recharge.desc_left",
             "Recharges your left-hand weapon using a soul gem from inventory - for dual-wielding. Does nothing if a "
             "two-handed weapon is equipped (use Recharge Weapon for that). Doesn't toggle."},
            {"actioneditor.recharge.desc_right",
             "Recharges your right-hand weapon (or two-handed weapon) using a soul gem from inventory. Doesn't toggle."},
            {"actioneditor.recharge.prefer_smaller", "Prefer smaller soul gems first"},
            {"actioneditor.recharge.notify", "Notify when recharging"},
            {"actioneditor.recharge.prefer_smaller_tooltip",
             "On reaches for the smallest eligible soul gem you own first; off reaches for the largest."},
            {"actioneditor.recharge.never_use_above", "Never use gems above:"},
            {"actioneditor.recharge.contained_soul_tooltip",
             "Excludes any soul gem whose CONTAINED soul is larger than this, regardless of the setting above - "
             "checked against the soul actually trapped inside a gem, not the gem's own capacity."},
            {"actioneditor.recharge.soul_petty", "Petty"},
            {"actioneditor.recharge.soul_lesser", "Lesser"},
            {"actioneditor.recharge.soul_common", "Common"},
            {"actioneditor.recharge.soul_greater", "Greater"},
            {"actioneditor.recharge.soul_grand", "Grand"},

            {"actioneditor.movement.direction", "Direction"},
            {"actioneditor.movement.forward", "Move Forward"},
            {"actioneditor.movement.backward", "Move Backward"},
            {"actioneditor.movement.strafe_left", "Strafe Left"},
            {"actioneditor.movement.strafe_right", "Strafe Right"},
            {"actioneditor.movement.tooltip",
             "Moves in this direction for as long as the key is held - remaps that key's own movement rather than "
             "adding to it, and ignores the Press setting above (Tap/Hold doesn't apply to Movement)."},

            {"actioneditor.openmenu.inventory", "Inventory"},
            {"actioneditor.openmenu.spells", "Spells"},
            {"actioneditor.openmenu.map", "Map"},
            {"actioneditor.openmenu.skills", "Skills"},
            {"actioneditor.openmenu.favorites", "Favorites"},
            {"actioneditor.openmenu.wait_rest", "Wait/Rest"},
            {"actioneditor.openmenu.tooltip",
             "Opens the selected menu, the same as Skyrim's own hotkey for it - pressing a different Open Menu bind "
             "while one of these is already open switches directly to it."},

            {"actioneditor.panic.unequip_weapons", "Unequip Weapons"},
            {"actioneditor.panic.unequip_spells", "Unequip Spells"},
            {"actioneditor.panic.unequip_armor", "Unequip Armor"},
            {"actioneditor.panic.unequip_shouts", "Unequip Shouts"},
            {"actioneditor.panic.unequip_ammo", "Unequip Ammo"},
            {"actioneditor.panic.disabled_tooltip",
             "(disabled - specific items below take priority, to avoid unequip order conflicts)"},
            {"actioneditor.panic.unequip_worn_item", "Unequip Worn Item"},
            {"actioneditor.panic.items_to_unequip", "Items to unequip (in order):"},

            {"actioneditor.error.pick_right_or_left", "Pick a right-hand or left-hand item first."},
            {"actioneditor.error.pick_outfit_record", "Pick an outfit record first."},
            {"actioneditor.error.add_armor_item", "Add at least one armor item."},
            {"actioneditor.error.pick_spell", "Pick a spell for at least one hand."},
            {"actioneditor.error.pick_shout", "Pick a shout first."},
            {"actioneditor.error.pick_item", "Pick an item first."},
            {"actioneditor.error.pick_ammo", "Pick an ammo type first."},
            {"actioneditor.error.pick_torch", "Pick a torch first."},
            {"actioneditor.error.couldnt_build_action", "Couldn't build that action - double check your selections."},

            {"common.save", "Save"},
            {"common.overwrite", "Overwrite"},
            {"common.unbind", "Unbind"},

            {"profiles.active_profile", "Active Profile: %s"},
            {"profiles.no_profiles_yet", "No profiles yet - create one below."},
            {"profiles.new_profile_name", "New Profile Name"},
            {"profiles.save_as_new", "Save As New"},
            {"profiles.profile_cycle_key", "Profile-Cycle Key: %s"},
            {"profiles.make_default", "Make default"},
            {"profiles.cycle_tooltip", "Cycles through saved profiles without opening this menu."},
            {"profiles.make_default_tooltip",
             "Make default: newly created profiles (via Save As New) start with this key already set."},

            {"capture.press_a_key", "press a key..."},
            {"capture.modifier_plus_ellipsis", "Modifier + ..."},
            {"capture.bind", "Bind"},

            {"picker.filter", "Filter"},
            {"picker.sort", "Sort"},

            {"actioneditor.action_type", "Action Type"},

            {"actiontype.weapon", "Weapon"},
            {"actiontype.ammo", "Ammo"},
            {"actiontype.spell", "Spell"},
            {"actiontype.shout", "Shout"},
            {"actiontype.outfit", "Outfit"},
            {"actiontype.consumable", "Consumable"},
            {"actiontype.unequip", "Unequip"},
            {"actiontype.movement", "Movement"},
            {"actiontype.ready_sheath", "Ready/Sheath"},
            {"actiontype.jump", "Jump"},
            {"actiontype.toggle_sneak", "Toggle Sneak"},
            {"actiontype.toggle_sprint", "Toggle Sprint"},
            {"actiontype.toggle_auto_move", "Toggle Auto Move"},
            {"actiontype.toggle_torch", "Toggle Torch"},
            {"actiontype.toggle_pov", "Toggle POV"},
            {"actiontype.toggle_freecam", "Toggle FreeCam"},
            {"actiontype.toggle_freecam_paused", "Toggle FreeCam (Paused)"},
            {"actiontype.toggle_menus", "Toggle Menus"},
            {"actiontype.open_menu", "Open Menu"},
            {"actiontype.quick_save", "Quick Save"},
            {"actiontype.quick_load", "Quick Load"},
            {"actiontype.recharge_weapon", "Recharge Weapon"},
            {"actiontype.recharge_weapon_left_hand", "Recharge Weapon (Left Hand)"},

            {"common.grants_missing_suffix", " [grants missing]"},
            {"common.unknown", "Unknown"},

            {"actiontype.weapon.label", "Weapon:"},
            {"actiontype.weapon.item_suffix", " {}"},
            {"actiontype.weapon.plus_item_suffix", " + {}"},
            {"actiontype.weapon.left_hand_suffix", " {} (Left Hand)"},
            {"actiontype.weapon.ammo_suffix", " ({})"},

            {"actiontype.outfit.strips_everything_suffix", " (strips everything first)"},
            {"actiontype.outfit.display", "Outfit: {}{}"},
            {"actiontype.outfit.label", "Outfit"},
            {"actiontype.outfit.label_with_name", "Outfit ({})"},
            {"actiontype.outfit.item_count_display", "{}: {} item(s){}"},

            {"actiontype.spell.label", "Spell:"},
            {"actiontype.spell.right_suffix", " R={}"},
            {"actiontype.spell.left_suffix", " L={}"},

            {"actiontype.shout.display", "Shout: {}{}"},
            {"actiontype.consumable.display", "Use: {}{}"},
            {"actiontype.ammo.display", "Ammo: {}{}"},
            {"actiontype.toggle_torch.display", "Toggle Torch: {}{}"},

            {"actiontype.movement.forward", "Movement: Move Forward"},
            {"actiontype.movement.backward", "Movement: Move Backward"},
            {"actiontype.movement.strafe_left", "Movement: Strafe Left"},
            {"actiontype.movement.strafe_right", "Movement: Strafe Right"},
            {"actiontype.movement.label", "Movement"},

            {"actiontype.open_menu.display", "Open Menu: {}"},

            {"actiontype.panic.unequip_display", "Unequip: {}"},
            {"actiontype.panic.nothing_selected", "(nothing selected)"},
            {"actiontype.panic.category_weapons", "Weapons"},
            {"actiontype.panic.category_spells", "Spells"},
            {"actiontype.panic.category_armor", "Armor"},
            {"actiontype.panic.category_shouts", "Shouts"},
            {"actiontype.panic.category_ammo", "Ammo"},

            {"keybinds.press_suffix_hold", " (Hold)"},
            {"keybinds.press_suffix_tap", " (Tap)"},
            {"keybinds.profile_cycle_collision_confirm",
             "This key bind already exists in the current profile. Setting it as the Profile-Cycle hotkey will take priority "
             "over that key bind whenever this exact combination is pressed. Continue?"},
            {"keybinds.rekey_collision_confirm", "Key bind already exists. Proceeding will clear the actions from this key bind. Continue?"},
            {"keybinds.delete_bind_confirm", "Delete the key bind \"{}\" and everything it does? This can't be undone."},
            {"keybinds.delete_action_confirm", "Delete the \"{}\" action from \"{}\"? This can't be undone."},
            {"profiles.overwrite_confirm", "Overwrite the profile \"{}\" with its current key binds? This can't be undone."},
            {"profiles.save_as_new_overwrite_confirm",
             "A profile named \"{}\" already exists. Overwrite it with the current key binds? This can't be undone."},
            {"actiontype.recharge.notify_message", "{} recharged using {}"},
        };

        [[nodiscard]] std::filesystem::path DataRoot() { return std::filesystem::path(kDataRelativeRoot); }

        [[nodiscard]] std::string ReadLanguageFromSettingsFile() {
            std::ifstream file(DataRoot() / "Settings.ini");
            if (!file.is_open()) {
                return {};
            }
            std::string line;
            while (std::getline(file, line)) {
                auto eq = line.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                if (line.substr(0, eq) == "Language") {
                    return line.substr(eq + 1);
                }
            }
            return {};
        }
    }

    Locale* Locale::GetSingleton() {
        static Locale singleton;
        return &singleton;
    }

    std::filesystem::path Locale::LanguagesDirectory() const { return DataRoot() / "Languages"; }

    void Locale::WriteDefaultEnglishFile(const std::filesystem::path& a_path) const {
        std::error_code ec;
        if (a_path.has_parent_path()) {
            std::filesystem::create_directories(a_path.parent_path(), ec);
        }
        std::ofstream file(a_path, std::ios::trunc);
        if (!file.is_open()) {
            return;
        }
        for (const auto& [key, value] : kDefaultStrings) {
            file << key << "=" << value << "\n";
        }
    }

    bool Locale::LoadLanguageFile(const std::filesystem::path& a_path) {
        std::ifstream file(a_path);
        if (!file.is_open()) {
            return false;
        }
        std::unordered_map<std::string, std::string> loaded;
        std::string line;
        while (std::getline(file, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            loaded[line.substr(0, eq)] = line.substr(eq + 1);
        }
        m_strings = std::move(loaded);
        return true;
    }

    void Locale::Initialize(std::string_view a_languageName) {
        auto englishPath = LanguagesDirectory() / "English.ini";
        std::error_code ec;
        if (!std::filesystem::exists(englishPath, ec)) {
            WriteDefaultEnglishFile(englishPath);
        }

        if (a_languageName.empty() || a_languageName == "English") {
            m_strings.clear();
            m_currentLanguage = "English";
            return;
        }

        SetLanguage(a_languageName);
    }

    void Locale::InitializeEarly() { Initialize(ReadLanguageFromSettingsFile()); }

    std::vector<std::string> Locale::ListLanguages() const {
        std::vector<std::string> names;
        std::error_code ec;
        auto dir = LanguagesDirectory();
        if (std::filesystem::exists(dir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                    names.push_back(entry.path().stem().string());
                }
            }
        }
        if (std::find(names.begin(), names.end(), "English") == names.end()) {
            names.push_back("English");
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool Locale::SetLanguage(std::string_view a_languageName) {
        if (a_languageName == "English") {
            m_strings.clear();
            m_currentLanguage = "English";
            return true;
        }
        auto path = LanguagesDirectory() / std::format("{}.ini", a_languageName);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !LoadLanguageFile(path)) {
            return false;
        }
        m_currentLanguage = std::string(a_languageName);
        return true;
    }

    const char* Locale::T(std::string_view a_key) const {
        if (auto it = m_strings.find(std::string(a_key)); it != m_strings.end()) {
            return it->second.c_str();
        }
        for (const auto& [key, value] : kDefaultStrings) {
            if (key == a_key) {
                return value.data();  // string_view into a string-literal-backed constexpr array - always null-terminated
            }
        }
        return a_key.data();  // last resort - see this method's own header comment
    }
}
