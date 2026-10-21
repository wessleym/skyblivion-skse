#include "Platform/MainThread.h"

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

#include <Windows.h>

namespace MainThread {

	namespace {
		// A task that throws is logged and skipped; it must not unwind into the game's frame.
		void RunGuarded(const std::function<void()>& a_task) {
			try {
				a_task();
			} catch (const std::exception& e) {
				Log::ERROR("MainThread: a posted task threw: {}", e.what());
			} catch (...) {
				Log::ERROR("MainThread: a posted task threw");
			}
		}

		std::mutex                         g_lock;
		std::vector<std::function<void()>> g_queue;
		std::atomic<bool>                  g_installed{ false };
		std::atomic<bool>                  g_firstFrameLogged{ false };

		//Runs what was posted before this frame. A task posting another task gets the next frame.
		void Drain() {
			std::vector<std::function<void()>> tasks;
			{
				std::scoped_lock lock(g_lock);
				tasks.swap(g_queue);
			}
			if (!g_firstFrameLogged.exchange(true)) {
				Log::INFO("MainThread: frame hook running on thread {}", ::GetCurrentThreadId());
			}
			for (auto& task : tasks) {
				if (task) {
					RunGuarded(task);
				}
			}
		}

		struct FrameHook {
			static void thunk(RE::Main* a_main, float a_delta) {
				Drain();
				func(a_main, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	bool Install() {
		//The frame loop's call to Main::Update, the site every "once a frame" plugin hooks
		//(DebugMenu.dll sits on it too; the trampoline chains them). SE 1.5.97 / AE / VR offsets.
		REL::Relocation<std::uintptr_t> site{ REL::RelocationID(35551, 36544), REL::VariantOffset(0x11F, 0x160, 0x11F) };
		const auto address = site.address();
		const auto version = REL::Module::get().version().string();
		const auto opcode = *reinterpret_cast<const std::uint8_t*>(address);
		if (opcode != 0xE8) {
			Log::ERROR("MainThread: frame hook NOT installed on runtime {}: no call at {:X} (byte {:02X}); "
					   "movie pushes fall back to SKSE's UI task queue",
				version, address, opcode);
			return false;
		}
		FrameHook::func = SKSE::GetTrampoline().write_call<5>(address, FrameHook::thunk);
		g_installed = true;
		Log::INFO("MainThread: frame hook installed at {:X} on runtime {}", address, version);
		return true;
	}

	void Post(std::function<void()> a_task) {
		if (!a_task) {
			return;
		}
		if (!g_installed) {
			SKSE::GetTaskInterface()->AddUITask([task = std::move(a_task)] { RunGuarded(task); });
			return;
		}
		std::scoped_lock lock(g_lock);
		g_queue.push_back(std::move(a_task));
	}

	bool Installed() {
		return g_installed;
	}

}
