#include "SpellReflect/BlockTiming.h"

#include "SpellReflect/Settings.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <utility>

using namespace std::literals;

namespace SpellReflect::BlockTiming {
    namespace {
        using Clock = std::chrono::steady_clock;

        // Clock ticks of the last press; 0 = never. Written by the input
        // thread, read by projectile hits.
        std::atomic<Clock::rep> g_lastPress{ 0 };
        RE::ActorValue          g_windowAV = RE::ActorValue::kNone;

        class InputSink final : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            static InputSink* GetSingleton() {
                static InputSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                                  RE::BSTEventSource<RE::InputEvent*>*) override {
                const auto userEvents = RE::UserEvents::GetSingleton();
                if (!a_event || !userEvents) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                for (auto event = *a_event; event; event = event->next) {
                    const auto button = event->AsButtonEvent();
                    if (button && button->IsDown() && button->QUserEvent() == userEvents->leftAttack) {
                        g_lastPress.store(Clock::now().time_since_epoch().count(), std::memory_order_relaxed);
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void Register() {
        const auto input = RE::BSInputDeviceManager::GetSingleton();
        if (!input) {
            Log::ERROR("SpellReflect: block timing: no input manager; timed arrow reflection and timed blocks are off");
            return;
        }
        input->AddEventSink(InputSink::GetSingleton());
        Log::INFO("SpellReflect: block timing: listening for Left Attack/Block presses");
    }

    void OnDataLoaded() {
        const auto& name = Settings::Get().windowActorValue;
        g_windowAV = REBridge::ActorValueByName(name.c_str());
        if (g_windowAV == RE::ActorValue::kNone) {
            Log::WARN("SpellReflect: window actor value '{}' not found; the window is fWindowBase only", name);
        } else {
            Log::INFO("SpellReflect: window actor value: {} ({})", name, std::to_underlying(g_windowAV));
        }
    }

    float SecondsSinceBlockPress() {
        const auto last = g_lastPress.load(std::memory_order_relaxed);
        if (last == 0) {
            return -1.0f;
        }
        const auto elapsed = Clock::now().time_since_epoch() - Clock::duration(last);
        return std::chrono::duration<float>(elapsed).count();
    }

    float WindowPoints(RE::Actor* a_actor) {
        return g_windowAV != RE::ActorValue::kNone ? REBridge::AVOwner(a_actor)->GetActorValue(g_windowAV) : 0.0f;
    }

    float Window(RE::Actor* a_actor) {
        const auto& s = Settings::Get();
        return std::clamp(s.windowBase + WindowPoints(a_actor) * s.windowPerPoint, 0.0f, s.windowMax);
    }
}
