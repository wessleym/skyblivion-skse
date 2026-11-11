#include "Degradation/Settings.h"

#include <cstdint>
#include <format>

namespace SKYBDeg
{
	void Settings::Resolve()
	{
		s_table.Load();

		s_tools = RE::TESForm::LookupByEditorID<RE::BGSListForm>(kToolsList);
		s_hammers.clear();
		if (!s_tools) {
			Log::WARN("FLST {} not found in any loaded plugin: no repair tool is known (the hammer will not open the menu)", kToolsList);
			return;
		}
		Log::INFO("repair tools list {:08X}: {} entr{}", s_tools->GetFormID(), s_tools->forms.size(),
					 s_tools->forms.size() == 1 ? "y" : "ies");
		for (std::uint32_t i = 0; i < s_tools->forms.size(); ++i) {
			auto form = s_tools->forms[i] ? s_tools->forms[i]->As<RE::TESBoundObject>() : nullptr;
			if (!form) {
				Log::ERROR("  tool entry {} is not an item - skipped", i);
				continue;
			}
			if (i >= kHammerKeys.size()) {
				Log::ERROR("  tool {:08X} '{}' (entry {}) has no [Hammers] keys - skipped", form->GetFormID(), form->GetName(), i);
				continue;
			}
			// every kHammerKeys entry has its two [Hammers] rows in the table
			const Hammer h{ form, kHammerKeys[i], Find("Hammers", std::format("{}Break", kHammerKeys[i])),
							Find("Hammers", std::format("{}Tier", kHammerKeys[i])) };
			Log::INFO("  hammer {} '{}' ({:08X}) = [Hammers] {}: break {:.2f}, tier {:.1f}", s_hammers.size(), form->GetName(),
						 form->GetFormID(), h.key, At(h.breakChance), At(h.tier));
			s_hammers.push_back(h);
		}
	}
}
