#pragma once

namespace Papyrus
{
	// How long a speaker takes to say a topic, measured from the voice files, as Oblivion's Say and SayTo returned.
	// Replaces the modified Fuz Ro D'oh this plugin used until 2024.
	namespace VoiceDuration
	{
		// The total length of the responses of the topic's first TESTopicInfo whose conditions are met for a_speaker
		// and a_target (which may be null). Skyrim's random and say-once choices are not modelled. 5 seconds for a
		// response with no readable voice file, and when no TESTopicInfo is selected or it has no responses.
		[[nodiscard]] double Seconds(RE::TESTopic* a_topic, RE::TESObjectREFR* a_speaker, RE::TESObjectREFR* a_target);
	}
}
