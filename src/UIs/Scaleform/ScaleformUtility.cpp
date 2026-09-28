#include "ScaleformUtility.h"
#include "Papyrus/ModEvents.h"

#include <thread>

RE::GPtr<RE::GFxMovieView> ScaleformUtility::OpenMenuMovie(std::string_view menuName) {
	auto* ui = RE::UI::GetSingleton();
	if (!ui || !ui->IsMenuOpen(menuName)) {
		return nullptr;
	}
	auto menu = ui->GetMenu(menuName);
	return menu ? menu->uiMovie : nullptr;
}

long long ScaleformUtility::MicrosecondsSince(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}

void ScaleformUtility::RetryOnUIThread(int attempts, std::chrono::milliseconds interval,
	std::function<bool()> attempt, std::function<void()> onGiveUp) {
	SKSE::GetTaskInterface()->AddUITask([attempts, interval, attempt, onGiveUp] {
		if (attempt()) {
			return;
		}
		if (attempts <= 1) {
			if (onGiveUp) {
				onGiveUp();
			}
			return;
		}
		std::thread([attempts, interval, attempt, onGiveUp] {
			std::this_thread::sleep_for(interval);
			RetryOnUIThread(attempts - 1, interval, attempt, onGiveUp);
		}).detach();
	});
}

std::optional<FeedRequest> FeedRequest::Open(const std::string& request, Mode modeField, std::string_view source) {
	const auto pieces = ModEvents::Split(request, ',');
	const std::size_t needed = modeField == Mode::Required ? 3 : 2;
	if (pieces.size() < needed) {
		Log::WARN("{}: Malformed request '{}'.", source, request);
		return std::nullopt;
	}
	auto movie = ScaleformUtility::OpenMenuMovie(pieces[0]);
	if (!movie) {
		return std::nullopt;
	}
	const bool readsMode = modeField == Mode::Required || (modeField == Mode::Optional && pieces.size() >= 3);
	return FeedRequest{ std::move(movie), pieces[1], readsMode ? pieces[2] : "function" };
}

bool FeedRequest::Push(const RE::GFxValue& value) const {
	if (mode == "function") {
		movie->Invoke(target.c_str(), nullptr, &value, 1);
		return true;
	}
	if (mode == "variable") {
		movie->SetVariable(target.c_str(), value);
		return true;
	}
	Log::INFO("FeedRequest: Unknown push mode '{}'. Nothing pushed to {}.", mode, target);
	return false;
}
