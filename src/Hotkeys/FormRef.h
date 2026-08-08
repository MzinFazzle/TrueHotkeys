#pragma once


#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace Hotkeys {
    struct FormRef {
        std::string plugin;
        std::uint32_t localFormID = 0;

        [[nodiscard]] bool IsValid() const noexcept { return !plugin.empty() && localFormID != 0; }

        template <class T = RE::TESForm>
        [[nodiscard]] T* Resolve() const {
            if (!IsValid()) {
                return nullptr;
            }
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return nullptr;
            }
            auto* form = dataHandler->LookupForm(static_cast<RE::FormID>(localFormID), plugin);
            if (!form) {
                return nullptr;
            }
            if constexpr (std::is_same_v<T, RE::TESForm>) {
                return form;
            } else {
                return form->As<T>();
            }
        }

        [[nodiscard]] std::string ToString() const { return std::format("{}:0x{:08X}", plugin, localFormID); }

        [[nodiscard]] std::string ToDisplayString() const {
            if (auto* form = Resolve<RE::TESForm>()) {
                if (auto* named = form->As<RE::TESFullName>()) {
                    const char* name = named->GetFullName();
                    if (name && name[0] != '\0') {
                        return name;
                    }
                }
            }
            return ToString();
        }

        [[nodiscard]] static std::optional<FormRef> Parse(std::string_view a_str) {
            auto colon = a_str.rfind(':');
            if (colon == std::string_view::npos || colon == 0 || colon + 1 >= a_str.size()) {
                return std::nullopt;
            }

            FormRef ref;
            ref.plugin = std::string(a_str.substr(0, colon));

            auto idStr = a_str.substr(colon + 1);
            if (idStr.starts_with("0x") || idStr.starts_with("0X")) {
                idStr.remove_prefix(2);
            }
            if (idStr.empty()) {
                return std::nullopt;
            }

            std::uint32_t id = 0;
            auto result = std::from_chars(idStr.data(), idStr.data() + idStr.size(), id, 16);
            if (result.ec != std::errc{}) {
                return std::nullopt;
            }

            ref.localFormID = id;
            return ref;
        }
    };
}
