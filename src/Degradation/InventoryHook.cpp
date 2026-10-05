// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include "Degradation/InventoryHook.h"

#include "Degradation/Condition.h"
#include "Degradation/RepairMenu.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SKYBDeg::InventoryHook
{
	namespace
	{
		using CallbackFn = RE::FxDelegateHandler::CallbackFn;

		CallbackFn* g_engineItemSelect = nullptr;

		const RE::TESForm* SelectedRepairTool()
		{
			auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			auto menu = ui->GetMenu<RE::InventoryMenu>();
			if (!menu) {
				return nullptr;
			}
			auto list = menu->GetRuntimeData().itemList;
			auto item = list ? list->GetSelectedItem() : nullptr;
			auto entry = item ? item->data.objDesc : nullptr;
			return entry && Condition::IsRepairTool(entry->object) ? entry->object : nullptr;
		}

		void HookedItemSelect(const RE::FxDelegateArgs& a_params)
		{
			if (auto tool = SelectedRepairTool()) {
				Log::INFO("inventory: '{}' activated -> repair menu", tool->GetName());
				RepairMenu::Open(true, tool);
				return;
			}
			if (g_engineItemSelect) {
				g_engineItemSelect(a_params);
			}
		}

		class Proxy : public RE::FxDelegateHandler::CallbackProcessor
		{
		public:
			explicit Proxy(RE::FxDelegateHandler::CallbackProcessor* a_inner) :
				_inner(a_inner) {}

			void Process(const RE::GString& a_methodName, CallbackFn* a_method) override
			{
				if (std::strcmp(a_methodName.c_str(), "ItemSelect") == 0 && a_method != HookedItemSelect) {
					g_engineItemSelect = a_method;
					_inner->Process(a_methodName, HookedItemSelect);
					return;
				}
				_inner->Process(a_methodName, a_method);
			}

		private:
			RE::FxDelegateHandler::CallbackProcessor* _inner;
		};

		struct InventoryMenuAccept
		{
			static void thunk(RE::InventoryMenu* a_menu, RE::FxDelegateHandler::CallbackProcessor* a_cbReg)
			{
				Proxy proxy{ a_cbReg };
				func(a_menu, std::addressof(proxy));
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_InventoryMenu[0] };
		InventoryMenuAccept::func = vtbl.write_vfunc(0x1, InventoryMenuAccept::thunk);
		Log::INFO("inventory: ItemSelect interception installed");
	}
}
