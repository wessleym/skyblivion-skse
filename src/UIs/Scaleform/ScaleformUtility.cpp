#include "ScaleformUtility.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"

#include <thread>

RE::GPtr<RE::GFxMovieView> ScaleformUtility::OpenMenuMovie(std::string_view menuName) {
	auto* ui = RE::UI::GetSingleton();
	if (!ui || !ui->IsMenuOpen(menuName)) {
		return nullptr;
	}
	auto menu = ui->GetMenu(menuName);
	return menu ? menu->uiMovie : nullptr;
}

bool ScaleformUtility::MovieExists(std::string_view a_name) {
	RE::BSResourceNiBinaryStream swf{ std::format("Interface/{}.swf", a_name) };
	return swf.good();
}

long long ScaleformUtility::MicrosecondsSince(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}

void ScaleformUtility::RetryOnUIThread(int attempts, std::chrono::milliseconds interval,
	std::function<bool()> attempt, std::function<void()> onGiveUp) {
	MainThread::Post([attempts, interval, attempt, onGiveUp] {
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

std::optional<MenuCallback> MenuCallback::Open(const std::string& request, std::string_view source) {
	const auto pieces = ModEvents::Split(request, '|');
	if (pieces.size() < 2 || pieces[0].empty() || pieces[1].empty()) {
		Log::WARN("{}: Malformed request '{}'.", source, request);
		return std::nullopt;
	}
	auto movie = ScaleformUtility::OpenMenuMovie(pieces[0]);
	if (!movie) {
		return std::nullopt;
	}
	return MenuCallback{ std::move(movie), pieces[1] };
}

void MenuCallback::Invoke(const RE::GFxValue& value) const {
	movie->Invoke(target.c_str(), nullptr, &value, 1);
}
