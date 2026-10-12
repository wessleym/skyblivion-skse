#pragma once

#include <functional>

//The game's main thread, once per frame.
//
//SKSE's task queues (AddTask, AddUITask) can run on worker threads. A call into a live
//Scaleform movie from a worker thread races the main thread advancing and drawing it, and can
//crash inside Scaleform. So every push into a movie goes through Post: the task runs at the
//start of the next frame, on the thread that advances every movie, before the engine touches them.
namespace MainThread {
	//Hooks the frame loop's call to Main::Update. Once, at plugin load, after the trampoline
	//is allocated. False when the call site is not where this runtime is expected to have it;
	//Post then falls back to SKSE's UI queue.
	bool Install();

	//Runs a_task on the main thread at the start of the next frame. Never inline, even when
	//called from the main thread: a caller inside a movie's script stays inside it.
	void Post(std::function<void()> a_task);

	//True when the frame hook is in place.
	bool Installed();
}
