#include "Hotkeys/EditorIDLookup.h"

#include <Windows.h>

namespace Hotkeys::EditorIDLookup {
    namespace {
        using GetFormEditorIDFn = const char* (*)(std::uint32_t);

        [[nodiscard]] GetFormEditorIDFn ResolvePo3Tweaks() {
            static GetFormEditorIDFn function = [] {
                HMODULE module = GetModuleHandleA("po3_Tweaks");
                if (!module) {
                    return static_cast<GetFormEditorIDFn>(nullptr);
                }
                return reinterpret_cast<GetFormEditorIDFn>(GetProcAddress(module, "GetFormEditorID"));
            }();
            return function;
        }
    }

    std::string Get(std::uint32_t a_formID) {
        if (auto* function = ResolvePo3Tweaks()) {
            if (const char* editorID = function(a_formID); editorID && editorID[0] != '\0') {
                return editorID;
            }
        }
        if (auto* form = RE::TESForm::LookupByID(a_formID)) {
            std::string_view editorID = form->GetFormEditorID();
            if (!editorID.empty()) {
                return std::string(editorID);
            }
        }
        return {};
    }
}
