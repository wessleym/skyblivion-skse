#include "RE/UI.h"


namespace RE
{
	void ShowTrainingMenu(RE::Actor* target)
	{
		using func_t = decltype(ShowTrainingMenu);

		//AE(1.6.640): 91C4B0
		static REL::Relocation<func_t> func{ 52942 };

		return func(target);
	}

	void ShowBarterMenu(RE::Actor* target)
	{
		using func_t = decltype(ShowBarterMenu);

		//AE(1.6.640): 891FB0
		static REL::Relocation<func_t> func{ 50955 };

		return func(target);
	}
}
