#pragma once
// The persuasion wheel as a Scaleform menu: a native IMenu around
// Interface\Skyblivion\SKYBPersuasionWheel.swf. It is the wheel's default host; the PrismaUI
// view is used instead when SKYBPersuasion.ini [General] UsePrismaView = 1.
//
// Contract with the movie (gfx.io.GameDelegate / Invoke):
//   movie -> DLL   SKYBPersuade_Ready()                 movie is up: send the conversation
//                  SKYBPersuade_Hover(preferenceName)   the wedge under the cursor or focus ("" = none): the NPC's face
//                  SKYBPersuade_Disposition(value)      the disposition to store (after a pick, a decay turn, on close)
//                  SKYBPersuade_Bribe(disposition)      a bribe at the value the wheel shows
//                  SKYBPersuade_BribeQuote(disposition) the Bribe button's price at that value
//                  SKYBPersuade_Close()
//                  PlaySound(editorID)
//   DLL -> movie   _root.wheel.SetPersuasion({name, disposition, preferences, rules, gamepad})
//                  _root.wheel.SetBribeResult({success, price, gain, disposition, reason})
//                  _root.wheel.SetBribeQuote({disposition, price, gain, enabled, useful, affordable})
//
// The game is not paused while the wheel is open (as with the PrismaUI view), so the face
// animates and the dialogue session keeps the NPC in place (DialogueSession). Every push into
// the movie goes through MainThread::Post.
namespace RE { class Actor; }

namespace Persuasion
{
	class PersuasionMenu : public RE::IMenu
	{
	public:
		static constexpr const char*      MENU_NAME = "SKYBPersuasionMenu";
		static constexpr std::string_view FILE_NAME = "Skyblivion/SKYBPersuasionWheel";

		PersuasionMenu();

		static void Register();
		static void Open(RE::Actor* a_target);
		static void Close();

		void                   Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg) override;
		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

		static RE::stl::owner<RE::IMenu*> Creator() { return new PersuasionMenu(); }

	private:
		static void OnReady(const RE::FxDelegateArgs& a_params);
		static void OnHover(const RE::FxDelegateArgs& a_params);
		static void OnDisposition(const RE::FxDelegateArgs& a_params);
		static void OnBribe(const RE::FxDelegateArgs& a_params);
		static void OnBribeQuote(const RE::FxDelegateArgs& a_params);
		static void OnClose(const RE::FxDelegateArgs& a_params);
		static void OnPlaySound(const RE::FxDelegateArgs& a_params);
	};
}
