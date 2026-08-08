#include "Hotkeys/FormBrowser.h"

#include "Hotkeys/EditorIDLookup.h"

#include <algorithm>
#include <format>
#include <unordered_set>

namespace Hotkeys::FormBrowser {
    namespace {
        template <class T>
        [[nodiscard]] std::optional<std::string> DisplayNameFor(T& a_form) {
            const char* rawName = a_form.GetFullName();
            if (!rawName || rawName[0] == '\0') {
                return std::nullopt;
            }
            return std::string(rawName);
        }

        template <class T, class Predicate>
        [[nodiscard]] FormCatalog BuildCatalog(Predicate a_predicate) {
            FormCatalog catalog;

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return catalog;
            }

            for (auto* form : dataHandler->GetFormArray<T>()) {
                if (!form || !a_predicate(*form)) {
                    continue;
                }
                auto name = DisplayNameFor(*form);
                if (!name) {
                    continue;  // unnamed - skip (see DisplayNameFor's comment)
                }
                auto* file = form->GetFile(0);
                if (!file) {
                    continue;
                }

                std::string plugin(file->GetFilename());
                catalog.byPlugin[plugin].push_back(FormChoice{
                    .displayName = std::move(*name),
                    .ref = FormRef{.plugin = plugin, .localFormID = form->GetLocalFormID()},
                });
            }

            catalog.plugins.reserve(catalog.byPlugin.size());
            for (auto& [plugin, choices] : catalog.byPlugin) {
                std::sort(choices.begin(), choices.end(), [](const FormChoice& a_lhs, const FormChoice& a_rhs) { return a_lhs.displayName < a_rhs.displayName; });
                catalog.plugins.push_back(plugin);
            }
            std::sort(catalog.plugins.begin(), catalog.plugins.end());

            return catalog;
        }

        template <class T>
        [[nodiscard]] FormCatalog BuildCatalog() {
            return BuildCatalog<T>([](T&) { return true; });
        }

        [[nodiscard]] bool IsTwoHandedWeapon(RE::TESObjectWEAP& a_weapon) {
            return a_weapon.IsTwoHandedSword() || a_weapon.IsTwoHandedAxe() || a_weapon.IsBow() || a_weapon.IsCrossbow();
        }

