#include "SpellMakingStore.h"
#include "MadeSpellEquipRefresh.h"
#include "Resources.h"
#include "Storage/CoSave.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string_view>

//See SpellMakingStore.h for the placeholder-pool trick and why made spells cannot be created at runtime.
//The SpellMakingStore members manage the placeholder pool and the SKSE serialization callbacks.

namespace SpellMaking {

	std::vector<SpellMakingStore::Slot> SpellMakingStore::s_slots;

	namespace {
		//Placeholder Pool: Every SpellItem whose name is exactly this string is a placeholder pool slot.
		constexpr std::string_view kPoolMarkerName = "SKYBSpellMakingSpell";//Spell Making Required Plugin Data (1/2)

		//Co-Save Record:
		//The plugin's co-save id (Storage/CoSave owns the SKSE callbacks; this file reads the file directly too).
		constexpr std::uint32_t kSerializationID = Storage::CoSave::UniqueID;
		constexpr std::uint32_t kRecipeRecord = 'SPRC';
		constexpr std::uint32_t kRecipeVersion = 1;//OpenRecord requires a version.

		//The co-save file's header signature, as SKSE writes it and a little-endian read returns it.
		constexpr std::uint32_t kCoSaveSignature = 'ESKS';

		//Reads a co-save record that is already in memory.
		//The record is read from memory because it comes from two places: the co-save file itself, before the
		//base save loads (RestoreBeforeLoad), and SKSE's load callback afterward (OnGameLoaded).
		//Every read reports whether the data held enough bytes, so a truncated record is detected, not misread.
		class RecordReader {
		public:
			explicit RecordReader(std::span<const std::byte> data) : m_data(data) {}

			template <class T>
			bool Read(T& value) {
				auto bytes = Take(sizeof(T));
				if (!bytes) {
					return false;
				}
				std::memcpy(&value, bytes->data(), sizeof(T));
				return true;
			}

			bool ReadString(std::string& out) {
				std::uint32_t len = 0;
				if (!Read(len)) {
					return false;
				}
				auto bytes = Take(len);
				if (!bytes) {
					return false;
				}
				out.assign(reinterpret_cast<const char*>(bytes->data()), len);
				return true;
			}

			//The next count bytes, or nullopt if fewer remain.
			std::optional<std::span<const std::byte>> Take(std::size_t count) {
				if (m_data.size() < count) {
					return std::nullopt;
				}
				auto bytes = m_data.first(count);
				m_data = m_data.subspan(count);
				return bytes;
			}

		private:
			std::span<const std::byte> m_data;
		};

		void WriteString(SKSE::SerializationInterface* intfc, std::string_view str) {
			const std::uint32_t len = static_cast<std::uint32_t>(str.size());
			intfc->WriteRecordData(len);
			if (len > 0) {
				intfc->WriteRecordData(str.data(), len);
			}
		}

		//WriteRecipe and ReadRecipe are kept adjacent on purpose: the co-save byte layout must match exactly between save and load.
		//So the two are easiest to keep correct side by side. Any field added to one must be added to the other in the same order.
		void WriteRecipe(SKSE::SerializationInterface* intfc, const SpellMakingStore::SpellRecipe& recipe) {
			WriteString(intfc, recipe.name);
			intfc->WriteRecordData(recipe.castingType);
			intfc->WriteRecordData(recipe.delivery);
			intfc->WriteRecordData(recipe.magickaCost);
			const std::uint32_t effectCount = static_cast<std::uint32_t>(recipe.effects.size());
			intfc->WriteRecordData(effectCount);
			for (const auto& spec : recipe.effects) {
				intfc->WriteRecordData(spec.formID);
				WriteString(intfc, spec.editorId);
				intfc->WriteRecordData(spec.magnitude);
				intfc->WriteRecordData(spec.duration);
				intfc->WriteRecordData(spec.area);
				intfc->WriteRecordData(spec.magickaCost);
			}
		}

