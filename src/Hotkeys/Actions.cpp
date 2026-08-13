#include "Hotkeys/Actions.h"

#include "Hotkeys/ContinuousMovement.h"
#include "Hotkeys/EditorIDLookup.h"
#include "Hotkeys/HotkeyManager.h"
#include "Hotkeys/Locale.h"
#include "Hotkeys/SyntheticTap.h"
#include "Hotkeys/ToggleSprint.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string_view>
#include <utility>

namespace Hotkeys {
    namespace {
        [[nodiscard]] const char* T(std::string_view a_key) { return Locale::GetSingleton()->T(a_key); }

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

        [[nodiscard]] bool IsWorn(RE::Actor* a_actor, RE::TESBoundObject* a_object) {
            auto owned = a_actor->GetInventory([a_object](RE::TESBoundObject& a_obj) { return &a_obj == a_object; });
            for (auto& [item, entry] : owned) {
                auto& [count, data] = entry;
                if (data && data->IsWorn()) {
                    return true;
                }
            }
            return false;
        }

        constexpr std::array<float, 6> kSoulGemPointValue = {0.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f};

        [[nodiscard]] float SoulGemChargeValue(RE::SOUL_LEVEL a_soul) {
            auto index = static_cast<std::size_t>(a_soul);
            return index < kSoulGemPointValue.size() ? kSoulGemPointValue[index] : 0.0f;
        }

        struct SoulGemCandidate {
            RE::TESBoundObject* form = nullptr;
            RE::ExtraDataList* extraList = nullptr;
            RE::SOUL_LEVEL soul = RE::SOUL_LEVEL::kNone;
            float chargeValue = 0.0f;
        };

