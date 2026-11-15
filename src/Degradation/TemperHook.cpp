#include "Degradation/TemperHook.h"

#include "Degradation/Condition.h"
#include "Degradation/Settings.h"
#include <algorithm>
#include <cstdint>
#include <optional>

namespace SKYBDeg::TemperHook
{
	namespace
	{
		using DamageFn = float(RE::ActorValueOwner*, RE::TESObjectWEAP*, RE::TESAmmo*, float, float, bool);
		// 26409: 26410 without the ammo (it passes none).
		using MeleeDamageFn = float(RE::ActorValueOwner*, RE::TESObjectWEAP*, float, float, bool);
		using ArmorBonusFn = float(float, RE::TESObjectARMO*);
		REL::Relocation<DamageFn> g_damage40291;
		REL::Relocation<DamageFn> g_damage44100;
		REL::Relocation<MeleeDamageFn> g_damage16016;
		REL::Relocation<ArmorBonusFn> g_armorBonus;
		bool g_installed = false;

		// The engine's rule (highest ExtraHealth over the entry's lists) minus its 1.0 floor, except that
		// when a copy is worn only the worn copies count (a list without ExtraHealth is 1.0): the engine
		// passes the whole entry, so a carried spare would otherwise lend the worn copy its condition.
		// No list carries a value -> 1.0, as before.
		float Health(RE::InventoryEntryData* a_entry)
		{
			float best = -1.0f;
			float worn = -1.0f;
			if (a_entry && a_entry->extraLists) {
				for (auto list : *a_entry->extraLists) {
					if (!list) {
						continue;
					}
					auto h = list->GetByType<RE::ExtraHealth>();
					if (h) {
						best = std::max(best, h->health);
					}
					if (list->HasType<RE::ExtraWorn>() || list->HasType<RE::ExtraWornLeft>()) {
						worn = std::max(worn, h ? h->health : 1.0f);
					}
				}
			}
			if (worn >= 0.0f) {
				best = worn;
			}
			if (best < 0.0f) {
				return 1.0f;
			}
			return Settings::On(G::Enabled) && Settings::On(G::StatEnabled) ? best : std::max(best, 1.0f);
		}

		float Scale(float a_health) { return a_health < 1.0f ? Condition::StatMult(a_health) : 1.0f; }

		float Damage40291(RE::ActorValueOwner* a_av, RE::TESObjectWEAP* a_weap, RE::TESAmmo* a_ammo, float a_health,
			float a_mult, bool a_flag)
		{
			return g_damage40291(a_av, a_weap, a_ammo, std::max(a_health, 1.0f), a_mult, a_flag) * Scale(a_health);
		}

		float Damage44100(RE::ActorValueOwner* a_av, RE::TESObjectWEAP* a_weap, RE::TESAmmo* a_ammo, float a_health,
			float a_mult, bool a_flag)
		{
			return g_damage44100(a_av, a_weap, a_ammo, std::max(a_health, 1.0f), a_mult, a_flag) * Scale(a_health);
		}

		float Damage16016(RE::ActorValueOwner* a_av, RE::TESObjectWEAP* a_weap, float a_health, float a_mult, bool a_flag)
		{
			return g_damage16016(a_av, a_weap, std::max(a_health, 1.0f), a_mult, a_flag) * Scale(a_health);
		}

		// 16017: rating = (armor->armorRating * 0.01 + bonus) * skillMult.
		// bonus = base * (scale - 1) makes that base * scale * skillMult.
		float ArmorBonus(float a_health, RE::TESObjectARMO* a_armor)
		{
			if (a_health >= 1.0f || !a_armor) {
				return g_armorBonus(std::max(a_health, 1.0f), a_armor);
			}
			const float base = static_cast<float>(a_armor->armorRating & 0xFFFF) * 0.01f;
			return base * (Scale(a_health) - 1.0f);
		}