		//Returns false if the record ends early.
		//Effect FormIDs are stored as they are: the view takes them from Skyrim.esm, whose load index is always 00.
		//They need no remapping, so a recipe reads the same before the base save loads, when SKSE's
		//ResolveFormID is not yet available.
		bool ReadRecipe(RecordReader& reader, SpellMakingStore::SpellRecipe& recipe) {
			std::uint32_t effectCount = 0;
			if (!reader.ReadString(recipe.name) ||
				!reader.Read(recipe.castingType) ||
				!reader.Read(recipe.delivery) ||
				!reader.Read(recipe.magickaCost) ||
				!reader.Read(effectCount)) {
				return false;
			}
			recipe.effects.reserve(effectCount);
			for (std::uint32_t i = 0; i < effectCount; ++i) {
				SpellMakingStore::EffectSpec spec{};
				if (!reader.Read(spec.formID) ||
					!reader.ReadString(spec.editorId) ||
					!reader.Read(spec.magnitude) ||
					!reader.Read(spec.duration) ||
					!reader.Read(spec.area) ||
					!reader.Read(spec.magickaCost)) {
					return false;
				}
				recipe.effects.push_back(std::move(spec));
			}
			return true;
		}

		//The co-save file paired with a save, in the folder SKSE uses: sLocalSavePath (default "Saves\")
		//under the game's My Games folder, the parent of the SKSE log folder.
		std::optional<std::filesystem::path> CoSavePath(std::string_view saveName) {
			auto skseFolder = SKSE::log::log_directory();
			if (!skseFolder) {
				return std::nullopt;
			}
			std::filesystem::path path = skseFolder->parent_path();
			auto* settings = RE::INISettingCollection::GetSingleton();
			auto* localSavePath = settings ? settings->GetSetting("sLocalSavePath:General") : nullptr;
			if (localSavePath && localSavePath->GetType() == RE::Setting::Type::kString) {
				path /= localSavePath->GetString();
			}
			else {
				path /= "Saves";
			}
			//kPreLoadGame names the base save file itself (".ess"); its co-save sits beside it.
			constexpr std::string_view kSaveExtension = ".ess";
			if (saveName.ends_with(kSaveExtension)) {
				saveName.remove_suffix(kSaveExtension.size());
			}
			path /= std::string(saveName) + ".skse";
			return path;
		}

		//This plugin's recipe record from a co-save file, or nullopt if the file or the record is missing.
		//Layout (SKSE's Serialization.cpp): a header {signature, formatVersion, skseVersion, runtimeVersion, pluginCount},
		//then per plugin {uid, chunkCount, length} followed by its chunks, each {type, version, length, data}.
		std::optional<std::vector<std::byte>> ReadRecipeRecordFromFile(const std::filesystem::path& path) {
			std::ifstream file(path, std::ios::binary);
			if (!file) {
				return std::nullopt;
			}
			const std::vector<char> raw{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			RecordReader reader(std::as_bytes(std::span(raw)));

			std::uint32_t signature = 0;
			std::uint32_t formatVersion = 0;
			std::uint32_t skseVersion = 0;
			std::uint32_t runtimeVersion = 0;
			std::uint32_t pluginCount = 0;
			if (!reader.Read(signature) || signature != kCoSaveSignature ||
				!reader.Read(formatVersion) ||
				!reader.Read(skseVersion) ||
				!reader.Read(runtimeVersion) ||
				!reader.Read(pluginCount)) {
				Log::WARN("SpellMakingStore: {} Is Not a Co-Save File", path.generic_string());
				return std::nullopt;
			}
			for (std::uint32_t p = 0; p < pluginCount; ++p) {
				std::uint32_t uid = 0;
				std::uint32_t chunkCount = 0;
				std::uint32_t length = 0;
				if (!reader.Read(uid) || !reader.Read(chunkCount) || !reader.Read(length)) {
					break;
				}
				auto block = reader.Take(length);
				if (!block) {
					break;
				}
				if (uid != kSerializationID) {
					continue;
				}
				RecordReader chunks(*block);
				for (std::uint32_t c = 0; c < chunkCount; ++c) {
					std::uint32_t type = 0;
					std::uint32_t version = 0;
					std::uint32_t chunkLength = 0;
					if (!chunks.Read(type) || !chunks.Read(version) || !chunks.Read(chunkLength)) {
						break;
					}
					auto data = chunks.Take(chunkLength);
					if (!data) {
						break;
					}
					if (type == kRecipeRecord && version == kRecipeVersion) {
						return std::vector<std::byte>(data->begin(), data->end());
					}
				}
				return std::nullopt;
			}
			return std::nullopt;
		}
	}

