#pragma once


#include <cstdint>
#include <string>
#include <string_view>

namespace Hotkeys {
    enum class ActionType : std::uint8_t {
        kWeaponSet,
        kOutfit,
        kSpell,
        kShout,
        kConsumable,
        kAmmoSwap,
        kToggleTorch,
        kTogglePOV,
        kPanic,
        kMovement,
    };

    enum class Hand : std::uint8_t {
        kLeft,
        kRight,
        kEitherEmpty,   // whichever hand is currently unoccupied by a spell; right hand if both are
        kBothInstant,   // equips into both hands (dual-cast setup)
    };

    enum class MovementDirection : std::uint8_t {
        kForward,
        kBackward,
        kStrafeLeft,
        kStrafeRight,
    };

    [[nodiscard]] std::string_view ToString(ActionType a_type) noexcept;
    [[nodiscard]] std::string_view ToString(Hand a_hand) noexcept;
    [[nodiscard]] std::string_view ToString(MovementDirection a_direction) noexcept;

    class IHotkeyAction {
    public:
        virtual ~IHotkeyAction() = default;

        [[nodiscard]] virtual ActionType GetType() const noexcept = 0;

        [[nodiscard]] virtual std::string GetDisplayName() const = 0;

        virtual void Execute(RE::Actor* a_actor) const = 0;

        [[nodiscard]] virtual bool SupportsUndo() const noexcept { return false; }

        virtual void Undo(RE::Actor* a_actor) const {}

        [[nodiscard]] virtual std::string Serialize() const = 0;
    };
}
