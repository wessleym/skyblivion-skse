#include "DialogueLipTrace.h"
#include "Platform/MainThread.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace Dialogue {

	namespace {
		constexpr int  kSamples = 60;
		constexpr auto kInterval = std::chrono::milliseconds(100);

		//The newest pick wins: an older trace stops sampling and logs what it has.
		std::atomic<std::uint32_t> s_generation{ 0 };

		struct Trace {
			std::string   how;
			int           topicIndex = -1;
			std::uint32_t generation = 0;
			std::string   speaker;
			RE::FormID    speakerID = 0;
			RE::FormID    firstInfo = 0;
			RE::FormID    lastInfo = 0;
			int           infoChanges = 0;
			int           taken = 0;
			//One character per sample, '-' when there was no speaker or no face to read.
			std::string p1, p3, lip, voice, over, ai;
			bool        logged = false;
		};

		//0.0 .. 1.0 as one digit, '9' at full.
		char Digit(float value) {
			const int d = static_cast<int>(value * 10.0f);
			return static_cast<char>('0' + std::clamp(d, 0, 9));
		}

		RE::Actor* Speaker() {
			auto* topicManager = RE::MenuTopicManager::GetSingleton();
			if (!topicManager) {
				return nullptr;
			}
			auto handle = topicManager->speaker ? topicManager->speaker : topicManager->lastSpeaker;
			auto reference = handle.get();
			return reference ? reference->As<RE::Actor>() : nullptr;
		}

		//Main thread, start of a frame.
		void Sample(Trace& trace) {
			++trace.taken;
			auto* actor = Speaker();
			if (!actor) {
				for (auto* column : { &trace.p1, &trace.p3, &trace.lip, &trace.voice, &trace.over, &trace.ai }) {
					*column += '-';
				}
				return;
			}
			if (trace.speakerID == 0) {
				trace.speakerID = actor->formID;
				trace.speaker = actor->GetName();
			}
			if (auto* topicManager = RE::MenuTopicManager::GetSingleton()) {
				const RE::FormID info = topicManager->currentTopicInfo ? topicManager->currentTopicInfo->formID : 0;
				if (info != 0) {
					if (trace.firstInfo == 0) {
						trace.firstInfo = info;
					}
					else if (info != trace.lastInfo) {
						++trace.infoChanges;
					}
					trace.lastInfo = info;
				}
			}
			if (auto* face = actor->GetFaceGenAnimationData()) {
				trace.p1 += Digit(face->phoneme1.GetMaxValue());
				trace.p3 += Digit(face->phoneme3.GetMaxValue());
				trace.lip += face->dialogueData ? '1' : '0';
				trace.over += face->exprOverride ? '1' : '0';
			}
			else {
				for (auto* column : { &trace.p1, &trace.p3, &trace.lip, &trace.over }) {
					*column += '-';
				}
			}
			if (auto* high = actor->GetHighProcess()) {
				const int state = static_cast<int>(high->voiceState.underlying());
				trace.voice += static_cast<char>('0' + std::clamp(state, 0, 9));
			}
			else {
				trace.voice += '-';
			}
			trace.ai += actor->IsAIEnabled() ? '1' : '0';
		}

		void LogTrace(Trace& trace) {
			if (trace.logged) {
				return;
			}
			trace.logged = true;
			Log::INFO("LipTrace: {} topic {} speaker {} ({:08X}) info {:08X}{} : {} samples 100 ms apart\n"
					  "  phoneme1 (streamed) {}\n  phoneme3 (blended)  {}\n  lip data attached   {}\n"
					  "  voice state         {}  (1 request, 2 start, 3 continue, 0 none)\n"
					  "  expression override {}\n  AI enabled          {}",
				trace.how, trace.topicIndex, trace.speaker.empty() ? "(none)" : trace.speaker, trace.speakerID,
				trace.firstInfo, trace.infoChanges ? std::format(" (+{} change(s))", trace.infoChanges) : "",
				trace.taken, trace.p1, trace.p3, trace.lip, trace.voice, trace.over, trace.ai);
		}
	}

	void DialogueLipTrace::Start(std::string how, int topicIndex) {
		auto trace = std::make_shared<Trace>();
		trace->how = std::move(how);
		trace->topicIndex = topicIndex;
		trace->generation = s_generation.fetch_add(1) + 1;
		Log::INFO("LipTrace: {} topic {} picked; sampling the speaker for {} s.", trace->how, topicIndex,
			kSamples * kInterval.count() / 1000);
		std::thread([trace]() {
			for (int i = 0; i < kSamples; ++i) {
				std::this_thread::sleep_for(kInterval);
				if (s_generation.load() != trace->generation) {
					break;
				}
				//On the main thread at the start of the next frame, where the engine reads and writes faces.
				MainThread::Post([trace]() {
					if (s_generation.load() == trace->generation) {
						Sample(*trace);
					}
				});
			}
			MainThread::Post([trace]() { LogTrace(*trace); });
		}).detach();
	}

}
