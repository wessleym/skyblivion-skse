#include "SpellMakingStore.h"
#include "MadeSpellEquipRefresh.h"

#include <algorithm>
#include <string_view>

//See SpellMakingStore.h for the placeholder-pool trick and why made spells cannot be created at runtime.
//The SpellMakingStore members manage the placeholder pool and the SKSE serialization callbacks.

namespace SpellMaking {

	std::vector<SpellMakingStore::Slot> SpellMakingStore::s_slots;

	namespace {
		//Placeholder Pool: Every SpellItem whose name is exactly this string is a placeholder pool slot.
		constexpr std::string_view kPoolMarkerName = "SKYBSpellMakingSpell";//Spell Making Required Plugin Data (1/2)

		//Co-Save Record:
		constexpr std::uint32_t kSerializationID = 'SKYB';
		constexpr std::uint32_t kRecipeRecord = 'SPRC';
		constexpr std::uint32_t kRecipeVersion = 1;//OpenRecord requires a version.

		void WriteString(SKSE::SerializationInterface* intfc, const std::string& str) {
			const std::uint32_t len = static_cast<std::uint32_t>(str.size());
			intfc->WriteRecordData(len);
			if (len > 0) {
				intfc->WriteRecordData(str.data(), len);
			}
		}

		bool ReadString(SKSE::SerializationInterface* intfc, std::string& out) {
			std::uint32_t len = 0;
			if (intfc->ReadRecordData(len) == 0) {
				return false;
			}
			out.resize(len);
			if (len > 0 && intfc->ReadRecordData(out.data(), len) != len) {
				return false;
			}
			return true;
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

		void ReadRecipe(SKSE::SerializationInterface* intfc, SpellMakingStore::SpellRecipe& recipe) {
			ReadString(intfc, recipe.name);
			intfc->ReadRecordData(recipe.castingType);
			intfc->ReadRecordData(recipe.delivery);
			intfc->ReadRecordData(recipe.magickaCost);
			std::uint32_t effectCount = 0;
			intfc->ReadRecordData(effectCount);
			recipe.effects.reserve(effectCount);
			for (std::uint32_t i = 0; i < effectCount; ++i) {
				SpellMakingStore::EffectSpec spec{};
				RE::FormID savedID = 0;
				intfc->ReadRecordData(savedID);
				ReadString(intfc, spec.editorId);
				intfc->ReadRecordData(spec.magnitude);
				intfc->ReadRecordData(spec.duration);
				intfc->ReadRecordData(spec.area);
				intfc->ReadRecordData(spec.magickaCost);
				//Remap the MGEF FormID into the current load order.
				RE::FormID resolvedID = 0;
				spec.formID = intfc->ResolveFormID(savedID, resolvedID) ? resolvedID : savedID;
				recipe.effects.push_back(std::move(spec));
			}
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
		//Clear any existing effects.
		//A placeholder may be blank (first use) or carry stale effects from a prior session.
		for (auto* effect : form->effects) {
			delete effect;
		}
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
			auto* effect = new RE::Effect();
			effect->baseEffect = mgef;
			effect->effectItem.magnitude = static_cast<float>(spec.magnitude);
			effect->effectItem.duration = static_cast<std::uint32_t>(spec.duration);
			effect->effectItem.area = spec.area;
			effect->cost = static_cast<float>(spec.magickaCost);
			form->effects.push_back(effect);
		}

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
		//"Either Hand" equip slot (vanilla form 0x00013F44) so the spell is equippable.
		if (auto* eitherHand = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F44)) {
			form->SetEquipSlot(eitherHand);
		}
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
		if (!spell) {
			return false;
		}
		for (const auto& slot : s_slots) {
			if (slot.form == spell) {
				return true;
			}
		}
		return false;
	}

	void SpellMakingStore::ResetSlots() {
		for (auto& slot : s_slots) {
			slot.used = false;
			slot.recipe = SpellRecipe{};
		}
	}

	void SpellMakingStore::RegisterSerialization() {
		auto* serializationInterface = SKSE::GetSerializationInterface();
		if (!serializationInterface) {
			Log::WARN("SpellMakingStore::RegisterSerialization: Serialization Interface Unavailable");
			return;
		}
		serializationInterface->SetUniqueID(kSerializationID);
		serializationInterface->SetSaveCallback(OnGameSaved);
		serializationInterface->SetLoadCallback(OnGameLoaded);
		serializationInterface->SetRevertCallback(OnRevert);
		Log::INFO("SpellMakingStore::RegisterSerialization: Serialization Callbacks Registered");
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
		for (std::uint32_t i = 0; i < s_slots.size(); ++i) {
			const auto& slot = s_slots[i];
			if (!slot.used) {
				continue;
			}
			intfc->WriteRecordData(i);//ties the recipe to its placeholder
			WriteRecipe(intfc, slot.recipe);
		}
		Log::INFO("SpellMakingStore::OnGameSaved: Wrote {} Made-Spell Recipe(s) to Co-Save", usedCount);
	}

	void SpellMakingStore::OnGameLoaded(SKSE::SerializationInterface* intfc) {
		ResetSlots();//These slots are soon filled with co-save data.

		std::uint32_t restored = 0;
		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;
		while (intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kRecipeRecord) {
				continue;
			}
			std::uint32_t count = 0;
			intfc->ReadRecordData(count);
			for (std::uint32_t n = 0; n < count; ++n) {
				std::uint32_t slotIndex = 0;
				intfc->ReadRecordData(slotIndex);

				SpellRecipe recipe;
				ReadRecipe(intfc, recipe);

				if (slotIndex < s_slots.size()) {
					auto& slot = s_slots[slotIndex];
					slot.used = true;
					slot.recipe = recipe;
					if (slot.form) {
						//Refill the placeholder.
						//The game contains information about the spell being in the inventory, equipped, favorited, or set to a hotkey via its stable FormID.
						//Only the contents need to be rebuilt.
						PopulateSpell(slot.form, recipe);
						++restored;
					}
				}
				else {
					Log::WARN("SpellMakingStore::OnGameLoaded: Co-Save Slot Index {} Out of Pool Range ({})",
						slotIndex, s_slots.size());
				}
			}
		}
		Log::INFO("SpellMakingStore::OnGameLoaded: Refilled {} Made-Spell Placeholder(s) from Co-Save", restored);
		if (restored > 0) {
			MadeSpellEquipRefresh::Schedule();
		}
	}

	void SpellMakingStore::OnRevert(SKSE::SerializationInterface*) {
		ResetSlots();
	}

}
