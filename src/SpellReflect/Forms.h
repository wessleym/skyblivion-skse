#pragma once

#include <string>
#include <string_view>

// INI form references: an editor ID (survives ESM merges and renumbering), or
// "Plugin.esp|0xFormID". Empty = none.

namespace SpellReflect::Forms {
    inline RE::TESForm* Resolve(const std::string& a_spec) {
        if (a_spec.empty()) {
            return nullptr;
        }
        if (const auto bar = a_spec.find('|'); bar != std::string::npos) {
            try {
                const auto id = static_cast<RE::FormID>(std::stoul(a_spec.substr(bar + 1), nullptr, 16));
                return RE::TESDataHandler::GetSingleton()->LookupForm(id, a_spec.substr(0, bar));
            } catch (...) {
                return nullptr;
            }
        }
        return RE::TESForm::LookupByEditorID(a_spec);
    }

    template <class T>
    T* Resolve(const std::string& a_spec, std::string_view a_what) {
        const auto form = Resolve(a_spec);
        const auto typed = form ? form->As<T>() : nullptr;
        if (typed) {
            Log::INFO("SpellReflect: {}: {} {:08X}", a_what, a_spec, typed->GetFormID());
        } else if (!a_spec.empty()) {
            Log::WARN("SpellReflect: {}: '{}' {}", a_what, a_spec, form ? "has the wrong record type" : "not found");
        }
        return typed;
    }
}
