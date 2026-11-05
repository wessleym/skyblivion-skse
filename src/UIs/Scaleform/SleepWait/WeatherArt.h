#pragma once

//Picks the wait menu's weather picture when the menu opens. See SleepWaitContract.h.
//
//The weather is the sky's: the one it is moving to, or, while a change is less than half done, the one it is
//leaving. Its DATA flags give the class (pleasant, cloudy, rainy, snow, tested in that order, as
//Weather.GetClassification tests them). A weather with none of the four flags falls back to the other weather of
//the change, then to Cloudy. With no sky weather at all (some interiors) the last class read in this session is used,
//and Clear when there is none yet.
namespace SleepWait {

	class WeatherArt {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void Push();

		class Sink : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