		// How far into a function another AE build's call may sit (the furthest 1.6.1170 site is +0x2F8).
		constexpr std::ptrdiff_t kScanBytes = 0x400;

		struct Site
		{
			const char* what;
			REL::VariantID func;       // the function holding the call
			std::ptrdiff_t seOffset;   // call instruction inside it on SE 1.5.97, and on VR 1.4.15 (VR's code matches SE's)
			std::ptrdiff_t aeOffset;   // call instruction inside it on AE 1.6.1170
			REL::VariantID target;     // vanilla callee
			std::ptrdiff_t scanBytes = kScanBytes;  // the search on another AE build stays within this
		};

		// (SE id, AE id, VR offset). VR's address library lacks several of these, hence offsets.
		// The callees:
		constexpr REL::VariantID kHealthFn{ 15752, 15990, 0x1E7080 };
		constexpr REL::VariantID kDamageFn{ 25847, 26410, 0x3CD7A0 };
		constexpr REL::VariantID kMeleeDamageFn{ 25846, 26409, 0x3CD770 };
		constexpr REL::VariantID kArmorBonusFn{ 25916, 26499, 0x3D08C0 };
		// The functions holding more than one site:
		constexpr REL::VariantID kProjectile{ 42920, 44100, 0x775C20 };
		constexpr REL::VariantID kArmorRating{ 15779, 16017, 0x1E7ED0 };
		constexpr REL::VariantID kMeleeHit{ 15778, 16016, 0x1E7E60 };

		// Other AE builds than 1.6.1170 are searched (Locate).
		const Site kHealth44100{ "projectile weapon health", kProjectile, 0x2CC, 0x2CC, kHealthFn };
		const Site kDamage44100{ "projectile weapon damage", kProjectile, 0x2F4, 0x2F8, kDamageFn };
		const Site kHealth40253{ "display damage health", REL::VariantID(39179, 40253, 0x69C650), 0x26, 0x26, kHealthFn };
		const Site kHealth40252{ "display equipped damage health", REL::VariantID(39178, 40252, 0x69C4B0), 0x8D, 0x92, kHealthFn };
		const Site kDamage40291{ "display weapon damage", REL::VariantID(39215, 40291, 0x69D950), 0x2F, 0x37, kDamageFn };
		const Site kHealth16017{ "armor rating health", kArmorRating, 0x27, 0x27, kHealthFn };
		const Site kBonus16017{ "armor temper bonus", kArmorRating, 0x2F, 0x2F, kArmorBonusFn };
		// The melee hit's damage (hit data, id 44001 -> 16016). 16016 is 0x5C bytes and 16017, which calls
		// 15990 too, follows it: the search stays inside 16016.
		const Site kHealth16016{ "melee weapon health", kMeleeHit, 0x27, 0x27, kHealthFn, 0x60 };
		const Site kDamage16016{ "melee weapon damage", kMeleeHit, 0x3D, 0x3D, kMeleeDamageFn, 0x60 };

		// The call's destination if the bytes at a_at are a 5-byte call, else nothing.
		std::optional<std::uintptr_t> CallDest(std::uintptr_t a_at)
		{
			const auto p = reinterpret_cast<const std::uint8_t*>(a_at);
			if (p[0] != 0xE8) {
				return std::nullopt;
			}
			const auto rel = *reinterpret_cast<const std::int32_t*>(p + 1);
			return a_at + 5 + static_cast<std::intptr_t>(rel);
		}

