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
// A Derived must provide (and befriend this base so they can stay private):
//   static constexpr const char* kHtmlPath;      // e.g. "Persuasion/index.html"
//   static constexpr const char* kVerifyJsFunc;  // JS function that verifies the bridges
//   static void RegisterListeners(const PrismaViewHandle& view);
//   friend class PrismaFeatureView<Derived>;
//
// The JavaScript listener callbacks must be free/static functions: the PrismaUI API takes a
// context-free function pointer (void(*)(const char*)), so they cannot capture state,
// which is why the whole feature view is a static singleton rather than an instance.
template <typename Derived>
class PrismaFeatureView {
public:
	static void Initialize(const PrismaUIService& service) {
		const PrismaView view = ViewUtility::CreateHiddenView(service, Derived::kHtmlPath, &PrismaFeatureView::OnDomReady);
		s_handle = PrismaViewHandle(&service, view);
	}

protected:
	static inline PrismaViewHandle s_handle{};

	// Guard for entry points that require a live view. Logs and returns false when not ready.
	[[nodiscard]] static bool Ready(const char* context) {
		if (s_handle.IsValid()) {
			return true;
		}
		Log::WARN("{}: view not ready.", context);
		return false;
	}

private:
	// Fires once the view's DOM is ready (asynchronously, after CreateView returns). s_handle
	// is already assigned by then, since Initialize sets it before this can run.
	static void OnDomReady(PrismaView) {
		Derived::RegisterListeners(s_handle);
		s_handle.InvokeByFunctionName(Derived::kVerifyJsFunc);
		s_handle.Hide();
	}
};
