#pragma once

#include <string>
#include <string_view>
#include <vector>

//SKSE mod events: the channel the Scaleform menus and their Papyrus scripts use to reach this plugin.
//
//Only the legacy shape reaches a native SKSE::ModCallbackEvent sink:
//Papyrus Form.SendModEvent(name, strArg, numArg) and Scaleform skse.SendModEvent.
//Papyrus ModEvent.Create/Push/Send events never arrive here.
//
//A sink fires on the sender's thread: the Papyrus VM's, or the movie's for Scaleform.
//Handlers copy what they need out of the event, then do game work inside SKSE::GetTaskInterface()->AddTask.
class ModEvents {
public:
	//Adds a sink to SKSE's mod-event source. False when SKSE has no source yet.
	static bool AddSink(RE::BSTEventSink<SKSE::ModCallbackEvent>* sink);

	//The event's strArg as an owned string, empty when absent.
	[[nodiscard]] static std::string StrArg(const SKSE::ModCallbackEvent& event);

	//Answers a request with a legacy-shape mod event, which Papyrus receives through RegisterForModEvent.
	static void Send(const char* eventName, std::string_view strArg, float numArg = 0.0f);

	//Splits a payload on a separator, keeping empty fields: "a^^b" gives {"a", "", "b"}.
	[[nodiscard]] static std::vector<std::string> Split(std::string_view payload, char separator);

	//A decimal integer field. Parsing stops at the first non-digit; a field with no digits reads as fallback.
	[[nodiscard]] static int ParseInt(std::string_view field, int fallback = 0);

	//A FormID as Papyrus writes one: GetFormID's signed 32-bit value in decimal, the form Game.GetFormEx takes.
	//Zero when the field holds none.
	[[nodiscard]] static RE::FormID ParseFormID(std::string_view field);
	[[nodiscard]] static std::string FormatFormID(RE::FormID formID);
};
