#include "ScaleformUtility.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"

#include <thread>

#include <nlohmann/json.hpp>

RE::GPtr<RE::GFxMovieView> ScaleformUtility::OpenMenuMovie(std::string_view menuName) {
	auto* ui = RE::UI::GetSingleton();
	if (!ui || !ui->IsMenuOpen(menuName)) {
		return nullptr;
	}
	auto menu = ui->GetMenu(menuName);
	return menu ? menu->uiMovie : nullptr;
}

void ScaleformUtility::LoadMenuMovie(RE::IMenu& a_menu, std::string_view a_file, std::int8_t a_depth, bool a_pausesGame, void (*a_close)()) {
	auto*      menu = std::addressof(a_menu);
	const bool loaded = RE::BSScaleformManager::GetSingleton()->LoadMovieEx(menu, a_file, [menu](RE::GFxMovieDef* a_def) {
		menu->fxDelegate.reset(new RE::FxDelegate());
		menu->fxDelegate->RegisterHandler(menu);
		a_def->SetState(RE::GFxState::StateType::kExternalInterface, menu->fxDelegate.get());
		menu->fxDelegate->Release();
	});
	if (!loaded) {
		Log::ERROR("Interface\\{}.swf failed to load - its menu is closed", a_file);
		a_close();
	}

	using Flag = RE::UI_MENU_FLAGS;
	menu->depthPriority = a_depth;
	menu->inputContext = RE::IMenu::Context::kMenuMode;
	menu->menuFlags.set(Flag::kDisablePauseMenu, Flag::kModal, Flag::kUsesMenuContext, Flag::kTopmostRenderedMenu,
		Flag::kRequiresUpdate, Flag::kUpdateUsesCursor);
	if (a_pausesGame) {
		menu->menuFlags.set(Flag::kPausesGame);
	}
	if (!GamepadActive()) {
		menu->menuFlags.set(Flag::kUsesCursor);
	}
}

bool ScaleformUtility::CanShowMenu(std::string_view a_menu, std::string_view a_file) {
	auto* ui = RE::UI::GetSingleton();
	if (!ui || !RE::UIMessageQueue::GetSingleton() || ui->IsMenuOpen(a_menu)) {
		return false;
	}
	RE::BSResourceNiBinaryStream swf{ std::format("Interface/{}.swf", a_file) };
	if (!swf.good()) {
		Log::ERROR("Interface\\{}.swf not found - {} not opened", a_file, a_menu);
		return false;
	}
	return true;
}

void ScaleformUtility::ShowMenu(std::string_view a_menu) {
	if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
		queue->AddMessage(a_menu, RE::UI_MESSAGE_TYPE::kShow, nullptr);
	}
}

void ScaleformUtility::HideMenu(std::string_view a_menu) {
	if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
		queue->AddMessage(a_menu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
	}
}

RE::GFxValue ScaleformUtility::ToGFx(RE::GFxMovieView& a_movie, const nlohmann::json& a_json) {
	RE::GFxValue out;
	if (a_json.is_object()) {
		a_movie.CreateObject(&out);
		for (const auto& [key, value] : a_json.items()) {
			out.SetMember(key.c_str(), ToGFx(a_movie, value));
		}
	} else if (a_json.is_array()) {
		a_movie.CreateArray(&out);
		for (const auto& value : a_json) {
			out.PushBack(ToGFx(a_movie, value));
		}
	} else if (a_json.is_string()) {
		a_movie.CreateString(&out, a_json.get_ref<const std::string&>().c_str());
	} else if (a_json.is_boolean()) {
		out.SetBoolean(a_json.get<bool>());
	} else if (a_json.is_number()) {
		out.SetNumber(a_json.get<double>());
	} else {
		out.SetNull();
	}
	return out;
}

bool ScaleformUtility::GamepadActive() {
	auto* input = RE::BSInputDeviceManager::GetSingleton();
	return input && input->IsGamepadEnabled();
}

long long ScaleformUtility::MicrosecondsSince(std::chrono::steady_clock::time_point start) {
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}

void ScaleformUtility::RetryOnMainThread(int attempts, std::chrono::milliseconds interval,
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
			RetryOnMainThread(attempts - 1, interval, attempt, onGiveUp);
		}).detach();
	});
}

std::optional<FeedRequest> FeedRequest::Open(const std::string& request, Mode modeField, std::string_view source) {
	const auto pieces = ModEvents::Split(request, ',');
	if (pieces.size() < 2) {
		Log::WARN("{}: Malformed request '{}'.", source, request);
		return std::nullopt;
	}
	auto movie = ScaleformUtility::OpenMenuMovie(pieces[0]);
	if (!movie) {
		return std::nullopt;
	}
	const bool readsMode = modeField == Mode::Optional && pieces.size() >= 3;
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
