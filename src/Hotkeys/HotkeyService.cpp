#include "Hotkeys/HotkeyService.h"

#include <charconv>

#include "Platform/MainThread.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "Settings/SettingsService.h"

using namespace SKYBSettingsMenu;

namespace {
	// DirectInput scan code -> the Flash key code a menu sees. Numpad keys are
	// left out on purpose: Skyrim's menus use 96..107 for gamepad buttons.
	int DikToFlash(std::uint32_t a_dik) {
		static const std::unordered_map<std::uint32_t, int> map = {
			{0x01, 27},  {0x02, 49},  {0x03, 50},  {0x04, 51},  {0x05, 52},  {0x06, 53},  {0x07, 54},  {0x08, 55},
			{0x09, 56},  {0x0A, 57},  {0x0B, 48},  {0x0C, 189}, {0x0D, 187}, {0x0E, 8},   {0x0F, 9},   {0x10, 81},
			{0x11, 87},  {0x12, 69},  {0x13, 82},  {0x14, 84},  {0x15, 89},  {0x16, 85},  {0x17, 73},  {0x18, 79},
			{0x19, 80},  {0x1A, 219}, {0x1B, 221}, {0x1C, 13},  {0x1D, 17},  {0x1E, 65},  {0x1F, 83},  {0x20, 68},
			{0x21, 70},  {0x22, 71},  {0x23, 72},  {0x24, 74},  {0x25, 75},  {0x26, 76},  {0x27, 186}, {0x28, 222},
			{0x29, 192}, {0x2A, 16},  {0x2B, 220}, {0x2C, 90},  {0x2D, 88},  {0x2E, 67},  {0x2F, 86},  {0x30, 66},
			{0x31, 78},  {0x32, 77},  {0x33, 188}, {0x34, 190}, {0x35, 191}, {0x36, 16},  {0x38, 18},  {0x39, 32},
			{0x3A, 20},  {0x3B, 112}, {0x3C, 113}, {0x3D, 114}, {0x3E, 115}, {0x3F, 116}, {0x40, 117}, {0x41, 118},
			{0x42, 119}, {0x43, 120}, {0x44, 121}, {0x46, 145}, {0x57, 122}, {0x58, 123}, {0x9D, 17},  {0xB8, 18},
			{0xC5, 19},  {0xC7, 36},  {0xC8, 38},  {0xC9, 33},  {0xCB, 37},  {0xCD, 39},  {0xCF, 35},  {0xD0, 40},
			{0xD1, 34},  {0xD2, 45},  {0xD3, 46},
		};
		const auto it = map.find(a_dik);
		return it != map.end() ? it->second : -1;
	}

	// XInput button mask (CommonLib's gamepad idCode) -> menu code 96..107.
	int PadToFlash(std::uint32_t a_mask) {
		using K = RE::BSWin32GamepadDevice::Keys;
		switch (a_mask) {
			case K::kA: return 96;
			case K::kB: return 97;
			case K::kX: return 98;
			case K::kY: return 99;
			case K::kLeftShoulder: return 100;
			case K::kLeftTrigger: return 101;
			case K::kLeftThumb: return 102;
			case K::kRightShoulder: return 103;
			case K::kRightTrigger: return 104;
			case K::kRightThumb: return 105;
			case K::kStart: return 106;
			case K::kBack: return 107;
			default: return -1;  // d-pad: the menus' own navigation
		}
	}

	// XInput button mask -> SKSE's gamepad code (Papyrus RegisterForKey, QuickLoot)
	int PadToSkse(std::uint32_t a_mask) {
		using K = RE::BSWin32GamepadDevice::Keys;
		switch (a_mask) {
			case K::kUp: return 266;
			case K::kDown: return 267;
			case K::kLeft: return 268;
			case K::kRight: return 269;
			case K::kStart: return 270;
			case K::kBack: return 271;
			case K::kLeftThumb: return 272;
			case K::kRightThumb: return 273;
			case K::kLeftShoulder: return 274;
			case K::kRightShoulder: return 275;
			case K::kA: return 276;
			case K::kB: return 277;
			case K::kX: return 278;
			case K::kY: return 279;
			case K::kLeftTrigger: return 280;
			case K::kRightTrigger: return 281;
			default: return -1;
		}
	}

