#include "CoSave.h"

#include <string>
#include <vector>

namespace Storage::CoSave {

	namespace {
		std::vector<Store>& Stores() {
			static std::vector<Store> stores;
			return stores;
		}

		std::string FourCC(std::uint32_t type) {
			std::string text;
			for (int shift = 24; shift >= 0; shift -= 8) {
				const char c = static_cast<char>((type >> shift) & 0xFF);
				text += (c >= 32 && c < 127) ? c : '?';
			}
			return text;
		}

		void OnSave(SKSE::SerializationInterface* intfc) {
			for (const auto& store : Stores()) {
				store.save(intfc);
			}
		}

		void OnRevert(SKSE::SerializationInterface* intfc) {
			for (const auto& store : Stores()) {
				store.revert(intfc);
			}
		}

		void OnLoad(SKSE::SerializationInterface* intfc) {
			//Every store starts from empty, whatever SKSE did before.
			OnRevert(intfc);
			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;
			while (intfc->GetNextRecordInfo(type, version, length)) {
				const Store* owner = nullptr;
				for (const auto& store : Stores()) {
					if (store.recordType == type) {
						owner = &store;
						break;
					}
				}
				if (!owner) {
					Log::WARN("CoSave: Record {} (version {}, {} bytes) belongs to no store; skipped.", FourCC(type), version, length);
					continue;
				}
				owner->load(intfc, version, length);
			}
			for (const auto& store : Stores()) {
				if (store.loaded) {
					store.loaded();
				}
			}
		}
	}

	void Add(const Store& store) {
		for (const auto& known : Stores()) {
			if (known.recordType == store.recordType) {
				Log::ERROR("CoSave: {} and {} both use record {}; {} is not added.", known.name, store.name, FourCC(store.recordType), store.name);
				return;
			}
		}
		if (!store.save || !store.load || !store.revert) {
			Log::ERROR("CoSave: {} lacks a save, load or revert function; not added.", store.name);
			return;
		}
		Stores().push_back(store);
	}

	bool Install() {
		auto* intfc = SKSE::GetSerializationInterface();
		if (!intfc) {
			Log::WARN("CoSave: No serialization interface. Nothing is kept in saves.");
			return false;
		}
		intfc->SetUniqueID(UniqueID);
		intfc->SetSaveCallback(OnSave);
		intfc->SetLoadCallback(OnLoad);
		intfc->SetRevertCallback(OnRevert);
		std::string names;
		for (const auto& store : Stores()) {
			names += (names.empty() ? "" : ", ") + std::string(store.name) + " (" + FourCC(store.recordType) + ")";
		}
		Log::INFO("CoSave: Installed as {} for {}.", FourCC(UniqueID), names.empty() ? std::string("no store") : names);
		return true;
	}

}
