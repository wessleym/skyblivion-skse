#include "SpellReflect/Feedback.h"

#include "SpellReflect/Forms.h"
#include "SpellReflect/Settings.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

using namespace std::literals;

namespace SpellReflect::Feedback {
    namespace {
        using Clock = std::chrono::steady_clock;

        constexpr auto kRepeatGap = 250ms;
        constexpr auto kEvents = std::to_underlying(Event::kTotal);

        struct Records {
            RE::BGSSoundDescriptorForm* sound = nullptr;
            RE::BGSArtObject*           art = nullptr;
            RE::TESEffectShader*        shader = nullptr;
        };
        std::array<Records, kEvents> g_records;

        // main thread only (touched inside tasks)
        std::unordered_map<std::uint64_t, Clock::time_point> g_lastPlayed;

        std::string_view Name(Event a_event) {
            switch (a_event) {
                case Event::kSpellReflect:
                    return "spell reflect"sv;
                case Event::kArrowReflect:
                    return "arrow reflect"sv;
                case Event::kTimedBlock:
                    return "timed block"sv;
                default:
                    return "?"sv;
            }
        }

        Records ResolveRecords(Event a_event, const std::string& a_sound, const std::string& a_visual, RE::BGSListForm* a_sounds) {
            Records r;
            const auto what = std::string(Name(a_event));
            if (!a_sound.empty()) {
                r.sound = Forms::Resolve<RE::BGSSoundDescriptorForm>(a_sound, what + " sound");
            } else if (const auto index = std::to_underlying(a_event); a_sounds && index < a_sounds->forms.size()) {
                const auto entry = a_sounds->forms[index];
                r.sound = entry ? entry->As<RE::BGSSoundDescriptorForm>() : nullptr;
                if (r.sound) {
                    Log::INFO("SpellReflect: {} sound: sound list entry {} = {:08X}", what, index, r.sound->GetFormID());
                } else {
                    Log::WARN("SpellReflect: {} sound: sound list entry {} is not a sound descriptor", what, index);
                }
            } else {
                Log::WARN("SpellReflect: {} sound: none (no sound list entry {})", what, std::to_underlying(a_event));
            }
            if (const auto visual = Forms::Resolve(a_visual)) {
                r.art = visual->As<RE::BGSArtObject>();
                r.shader = visual->As<RE::TESEffectShader>();
                if (r.art || r.shader) {
                    Log::INFO("SpellReflect: {} visual: {} {:08X} ({})", what, a_visual, visual->GetFormID(), r.art ? "art object" : "effect shader");
                } else {
                    Log::WARN("SpellReflect: {} visual: '{}' is neither an art object nor an effect shader", what, a_visual);
                }
            } else if (!a_visual.empty()) {
                Log::WARN("SpellReflect: {} visual: '{}' not found", what, a_visual);
            }
            return r;
        }

        void PlayNow(Event a_event, RE::ActorHandle a_handle) {
            const auto actorPtr = REBridge::Lookup(a_handle);
            const auto actor = actorPtr.get();
            if (!actor) {
                return;
            }
            const auto key = (static_cast<std::uint64_t>(actor->GetFormID()) << 8) | std::to_underlying(a_event);
            const auto now = Clock::now();
            if (const auto it = g_lastPlayed.find(key); it != g_lastPlayed.end() && now - it->second < kRepeatGap) {
                return;
            }
            g_lastPlayed[key] = now;
            if (g_lastPlayed.size() > 64) {
                std::erase_if(g_lastPlayed, [&](const auto& a_entry) { return now - a_entry.second > 5s; });
            }

            const auto& r = g_records[std::to_underlying(a_event)];
            const auto  root = actor->Get3D();
            if (r.sound && root) {
                RE::BSSoundHandle sound;
                const auto        audio = RE::BSAudioManager::GetSingleton();
                if (audio && audio->GetSoundHandle(sound, r.sound)) {
                    sound.SetObjectToFollow(root);
                    sound.Play();
                }
            }
            const float duration = Settings::Get().visualDuration;
            if (r.art) {
                actor->ApplyArtObject(r.art, duration, nullptr, false, false);
            } else if (r.shader) {
                actor->ApplyEffectShader(r.shader, duration);
            }
        }
    }

    void OnDataLoaded() {
        const auto& s = Settings::Get();
        const auto  sounds = Forms::Resolve<RE::BGSListForm>(s.soundList, "feedback sound list");
        g_records[std::to_underlying(Event::kSpellReflect)] = ResolveRecords(Event::kSpellReflect, s.spellReflectSound, s.spellReflectVisual, sounds);
        g_records[std::to_underlying(Event::kArrowReflect)] = ResolveRecords(Event::kArrowReflect, s.arrowReflectSound, s.arrowReflectVisual, sounds);
        g_records[std::to_underlying(Event::kTimedBlock)] = ResolveRecords(Event::kTimedBlock, s.timedBlockSound, s.timedBlockVisual, sounds);
    }

    void Play(Event a_event, RE::Actor* a_actor) {
        const auto tasks = SKSE::GetTaskInterface();
        if (!a_actor || !tasks) {
            return;
        }
        const auto handle = a_actor->GetHandle();
        tasks->AddTask([a_event, handle]() { PlayNow(a_event, handle); });
    }
}