        void RechargeEquippedWeapon(RE::Actor* a_actor, bool a_leftHand, bool a_preferSmaller, std::uint8_t a_maxSize, bool a_notify) {
            if (!a_actor) {
                return;
            }

            auto* rightEquipped = a_actor->GetEquippedObject(false);
            auto* handEquipped = a_actor->GetEquippedObject(a_leftHand);
            if (!handEquipped || (a_leftHand && handEquipped == rightEquipped)) {
                return;
            }

            auto* weapon = handEquipped->As<RE::TESObjectWEAP>();
            if (!weapon) {
                return;  // torch, shield, spell, etc. - nothing to recharge
            }

            auto* entryData = a_actor->GetEquippedEntryData(a_leftHand);
            auto* enchantable = weapon->As<RE::TESEnchantableForm>();
            bool entryEnchanted = entryData && entryData->IsEnchanted();
            bool baseEnchanted = enchantable && enchantable->formEnchanting;
            bool isEnchanted = entryEnchanted || baseEnchanted;
            if (!isEnchanted) {
                return;  // not enchanted
            }

            auto chargeAV = a_leftHand ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
            float maxCharge = a_actor->AsActorValueOwner()->GetBaseActorValue(chargeAV);
            float currentCharge = a_actor->AsActorValueOwner()->GetActorValue(chargeAV);
            if (maxCharge <= 0.0f) {
                return;
            }
            if (currentCharge >= maxCharge) {
                return;  // already full
            }

            auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::SoulGem); });

            std::vector<SoulGemCandidate> candidates;
            for (auto& [obj, entry] : inventory) {
                auto& [count, data] = entry;
                if (count <= 0) {
                    continue;
                }
                auto* soulGemForm = obj->As<RE::TESSoulGem>();
                if (!soulGemForm) {
                    continue;
                }
                if (obj->GetFormID() == 0x00063B27 || obj->GetFormID() == 0x00063B29) {
                    continue;
                }

                bool sawSoulOverride = false;
                if (data && data->extraLists) {
                    for (auto* list : *data->extraLists) {
                        if (!list || !list->HasType<RE::ExtraSoul>()) {
                            continue;  // no soul override on this specific instance - not this gem's soul data
                        }
                        sawSoulOverride = true;
                        auto soul = list->GetSoulLevel();
                        if (soul == RE::SOUL_LEVEL::kNone) {
                            continue;  // this specific variant is an empty gem
                        }
                        if (static_cast<std::uint8_t>(soul) > a_maxSize) {
                            continue;  // excluded by the "never use above size X" cap
                        }
                        candidates.push_back({obj, list, soul, SoulGemChargeValue(soul)});
                    }
                }
                if (!sawSoulOverride) {
                    auto soul = soulGemForm->GetContainedSoul();
                    if (soul != RE::SOUL_LEVEL::kNone && static_cast<std::uint8_t>(soul) <= a_maxSize) {
                        candidates.push_back({obj, nullptr, soul, SoulGemChargeValue(soul)});
                    }
                }
            }

            if (candidates.empty()) {
                return;  // no eligible soul gem owned
            }

            auto best = std::min_element(candidates.begin(), candidates.end(), [&](const SoulGemCandidate& a, const SoulGemCandidate& b) {
                return a_preferSmaller ? a.chargeValue < b.chargeValue : a.chargeValue > b.chargeValue;
            });

            auto* soulSqueezerForm = RE::TESDataHandler::GetSingleton()->LookupForm(0x58F7C, "Skyrim.esm");
            auto* soulSqueezerPerk = soulSqueezerForm ? soulSqueezerForm->As<RE::BGSPerk>() : nullptr;
            bool hasSoulSqueezer = soulSqueezerPerk && a_actor->HasPerk(soulSqueezerPerk);
            float chargeValue = best->chargeValue + (hasSoulSqueezer ? 250.0f : 0.0f);

            float delta = std::min(chargeValue, maxCharge - currentCharge);
            if (delta <= 0.0f) {
                return;
            }
            a_actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, chargeAV, delta);

            a_actor->RemoveItem(best->form, 1, RE::ITEM_REMOVE_REASON::kRemove, best->extraList, nullptr);

            if (a_notify) {
                HotkeyManager::GetSingleton()->Notify(TF("actiontype.recharge.notify_message", weapon->GetName(), best->form->GetName()));
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

        void UnequipItemViaPapyrus(RE::Actor* a_actor, RE::TESBoundObject* a_item);

        void UnequipBound(RE::Actor* a_actor, RE::TESBoundObject* a_object) { UnequipItemViaPapyrus(a_actor, a_object); }

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
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESForm*>(a_item), false, true);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "EquipItem", args, callback);
        }

        void UnequipItemViaPapyrus(RE::Actor* a_actor, RE::TESBoundObject* a_item) {
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
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESForm*>(a_item), false, true);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "UnequipItem", args, callback);
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
            auto* args = RE::MakeFunctionArguments(
                static_cast<RE::TESForm*>(a_item), static_cast<std::int32_t>(a_equipSlot), false, false);
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

        void EquipShoutViaPapyrus(RE::Actor* a_actor, RE::TESShout* a_shout) {
            if (!a_actor) {
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
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESShout*>(a_shout));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "EquipShout", args, callback);
        }

        void UnequipShoutViaPapyrus(RE::Actor* a_actor, RE::TESShout* a_shout) {
            if (!a_actor || !a_shout) {
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
            auto* args = RE::MakeFunctionArguments(static_cast<RE::TESShout*>(a_shout));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            vm->DispatchMethodCall2(handle, "Actor", "UnequipShout", args, callback);
        }

        void GrantShoutIfMissing(RE::Actor* a_actor, RE::TESShout* a_shout) {
            if (!a_actor || !a_shout || a_shout->GetKnown()) {
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
            auto* equipped = a_actor->GetEquippedObject(a_leftHand);
            if (!equipped) {
                return;
            }
            if (auto* bound = equipped->As<RE::TESBoundObject>()) {
                UnequipBound(a_actor, bound);
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
        void UnequipAllArmor(RE::Actor* a_actor) {
            auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_obj) { return a_obj.Is(RE::FormType::Armor); });
            for (auto& [item, entry] : inventory) {
                auto& [count, data] = entry;
                if (data && data->IsWorn()) {
                    UnequipBound(a_actor, item);
                }
            }
        }
        void UnequipAllAmmo(RE::Actor* a_actor) {
            if (auto* ammo = a_actor->GetCurrentAmmo()) {
                UnequipBound(a_actor, ammo);
            }
        }
        void UnequipEverything(RE::Actor* a_actor, RE::ActorEquipManager* a_equipManager) {
            UnequipAllWeapons(a_actor);
            UnequipAllSpells(a_actor);
            UnequipAllShouts(a_actor, a_equipManager);
            UnequipAllArmor(a_actor);
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
            case ActionType::kOpenMenu:
                return "OpenMenu";
            case ActionType::kReadySheath:
                return "ReadySheath";
            case ActionType::kToggleSneak:
                return "ToggleSneak";
            case ActionType::kToggleAutoMove:
                return "ToggleAutoMove";
            case ActionType::kJump:
                return "Jump";
            case ActionType::kToggleFreeCam:
                return "ToggleFreeCam";
            case ActionType::kToggleFreeCamPaused:
                return "ToggleFreeCamPaused";
            case ActionType::kToggleSprint:
                return "ToggleSprint";
            case ActionType::kQuickSave:
                return "QuickSave";
            case ActionType::kQuickLoad:
                return "QuickLoad";
            case ActionType::kToggleMenus:
                return "ToggleMenus";
            case ActionType::kRechargeWeapon:
                return "RechargeWeapon";
            case ActionType::kRechargeWeaponLeftHand:
                return "RechargeWeaponLeftHand";
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

    std::string_view ToString(OpenMenuTarget a_target) noexcept {
        switch (a_target) {
            case OpenMenuTarget::kInventory:
                return "Inventory";
            case OpenMenuTarget::kSpells:
                return "Spells";
            case OpenMenuTarget::kMap:
                return "Map";
            case OpenMenuTarget::kSkills:
                return "Skills";
            case OpenMenuTarget::kFavorites:
                return "Favorites";
            case OpenMenuTarget::kWaitRest:
                return "Rest";
        }
        return "Unknown";
    }

    std::string_view ToDisplayString(OpenMenuTarget a_target) noexcept {
        switch (a_target) {
            case OpenMenuTarget::kInventory:
                return T("actioneditor.openmenu.inventory");
            case OpenMenuTarget::kSpells:
                return T("actioneditor.openmenu.spells");
            case OpenMenuTarget::kMap:
                return T("actioneditor.openmenu.map");
            case OpenMenuTarget::kSkills:
                return T("actioneditor.openmenu.skills");
            case OpenMenuTarget::kFavorites:
                return T("actioneditor.openmenu.favorites");
            case OpenMenuTarget::kWaitRest:
                return T("actioneditor.openmenu.wait_rest");
        }
        return T("common.unknown");
    }


    std::string WeaponSetAction::GetDisplayName() const {
        std::string name = T("actiontype.weapon.label");
        if (m_right) {
            name += TF("actiontype.weapon.item_suffix", m_right->ToDisplayString());
            if (m_left) {
                name += TF("actiontype.weapon.plus_item_suffix", m_left->ToDisplayString());
            }
        } else if (m_left) {
            name += TF("actiontype.weapon.left_hand_suffix", m_left->ToDisplayString());
        }
        if (m_ammo) {
            name += TF("actiontype.weapon.ammo_suffix", m_ammo->ToDisplayString());
        }
        if (m_addIfMissing) {
            name += T("common.grants_missing_suffix");
        }
        return name;
    }

    void WeaponSetAction::Execute(RE::Actor* a_actor) const {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager || !a_actor) {
            return;
        }

        auto* rightBound = m_right ? m_right->Resolve<RE::TESBoundObject>() : nullptr;
        auto* leftBound = m_left ? m_left->Resolve<RE::TESBoundObject>() : nullptr;
        auto* liveRightEquipped = a_actor->GetEquippedObject(false);
        auto* liveRightBound = liveRightEquipped ? liveRightEquipped->As<RE::TESBoundObject>() : nullptr;
        auto* liveLeftEquipped = a_actor->GetEquippedObject(true);
        auto* liveLeftBound = liveLeftEquipped ? liveLeftEquipped->As<RE::TESBoundObject>() : nullptr;
        bool rightAlreadyCorrect = rightBound && liveRightBound == rightBound;
        bool leftAlreadyCorrect = leftBound && liveLeftBound == leftBound;

        bool leftMirrorsRightTwoHanded = (m_right && !m_left) && liveRightBound && liveLeftBound == liveRightBound;

        ClearHandSpellIfAny(a_actor, false);
        ClearHandSpellIfAny(a_actor, true);
        if (!(m_right && !m_left) && !rightAlreadyCorrect) {
            UnequipHand(a_actor, false);
        }
        if (!leftAlreadyCorrect && !leftMirrorsRightTwoHanded) {
            UnequipHand(a_actor, true);
        }

        std::optional<FormRef> right = m_right;
        std::optional<FormRef> left = m_left;
        std::optional<FormRef> ammo = m_ammo;
        bool addIfMissing = m_addIfMissing;
        SKSE::GetTaskInterface()->AddTask([right, left, ammo, addIfMissing, rightAlreadyCorrect, leftAlreadyCorrect]() {
            auto* actor = RE::PlayerCharacter::GetSingleton();
            if (!actor) {
                return;
            }

            if (right && left) {
                if (auto* boundRight = right->Resolve<RE::TESBoundObject>()) {
                    if (!rightAlreadyCorrect && GrantIfAllowed(actor, boundRight, addIfMissing)) {
                        EquipItemExViaPapyrus(actor, boundRight, kEquipSlotRightHand);
                    }
                }
                if (auto* boundLeft = left->Resolve<RE::TESBoundObject>()) {
                    if (!leftAlreadyCorrect && GrantIfAllowed(actor, boundLeft, addIfMissing)) {
                        EquipItemExViaPapyrus(actor, boundLeft, kEquipSlotLeftHand);
                    }
                }
            } else if (right) {
                if (auto* boundRight = right->Resolve<RE::TESBoundObject>()) {
                    if (!rightAlreadyCorrect) {
                        EquipBoundViaPapyrus(actor, boundRight, addIfMissing);
                    }
                }
            } else if (left) {
                if (auto* boundLeft = left->Resolve<RE::TESBoundObject>()) {
                    if (!leftAlreadyCorrect && GrantIfAllowed(actor, boundLeft, addIfMissing)) {
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
        if (!a_actor) {
            return;
        }

        UnequipHand(a_actor, false);
        UnequipHand(a_actor, true);

        if (m_ammo) {
            if (auto* ammo = a_actor->GetCurrentAmmo()) {
                UnequipBound(a_actor, ammo);
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
        std::string suffix = m_unequipEverythingElse ? T("actiontype.outfit.strips_everything_suffix") : "";
        if (m_addIfMissing) {
            suffix += T("common.grants_missing_suffix");
        }
        if (m_outfitForm) {
            if (auto* outfit = m_outfitForm->Resolve<RE::BGSOutfit>()) {
                std::string editorID = EditorIDLookup::Get(outfit->GetFormID());
                if (!editorID.empty()) {
                    return TF("actiontype.outfit.display", editorID, suffix);
                }
            }
            return TF("actiontype.outfit.display", m_outfitForm->ToDisplayString(), suffix);
        }
        std::string label = m_customName.empty() ? T("actiontype.outfit.label") : TF("actiontype.outfit.label_with_name", m_customName);
        return TF("actiontype.outfit.item_count_display", label, m_items.size(), suffix);
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
            outfit->ForEachItem([&](RE::TESForm* a_item) {
                if (!a_item) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (a_item->As<RE::TESLevItem>()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (auto* bound = a_item->As<RE::TESBoundObject>()) {
                    if (!IsWorn(a_actor, bound)) {
                        EquipBoundViaPapyrus(a_actor, bound, m_addIfMissing);
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return;
        }

        for (const auto& item : m_items) {
            if (auto* armor = item.Resolve<RE::TESBoundObject>()) {
                if (!IsWorn(a_actor, armor)) {
                    EquipBoundViaPapyrus(a_actor, armor, m_addIfMissing);
                }
            }
        }
    }

    void OutfitAction::Undo(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }

        if (m_outfitForm) {
            auto* outfit = m_outfitForm->Resolve<RE::BGSOutfit>();
            if (!outfit) {
                return;
            }
            outfit->ForEachItem([&](RE::TESForm* a_item) {
                if (!a_item) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (a_item->As<RE::TESLevItem>()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (auto* bound = a_item->As<RE::TESBoundObject>()) {
                    UnequipBound(a_actor, bound);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return;
        }

        for (const auto& item : m_items) {
            if (auto* armor = item.Resolve<RE::TESBoundObject>()) {
                UnequipBound(a_actor, armor);
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
        std::string name = T("actiontype.spell.label");
        if (m_right) {
            name += TF("actiontype.spell.right_suffix", m_right->ToDisplayString());
        }
        if (m_left) {
            name += TF("actiontype.spell.left_suffix", m_left->ToDisplayString());
        }
        if (m_addIfMissing) {
            name += T("common.grants_missing_suffix");
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
        std::string suffix = m_addIfMissing ? T("common.grants_missing_suffix") : "";
        return TF("actiontype.shout.display", m_shout.ToDisplayString(), suffix);
    }

    void ShoutAction::Execute(RE::Actor* a_actor) const {
        auto* shout = m_shout.Resolve<RE::TESShout>();
        if (!shout || !a_actor) {
            return;
        }
        if (m_addIfMissing) {
            GrantShoutIfMissing(a_actor, shout);
        }
        EquipShoutViaPapyrus(a_actor, shout);
    }

    void ShoutAction::Undo(RE::Actor* a_actor) const {
        auto* shout = m_shout.Resolve<RE::TESShout>();
        if (!shout || !a_actor) {
            return;
        }
        UnequipShoutViaPapyrus(a_actor, shout);
    }

    std::string ShoutAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:Shout|Form:{}{}", m_shout.ToString(), suffix);
    }


    std::string ConsumableAction::GetDisplayName() const {
        std::string suffix = m_addIfMissing ? T("common.grants_missing_suffix") : "";
        return TF("actiontype.consumable.display", m_item.ToDisplayString(), suffix);
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
        std::string suffix = m_addIfMissing ? T("common.grants_missing_suffix") : "";
        return TF("actiontype.ammo.display", m_ammo.ToDisplayString(), suffix);
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
        std::string suffix = m_addIfMissing ? T("common.grants_missing_suffix") : "";
        return TF("actiontype.toggle_torch.display", m_torch.ToDisplayString(), suffix);
    }

    void ToggleTorchAction::Execute(RE::Actor* a_actor) const {
        auto* torch = m_torch.Resolve<RE::TESBoundObject>();
        if (!torch || !a_actor) {
            return;
        }
        if (auto* equipped = a_actor->GetEquippedObject(true); equipped && equipped->As<RE::TESBoundObject>() == torch) {
            UnequipHand(a_actor, true);
        } else {
            ClearHandSpellIfAny(a_actor, true);
            UnequipHand(a_actor, true);
            EquipBoundViaPapyrus(a_actor, torch, m_addIfMissing);
        }
    }

    std::string ToggleTorchAction::Serialize() const {
        std::string suffix = m_addIfMissing ? "|AddIfMissing:1" : "";
        return std::format("Type:ToggleTorch|Form:{}{}", m_torch.ToString(), suffix);
    }


    namespace {
        float g_savedThirdPersonZoomOffset = 0.0f;

        [[nodiscard]] RE::ThirdPersonState* GetThirdPersonState(RE::PlayerCamera* a_camera) {
            auto& state = a_camera->cameraStates[RE::CameraStates::kThirdPerson];
            return state ? skyrim_cast<RE::ThirdPersonState*>(state.get()) : nullptr;
        }
    }

    void TogglePOVAction::Execute(RE::Actor* a_actor) const {
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return;
        }
        if (camera->IsInFirstPerson()) {
            camera->ForceThirdPerson();
            if (auto* thirdPerson = GetThirdPersonState(camera)) {
                thirdPerson->currentZoomOffset = g_savedThirdPersonZoomOffset;
                thirdPerson->targetZoomOffset = g_savedThirdPersonZoomOffset;
            }
        } else {
            if (auto* thirdPerson = GetThirdPersonState(camera)) {
                g_savedThirdPersonZoomOffset = thirdPerson->currentZoomOffset;
            }
            camera->ForceFirstPerson();
        }
    }


    void ReadySheathAction::Execute(RE::Actor* a_actor) const {
        if (!a_actor) {
            return;
        }
        a_actor->DrawWeaponMagicHands(!a_actor->AsActorState()->IsWeaponDrawn());
    }


    void ToggleSneakAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        SyntheticTap::Queue(SyntheticTap::Kind::kSneak);
    }


    void ToggleAutoMoveAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        SyntheticTap::Queue(SyntheticTap::Kind::kAutoMove);
    }


    void JumpAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        SyntheticTap::Queue(SyntheticTap::Kind::kJump);
    }


    void ToggleFreeCamAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return;
        }
        camera->ToggleFreeCameraMode(false);
        if (auto* controls = RE::ControlMap::GetSingleton()) {
            constexpr auto context = RE::ControlMap::InputContextID::kTFCMode;
            if (camera->IsInFreeCameraMode()) {
                controls->PushInputContext(context);
            } else {
                controls->PopInputContext(context);
            }
        }
    }


    void ToggleFreeCamPausedAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            return;
        }
        camera->ToggleFreeCameraMode(true);
        if (auto* controls = RE::ControlMap::GetSingleton()) {
            constexpr auto context = RE::ControlMap::InputContextID::kTFCMode;
            if (camera->IsInFreeCameraMode()) {
                controls->PushInputContext(context);
            } else {
                controls->PopInputContext(context);
            }
        }
    }


    void ToggleSprintAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        ToggleSprint::Toggle();
    }

    void QuickSaveAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        SyntheticTap::Queue(SyntheticTap::Kind::kQuickSave);
    }

    void QuickLoadAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        SyntheticTap::Queue(SyntheticTap::Kind::kQuickLoad);
    }

    void ToggleMenusAction::Execute(RE::Actor* a_actor) const {
        (void)a_actor;
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->ShowMenus(!ui->IsShowingMenus());
        }
    }

    void RechargeWeaponAction::Execute(RE::Actor* a_actor) const {
        RechargeEquippedWeapon(a_actor, false, m_preferSmaller, m_maxSize, m_notify);
    }

    std::string RechargeWeaponAction::Serialize() const {
        return std::format("Type:RechargeWeapon|PreferSmaller:{}|MaxSize:{}|Notify:{}", m_preferSmaller ? 1 : 0,
                            static_cast<int>(m_maxSize), m_notify ? 1 : 0);
    }

    void RechargeWeaponLeftHandAction::Execute(RE::Actor* a_actor) const {
        RechargeEquippedWeapon(a_actor, true, m_preferSmaller, m_maxSize, m_notify);
    }

    std::string RechargeWeaponLeftHandAction::Serialize() const {
        return std::format("Type:RechargeWeaponLeftHand|PreferSmaller:{}|MaxSize:{}|Notify:{}", m_preferSmaller ? 1 : 0,
                            static_cast<int>(m_maxSize), m_notify ? 1 : 0);
    }


    std::string MovementAction::GetDisplayName() const {
        switch (m_direction) {
            case MovementDirection::kForward:
                return T("actiontype.movement.forward");
            case MovementDirection::kBackward:
                return T("actiontype.movement.backward");
            case MovementDirection::kStrafeLeft:
                return T("actiontype.movement.strafe_left");
            case MovementDirection::kStrafeRight:
                return T("actiontype.movement.strafe_right");
        }
        return T("actiontype.movement.label");
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


    std::string OpenMenuAction::GetDisplayName() const { return TF("actiontype.open_menu.display", ToDisplayString(m_target)); }

    namespace {
        void ShowSleepWaitMenu(bool a_sleep) {
            using func_t = decltype(&ShowSleepWaitMenu);
            REL::Relocation<func_t> func{REL::ID{52490}};
            func(a_sleep);
        }
    }

    void OpenMenuAction::Execute(RE::Actor* a_actor) const {
        if (m_target == OpenMenuTarget::kWaitRest) {
            bool sleep = a_actor && a_actor->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kIsSleeping;
            ShowSleepWaitMenu(sleep);
            return;
        }

        (void)a_actor;
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) {
            return;
        }
        std::string_view menuName;
        switch (m_target) {
            case OpenMenuTarget::kInventory:
                menuName = RE::InventoryMenu::MENU_NAME;
                break;
            case OpenMenuTarget::kSpells:
                menuName = RE::MagicMenu::MENU_NAME;
                break;
            case OpenMenuTarget::kMap:
                menuName = RE::MapMenu::MENU_NAME;
                break;
            case OpenMenuTarget::kSkills:
                menuName = RE::StatsMenu::MENU_NAME;
                break;
            case OpenMenuTarget::kFavorites:
                menuName = RE::FavoritesMenu::MENU_NAME;
                break;
            case OpenMenuTarget::kWaitRest:
                return;  // handled above, never reached
        }
        queue->AddMessage(menuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }

    std::string OpenMenuAction::Serialize() const { return std::format("Type:OpenMenu|Target:{}", ToString(m_target)); }


    std::string PanicAction::GetDisplayName() const {
        if (!m_specificItems.empty()) {
            std::string names;
            for (std::size_t i = 0; i < m_specificItems.size(); ++i) {
                if (i > 0) {
                    names += ", ";
                }
                names += m_specificItems[i].ToDisplayString();
            }
            return TF("actiontype.panic.unequip_display", names);
        }
        std::string categories;
        auto appendCategory = [&categories](const char* a_word) {
            if (!categories.empty()) {
                categories += ",";
            }
            categories += a_word;
        };
        if (m_categories.weapons) appendCategory(T("actiontype.panic.category_weapons"));
        if (m_categories.spells) appendCategory(T("actiontype.panic.category_spells"));
        if (m_categories.armor) appendCategory(T("actiontype.panic.category_armor"));
        if (m_categories.shouts) appendCategory(T("actiontype.panic.category_shouts"));
        if (m_categories.ammo) appendCategory(T("actiontype.panic.category_ammo"));
        return TF("actiontype.panic.unequip_display", categories.empty() ? T("actiontype.panic.nothing_selected") : categories);
    }

    void PanicAction::Execute(RE::Actor* a_actor) const {
        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager || !a_actor) {
            return;
        }

        if (!m_specificItems.empty()) {
            for (const auto& item : m_specificItems) {
                if (auto* bound = item.Resolve<RE::TESBoundObject>()) {
                    UnequipBound(a_actor, bound);
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
            UnequipAllArmor(a_actor);
        }
        if (m_categories.ammo) {
            UnequipAllAmmo(a_actor);
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

        [[nodiscard]] std::optional<OpenMenuTarget> ParseOpenMenuTarget(std::string_view a_str) {
            if (a_str == "Inventory") return OpenMenuTarget::kInventory;
            if (a_str == "Spells") return OpenMenuTarget::kSpells;
            if (a_str == "Map") return OpenMenuTarget::kMap;
            if (a_str == "Skills") return OpenMenuTarget::kSkills;
            if (a_str == "Favorites") return OpenMenuTarget::kFavorites;
            if (a_str == "Rest") return OpenMenuTarget::kWaitRest;
            if (a_str == "Wait") return OpenMenuTarget::kWaitRest;
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

        if (*type == "ReadySheath") {
            return std::make_unique<ReadySheathAction>();
        }

        if (*type == "ToggleSneak") {
            return std::make_unique<ToggleSneakAction>();
        }

        if (*type == "ToggleAutoMove") {
            return std::make_unique<ToggleAutoMoveAction>();
        }

        if (*type == "Jump") {
            return std::make_unique<JumpAction>();
        }

        if (*type == "ToggleFreeCam") {
            return std::make_unique<ToggleFreeCamAction>();
        }

        if (*type == "ToggleFreeCamPaused") {
            return std::make_unique<ToggleFreeCamPausedAction>();
        }


        if (*type == "ToggleSprint") {
            return std::make_unique<ToggleSprintAction>();
        }

        if (*type == "QuickSave") {
            return std::make_unique<QuickSaveAction>();
        }

        if (*type == "QuickLoad") {
            return std::make_unique<QuickLoadAction>();
        }

        if (*type == "ToggleMenus") {
            return std::make_unique<ToggleMenusAction>();
        }

        if (*type == "RechargeWeapon") {
            bool preferSmaller = true;
            if (const auto* preferSmallerStr = FindField(a_fields, "PreferSmaller")) {
                preferSmaller = (*preferSmallerStr == "1");
            }
            std::uint8_t maxSize = 5;
            if (const auto* maxSizeStr = FindField(a_fields, "MaxSize")) {
                auto parsed = std::atoi(maxSizeStr->c_str());
                if (parsed >= 0 && parsed <= 5) {
                    maxSize = static_cast<std::uint8_t>(parsed);
                }
            }
            bool notify = false;
            if (const auto* notifyStr = FindField(a_fields, "Notify")) {
                notify = (*notifyStr == "1");
            }
            return std::make_unique<RechargeWeaponAction>(preferSmaller, maxSize, notify);
        }

        if (*type == "RechargeWeaponLeftHand") {
            bool preferSmaller = true;
            if (const auto* preferSmallerStr = FindField(a_fields, "PreferSmaller")) {
                preferSmaller = (*preferSmallerStr == "1");
            }
            std::uint8_t maxSize = 5;
            if (const auto* maxSizeStr = FindField(a_fields, "MaxSize")) {
                auto parsed = std::atoi(maxSizeStr->c_str());
                if (parsed >= 0 && parsed <= 5) {
                    maxSize = static_cast<std::uint8_t>(parsed);
                }
            }
            bool notify = false;
            if (const auto* notifyStr = FindField(a_fields, "Notify")) {
                notify = (*notifyStr == "1");
            }
            return std::make_unique<RechargeWeaponLeftHandAction>(preferSmaller, maxSize, notify);
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

        if (*type == "OpenMenu") {
            const auto* targetStr = FindField(a_fields, "Target");
            if (!targetStr) {
                return nullptr;
            }
            auto target = ParseOpenMenuTarget(*targetStr);
            if (!target) {
                return nullptr;
            }
            return std::make_unique<OpenMenuAction>(*target);
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
