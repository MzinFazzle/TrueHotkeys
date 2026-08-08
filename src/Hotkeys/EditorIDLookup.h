#pragma once


#include <cstdint>
#include <string>

namespace Hotkeys::EditorIDLookup {
    [[nodiscard]] std::string Get(std::uint32_t a_formID);
}