	int ParseInt(std::string_view a_s, int a_fallback) {
		int v = 0;
		const auto [ptr, err] = std::from_chars(a_s.data(), a_s.data() + a_s.size(), v);
		return err == std::errc{} && ptr == a_s.data() + a_s.size() ? v : a_fallback;
	}
}

HotkeyService* HotkeyService::GetSingleton() {
	static HotkeyService singleton;
	return &singleton;
}

void HotkeyService::Register() {
	if (auto src = SKSE::GetModCallbackEventSource()) {
		src->AddEventSink<SKSE::ModCallbackEvent>(this);
	}
	if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
		input->AddEventSink<RE::InputEvent*>(this);
	}
	Log::INFO("hotkeys: {} action(s) in {} menu(s); answering {}", kHotkeys.size(), kHotkeyGroups.size(), kRequestEvent);
	DropSharedMemberEntries();
}

// A row that follows a shared action on every device drops its old [Hotkeys]
// entry; a row that still binds one device keeps it (only that device is read).
void HotkeyService::DropSharedMemberEntries() {
	std::size_t n = 0;
	for (const auto& h : kHotkeys) {
		if (h.shareMask != 0 && !Listed(h) && Settings::SettingsService::RemoveKey(kSection, h.id)) {
			++n;
		}
	}
	if (n > 0) {
		Log::INFO("hotkeys: {} old per-menu binding(s) dropped - those actions follow a shared one now", n);
	}
}

const HotkeyDef* HotkeyService::Find(std::string_view a_id) {
	for (const auto& h : kHotkeys) {
		if (a_id == h.id) {
			return &h;
		}
	}
	return nullptr;
}

const HotkeyGroupDef* HotkeyService::Group(std::string_view a_id) {
	for (const auto& g : kHotkeyGroups) {
		if (a_id == g.id) {
			return &g;
		}
	}
	return nullptr;
}

const KeyName* HotkeyService::Name(int a_code, bool a_game) {
	// the d-pad has an SKSE code but no entry: it stays the game's own
	for (const auto& k : a_game ? kGameKeyNames : kKeyNames) {
		if (k.code == a_code) {
			return &k;
		}
	}
	return nullptr;
}

bool HotkeyService::IsGame(const HotkeyDef& a_def) {
	const auto* g = Group(a_def.group);
	return g && g->game;
}

bool HotkeyService::IsShared(const HotkeyDef& a_def) {
	return a_def.group == kSharedGroup;
}

int HotkeyService::OwnDevices(const HotkeyDef& a_def) {
	const int devices = a_def.devices == 1 ? 1 : a_def.devices == 2 ? 2 : 3;
	if (a_def.shareMask == 0) {
		return devices;
	}
	// a member binds itself only where it doesn't share and has a key today
	const int defaults = (a_def.pc >= 0 ? 1 : 0) | (a_def.pad >= 0 ? 2 : 0);
	return devices & ~a_def.shareMask & defaults;
}

const HotkeyDef* HotkeyService::Owner(const HotkeyDef& a_def, bool a_pad) {
	if (a_def.shareMask & (a_pad ? 2 : 1)) {
		if (const auto* s = Find(a_def.shares)) {
			return s;
		}
	}
	return &a_def;
}

std::vector<const HotkeyGroupDef*> HotkeyService::Groups(const HotkeyDef& a_def, bool a_pad) {
	std::vector<const HotkeyGroupDef*> out;
	if (!IsShared(a_def)) {
		if (const auto* g = Group(a_def.group)) {
			out.push_back(g);
		}
		return out;
	}
	const int bit = a_pad ? 2 : 1;
	for (const auto& m : kHotkeys) {
		if ((m.shareMask & bit) && a_def.id == std::string_view(m.shares)) {
			const auto* g = Group(m.group);
			if (g && std::ranges::find(out, g) == out.end()) {
				out.push_back(g);
			}
		}
	}
	return out;
}

std::string HotkeyService::Text(const HotkeyDef& a_def, Binding a_binding) {
	const bool        game = IsGame(a_def);
	const auto*       pc = Name(a_binding.pc, game);
	const auto*       pad = Name(a_binding.pad, game);
	const std::string a = pc ? pc->display : "-";
	const std::string b = pad ? pad->display : "-";
	const int         own = OwnDevices(a_def);
	if (own == 1) {
		return a;
	}
	if (own == 2) {
		return b;
	}
	return a + "  /  " + b;
}

