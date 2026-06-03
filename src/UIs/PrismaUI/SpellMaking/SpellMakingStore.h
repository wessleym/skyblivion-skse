#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SKSE { class SerializationInterface; }

//Persists player-made spells across saves via a pre-authored placeholder pool.
//
//Made spells are NOT created at runtime. A runtime form gets a dynamic 0xFF FormID that
//is reassigned every session, so any save reference to it (known, equipped, favorited,
//hotkeyed) resolves to an unrelated form on load. Instead, a plugin includes a pool of blank
//placeholder SpellItem forms, each named with a marker string. "Making a spell" claims a
//free placeholder and fills in its contents. Because the placeholder has a stable plugin
//FormID, the base game persists everything that references it automatically.
//
//The base save does not carry the placeholder's runtime *contents* (effects, name, and magicka cost),
//so the SKSE co-save stores each used slot's recipe, and the placeholder is refilled on
//load.
namespace SpellMaking {

	class SpellMakingStore {
	public:
		struct EffectSpec {
			//MGEF identifier
			//When editorId is non-empty, it is used for area-bucket MGEFs that this plugin creates (identified by EDID).
			//Otherwise, formID is used, covering vanilla Skyrim.esm MGEFs and the area=None case.
			std::string editorId;
			RE::FormID formID = 0;

			std::int32_t magnitude = 0;
			std::int32_t duration = 0;
			std::uint32_t area = 0;//In-game units. 0 = no area. Otherwise written to effectItem.area.
			std::int32_t magickaCost = 0;
		};

		struct SpellRecipe {
			std::string name;
			RE::MagicSystem::CastingType castingType = RE::MagicSystem::CastingType::kFireAndForget;
			RE::MagicSystem::Delivery delivery = RE::MagicSystem::Delivery::kAimed;
			std::int32_t magickaCost = 0;
			std::vector<EffectSpec> effects;
		};

		//Resolves the placeholder pool. Call once after data load.
		static void Initialize();

		//Registers the SKSE co-save serialization callbacks. Call once at plugin load.
		static void RegisterSerialization();

		//Claims an unused placeholder, fills it from the recipe, and returns it ready to give to the player.
		//Returns nullptr if the pool is exhausted or unavailable.
		static RE::SpellItem* CreateSpell(const SpellRecipe& recipe);

		//True if the spell is one of the placeholder pool forms.
		static bool IsPlaceholder(const RE::SpellItem* spell);

	private:
		struct Slot {
			RE::SpellItem* form = nullptr;//the placeholder SpellItem
			bool used = false;
			SpellRecipe recipe;
		};

		//Fills a placeholder form in place from a recipe (effects, name, cast data, magicka cost).
		static void PopulateSpell(RE::SpellItem* form, const SpellRecipe& recipe);

		//Frees every pool slot. Used at the start of a co-save load and by OnRevert.
		static void ResetSlots();

		static void OnGameSaved(SKSE::SerializationInterface* intfc);
		static void OnGameLoaded(SKSE::SerializationInterface* intfc);
		static void OnRevert(SKSE::SerializationInterface* intfc);

		static std::vector<Slot> s_slots;
	};

}
