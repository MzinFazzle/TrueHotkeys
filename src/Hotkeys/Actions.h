#pragma once


#include "Hotkeys/FormRef.h"
#include "Hotkeys/HotkeyAction.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hotkeys {
    class WeaponSetAction final : public IHotkeyAction {
    public:
        WeaponSetAction(std::optional<FormRef> a_right, std::optional<FormRef> a_left, std::optional<FormRef> a_ammo,
                        bool a_addIfMissing = false) :
            m_right(std::move(a_right)), m_left(std::move(a_left)), m_ammo(std::move(a_ammo)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kWeaponSet; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] bool SupportsUndo() const noexcept override { return true; }
        void Undo(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        std::optional<FormRef> m_right;
        std::optional<FormRef> m_left;
        std::optional<FormRef> m_ammo;
        bool m_addIfMissing;

    };

    class OutfitAction final : public IHotkeyAction {
    public:
        explicit OutfitAction(std::vector<FormRef> a_items, bool a_unequipEverythingElse = false, bool a_addIfMissing = false,
                               std::string a_customName = "") :
            m_items(std::move(a_items)), m_unequipEverythingElse(a_unequipEverythingElse), m_addIfMissing(a_addIfMissing),
            m_customName(std::move(a_customName)) {}
        explicit OutfitAction(FormRef a_outfitForm, bool a_unequipEverythingElse = false, bool a_addIfMissing = false) :
            m_outfitForm(std::move(a_outfitForm)), m_unequipEverythingElse(a_unequipEverythingElse), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kOutfit; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] bool SupportsUndo() const noexcept override { return true; }
        void Undo(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        std::vector<FormRef> m_items;
        std::optional<FormRef> m_outfitForm;
        bool m_unequipEverythingElse;
        bool m_addIfMissing;
        std::string m_customName;
    };

    class SpellAction final : public IHotkeyAction {
    public:
        SpellAction(std::optional<FormRef> a_right, std::optional<FormRef> a_left, bool a_addIfMissing = false) :
            m_right(std::move(a_right)), m_left(std::move(a_left)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kSpell; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] bool SupportsUndo() const noexcept override { return true; }
        void Undo(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        std::optional<FormRef> m_right;
        std::optional<FormRef> m_left;
        bool m_addIfMissing;
    };

    class ShoutAction final : public IHotkeyAction {
    public:
        explicit ShoutAction(FormRef a_shout, bool a_addIfMissing = false) :
            m_shout(std::move(a_shout)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kShout; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] bool SupportsUndo() const noexcept override { return true; }
        void Undo(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        FormRef m_shout;
        bool m_addIfMissing;
    };

    class ConsumableAction final : public IHotkeyAction {
    public:
        explicit ConsumableAction(FormRef a_item, bool a_addIfMissing = false) :
            m_item(std::move(a_item)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kConsumable; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        FormRef m_item;
        bool m_addIfMissing;
    };

    class AmmoSwapAction final : public IHotkeyAction {
    public:
        explicit AmmoSwapAction(FormRef a_ammo, bool a_addIfMissing = false) :
            m_ammo(std::move(a_ammo)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kAmmoSwap; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        FormRef m_ammo;
        bool m_addIfMissing;
    };

    class ToggleTorchAction final : public IHotkeyAction {
    public:
        explicit ToggleTorchAction(FormRef a_torch, bool a_addIfMissing = false) :
            m_torch(std::move(a_torch)), m_addIfMissing(a_addIfMissing) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kToggleTorch; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        FormRef m_torch;
        bool m_addIfMissing;
    };

    class TogglePOVAction final : public IHotkeyAction {
    public:
        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kTogglePOV; }
        [[nodiscard]] std::string GetDisplayName() const override { return "Toggle POV"; }
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override { return "Type:TogglePOV"; }
    };

    struct PanicCategories {
        bool weapons = false;
        bool spells = false;
        bool armor = false;
        bool shouts = false;
        bool ammo = false;
    };

    class PanicAction final : public IHotkeyAction {
    public:
        explicit PanicAction(PanicCategories a_categories) : m_categories(a_categories) {}
        explicit PanicAction(std::vector<FormRef> a_specificItems) : m_specificItems(std::move(a_specificItems)) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kPanic; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

    private:
        PanicCategories m_categories;
        std::vector<FormRef> m_specificItems;
    };

    class MovementAction final : public IHotkeyAction {
    public:
        explicit MovementAction(MovementDirection a_direction) : m_direction(a_direction) {}

        [[nodiscard]] ActionType GetType() const noexcept override { return ActionType::kMovement; }
        [[nodiscard]] std::string GetDisplayName() const override;
        void Execute(RE::Actor* a_actor) const override;
        [[nodiscard]] bool SupportsUndo() const noexcept override { return true; }
        void Undo(RE::Actor* a_actor) const override;
        [[nodiscard]] std::string Serialize() const override;

        [[nodiscard]] MovementDirection GetDirection() const noexcept { return m_direction; }

    private:
        MovementDirection m_direction;
    };

    [[nodiscard]] std::unique_ptr<IHotkeyAction> DeserializeAction(const std::unordered_map<std::string, std::string>& a_fields);
}
