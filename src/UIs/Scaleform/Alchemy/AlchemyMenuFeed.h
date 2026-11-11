#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

//The Crafting Menu's alchemy data (AlchemyMenuContract.h): the best carried apparatus per slot with the
//bonus its perk applies, and every ingredient's effect strengths against the effect's baseline.
namespace Alchemy {

	class AlchemyMenuFeed {
	public:
		//SKSEPlugin_Load: registers the Scaleform callback (SKSE only accepts it while loading).
		static void InstallScaleform();
		//kDataLoaded: parses the four apparatus perks and reads the baseline table.
		static void OnDataLoaded();

		//One apparatus slot's perk, as parsed from its ESM record.
		struct Tool {
			const char* id;              //"mortar" ...
			const char* globalEditorID;  //SKYBAlchemyMortarPestleGlob ...
			const char* perkEditorID;    //SKYBAlchemyPerkMortarPestle ...
			const char* listEditorID;    //SKYBAlchemyMortarPestleList ... (Master first)
			std::string scope;           //"all" | "beneficial" | "harmful" | "" (unread)
			bool potionOnly = false;
			std::array<float, 5> tiers{};  //Novice .. Master; 0 = unread
		};

		//A baseline: which axis the effect varies on and its value there.
		struct Baseline {
			char axis = 'n';  //'m' magnitude, 'd' duration, 'n' none
			float mag = 0.0f;
			std::uint32_t dur = 0;
		};

	private:
		friend class FetchHandler;
		friend class RememberHandler;

		static bool OnMovieLoad(RE::GFxMovieView* a_view, RE::GFxValue* a_root);
		static void BuildData(RE::GFxMovieView& a_view, RE::GFxValue& a_out);
		static void ParsePerks();
		static void ReadBaselines();
		static Baseline BaselineFor(RE::EffectSetting* a_effect, const std::unordered_map<RE::FormID, Baseline>& a_modes);
		static std::unordered_map<RE::FormID, Baseline> ModesInLoadOrder();
		static int CarriedTier(const Tool& a_tool);

		static inline std::vector<Tool> s_tools;
		static inline std::unordered_map<std::string, Baseline> s_baselines;  //"03EB15:Skyrim.esm" -> baseline
		static inline bool s_preview = false;
		static inline bool s_parsed = false;
	};
}
