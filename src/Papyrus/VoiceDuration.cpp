#include "Papyrus/VoiceDuration.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace VoiceDuration
{
	namespace
	{
		// The length counted for a response with no readable voice file, and for a topic with no selectable info.
		constexpr double kSilentResponseSeconds = 5.0;

		// The game's destructor, which frees the responses; CommonLib's default one would not.
		// VR's address library lacks the id, hence the VR offset.
		void DestroyDialogueItem(RE::DialogueItem* a_item)
		{
			static REL::Relocation<void(RE::DialogueItem*)> func{ REL::VariantID(34414, 35221, 0x573180) };
			func(a_item);
		}

		struct WaveFormat
		{
			std::uint16_t format;
			std::uint16_t channels;
			std::uint32_t samplesPerSec;
			std::uint32_t bytesPerSec;
			std::uint16_t blockAlign;
			std::uint16_t bitsPerSample;
		};
		static_assert(sizeof(WaveFormat) == 16);

		bool Is(const char (&a_id)[4], const char* a_expected) { return std::memcmp(a_id, a_expected, 4) == 0; }

		// The length of the xWMA or WAVE RIFF stream at the stream's position.
		std::optional<double> RiffSeconds(RE::NiBinaryStream& a_stream)
		{
			char id[4];
			char form[4];
			std::uint32_t size = 0;
			if (!a_stream.read(id, 4) || !Is(id, "RIFF") || !a_stream.read(&size, 1) || !a_stream.read(form, 4)) {
				return std::nullopt;
			}
			const bool xwma = Is(form, "XWMA");
			WaveFormat fmt{};
			while (a_stream.read(id, 4) && a_stream.read(&size, 1)) {
				if (Is(id, "fmt ") && size >= sizeof(WaveFormat)) {
					if (!a_stream.read(&fmt, 1)) {
						return std::nullopt;
					}
					size -= sizeof(WaveFormat);
				} else if (xwma && Is(id, "dpds") && size >= 4) {
					// The last entry is the size of the decoded audio.
					a_stream.seek(static_cast<std::int32_t>(size - 4));
					std::uint32_t decodedBytes = 0;
					const std::uint32_t bytesPerSecond = fmt.samplesPerSec * fmt.channels * (fmt.bitsPerSample / 8u);
					if (!a_stream.read(&decodedBytes, 1) || bytesPerSecond == 0) {
						return std::nullopt;
					}
					return static_cast<double>(decodedBytes) / bytesPerSecond;
				} else if (!xwma && Is(id, "data")) {
					return fmt.bytesPerSec ? std::optional<double>(static_cast<double>(size) / fmt.bytesPerSec) : std::nullopt;
				}
				// Chunks are padded to an even size.
				a_stream.seek(static_cast<std::int32_t>(size + (size & 1)));
			}
			return std::nullopt;
		}

		// A .fuz is "FUZE", a version, the lip data's size and the lip data, then the audio.
		std::optional<double> FileSeconds(const std::string& a_path)
		{
			RE::BSResourceNiBinaryStream stream(a_path);
			if (!stream.good()) {
				return std::nullopt;
			}
			if (a_path.ends_with(".fuz")) {
				char id[4];
				std::uint32_t version = 0;
				std::uint32_t lipBytes = 0;
				if (!stream.read(id, 4) || !Is(id, "FUZE") || !stream.read(&version, 1) || !stream.read(&lipBytes, 1)) {
					return std::nullopt;
				}
				stream.seek(static_cast<std::int32_t>(lipBytes));
			}
			return RiffSeconds(stream);
		}

		// a_voice is the path the game stores, such as "data\SOUND\Voice\...\name.wav". Resource paths start
		// under Data, and the game plays a .fuz of the same name in preference.
		std::optional<double> ResponseSeconds(std::string_view a_voice)
		{
			if (a_voice.size() > 5 && _strnicmp(a_voice.data(), "data\\", 5) == 0) {
				a_voice.remove_prefix(5);
			}
			const std::string stem(a_voice.substr(0, a_voice.rfind('.')));
			for (const char* extension : { ".fuz", ".xwm", ".wav" }) {
				if (auto seconds = FileSeconds(stem + extension)) {
					return seconds;
				}
			}
			return std::nullopt;
		}

		RE::TESTopicInfo* FirstSelectableInfo(RE::TESTopic* a_topic, RE::TESObjectREFR* a_speaker, RE::TESObjectREFR* a_target)
		{
			for (std::uint32_t i = 0; i < a_topic->numTopicInfos; ++i) {
				RE::TESTopicInfo* info = a_topic->topicInfos[i];
				if (info && (!info->objConditions.head || info->objConditions(a_speaker, a_target))) {
					return info;
				}
			}
			return nullptr;
		}
	}

	double Seconds(RE::TESTopic* a_topic, RE::TESObjectREFR* a_speaker, RE::TESObjectREFR* a_target)
	{
		RE::TESTopicInfo* info = FirstSelectableInfo(a_topic, a_speaker, a_target);
		if (!info) {
			return kSilentResponseSeconds;
		}
		auto* item = new RE::DialogueItem(a_topic->ownerQuest, a_topic, info, a_speaker);
		double total = 0.0;
		for (RE::DialogueResponse* response : item->responses) {
			if (response) {
				total += ResponseSeconds(response->voice.c_str()).value_or(kSilentResponseSeconds);
			}
		}
		// Not delete: ~DialogueItem would free the response list a second time.
		DestroyDialogueItem(item);
		RE::free(item);
		// 0 only when the info has no responses.
		return total > 0.0 ? total : kSilentResponseSeconds;
	}
}