        [[nodiscard]] RE::TESObjectREFR::InventoryItemMap PlayerInventory() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return {};
            }
            return player->GetInventory();
        }

        [[nodiscard]] bool IsAllowedTorchForm(RE::TESObjectLIGH& a_light) {
            constexpr std::uint32_t kSkyrimEsmTorchLocalFormID = 0x01D4EC;
            if (auto* file = a_light.GetFile(0); file && std::string(file->GetFilename()) == "Skyrim.esm") {
                return a_light.GetLocalFormID() == kSkyrimEsmTorchLocalFormID;
            }
            return a_light.CanBeCarried();
        }
    }

    FormCatalog GetWeaponCatalog() { return BuildCatalog<RE::TESObjectWEAP>(); }
    FormCatalog GetOneHandedWeaponCatalog() {
        return BuildCatalog<RE::TESObjectWEAP>([](RE::TESObjectWEAP& a_weapon) { return !IsTwoHandedWeapon(a_weapon); });
    }
    FormCatalog GetArmorCatalog() { return BuildCatalog<RE::TESObjectARMO>(); }
    FormCatalog GetShieldCatalog() {
        return BuildCatalog<RE::TESObjectARMO>([](RE::TESObjectARMO& a_armor) { return a_armor.IsShield(); });
    }
    FormCatalog GetAmmoCatalog() { return BuildCatalog<RE::TESAmmo>(); }
    FormCatalog GetSpellCatalog() { return BuildCatalog<RE::SpellItem>(); }
    FormCatalog GetShoutCatalog() { return BuildCatalog<RE::TESShout>(); }
    FormCatalog GetTorchCatalog() { return BuildCatalog<RE::TESObjectLIGH>(IsAllowedTorchForm); }
    FormCatalog GetConsumableCatalog() { return BuildCatalog<RE::AlchemyItem>(); }


    FormCatalog GetInventoryWeaponCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESObjectWEAP>([&inventory](RE::TESObjectWEAP& a_weapon) { return inventory.contains(&a_weapon); });
    }
    FormCatalog GetInventoryOneHandedWeaponCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESObjectWEAP>([&inventory](RE::TESObjectWEAP& a_weapon) {
            return inventory.contains(&a_weapon) && !IsTwoHandedWeapon(a_weapon);
        });
    }
    FormCatalog GetInventoryArmorCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESObjectARMO>([&inventory](RE::TESObjectARMO& a_armor) { return inventory.contains(&a_armor); });
    }
    FormCatalog GetInventoryShieldCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESObjectARMO>(
            [&inventory](RE::TESObjectARMO& a_armor) { return inventory.contains(&a_armor) && a_armor.IsShield(); });
    }
    FormCatalog GetInventoryAmmoCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESAmmo>([&inventory](RE::TESAmmo& a_ammo) { return inventory.contains(&a_ammo); });
    }
    FormCatalog GetInventoryConsumableCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::AlchemyItem>([&inventory](RE::AlchemyItem& a_item) { return inventory.contains(&a_item); });
    }
    FormCatalog GetInventoryTorchCatalog() {
        auto inventory = PlayerInventory();
        return BuildCatalog<RE::TESObjectLIGH>(
            [&inventory](RE::TESObjectLIGH& a_light) { return IsAllowedTorchForm(a_light) && inventory.contains(&a_light); });
    }
    FormCatalog GetKnownSpellCatalog() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return BuildCatalog<RE::SpellItem>([player](RE::SpellItem& a_spell) { return player && player->HasSpell(&a_spell); });
    }
    FormCatalog GetKnownShoutCatalog() {
        return BuildCatalog<RE::TESShout>([](RE::TESShout& a_shout) { return a_shout.GetKnown(); });
    }

    FormCatalog GetWornArmorCatalog() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return {};
        }
        auto inventory = player->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::Armor); });
        std::unordered_set<RE::TESBoundObject*> worn;
        for (auto& [item, entry] : inventory) {
            auto& [count, data] = entry;
            if (item && data && data->IsWorn()) {
                worn.insert(item);
            }
        }
        return BuildCatalog<RE::TESObjectARMO>([&worn](RE::TESObjectARMO& a_armor) { return worn.contains(&a_armor); });
    }

    FormCatalog GetOutfitCatalog() {
        FormCatalog catalog;

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return catalog;
        }

        for (auto* form : dataHandler->GetFormArray<RE::BGSOutfit>()) {
            if (!form) {
                continue;
            }
            auto* file = form->GetFile(0);
            if (!file) {
                continue;
            }

            std::string plugin(file->GetFilename());

            std::string name = EditorIDLookup::Get(form->GetFormID());
            if (name.empty()) {
                name = std::format("Unnamed Outfit (0x{:08X})", form->GetFormID());
            }

            catalog.byPlugin[plugin].push_back(FormChoice{
                .displayName = std::move(name),
                .ref = FormRef{.plugin = plugin, .localFormID = form->GetLocalFormID()},
            });
        }

        catalog.plugins.reserve(catalog.byPlugin.size());
        for (auto& [plugin, choices] : catalog.byPlugin) {
            std::sort(choices.begin(), choices.end(), [](const FormChoice& a_lhs, const FormChoice& a_rhs) { return a_lhs.displayName < a_rhs.displayName; });
            catalog.plugins.push_back(plugin);
        }
        std::sort(catalog.plugins.begin(), catalog.plugins.end());

        return catalog;
    }

    std::optional<std::uint32_t> GetPluginLoadOrderIndex(std::string_view a_plugin) {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return std::nullopt;
        }
        if (auto* file = dataHandler->LookupLoadedModByName(a_plugin)) {
            return file->GetCombinedIndex();
        }
        if (auto* file = dataHandler->LookupLoadedLightModByName(a_plugin)) {
            return file->GetCombinedIndex();
        }
        return std::nullopt;
    }
}
