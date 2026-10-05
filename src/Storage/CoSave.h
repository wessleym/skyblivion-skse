#pragma once

#include <cstdint>

namespace SKSE { class SerializationInterface; }

//The plugin's one SKSE co-save.
//
//SKSE gives a plugin one unique id and one save, load and revert callback, and a load walks the
//save's records once, in order. So no store registers with SKSE itself: each adds its record
//type here at plugin load, and this file owns the callbacks and hands every record to the store
//that wrote it. A record type nobody added is skipped with a log line.
namespace Storage::CoSave {

	struct Store {
		//For the log.
		const char* name = "";
		//The store's record type; one per store.
		std::uint32_t recordType = 0;
		//Writes the store's record(s).
		void (*save)(SKSE::SerializationInterface* intfc) = nullptr;
		//Reads one record of the store's type. SKSE skips whatever it leaves unread.
		void (*load)(SKSE::SerializationInterface* intfc, std::uint32_t version, std::uint32_t length) = nullptr;
		//Back to "no save loaded". Also called at the start of every load.
		void (*revert)(SKSE::SerializationInterface* intfc) = nullptr;
		//Optional: after every record of a save was read.
		void (*loaded)() = nullptr;
	};

	//Call at plugin load, before Install.
	void Add(const Store& store);

	//Call once at plugin load, after every Add.
	bool Install();

}
