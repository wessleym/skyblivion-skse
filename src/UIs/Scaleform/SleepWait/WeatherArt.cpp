#include "WeatherArt.h"
#include "SleepWaitContract.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <atomic>
#include <chrono>
#include <optional>

namespace SleepWait {

	namespace {
		using Art = SleepWaitContract::Art;
		using Flag = RE::TESWeather::WeatherDataFlag;

		//The class read the last time the sky had a weather; used when it has none.
		std::atomic<int> g_lastArt{ static_cast<int>(Art::Clear) };

		//The menu's movie has run its first frame by the next frame; the retries cover a slow first advance.
		constexpr int kAttempts = 5;
		constexpr std::chrono::milliseconds kRetryInterval{ 50 };

		std::optional<Art> Classify(const RE::TESWeather* weather) {
			if (!weather) {
				return std::nullopt;
			}
			const auto& flags = weather->data.flags;
			if (flags.any(Flag::kPleasant)) {
				return Art::Clear;
			}
			if (flags.any(Flag::kCloudy)) {
				return Art::Cloudy;
			}
			if (flags.any(Flag::kRainy)) {
				return Art::Rain;
			}
			if (flags.any(Flag::kSnow)) {
				return Art::Snow;
			}
			return std::nullopt;
		}

		const char* ArtName(Art art) {
			switch (art) {
			case Art::Clear:
				return "clear";
			case Art::Cloudy:
				return "cloudy";
			case Art::Rain:
				return "rain";
			case Art::Snow:
				return "snow";
			}
			return "?";
		}

		const char* EditorID(const RE::TESWeather* weather) {
			const char* id = weather ? weather->GetFormEditorID() : nullptr;
			return id && *id ? id : "-";
		}

		//The picture for the sky as it is now, and a line saying why.
		Art CurrentArt(std::string& why) {
			auto* sky = RE::Sky::GetSingleton();
			auto* current = sky ? sky->currentWeather : nullptr;
			auto* last = sky ? sky->lastWeather : nullptr;
			const float pct = sky ? sky->currentWeatherPct : 1.0f;

			//The weather that dominates the picture: the incoming one from halfway through a change.
			const bool leaving = last && last != current && pct < 0.5f;
			const auto* first = leaving ? last : current;
			const auto* second = leaving ? current : last;

			std::optional<Art> art = Classify(first);
			if (!art && second && second != first) {
				art = Classify(second);
			}
			if (!art && (first || second)) {
				art = Art::Cloudy;
			}

			if (art) {
				g_lastArt = static_cast<int>(*art);
				why = std::format("sky {:08X} {} (last {:08X} {}, {:.0f}% in)", current ? current->GetFormID() : 0,
					EditorID(current), last ? last->GetFormID() : 0, EditorID(last), pct * 100.0f);
				return *art;
			}
			why = "no sky weather; last class read";
			return static_cast<Art>(g_lastArt.load());
		}
	}

	void WeatherArt::Register() {
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			Log::WARN("WeatherArt: No UI. The wait menu will show no weather picture.");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&s_sink);
		Log::INFO("WeatherArt: Registered.");
	}

	void WeatherArt::Push() {
		std::string why;
		const Art art = CurrentArt(why);
		ScaleformUtility::RetryOnMainThread(
			kAttempts, kRetryInterval,
			[art, why] {
				auto movie = ScaleformUtility::OpenMenuMovie(RE::SleepWaitMenu::MENU_NAME);
				if (!movie) {
					return true;  //closed already: nothing to do
				}
				RE::GFxValue clip;
				if (!movie->GetVariable(&clip, SleepWaitContract::AsPath::WeatherClip) || !clip.IsDisplayObject()) {
					return false;
				}
				RE::GFxValue arg(static_cast<double>(static_cast<int>(art)));
				movie->Invoke(SleepWaitContract::AsPath::SetWeather, nullptr, &arg, 1);
				Log::INFO("WeatherArt: {} ({}).", ArtName(art), why);
				return true;
			},
			[] { Log::INFO("WeatherArt: The wait menu has no WeatherBG clip; no weather picture (older sleepwaitmenu.swf)."); });
	}

	RE::BSEventNotifyControl WeatherArt::Sink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
		if (a_event && a_event->opening && a_event->menuName == RE::SleepWaitMenu::MENU_NAME) {
			Push();
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