HotkeyService::Binding HotkeyService::Get(const HotkeyDef& a_def) const {
	Binding b{a_def.pc, a_def.pad};
	const auto s = Settings::SettingsService::GetString(kSection, a_def.id, "");
	if (const auto comma = s.find(','); comma != std::string::npos) {
		const int pc = ParseInt(std::string_view(s).substr(0, comma), -2);
		const int pad = ParseInt(std::string_view(s).substr(comma + 1), -2);
		// an unreadable or unknown code keeps the default for that device
		const bool game = IsGame(a_def);
		if (pc == -1 || (pc >= 0 && Name(pc, game) && !Name(pc, game)->pad)) {
			b.pc = pc;
		}
		if (pad == -1 || (pad >= 0 && Name(pad, game) && Name(pad, game)->pad)) {
			b.pad = pad;
		}
	}
	return b;
}

HotkeyService::Binding HotkeyService::Resolve(const HotkeyDef& a_def) const {
	const Binding own = Get(a_def);
	const int     mine = OwnDevices(a_def);
	const auto*   pcOwner = Owner(a_def, false);
	const auto*   padOwner = Owner(a_def, true);
	Binding       b{a_def.pc, a_def.pad};  // a device neither shared nor its own keeps the default (none)
	if (pcOwner != &a_def) {
		b.pc = Get(*pcOwner).pc;
	} else if (mine & 1) {
		b.pc = own.pc;
	}
	if (padOwner != &a_def) {
		b.pad = Get(*padOwner).pad;
	} else if (mine & 2) {
		b.pad = own.pad;
	}
	return b;
}

void HotkeyService::Store(const HotkeyDef& a_def, Binding a_binding) {
	Settings::SettingsService::SetString(kSection, a_def.id, std::format("{},{}", a_binding.pc, a_binding.pad));
	if (IsGame(a_def)) {
		ApplyGame(a_def, a_binding);
	}
}

// "quickloot:<property>" / "global:<editor id>", space-separated
void HotkeyService::ApplyGame(const HotkeyDef& a_def, Binding a_binding) {
	auto push = [](std::string_view a_targets, int a_code) {
		std::size_t start = 0;
		while (start < a_targets.size()) {
			auto end = a_targets.find(' ', start);
			if (end == std::string_view::npos) {
				end = a_targets.size();
			}
			const auto t = a_targets.substr(start, end - start);
			start = end + 1;
			const auto colon = t.find(':');
			if (colon == std::string_view::npos) {
				continue;
			}
			const auto kind = t.substr(0, colon);
			const auto name = t.substr(colon + 1);
			if (kind == "quickloot") {
				InterfaceAdapters::GetSingleton()->SetQuickLoot(name, static_cast<float>(a_code));
			} else if (kind == "global") {
				InterfaceAdapters::GetSingleton()->SetGlobal(name, static_cast<float>(a_code));
			}
		}
	};
	if (a_def.devices != 2) {
		push(a_def.pcTargets, a_binding.pc);
	}
	if (a_def.devices != 1) {
		push(a_def.padTargets, a_binding.pad);
	}
}

void HotkeyService::ApplyGameKeys() {
	std::size_t n = 0;
	for (const auto& h : kHotkeys) {
		if (IsGame(h)) {
			ApplyGame(h, Get(h));
			++n;
		}
	}
	Log::INFO("hotkeys: {} in-world binding(s) applied", n);
}

void HotkeyService::Reset(const HotkeyDef& a_def) {
	// a reset can collide with a rebound neighbour: that one takes this one's
	// old key, as a capture would (a reset is never refused)
	const Binding cur = Get(a_def);
	const int     own = OwnDevices(a_def);
	if (own & 1) {
		Assign(a_def, false, a_def.pc, PlanAssign(a_def, false, a_def.pc, true));
	}
	if (own & 2) {
		Assign(a_def, true, a_def.pad, PlanAssign(a_def, true, a_def.pad, true));
	}
	Log::INFO("hotkeys: {} reset ({} -> {})", a_def.id, Text(a_def, cur), Text(a_def, Get(a_def)));
}

