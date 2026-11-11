#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Finds engine call sites at run time instead of trusting a fixed offset per
// game version. A hook point is named by two addresses (Address Library ids, or
// VR offsets where VR's library lacks the id): the function that holds the call
// and the function it calls. The scan walks the game's
// .text for every `call rel32` (E8) to the callee and keeps the ones inside the
// caller. "Inside" uses the exe's own unwind table (.pdata): a call in a
// split-off cold chunk still belongs to its parent function.
//
// Checked offline on 1.7.104 (2026-10-24): the six sites resolve to exactly one
// call each, and two of them moved from their 1.6.1170 offsets.

namespace SpellReflect::CallScan {
    struct Call {
        std::uintptr_t site;      // address of the E8
        std::uintptr_t function;  // start of the function holding it (0 = no unwind entry)
    };

    // Every direct call of a_callee in .text.
    std::vector<Call> FindCalls(std::uintptr_t a_callee);

    // A known offset of the call inside its caller for one game version. Only
    // used when the scan finds nothing: if that spot is an E8 into code outside
    // .text, another plugin has already hooked it and we chain onto it.
    struct Hint {
        REL::Version   runtime;
        std::ptrdiff_t offset;
    };

    // The single call of a_callee inside the function at a_caller, from a_calls
    // (FindCalls of that callee). Logs what it found under a_tag, naming the caller
    // a_callerName; nullopt when there is none or more than one.
    std::optional<std::uintptr_t> FindCallIn(std::string_view a_tag, std::string_view a_callerName, std::uintptr_t a_caller,
                                             std::uintptr_t a_callee, const std::vector<Call>& a_calls,
                                             std::span<const Hint> a_hints);

    // Start of the primary function holding a_address, or 0.
    std::uintptr_t FunctionStart(std::uintptr_t a_address);
}
