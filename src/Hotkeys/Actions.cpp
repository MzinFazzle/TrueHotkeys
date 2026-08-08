#include "Hotkeys/Actions.h"

#include "Hotkeys/ContinuousMovement.h"
#include "Hotkeys/EditorIDLookup.h"

#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

namespace Hotkeys {

    namespace {

        [[nodiscard]] RE::BGSEquipSlot* GetLeftHandSlot() {
            auto* objManager = RE::BGSDefaultObjectManager::GetSingleton();
            return objManager ? objManager->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip) : nullptr;
        }

        [[nodiscard]] RE::BGSEquipSlot* GetRightHandSlot() {
            auto* objManager = RE::BGSDefaultObjectManager::GetSingleton();
            return objManager ? objManager->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip) : nullptr;
        }

        [[nodiscard]] bool IsOwned(RE::Actor* a_actor, RE::TESBoundObject* a_object) {
            auto owned = a_actor->GetInventory([a_object](RE::TESBoundObject& a_obj) { return &a_obj == a_object; });
            for (auto& [item, entry] : owned) {
                auto& [count, data] = entry;
                if (count > 0) {
                    return true;
                }
            }
            return false;
        }

        void GrantIfMissing(RE::Actor* a_actor, RE::TESBoundObject* a_object) {
            if (!IsOwned(a_actor, a_object)) {
                a_actor->AddObjectToContainer(a_object, nullptr, 1, nullptr);
            }
        }

        [[nodiscard]] bool GrantIfAllowed(RE::Actor* a_actor, RE::TESBoundObject* a_object, bool a_addIfMissing) {
            if (a_addIfMissing) {
                GrantIfMissing(a_actor, a_object);
                return true;
            }
            return IsOwned(a_actor, a_object);
        }

        void EquipBound(RE::Actor* a_actor, RE::TESBoundObject* a_object, bool a_addIfMissing) {
            if (!GrantIfAllowed(a_actor, a_object, a_addIfMissing)) {
                return;
            }
            if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
                equipManager->EquipObject(a_actor, a_object, nullptr, 1, nullptr, true, true, true, true);
            }
        }

        void UnequipBound(RE::ActorEquipManager* a_equipManager, RE::Actor* a_actor, RE::TESBoundObject* a_object) {
            a_equipManager->UnequipObject(a_actor, a_object, nullptr, 1, nullptr, true, true, true, true);
        }

        void EquipItemViaPapyrus(RE::Actor* a_actor, RE::TESBoundObject* a_item) {
            if (!a_actor || !a_item) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* handlePolicy = vm->GetObjectHandlePolicy();
            if (!handlePolicy) {
                return;
            }
            auto handle = handlePolicy->GetHandleForObject(a_actor->GetFormType(), a_actor);
            if (handle == handlePolicy->EmptyHandle()) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESForm*>(a_item), false, false);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "EquipItem", args, callback);
        }

        void EquipBoundViaPapyrus(RE::Actor* a_actor, RE::TESBoundObject* a_object, bool a_addIfMissing) {
            if (!GrantIfAllowed(a_actor, a_object, a_addIfMissing)) {
                return;
            }
            EquipItemViaPapyrus(a_actor, a_object);
        }

        constexpr std::int32_t kEquipSlotDefault = 0;
        constexpr std::int32_t kEquipSlotRightHand = 1;
        constexpr std::int32_t kEquipSlotLeftHand = 2;

        void EquipItemExViaPapyrus(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::int32_t a_equipSlot) {
            if (!a_actor || !a_item) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* handlePolicy = vm->GetObjectHandlePolicy();
            if (!handlePolicy) {
                return;
            }
            auto handle = handlePolicy->GetHandleForObject(a_actor->GetFormType(), a_actor);
            if (handle == handlePolicy->EmptyHandle()) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESForm*>(a_item), static_cast<std::int32_t>(a_equipSlot), false, false);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "EquipItemEx", args, callback);
        }

        constexpr std::int32_t kUnequipSpellHandLeft = 0;
        constexpr std::int32_t kUnequipSpellHandRight = 1;

        void UnequipSpellViaPapyrus(RE::Actor* a_actor, RE::SpellItem* a_spell, std::int32_t a_hand) {
            if (!a_actor || !a_spell) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* handlePolicy = vm->GetObjectHandlePolicy();
            if (!handlePolicy) {
                return;
            }
            auto handle = handlePolicy->GetHandleForObject(a_actor->GetFormType(), a_actor);
            if (handle == handlePolicy->EmptyHandle()) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::SpellItem*>(a_spell), static_cast<std::int32_t>(a_hand));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "UnequipSpell", args, callback);
        }

        void TeachSpellViaPapyrus(RE::Actor* a_actor, RE::SpellItem* a_spell) {
            if (!a_actor || !a_spell) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* handlePolicy = vm->GetObjectHandlePolicy();
            if (!handlePolicy) {
                return;
            }
            auto handle = handlePolicy->GetHandleForObject(a_actor->GetFormType(), a_actor);
            if (handle == handlePolicy->EmptyHandle()) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::SpellItem*>(a_spell), false);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "AddSpell", args, callback);
        }

        void GrantSpellIfMissing(RE::Actor* a_actor, RE::SpellItem* a_spell) {
            if (!a_actor || !a_spell) {
                return;
            }
            if (!a_actor->HasSpell(a_spell)) {
                TeachSpellViaPapyrus(a_actor, a_spell);
            }
        }

        void UnlockWordViaPapyrus(RE::TESWordOfPower* a_word) {
            if (!a_word) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESWordOfPower*>(a_word));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchStaticCall("Game", "UnlockWord", args, callback);
        }

        void TeachWordViaPapyrus(RE::TESWordOfPower* a_word) {
            if (!a_word) {
                return;
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) {
                return;
            }
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESWordOfPower*>(a_word));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchStaticCall("Game", "TeachWord", args, callback);
        }

        void GrantShoutIfMissing(RE::Actor* a_actor, RE::TESShout* a_shout) {
            if (!a_actor || !a_shout) {
                return;
            }
            for (auto& variation : a_shout->variations) {
                if (variation.word) {
                    TeachWordViaPapyrus(variation.word);
                    UnlockWordViaPapyrus(variation.word);
                }
            }
        }

        void ClearHandSpellIfAny(RE::Actor* a_actor, bool a_leftHand) {
            auto* equipped = a_actor->GetEquippedObject(a_leftHand);
            if (!equipped) {
                return;
            }
            if (auto* spell = equipped->As<RE::SpellItem>()) {
                UnequipSpellViaPapyrus(a_actor, spell, a_leftHand ? kUnequipSpellHandLeft : kUnequipSpellHandRight);
            }
        }

        void UnequipHand(RE::Actor* a_actor, bool a_leftHand) {
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) {
                return;
            }
            auto* equipped = a_actor->GetEquippedObject(a_leftHand);
            if (!equipped) {
                return;
            }
            if (auto* bound = equipped->As<RE::TESBoundObject>()) {
                UnequipBound(equipManager, a_actor, bound);
            }
        }

        void UnequipAllWeapons(RE::Actor* a_actor) {
            UnequipHand(a_actor, false);
            UnequipHand(a_actor, true);
        }

        void UnequipAllSpells(RE::Actor* a_actor) {
            ClearHandSpellIfAny(a_actor, false);
            ClearHandSpellIfAny(a_actor, true);
        }

        void UnequipAllShouts(RE::Actor* a_actor, RE::ActorEquipManager* a_equipManager) {
            a_equipManager->EquipShout(a_actor, nullptr);
        }

        void UnequipAllArmor(RE::Actor* a_actor, RE::ActorEquipManager* a_equipManager) {
            auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::Armor); });
            for (auto& [item, entry] : inventory) {
                auto& [count, data] = entry;
                if (data && data->IsWorn()) {
                    UnequipBound(a_equipManager, a_actor, item);
                }
            }
        }

        void UnequipAllAmmo(RE::Actor* a_actor, RE::ActorEquipManager* a_equipManager) {
            if (auto* ammo = a_actor->GetCurrentAmmo()) {
                UnequipBound(a_equipManager, a_actor, ammo);
            }
        }
        
        void UnequipEverything(RE::Actor* a_actor, RE::ActorEquipManager* a_equipManager) {
            UnequipAllWeapons(a_actor);
            UnequipAllSpells(a_actor);
            UnequipAllShouts(a_actor, a_equipManager);
            UnequipAllArmor(a_actor, a_equipManager);
        }

        [[nodiscard]] std::string SerializeField(std::string_view a_key, const FormRef& a_ref) {
            return std::format("{}:{}", a_key, a_ref.ToString());
        }
    }

    std::string_view ToString(ActionType a_type) noexcept {
        switch (a_type) {
            case ActionType::kWeaponSet:
                return "Weapon";
            case ActionType::kOutfit:
                return "Outfit";
            case ActionType::kSpell:
                return "Spell";
            case ActionType::kShout:
                return "Shout";
            case ActionType::kConsumable:
                return "Consumable";
            case ActionType::kAmmoSwap:
                return "Ammo";
            case ActionType::kToggleTorch:
                return "ToggleTorch";
            case ActionType::kTogglePOV:
                return "TogglePOV";
            case ActionType::kPanic:
                return "Unequip";
            case ActionType::kMovement:
                return "Movement";
        }
        return "Unknown";
    }

    std::string_view ToString(Hand a_hand) noexcept {
        switch (a_hand) {
            case Hand::kLeft:
                return "Left";
            case Hand::kRight:
                return "Right";
            case Hand::kEitherEmpty:
                return "EitherEmpty";
            case Hand::kBothInstant:
                return "BothInstant";
        }
        return "Unknown";
    }

    std::string_view ToString(MovementDirection a_direction) noexcept {
        switch (a_direction) {
            case MovementDirection::kForward:
                return "Forward";
            case MovementDirection::kBackward:
                return "Backward";
            case MovementDirection::kStrafeLeft:
                return "StrafeLeft";
            case MovementDirection::kStrafeRight:
                return "StrafeRight";
        }
        return "Unknown";
    }


    std::string WeaponSetAction::GetDisplayName() const {
        std::string name = "Weapon:";
        if (m_right) {
            name += std::format(" {}", m_right->ToDisplayString());
            if (m_left) {
                name += std::format(" + {}", m_left->ToDisplayString());
            }
        } else if (m_left) {
            name += std::format(" {} (Left Hand)", m_left->ToDisplayString());
        }
        if (m_ammo) {
            name += std::format(" ({})", m_ammo->ToDisplayString());
        }
        if (m_addIfMissing) {
            name += " [grants missing]";
        }
        return name;
    }

    void WeaponSetAction::Execute(RE::Actor* a_actor) const {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager || !a_actor) {
            return;
        }

        ClearHandSpellIfAny(a_actor, false);
        ClearHandSpellIfAny(a_actor, true);
        UnequipHand(a_actor, false);
        UnequipHand(a_actor, true);

        std::optional<FormRef> right = m_right;
        std::optional<FormRef> left = m_left;
        std::optional<FormRef> ammo = m_ammo;
        bool addIfMissing = m_addIfMissing;
        SKSE::GetTaskInterface()->AddTask([right, left, ammo, addIfMissing]() {
            auto* actor = RE::PlayerCharacter::GetSingleton();
            if (!actor) {
                return;
            }

            if (right && left) {
                if (auto* boundRight = right->Resolve<RE::TESBoundObject>()) {
                    if (GrantIfAllowed(actor, boundRight, addIfMissing)) {
                        EquipItemExViaPapyrus(actor, boundRight, kEquipSlotRightHand);
                    }
                }
                if (auto* boundLeft = left->Resolve<RE::TESBoundObject>()) {
                    if (GrantIfAllowed(actor, boundLeft, addIfMissing)) {
                        EquipItemExViaPapyrus(actor, boundLeft, kEquipSlotLeftHand);
                    }
                }
            } else if (right) {
                if (auto* boundRight = right->Resolve<RE::TESBoundObject>()) {
                    EquipBoundViaPapyrus(actor, boundRight, addIfMissing);
                }
            } else if (left) {
                if (auto* boundLeft = left->Resolve<RE::TESBoundObject>()) {
                    if (GrantIfAllowed(actor, boundLeft, addIfMissing)) {
                        EquipItemExViaPapyrus(actor, boundLeft, kEquipSlotLeftHand);
                    }
                }
            }
            if (ammo) {
                if (auto* boundAmmo = ammo->Resolve<RE::TESBoundObject>()) {
                    EquipBound(actor, boundAmmo, addIfMissing);
                }
            }
        });
    }

    void WeaponSetAction::Undo(RE::Actor* a_actor) const {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager || !a_actor) {
            return;
        }

        UnequipHand(a_actor, false);
        UnequipHand(a_actor, true);

        if (m_ammo) {
            if (auto* ammo = a_actor->GetCurrentAmmo()) {
                UnequipBound(equipManager, a_actor, ammo);
            }
        }
    }

    std::string WeaponSetAction::Serialize() const {
        std::string out = "Type:Weapon";
        if (m_right) {
            out += std::format("|{}", SerializeField("Right", *m_right));
        }
        if (m_left) {
            out += std::format("|{}", SerializeField("Left", *m_left));
        }
        if (m_ammo) {
            out += std::format("|{}", SerializeField("Ammo", *m_ammo));
        }
        if (m_addIfMissing) {
            out += "|AddIfMissing:1";
        }
        return out;
    }


    std::string OutfitAction::GetDisplayName() const {
        std::string suffix = m_unequipEverythingElse ? " (strips everything first)" : "";
        if (m_addIfMissing) {
            suffix += " [grants missing]";
        }
        if (m_outfitForm) {
            if (auto* outfit = m_outfitForm->Resolve<RE::BGSOutfit>()) {
                std::string editorID = EditorIDLookup::Get(outfit->GetFormID());
                if (!editorID.empty()) {
                    return std::format("Outfit: {}{}", editorID, suffix);
                }
            }
            return std::format("Outfit: {}{}", m_outfitForm->ToDisplayString(), suffix);
        }
        std::string label = m_customName.empty() ? "Outfit" : std::format("Outfit ({})", m_customName);
        return std::format("{}: {} item(s){}", label, m_items.size(), suffix);
    }

    void OutfitAction::Execute(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }

        if (m_unequipEverythingElse) {
            if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
                UnequipEverything(a_actor, equipManager);
            }
        }

        if (m_outfitForm) {
            auto* outfit = m_outfitForm->Resolve<RE::BGSOutfit>();
            if (!outfit) {
                return;
            }
            outfit->ForEachItem([&](RE::TESForm& a_item) {
                if (a_item.As<RE::TESLevItem>()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (auto* bound = a_item.As<RE::TESBoundObject>()) {
                    EquipBoundViaPapyrus(a_actor, bound, m_addIfMissing);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return;
        }

        for (const auto& item : m_items) {
            if (auto* armor = item.Resolve<RE::TESBoundObject>()) {
                EquipBoundViaPapyrus(a_actor, armor, m_addIfMissing);
            }
        }
    }

    std::string OutfitAction::Serialize() const {
        std::string suffix = m_unequipEverythingElse ? "|UnequipAll:1" : "";
        if (m_addIfMissing) {
            suffix += "|AddIfMissing:1";
        }
        if (m_outfitForm) {
            return std::format("Type:Outfit|{}{}", SerializeField("Outfit", *m_outfitForm), suffix);
        }
        std::string items;
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (i > 0) {
                items += ',';
            }
            items += m_items[i].ToString();
        }
        if (!m_customName.empty()) {
            suffix += std::format("|OutfitName:{}", m_customName);
        }
        return std::format("Type:Outfit|Items:{}{}", items, suffix);
    }


    std::string SpellAction::GetDisplayName() const {
        std::string name = "Spell:";
        if (m_right) {
            name += std::format(" R={}", m_right->ToDisplayString());
        }
        if (m_left) {
            name += std::format(" L={}", m_left->ToDisplayString());
        }
        if (m_addIfMissing) {
            name += " [grants missing]";
        }
        return name;
    }

    void SpellAction::Execute(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }

        if (m_right) {
            if (auto* spell = m_right->Resolve<RE::SpellItem>()) {
                if (m_addIfMissing) {
                    GrantSpellIfMissing(a_actor, spell);
                }
            }
        }
        if (m_left) {
            if (auto* spell = m_left->Resolve<RE::SpellItem>()) {
                if (m_addIfMissing) {
                    GrantSpellIfMissing(a_actor, spell);
                }
            }
        }

        std::optional<FormRef> right = m_right;
        std::optional<FormRef> left = m_left;
        SKSE::GetTaskInterface()->AddTask([right, left]() {
            auto* actor = RE::PlayerCharacter::GetSingleton();
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!actor || !equipManager) {
                return;
            }
            if (right) {
                if (auto* spell = right->Resolve<RE::SpellItem>()) {
                    equipManager->EquipSpell(actor, spell, GetRightHandSlot());
                }
            }
            if (left) {
                if (auto* spell = left->Resolve<RE::SpellItem>()) {
                    equipManager->EquipSpell(actor, spell, GetLeftHandSlot());
                }
            }
        });
    }

    void SpellAction::Undo(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }

        if (m_right) {
            ClearHandSpellIfAny(a_actor, false);
        }
        if (m_left) {
            ClearHandSpellIfAny(a_actor, true);
        }
    }

    std::string SpellAction::Serialize() const {
        std::string out = "Type:Spell";
        if (m_right) {
            out += std::format("|{}", SerializeField("Right", *m_right));
        }
        if (m_left) {
            out += std::format("|{}", SerializeField("Left", *m_left));
        }
        if (m_addIfMissing) {
            out += "|AddIfMissing:1";
        }
        return out;
    }


    std::string ShoutAction::GetDisplayName() const {
        std::string suffix = m_addIfMissing ? " [grants missing]" : "";
        return std::format("Shout: {}{}", m_shout.ToDisplayString(), suffix);
    }

    void ShoutAction::Execute(RE::Actor* a_actor) const {
        auto* shout = m_shout.Resolve<RE::TESShout>();
        if (!shout || !a_actor) {
            return;
        }
        if (m_addIfMissing) {
            GrantShoutIfMissing(a_actor, shout);
        }
        if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
            equipManager->EquipShout(a_actor, shout);
        }
    }

    void ShoutAction::Undo(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }
        if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
            equipManager->EquipShout(a_actor, nullptr);
        }
    }

    std::string ShoutAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:Shout|Form:{}{}", m_shout.ToString(), suffix);
    }


    std::string ConsumableAction::GetDisplayName() const {
        std::string suffix = m_addIfMissing ? " [grants missing]" : "";
        return std::format("Use: {}{}", m_item.ToDisplayString(), suffix);
    }

    void ConsumableAction::Execute(RE::Actor* a_actor) const {
        auto* item = m_item.Resolve<RE::TESBoundObject>();
        if (!item || !a_actor) {
            return;
        }
        EquipBound(a_actor, item, m_addIfMissing);
    }

    std::string ConsumableAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:Consumable|Form:{}{}", m_item.ToString(), suffix);
    }


    std::string AmmoSwapAction::GetDisplayName() const {
        std::string suffix = m_addIfMissing ? " [grants missing]" : "";
        return std::format("Ammo: {}{}", m_ammo.ToDisplayString(), suffix);
    }

    void AmmoSwapAction::Execute(RE::Actor* a_actor) const {
        auto* ammo = m_ammo.Resolve<RE::TESBoundObject>();
        if (!ammo || !a_actor) {
            return;
        }
        EquipBound(a_actor, ammo, m_addIfMissing);
    }

    std::string AmmoSwapAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:Ammo|Form:{}{}", m_ammo.ToString(), suffix);
    }


    std::string ToggleTorchAction::GetDisplayName() const {
        std::string suffix = m_addIfMissing ? " [grants missing]" : "";
        return std::format("Toggle Torch: {}{}", m_torch.ToDisplayString(), suffix);
    }

    void ToggleTorchAction::Execute(RE::Actor* a_actor) const {
        auto* torch = m_torch.Resolve<RE::TESBoundObject>();
        if (!torch || !a_actor) {
            return;
        }
        ClearHandSpellIfAny(a_actor, true);
        UnequipHand(a_actor, true);
        EquipBoundViaPapyrus(a_actor, torch, m_addIfMissing);
    }

    void ToggleTorchAction::Undo(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }
        UnequipHand(a_actor, true);
    }

    std::string ToggleTorchAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:ToggleTorch|Form:{}{}", m_torch.ToString(), suffix);
    }


    void TogglePOVAction::Execute(RE::Actor* a_actor) const {
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return;
        }
        if (camera->IsInFirstPerson()) {
            camera->ForceThirdPerson();
        } else {
            camera->ForceFirstPerson();
        }
    }


    std::string MovementAction::GetDisplayName() const {
        switch (m_direction) {
            case MovementDirection::kForward:
                return "Movement: Move Forward";
            case MovementDirection::kBackward:
                return "Movement: Move Backward";
            case MovementDirection::kStrafeLeft:
                return "Movement: Strafe Left";
            case MovementDirection::kStrafeRight:
                return "Movement: Strafe Right";
        }
        return "Movement";
    }

    void MovementAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        ContinuousMovement::SetActive(m_direction, true);
    }

    void MovementAction::Undo(RE::Actor* a_actor) const {
        (void)a_actor;
        ContinuousMovement::SetActive(m_direction, false);
    }

    std::string MovementAction::Serialize() const { return std::format("Type:Movement|Direction:{}", ToString(m_direction)); }


    std::string PanicAction::GetDisplayName() const {
        if (!m_specificItems.empty()) {
            std::string names;
            for (std::size_t i = 0; i < m_specificItems.size(); ++i) {
                if (i > 0) {
                    names += ", ";
                }
                names += m_specificItems[i].ToDisplayString();
            }
            return std::format("Unequip: {}", names);
        }
        std::string categories;
        if (m_categories.weapons) categories += "Weapons,";
        if (m_categories.spells) categories += "Spells,";
        if (m_categories.armor) categories += "Armor,";
        if (m_categories.shouts) categories += "Shouts,";
        if (m_categories.ammo) categories += "Ammo,";
        if (!categories.empty()) {
            categories.pop_back();
        }
        return std::format("Unequip: {}", categories.empty() ? "(nothing selected)" : categories);
    }

    void PanicAction::Execute(RE::Actor* a_actor) const {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager || !a_actor) {
            return;
        }

        if (!m_specificItems.empty()) {
            for (const auto& item : m_specificItems) {
                if (auto* bound = item.Resolve<RE::TESBoundObject>()) {
                    UnequipBound(equipManager, a_actor, bound);
                }
            }
            return;
        }

        if (m_categories.weapons) {
            UnequipAllWeapons(a_actor);
        }
        if (m_categories.spells) {
            UnequipAllSpells(a_actor);
        }
        if (m_categories.shouts) {
            UnequipAllShouts(a_actor, equipManager);
        }
        if (m_categories.armor) {
            UnequipAllArmor(a_actor, equipManager);
        }
        if (m_categories.ammo) {
            UnequipAllAmmo(a_actor, equipManager);
        }
    }

    std::string PanicAction::Serialize() const {
        if (!m_specificItems.empty()) {
            std::string items;
            for (std::size_t i = 0; i < m_specificItems.size(); ++i) {
                if (i > 0) {
                    items += ',';
                }
                items += m_specificItems[i].ToString();
            }
            return std::format("Type:Panic|Items:{}", items);
        }
        std::string categories;
        if (m_categories.weapons) categories += "Weapons,";
        if (m_categories.spells) categories += "Spells,";
        if (m_categories.armor) categories += "Armor,";
        if (m_categories.shouts) categories += "Shouts,";
        if (m_categories.ammo) categories += "Ammo,";
        if (!categories.empty()) {
            categories.pop_back();
        }
        return std::format("Type:Panic|Categories:{}", categories);
    }


    namespace {
        [[nodiscard]] std::vector<std::string> SplitCommaList(std::string_view a_str) {
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

        [[nodiscard]] std::optional<Hand> ParseHand(std::string_view a_str) {
            if (a_str == "Left") return Hand::kLeft;
            if (a_str == "Right") return Hand::kRight;
            if (a_str == "EitherEmpty") return Hand::kEitherEmpty;
            if (a_str == "BothInstant") return Hand::kBothInstant;
            return std::nullopt;
        }

        [[nodiscard]] std::optional<MovementDirection> ParseMovementDirection(std::string_view a_str) {
            if (a_str == "Forward") return MovementDirection::kForward;
            if (a_str == "Backward") return MovementDirection::kBackward;
            if (a_str == "StrafeLeft") return MovementDirection::kStrafeLeft;
            if (a_str == "StrafeRight") return MovementDirection::kStrafeRight;
            return std::nullopt;
        }

        [[nodiscard]] const std::string* FindField(const std::unordered_map<std::string, std::string>& a_fields, std::string_view a_key) {
            auto it = a_fields.find(std::string(a_key));
            return it == a_fields.end() ? nullptr : &it->second;
        }

    }

    std::unique_ptr<IHotkeyAction> DeserializeAction(const std::unordered_map<std::string, std::string>& a_fields) {
        const auto* type = FindField(a_fields, "Type");
        if (!type) {
            return nullptr;
        }

        if (*type == "Weapon") {
            std::optional<FormRef> right;
            if (const auto* rightStr = FindField(a_fields, "Right")) {
                right = FormRef::Parse(*rightStr);
            }
            std::optional<FormRef> left;
            if (const auto* leftStr = FindField(a_fields, "Left")) {
                left = FormRef::Parse(*leftStr);
            }
            if (!right && !left) {
                return nullptr;
            }
            std::optional<FormRef> ammo;
            if (const auto* ammoStr = FindField(a_fields, "Ammo")) {
                ammo = FormRef::Parse(*ammoStr);
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<WeaponSetAction>(right, left, ammo, addIfMissing);
        }

        if (*type == "Outfit") {
            bool unequipEverythingElse = false;
            if (const auto* unequipAllStr = FindField(a_fields, "UnequipAll")) {
                unequipEverythingElse = (*unequipAllStr == "1");
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }

            if (const auto* outfitStr = FindField(a_fields, "Outfit")) {
                auto form = FormRef::Parse(*outfitStr);
                if (!form) {
                    return nullptr;
                }
                return std::make_unique<OutfitAction>(*form, unequipEverythingElse, addIfMissing);
            }

            const auto* itemsStr = FindField(a_fields, "Items");
            if (!itemsStr) {
                return nullptr;
            }
            std::vector<FormRef> items;
            for (const auto& part : SplitCommaList(*itemsStr)) {
                if (auto ref = FormRef::Parse(part)) {
                    items.push_back(*ref);
                }
            }
            if (items.empty()) {
                return nullptr;
            }
            std::string customName;
            if (const auto* customNameStr = FindField(a_fields, "OutfitName")) {
                customName = *customNameStr;
            }
            return std::make_unique<OutfitAction>(std::move(items), unequipEverythingElse, addIfMissing, std::move(customName));
        }

        if (*type == "Spell") {
            std::optional<FormRef> right;
            std::optional<FormRef> left;
            if (const auto* rightStr = FindField(a_fields, "Right")) {
                right = FormRef::Parse(*rightStr);
            }
            if (const auto* leftStr = FindField(a_fields, "Left")) {
                left = FormRef::Parse(*leftStr);
            }

            if (!right && !left) {
                const auto* formStr = FindField(a_fields, "Form");
                const auto* handStr = FindField(a_fields, "Hand");
                if (formStr && handStr) {
                    auto form = FormRef::Parse(*formStr);
                    auto hand = ParseHand(*handStr);
                    if (form && hand) {
                        switch (*hand) {
                            case Hand::kLeft:
                                left = form;
                                break;
                            case Hand::kBothInstant:
                                right = form;
                                left = form;
                                break;
                            case Hand::kRight:
                            case Hand::kEitherEmpty:
                                right = form;
                                break;
                        }
                    }
                }
            }

            if (!right && !left) {
                return nullptr;
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<SpellAction>(right, left, addIfMissing);
        }

        if (*type == "Shout") {
            const auto* formStr = FindField(a_fields, "Form");
            if (!formStr) {
                return nullptr;
            }
            auto form = FormRef::Parse(*formStr);
            if (!form) {
                return nullptr;
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<ShoutAction>(*form, addIfMissing);
        }

        if (*type == "Consumable") {
            const auto* formStr = FindField(a_fields, "Form");
            if (!formStr) {
                return nullptr;
            }
            auto form = FormRef::Parse(*formStr);
            if (!form) {
                return nullptr;
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<ConsumableAction>(*form, addIfMissing);
        }

        if (*type == "Ammo") {
            const auto* formStr = FindField(a_fields, "Form");
            if (!formStr) {
                return nullptr;
            }
            auto form = FormRef::Parse(*formStr);
            if (!form) {
                return nullptr;
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<AmmoSwapAction>(*form, addIfMissing);
        }

        if (*type == "ToggleTorch") {
            const auto* formStr = FindField(a_fields, "Form");
            if (!formStr) {
                return nullptr;
            }
            auto form = FormRef::Parse(*formStr);
            if (!form) {
                return nullptr;
            }
            bool addIfMissing = false;
            if (const auto* addIfMissingStr = FindField(a_fields, "AddIfMissing")) {
                addIfMissing = (*addIfMissingStr == "1");
            }
            return std::make_unique<ToggleTorchAction>(*form, addIfMissing);
        }

        if (*type == "TogglePOV") {
            return std::make_unique<TogglePOVAction>();
        }

        if (*type == "Movement") {
            const auto* directionStr = FindField(a_fields, "Direction");
            if (!directionStr) {
                return nullptr;
            }
            auto direction = ParseMovementDirection(*directionStr);
            if (!direction) {
                return nullptr;
            }
            return std::make_unique<MovementAction>(*direction);
        }

        if (*type == "Panic") {
            if (const auto* itemsStr = FindField(a_fields, "Items")) {
                std::vector<FormRef> items;
                for (const auto& part : SplitCommaList(*itemsStr)) {
                    if (auto ref = FormRef::Parse(part)) {
                        items.push_back(*ref);
                    }
                }
                if (!items.empty()) {
                    return std::make_unique<PanicAction>(std::move(items));
                }
            }

            PanicCategories categories;
            if (const auto* categoriesStr = FindField(a_fields, "Categories")) {
                for (const auto& part : SplitCommaList(*categoriesStr)) {
                    if (part == "Weapons") categories.weapons = true;
                    else if (part == "Spells") categories.spells = true;
                    else if (part == "Armor") categories.armor = true;
                    else if (part == "Shouts") categories.shouts = true;
                    else if (part == "Ammo") categories.ammo = true;
                }
            }
            return std::make_unique<PanicAction>(categories);
        }

        return nullptr;
    }
}
