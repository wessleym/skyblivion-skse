#include "PerkStick.h"
#include "PerksContract.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"

namespace PerksView {

	namespace {
		PerkStick s_stick;
	}

	void PerkStick::Register() {
		if (!ModEvents::AddSink(&s_stick)) {
			Log::WARN("PerkStick: No mod-event source. The perk menu's right-stick zoom is off.");
			return;
		}
		auto* input = RE::BSInputDeviceManager::GetSingleton();
		if (!input) {
			Log::WARN("PerkStick: No input source. The perk menu's right-stick zoom is off.");
			return;
		}
		input->AddEventSink<RE::InputEvent*>(&s_stick);
		Log::INFO("PerkStick: Registered.");
	}

	RE::BSEventNotifyControl PerkStick::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event || a_event->eventName != PerksContract::ModEvent::WatchStick) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const std::string arg = ModEvents::StrArg(*a_event);
		const auto        bar = arg.find('|');
		std::lock_guard   guard(_lock);
		if (bar == std::string::npos || bar == 0 || bar + 1 >= arg.size()) {
			Log::WARN("PerkStick: {} needs '<menu>|<function>', got '{}'", PerksContract::ModEvent::WatchStick, arg);
			_menu.clear();
			_function.clear();
			return RE::BSEventNotifyControl::kContinue;
		}
		_menu = arg.substr(0, bar);
		_function = arg.substr(bar + 1);
		_x = _y = 0.0f;
		Log::INFO("PerkStick: right stick -> {} ({})", _menu, _function);
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl PerkStick::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		for (auto* e = *a_event; e; e = e->next) {
			auto* stick = e->AsThumbstickEvent();
			if (!stick || !stick->IsRight()) {
				continue;
			}
			std::lock_guard guard(_lock);
			if (_function.empty()) {
				break;
			}
			_x = stick->xValue;
			_y = stick->yValue;
			if (!_posted) {
				_posted = true;
				MainThread::Post([] { s_stick.Deliver(); });
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	void PerkStick::Deliver() {
		std::string menu;
		std::string function;
		float       x;
		float       y;
		{
			std::lock_guard guard(_lock);
			_posted = false;
			menu = _menu;
			function = _function;
			x = _x;
			y = _y;
		}
		if (function.empty()) {
			return;
		}
		auto  ui = RE::UI::GetSingleton();
		auto  m = ui && ui->IsMenuOpen(menu) ? ui->GetMenu(menu) : nullptr;
		auto  movie = m ? m->uiMovie : nullptr;
		bool  ok = false;
		if (movie) {
			RE::GFxValue args[2] = { RE::GFxValue(static_cast<double>(x)), RE::GFxValue(static_cast<double>(y)) };
			ok = movie->Invoke(function.c_str(), nullptr, args, 2);
		}
		if (!ok) {
			std::lock_guard guard(_lock);
			if (_function == function) {
				_menu.clear();
				_function.clear();
			}
			Log::INFO("PerkStick: {} is gone; right stick released", function);
		}
	}

}
