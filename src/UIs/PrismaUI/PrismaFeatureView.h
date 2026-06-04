#pragma once

#include "PrismaUIService.h"
#include "PrismaViewHandle.h"
#include "ViewUtility.h"

#include "Platform/Log.h"

//CRTP base for a single PrismaUI feature view (e.g. PersuasionView, SpellMakingView).
//(The name PrismaView is already taken.)
//It contains the lifecycle every feature view repeats:
//    Initialize: Create the (hidden) view and hook up the OnDomReady handshake.
//    OnDomReady: Register the view's JS listeners, ask JavaScript to verify the bridges, stay hidden.
//    s_handle: The shared view handle
//    Ready: A readiness guard for entry points
//
//A Derived class should establish this function:
//    static void Initialize(const PrismaUIService& service)
//It should call Initialize(service, kHtmlPath, kVerifyJsFunc, &RegisterListeners).
//
// The JavaScript listener callbacks must be free/static functions: the PrismaUI API takes a
// context-free function pointer (void(*)(const char*)), so they cannot capture state,
// which is why the whole feature view is a static singleton rather than an instance.
template <typename Derived>
class PrismaFeatureView {
protected:
	using RegisterListenersFn = void (*)(const PrismaViewHandle&);

	static void Initialize(const PrismaUIService& service, const char* htmlPath, const char* verifyJsFunc, RegisterListenersFn registerListeners) {
		s_verifyJsFunc = verifyJsFunc;
		s_registerListeners = registerListeners;
		const PrismaView view = ViewUtility::CreateHiddenView(service, htmlPath, &PrismaFeatureView::OnDomReady);
		s_handle = PrismaViewHandle(&service, view);
	}

	static inline PrismaViewHandle s_handle{};

	// Guard for entry points that require a live view. Logs and returns false when not ready.
	[[nodiscard]] static bool Ready(const char* context) {
		if (!s_handle.IsValid()) {
			Log::WARN("{} view was not created. See previous log entries.", context);
			return false;
		}
		if (!s_domReady) {
			Log::WARN("{} view exists, but its page never finished loading. It will not be shown.", context);
			return false;
		}
		return true;
	}

private:
	static inline bool s_domReady = false;
	static inline const char* s_verifyJsFunc = nullptr;
	static inline RegisterListenersFn s_registerListeners = nullptr;

	//Fires once the view's DOM is ready (asynchronously, after CreateView returns).
	//s_handle, s_verifyJsFunc and s_registerListeners are already assigned by then.
	static void OnDomReady(PrismaView) {
		s_registerListeners(s_handle);
		s_handle.InvokeByFunctionName(s_verifyJsFunc);
		s_handle.Hide();
		s_domReady = true;
	}
};
