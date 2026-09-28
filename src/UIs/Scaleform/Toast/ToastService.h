#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace Toast {

	//Keeps SKYBAchievementToast.swf loaded in the HUD movie and delivers show requests to it.
	//
	//The HUD movie loses its child clips on every load screen,
	//so the clip is injected again when the HUD opens and when a load screen closes.
	//Requests that arrive before the clip reports ready are held and delivered once it does.
	//The movie has its own display queue, so this service guarantees only presence and delivery.
	//
	//Callers decide whether a toast should show. This service applies no gate of its own.
	class ToastService {
	public:
		//Call once, at kDataLoaded.
		static void Register();

		//Safe from any thread: all GFx work is marshaled to UI tasks.
		static void Show(std::string header, std::string title, std::string desc, std::string icon);

	private:
		using ToastFields = std::array<std::string, 4>;

		//Injects the clip if the HUD movie lacks a ready one.
		static void EnsureInjected();

		//Retries DeliverIfReady until the clip reports ready, then drops the held toasts after about three seconds.
		static void FlushWhenReady();

		//One try, as a UI task: delivers everything held once the clip is ready.
		//Ends the chain when there is no HUD movie; the held toasts wait for the next HUD open.
		//False while the clip is not ready yet.
		static bool DeliverIfReady();

		//Claims the flush chain when toasts are held and no chain is running.
		//The caller then starts it with FlushWhenReady.
		[[nodiscard]] static bool TryBeginFlushChain();

		class Sink :
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
			public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;

		//About three seconds.
		static constexpr int kReadyAttempts = 60;
		static constexpr std::chrono::milliseconds kReadyInterval{ 50 };

		//Guards s_pending and s_flushChainActive, which the sinks and UI tasks share.
		static inline std::mutex s_lock;
		static inline std::vector<ToastFields> s_pending;
		static inline bool s_flushChainActive = false;
	};

}
