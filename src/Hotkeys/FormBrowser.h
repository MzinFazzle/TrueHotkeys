#pragma once


#include "Hotkeys/FormRef.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Hotkeys::FormBrowser {
    struct FormChoice {
        std::string displayName;
        FormRef ref;
    };

    struct FormCatalog {
        std::vector<std::string> plugins;
        std::unordered_map<std::string, std::vector<FormChoice>> byPlugin;
    };

    [[nodiscard]] FormCatalog GetWeaponCatalog();

    [[nodiscard]] FormCatalog GetOneHandedWeaponCatalog();

    [[nodiscard]] FormCatalog GetArmorCatalog();

    [[nodiscard]] FormCatalog GetShieldCatalog();

    [[nodiscard]] FormCatalog GetAmmoCatalog();
    [[nodiscard]] FormCatalog GetSpellCatalog();
    [[nodiscard]] FormCatalog GetShoutCatalog();

    [[nodiscard]] FormCatalog GetTorchCatalog();

    [[nodiscard]] FormCatalog GetConsumableCatalog();

    [[nodiscard]] FormCatalog GetOutfitCatalog();

    [[nodiscard]] std::optional<std::uint32_t> GetPluginLoadOrderIndex(std::string_view a_plugin);

    [[nodiscard]] FormCatalog GetInventoryWeaponCatalog();
    [[nodiscard]] FormCatalog GetInventoryOneHandedWeaponCatalog();
    [[nodiscard]] FormCatalog GetInventoryArmorCatalog();
    [[nodiscard]] FormCatalog GetInventoryShieldCatalog();
    [[nodiscard]] FormCatalog GetInventoryAmmoCatalog();
    [[nodiscard]] FormCatalog GetInventoryConsumableCatalog();
    [[nodiscard]] FormCatalog GetKnownSpellCatalog();
    [[nodiscard]] FormCatalog GetKnownShoutCatalog();
    [[nodiscard]] FormCatalog GetInventoryTorchCatalog();

    [[nodiscard]] FormCatalog GetWornArmorCatalog();
}