// Who else answers a_code in the menus a_def's binding reaches:
//  * a key fixed in any of those menus: refused;
//  * a menu-specific action of the same menu: swapped (it gets a_def's old key);
//  * a shared action there, or anything in a linked menu (on screen at the
//    same time): refused. A reset swaps instead and ignores linked menus.
HotkeyService::Plan HotkeyService::PlanAssign(const HotkeyDef& a_def, bool a_pad, int a_code, bool a_reset) const {
	Plan plan;
	if (a_code < 0) {
		return plan;
	}
	const bool game = IsGame(a_def);
	const auto display = [&] {
		const auto* n = Name(a_code, game);
		return std::string(n ? n->display : "?");
	};
	auto code = [&](const HotkeyDef& x) {
		const Binding b = Resolve(x);
		return a_pad ? b.pad : b.pc;
	};
	for (const auto* g : Groups(a_def, a_pad)) {
		const auto& reserved = a_pad ? std::span<const int>(g->reservedPad) : std::span<const int>(g->reservedPc);
		if (!a_reset && std::ranges::find(reserved, a_code) != reserved.end()) {
			Log::INFO("hotkeys: {} - {} is fixed in {}", a_def.id, display(), g->id);
			plan.refuse = {"$SKYB_settings_key_reserved", display()};
			return plan;
		}
		const std::string links = std::string(" ") + g->links + " ";
		for (const auto& x : kHotkeys) {
			if (IsShared(x) || code(x) != a_code) {
				continue;
			}
			const auto* owner = Owner(x, a_pad);
			if (owner == &a_def) {
				continue;
			}
			if (x.group == std::string_view(g->id)) {
				if (!IsShared(*owner) || a_reset) {
					if (std::ranges::find(plan.swaps, owner) == plan.swaps.end()) {
						plan.swaps.push_back(owner);
					}
					continue;
				}
			} else if (a_reset || links.find(std::string(" ") + x.group + " ") == std::string::npos) {
				continue;
			}
			Log::INFO("hotkeys: {} - {} is used by {} ({})", a_def.id, display(), owner->id, x.group);
			plan.refuse = {"$SKYB_settings_key_taken", owner->label};
			plan.swaps.clear();
			return plan;
		}
	}
	return plan;
}

void HotkeyService::Assign(const HotkeyDef& a_def, bool a_pad, int a_code, const Plan& a_plan) {
	Binding   mine = Get(a_def);
	const int old = a_pad ? mine.pad : mine.pc;
	if (old == a_code) {
		return;
	}
	for (const auto* other : a_plan.swaps) {
		Binding theirs = Get(*other);
		(a_pad ? theirs.pad : theirs.pc) = old;  // swap
		Store(*other, theirs);
		_status = {"$SKYB_settings_key_swapped", other->label};
		Log::INFO("hotkeys: {} takes {} from {}, which gets {}", a_def.id, a_code, other->id, old);
	}
	(a_pad ? mine.pad : mine.pc) = a_code;
	Store(a_def, mine);
}

void HotkeyService::BeginCapture(std::string_view a_id) {
	if (!Find(a_id)) {
		return;
	}
	_capturing = std::string(a_id);
	_armedAt = std::chrono::steady_clock::now();
	_status = {};
	Log::INFO("hotkeys: waiting for a key for {}", a_id);
}

void HotkeyService::CancelCapture() {
	if (!_capturing.empty()) {
		EndCapture({});
	}
}

std::string HotkeyService::CapturingId() const {
	return _capturing;
}

HotkeyService::Status HotkeyService::TakeStatus() {
	return std::exchange(_status, {});
}

void HotkeyService::EndCapture(Status a_status) {
	_capturing.clear();
	if (!a_status.key.empty()) {
		_status = std::move(a_status);
	}
	if (_onChange) {
		_onChange();
	}
}