	void SpellMakingStore::Initialize() {
		s_slots.clear();

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			Log::WARN("SpellMakingStore::Initialize: TESDataHandler Unavailable");
			return;
		}

		std::vector<RE::SpellItem*> placeholders;//Every SpellItem whose name marks it as a placeholder
		for (auto* spell : dataHandler->GetFormArray<RE::SpellItem>()) {
			if (!spell) {
				continue;
			}
			const char* name = spell->GetFullName();
			if (name && kPoolMarkerName == name) {
				placeholders.push_back(spell);
			}
		}
		//Sort by Form ID
		std::sort(placeholders.begin(), placeholders.end(),
			[](RE::SpellItem* lhs, RE::SpellItem* rhs) {
				return lhs->GetFormID() < rhs->GetFormID();
			});

		for (auto* spell : placeholders) {
			Slot slot;
			slot.form = spell;
			s_slots.push_back(std::move(slot));
		}
		Log::INFO("SpellMakingStore: Placeholder Pool Initialized with {} Slots", s_slots.size());
	}

	void SpellMakingStore::PopulateSpell(RE::SpellItem* form, const SpellRecipe& recipe) {
		//A placeholder carries its plugin effect (first use this session) or the effects of a previously loaded save.
		//Those Effect objects are never deleted: an ActiveEffect holds a raw pointer to one, and the game links
		//restored ActiveEffects to the placeholder's current effects while the base save loads.
		//An old effect that already matches a recipe effect is reused, so an ActiveEffect from the same recipe stays on its spell.
		//The others are dropped from the list but left alive and unchanged, so an ActiveEffect never sees its MGEF swapped.
		std::vector<RE::Effect*> previous(form->effects.begin(), form->effects.end());
		form->effects.clear();

		for (const auto& spec : recipe.effects) {
			//Each effect carries both a FormID and an EditorID; which one resolves the
			//MGEF depends on what the player chose for this effect's Area slider:
			//    1. Area = 10..100 (a numeric area, only offered on area-capable effects):
			//    the view sends a non-empty EditorID naming one of the 91 per-element
			//    area-bucket MGEFs in its related .esm/.esp (e.g. "SKYBPlaceholderFireMGEF42").
			//    They are resolved by EditorID.
			//    2. Area = None or an effect with no Area slider at all:
			//    the EditorID is empty, so we fall back to the FormID, which the view took from the
			//    vanilla Skyrim.esm MGEF (load index 00, stable for every user).
			RE::EffectSetting* mgef = nullptr;
			if (!spec.editorId.empty()) {
				mgef = RE::TESForm::LookupByEditorID<RE::EffectSetting>(spec.editorId);
				if (!mgef) {
					Log::ERROR("SpellMakingStore::PopulateSpell: MGEF EditorID {} Not Found", spec.editorId);
					continue;
				}
			}
			else {
				mgef = RE::TESForm::LookupByID<RE::EffectSetting>(spec.formID);
				if (!mgef) {
					Log::ERROR("SpellMakingStore::PopulateSpell: MGEF FormID {:08X} Not Found", spec.formID);
					continue;
				}
			}
			const float magnitude = static_cast<float>(spec.magnitude);
			const auto duration = static_cast<std::uint32_t>(spec.duration);
			const float cost = static_cast<float>(spec.magickaCost);
			auto match = std::ranges::find_if(previous, [&](RE::Effect* effect) {
				return effect && REBridge::EffectMatches(effect, mgef,magnitude, spec.area, duration, cost);
			});
			RE::Effect* effect = nullptr;
			if (match != previous.end()) {
				effect = *match;
				*match = nullptr;//claimed, so a repeated effect in the recipe cannot reuse it twice
			}
			else {
				effect = new RE::Effect();
				effect->baseEffect = mgef;
				effect->effectItem.magnitude = magnitude;
				effect->effectItem.duration = duration;
				effect->effectItem.area = spec.area;
				effect->cost = cost;
			}
			form->effects.push_back(effect);
		}

		//The engine derives two fields from the effects and never recomputes them when the effects change.
		//avEffectSetting caches the costliest effect's MGEF (casting art, projectile) the first time GetAVEffect runs.
		//On a fresh launch that happens during the save load, before the co-save refill,
		//so it would keep the ESM placeholder's restoration effect.
		//Clearing it lets GetAVEffect rebuild it from the new effects.
		form->hostileCount = static_cast<std::int32_t>(std::ranges::count_if(form->effects,
			[](const RE::Effect* effect) { return effect->IsHostile(); }));
		form->avEffectSetting = nullptr;

		form->SetFullName(recipe.name.c_str());
		form->data.spellType = RE::MagicSystem::SpellType::kSpell;
		form->data.castingType = recipe.castingType;
		form->data.delivery = recipe.delivery;
		//Fire-and-forget spells need a positive charge time. Leaving them at 0 makes the menu show its magicka cost as per-second.
		//Only concentration spells charge at 0.
		form->data.chargeTime = recipe.castingType == RE::MagicSystem::CastingType::kConcentration ? 0.0f : 0.5f;
		form->data.castDuration = 0.0f;
		form->data.range = recipe.delivery == RE::MagicSystem::Delivery::kAimed ? 1000.0f : 0.0f;
		//Set fixed magicka cost. (I'm not sure if the engine could calculate a cost automatically.)
		form->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
		form->data.costOverride = recipe.magickaCost;
		//"Either Hand" equip slot so the spell is equippable.
		form->SetEquipSlot(Resources::eitherHandSlot);
	}