		// Where to patch the site on this runtime, or nothing.
		// On SE, VR and AE 1.6.1170, the site is exactly the known offset, which must hold the vanilla call.
		// A call to anything else there means another mod hooked it, so there is no search.
		// On other AE builds, code moves between builds: the site is the known offset if it holds the vanilla call.
		// Otherwise the one call to the expected callee in the function's first scanBytes.
		std::optional<std::uintptr_t> Locate(const Site& a_site, bool a_exactBuild)
		{
			const auto offset = REL::Module::IsAE() ? a_site.aeOffset : a_site.seOffset;
			const auto func = a_site.func.address();
			const auto target = a_site.target.address();
			const auto at = func + offset;
			const auto dest = CallDest(at);
			if (dest && *dest == target) {
				return at;
			}
			if (a_exactBuild) {
				if (dest) {
					Log::ERROR("temper hook: {} at {:#x} calls {:#x}, expected {:#x} (another mod hooks it?)",
						a_site.what, at, *dest, target);
				}
				else {
					Log::ERROR("temper hook: {} at {:#x} is not a call (byte {:02X})", a_site.what, at,
						*reinterpret_cast<const std::uint8_t*>(at));
				}
				return std::nullopt;
			}
			//Another AE build: code moves, so a different call at the 1.6.1170 offset says nothing; search. A site
			//another mod has hooked holds no call to the expected function and is not found.
			std::optional<std::uintptr_t> found;
			int matches = 0;
			for (std::ptrdiff_t i = 0; i + 5 <= a_site.scanBytes; ++i) {
				const auto d = CallDest(func + i);
				if (d && *d == target) {
					++matches;
					found = func + i;
				}
			}
			if (matches != 1) {
				Log::ERROR("temper hook: {}: {} call(s) to {:#x} in the first {:#x} bytes of {:#x}; need exactly one",
					a_site.what, matches, target, a_site.scanBytes, func);
				return std::nullopt;
			}
			Log::INFO("temper hook: {} found at +{:#x} (1.6.1170: +{:#x})", a_site.what, *found - func, offset);
			return found;
		}
	}

	// Nothing is patched unless all seven are found.
	void Install()
	{
		const auto version = REL::Module::get().version();
		const bool exactBuild = !REL::Module::IsAE() || version == REL::Version(1, 6, 1170, 0);
		const Site* sites[] = { &kHealth44100, &kDamage44100, &kHealth40253, &kHealth40252,
								&kDamage40291, &kHealth16017, &kBonus16017 };
		std::uintptr_t at[std::size(sites)]{};
		for (std::size_t i = 0; i < std::size(sites); ++i) {
			const auto found = Locate(*sites[i], exactBuild);
			if (!found) {
				Log::ERROR("temper hook: NOT installed (nothing patched) on {} - condition below 100% will not change damage / armor",
					version.string());
				return;
			}
			at[i] = *found;
		}

		//The plugin's one trampoline, allocated in Plugin::OnLoad (shared with the frame hook).
		auto& t = SKSE::GetTrampoline();
		t.write_call<5>(at[0], Health);
		g_damage44100 = t.write_call<5>(at[1], Damage44100);
		t.write_call<5>(at[2], Health);
		t.write_call<5>(at[3], Health);
		g_damage40291 = t.write_call<5>(at[4], Damage40291);
		t.write_call<5>(at[5], Health);
		g_armorBonus = t.write_call<5>(at[6], ArmorBonus);
		g_installed = true;
		Log::INFO("temper hook: installed at 7 call sites on {} - condition below 100% now scales projectile damage, "
			"armor and the displayed numbers through the engine's own temper path", version.string());

		// Melee: both sites or neither, apart from the seven above.
		const auto meleeHealth = Locate(kHealth16016, exactBuild);
		const auto meleeDamage = meleeHealth ? Locate(kDamage16016, exactBuild) : std::nullopt;
		if (meleeHealth && meleeDamage) {
			t.write_call<5>(*meleeHealth, Health);
			g_damage16016 = t.write_call<5>(*meleeDamage, Damage16016);
			Log::INFO("temper hook: melee hits covered too (2 more call sites)");
		}
		else {
			Log::ERROR("temper hook: melee sites not found on {} - condition below 100% will not change melee damage",
				version.string());
		}
	}

	bool Installed()
	{
		return g_installed;
	}
}