RE::BSEventNotifyControl HotkeyService::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
	if (_capturing.empty() || !a_event) {
		return RE::BSEventNotifyControl::kContinue;
	}
	// the press that asked for the capture (and its repeats) must not bind
	if (std::chrono::steady_clock::now() - _armedAt < std::chrono::milliseconds(150)) {
		return RE::BSEventNotifyControl::kContinue;
	}
	for (auto* e = *a_event; e; e = e->next) {
		auto* button = e->AsButtonEvent();
		if (!button || !button->IsDown()) {
			continue;
		}
		const auto device = button->GetDevice();
		if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kGamepad) {
			continue;  // mouse: keep waiting
		}
		const auto* def = Find(_capturing);
		const auto* group = def ? Group(def->group) : nullptr;
		if (!def || !group) {
			EndCapture({});
			break;
		}
		const bool pad = device == RE::INPUT_DEVICE::kGamepad;
		const auto id = button->GetIDCode();
		if (!pad && id == 0x01) {  // Esc
			Log::INFO("hotkeys: capture for {} cancelled", def->id);
			EndCapture({});
			break;
		}
		const int own = OwnDevices(*def);
		if (own == 1 && pad) {
			EndCapture({"$SKYB_settings_key_pc_only", ""});
			break;
		}
		if (own == 2 && !pad) {
			EndCapture({"$SKYB_settings_key_pad_only", ""});
			break;
		}
		// menu actions: the Flash codes menus see; in-world: DirectInput / SKSE codes
		const bool  game = group->game;
		const int   code = game ? (pad ? PadToSkse(id) : static_cast<int>(id)) : (pad ? PadToFlash(id) : DikToFlash(id));
		const auto* name = Name(code, game);
		if (code < 0 || !name) {
			Log::INFO("hotkeys: {} - {} code {:X} cannot be bound", def->id, pad ? "button" : "key", id);
			EndCapture({"$SKYB_settings_key_invalid", ""});
			break;
		}
		// fixed keys, shared actions and linked menus refuse; a menu's own
		// action on the same key swaps
		const Plan plan = PlanAssign(*def, pad, code, false);
		if (!plan.refuse.key.empty()) {
			EndCapture(plan.refuse);
			break;
		}
		_status = {};
		Assign(*def, pad, code, plan);
		Log::INFO("hotkeys: {} {} = {} ({})", def->id, pad ? "button" : "key", name->display, code);
		EndCapture(std::exchange(_status, {}));
		break;
	}
	return RE::BSEventNotifyControl::kContinue;
}

void HotkeyService::BuildMap(RE::GFxMovieView* a_movie, RE::GFxValue& a_out) const {
	a_movie->CreateObject(&a_out);
	for (const auto& h : kHotkeys) {
		if (IsShared(h)) {
			continue;  // menus know only their own ids; members carry the shared binding
		}
		const Binding b = Resolve(h);
		const auto*   pc = Name(b.pc, IsGame(h));
		const auto*   pad = Name(b.pad, IsGame(h));
		RE::GFxValue  o;
		a_movie->CreateObject(&o);
		o.SetMember("pc", RE::GFxValue(static_cast<double>(b.pc)));
		o.SetMember("pad", RE::GFxValue(static_cast<double>(b.pad)));
		o.SetMember("pcGlyph", RE::GFxValue(pc ? pc->glyph : ""));
		const std::string g360 = pad ? pad->glyph : "";
		const std::string gps3 = pad ? "PS3_" + g360.substr(4) : "";
		o.SetMember("padGlyph", RE::GFxValue(g360.c_str()));
		o.SetMember("ps3Glyph", RE::GFxValue(gps3.c_str()));
		a_out.SetMember(h.id, o);
	}
}

RE::BSEventNotifyControl HotkeyService::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
													 RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
	if (!a_event || a_event->eventName != kRequestEvent) {
		return RE::BSEventNotifyControl::kContinue;
	}
	const std::string arg = a_event->strArg.c_str() ? a_event->strArg.c_str() : "";
	const auto        bar = arg.find('|');
	if (bar == std::string::npos) {
		Log::WARN("hotkeys: {} needs '<menu>|<function>', got '{}'", kRequestEvent, arg);
		return RE::BSEventNotifyControl::kContinue;
	}
	std::string menu = arg.substr(0, bar);
	std::string path = arg.substr(bar + 1);
	// Replies on the main thread at the start of the next frame, after the asking movie's
	// script has returned. SKSE's task queues are also drained on worker threads, which
	// would race the main thread advancing the movie.
	auto reply = [this, menu, path] {
		auto ui = RE::UI::GetSingleton();
		auto m = ui ? ui->GetMenu(menu) : nullptr;
		auto movie = m ? m->uiMovie : nullptr;
		if (!movie) {
			Log::INFO("hotkeys: '{}' asked for its keys but is not open any more", menu);
			return;
		}
		const bool paused = ui->GameIsPaused();
		RE::GFxValue map;
		BuildMap(movie.get(), map);
		const bool ok = movie->Invoke(path.c_str(), nullptr, &map, 1);
		Log::INFO("hotkeys: sent to {} ({}) -> {} [main thread, game {}]", menu, path, ok ? "delivered" : "NOT delivered",
					 paused ? "paused" : "running");
	};
	MainThread::Post(reply);
	return RE::BSEventNotifyControl::kContinue;
}