	RE::SpellItem* SpellMakingStore::CreateSpell(const SpellRecipe& recipe) {
		for (auto& slot : s_slots) {
			if (!slot.used) {
				slot.used = true;
				slot.recipe = recipe;
				PopulateSpell(slot.form, recipe);
				return slot.form;
			}
		}
		Log::WARN("SpellMakingStore::CreateSpell: Placeholder pool exhausted ({} slots). '{}' was not created.", s_slots.size(), recipe.name);
		return nullptr;
	}

	bool SpellMakingStore::IsPlaceholder(const RE::SpellItem* spell) {
		return FindSlot(spell) != nullptr;
	}

	SpellMakingStore::Slot* SpellMakingStore::FindSlot(const RE::SpellItem* spell) {
		if (!spell) {
			return nullptr;
		}
		auto it = std::ranges::find(s_slots, spell, &Slot::form);
		return it != s_slots.end() ? &*it : nullptr;
	}

	void SpellMakingStore::ResetSlots() {
		for (auto& slot : s_slots) {
			slot.used = false;
			slot.recipe = SpellRecipe{};
		}
	}

	std::uint32_t SpellMakingStore::RestoreRecipes(std::span<const std::byte> record) {
		ResetSlots();//These slots are soon filled with co-save data.

		RecordReader reader(record);
		std::uint32_t count = 0;
		if (!reader.Read(count)) {
			Log::ERROR("SpellMakingStore::RestoreRecipes: Co-Save Recipe Record Is Empty");
			return 0;
		}
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		std::uint32_t restored = 0;
		for (std::uint32_t n = 0; n < count; ++n) {
			std::string placeholderPlugin;
			RE::FormID placeholderLocalID = 0;
			SpellRecipe recipe;
			if (!reader.ReadString(placeholderPlugin) || !reader.Read(placeholderLocalID) || !ReadRecipe(reader, recipe)) {
				Log::ERROR("SpellMakingStore::RestoreRecipes: Co-Save Recipe Record Truncated After {} of {} Recipe(s)", n, count);
				break;
			}

			auto* slot = FindSlot(dataHandler->LookupForm<RE::SpellItem>(placeholderLocalID, placeholderPlugin));
			if (!slot) {
				Log::WARN("SpellMakingStore::RestoreRecipes: Placeholder {:06X} in {} Is Not in the Pool. '{}' Was Not Restored.",
					placeholderLocalID, placeholderPlugin, recipe.name);
				continue;
			}
			//Refill the placeholder.
			//The game contains information about the spell being in the inventory, equipped, favorited, or set to a hotkey via its stable FormID.
			//Only the contents need to be rebuilt.
			slot->used = true;
			slot->recipe = std::move(recipe);
			PopulateSpell(slot->form, slot->recipe);
			++restored;
		}
		return restored;
	}

