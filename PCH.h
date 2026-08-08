#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

using namespace std::literals;

namespace stl {
    using namespace SKSE::stl;

    // Installs a trampoline hook that replaces a CALL instruction at a fixed
    // address with a call to T::thunk instead, saving the original target in
    // T::func so the thunk can still invoke it - the standard CommonLibSSE
    // pattern for hooking a specific call site inside the game's own code
    // (as opposed to a whole function or a vtable slot). Ported from Josh's
    // own CamDirector project (its PCH.h), where this same helper backs its
    // ProcessInput hook - see InputDispatchHook.cpp for True Hotkeys' use of
    // it, hooking the identical relocation ID/offset CamDirector already
    // proved works for exactly this purpose (splicing specific keys out of
    // the frame's input-event chain before anything else can see them).
    //
    // Usage:
    //   struct MyHook {
    //       static void thunk(RE::SomeType* a_arg) {
    //           func(a_arg);  // call the original implementation
    //           ...           // do our own work
    //       }
    //       static inline REL::Relocation<decltype(thunk)> func;
    //       static inline constexpr std::size_t size{ 5 };  // bytes of the original CALL instruction
    //   };
    //   stl::write_thunk_call<MyHook>(address);
    template <class T>
    void write_thunk_call(std::uintptr_t a_src) {
        auto& trampoline = SKSE::GetTrampoline();
        SKSE::AllocTrampoline(14);
        T::func = trampoline.write_call<T::size>(a_src, T::thunk);
    }
}
