#include "ModEvents.h"

#include <charconv>
#include <cstdint>

bool ModEvents::AddSink(RE::BSTEventSink<SKSE::ModCallbackEvent>* sink) {
	auto* source = SKSE::GetModCallbackEventSource();
	if (!source) {
		return false;
	}
	source->AddEventSink(sink);
	return true;
}

std::string ModEvents::StrArg(const SKSE::ModCallbackEvent& event) {
	const char* value = event.strArg.c_str();
	return value ? value : "";
}

void ModEvents::Send(const char* eventName, std::string_view strArg, float numArg) {
	auto* source = SKSE::GetModCallbackEventSource();
	if (!source) {
		Log::WARN("ModEvents: No mod-event source. {} not sent.", eventName);
		return;
	}
	const std::string ownedStrArg{ strArg };
	SKSE::ModCallbackEvent event{ eventName, RE::BSFixedString(ownedStrArg.c_str()), numArg, nullptr };
	source->SendEvent(&event);
}

std::vector<std::string> ModEvents::Split(std::string_view payload, char separator) {
	std::vector<std::string> fields;
	while (true) {
		const auto position = payload.find(separator);
		fields.emplace_back(payload.substr(0, position));
		if (position == std::string_view::npos) {
			return fields;
		}
		payload = payload.substr(position + 1);
	}
}

int ModEvents::ParseInt(std::string_view field, int fallback) {
	int value = 0;
	const auto result = std::from_chars(field.data(), field.data() + field.size(), value);
	return result.ec == std::errc{} ? value : fallback;
}

RE::FormID ModEvents::ParseFormID(std::string_view field) {
	std::int32_t value = 0;
	const auto result = std::from_chars(field.data(), field.data() + field.size(), value);
	return result.ec == std::errc{} ? static_cast<RE::FormID>(value) : 0;
}

std::string ModEvents::FormatFormID(RE::FormID formID) {
	return std::to_string(static_cast<std::int32_t>(formID));
}