	void SpellMakingStore::RestoreBeforeLoad(std::string_view saveName) {
		//SKSE's load callback runs after the base save has loaded, and by then the game has already linked the
		//save's active effects (timed buffs, damage over time, summons) to the placeholders' plugin effects.
		//An unlinked buff is dropped, but the stat bonus it applied is saved separately and stays forever.
		//Reading the recipes from the co-save file here, before the base save loads, gives the game the made spells' real effects to link to.
		auto path = CoSavePath(saveName);
		if (!path) {
			Log::WARN("SpellMakingStore::RestoreBeforeLoad: Save Folder Unavailable");
			return;
		}
		auto record = ReadRecipeRecordFromFile(*path);
		if (!record) {
			//A save with no made spells, or one without a co-save; OnGameLoaded still runs.
			Log::INFO("SpellMakingStore::RestoreBeforeLoad: No Recipe Record in {}", path->generic_string());
			ResetSlots();
			return;
		}
		const std::uint32_t restored = RestoreRecipes(*record);
		Log::INFO("SpellMakingStore::RestoreBeforeLoad: Refilled {} Made-Spell Placeholder(s) from {}", restored, path->filename().generic_string());
	}

	//The plugin's co-save (Storage/CoSave) owns the SKSE callbacks: the settings menu's store keeps
	//its record there too, and SKSE allows a plugin only one set.
	void SpellMakingStore::RegisterSerialization() {
		Storage::CoSave::Store store;
		store.name = "SpellMakingStore";
		store.recordType = kRecipeRecord;
		store.save = OnGameSaved;
		store.load = OnRecipeRecord;
		store.revert = OnRevert;
		store.loaded = OnGameLoaded;
		Storage::CoSave::Add(store);
		Log::INFO("SpellMakingStore::RegisterSerialization: Recipe Record Added to the Co-Save");
	}

	void SpellMakingStore::OnGameSaved(SKSE::SerializationInterface* intfc) {
		if (!intfc->OpenRecord(kRecipeRecord, kRecipeVersion)) {
			Log::WARN("SpellMakingStore::OnGameSaved: Failed to Open Co-Save Record");
			return;
		}
		std::uint32_t usedCount = 0;
		for (const auto& slot : s_slots) {
			if (slot.used) {
				++usedCount;
			}
		}
		intfc->WriteRecordData(usedCount);
		for (const auto& slot : s_slots) {
			if (!slot.used) {
				continue;
			}
			//The placeholder's plugin and local FormID tie the recipe to it.
			//Unlike a full FormID, they can be looked up before the base save loads, when SKSE cannot yet remap load-order indexes.
			//A pool index would shift whenever the plugin's pool changes.
			WriteString(intfc, slot.form->GetFile(0)->GetFilename());
			intfc->WriteRecordData(slot.form->GetLocalFormID());
			WriteRecipe(intfc, slot.recipe);
		}
		Log::INFO("SpellMakingStore::OnGameSaved: Wrote {} Made-Spell Recipe(s) to Co-Save", usedCount);
	}

	//One recipe record of the save being loaded (the co-save hands this store only those).
	//RestoreBeforeLoad has normally refilled the placeholders already. This refills them again from SKSE's
	//own copy, covering a load whose co-save file could not be read early. Refilling with the same
	//recipes reuses the same Effect objects, so it changes nothing. The co-save reverts every store
	//before it reads a save, and RestoreRecipes frees the slots itself.
	void SpellMakingStore::OnRecipeRecord(SKSE::SerializationInterface* intfc, std::uint32_t version, std::uint32_t length) {
		if (version != kRecipeVersion) {
			Log::WARN("SpellMakingStore::OnGameLoaded: Skipped Co-Save Recipe Record Version {} (Expected {})", version, kRecipeVersion);
			return;
		}
		std::vector<std::byte> record(length);
		if (intfc->ReadRecordData(record.data(), length) != length) {
			Log::ERROR("SpellMakingStore::OnGameLoaded: Co-Save Recipe Record Truncated");
			return;
		}
		s_restored = RestoreRecipes(record);
	}

	//After the whole save was read.
	void SpellMakingStore::OnGameLoaded() {
		Log::INFO("SpellMakingStore::OnGameLoaded: Refilled {} Made-Spell Placeholder(s) from Co-Save", s_restored);
		if (s_restored > 0) {
			MadeSpellEquipRefresh::Schedule();
		}
		s_restored = 0;
	}

	void SpellMakingStore::OnRevert(SKSE::SerializationInterface*) {
		ResetSlots();
		s_restored = 0;
	}

}
