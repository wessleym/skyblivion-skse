#pragma once

#include "../PrismaFeatureView.h"
#include "PersuasionContract.h"

namespace RE { class Actor; }
class PrismaViewHandle;

namespace Persuasion {

class PersuasionView : public PrismaFeatureView<PersuasionView> {
public:
    static void Initialize(const PrismaUIService& service);
    static void Open(RE::Actor* target);
    static void Close();

private:
    //I'm experimenting with two modes:
    enum class CaptivityMode {
        Standard,//Dialogue stays open and visible but the Persuasion game.
        Captive//Dialogue stays open but invisible.
    };

    static constexpr const char* kHtmlPath = "Persuasion/index.html";
    static constexpr const char* kVerifyJsFunc = PersuasionContract::JsFunc::PersuasionVerifyBridges;
    static void RegisterListeners(const PrismaViewHandle& view);

    static RE::FormID s_currentTargetFormID;//Form ID of NPC currently being persuaded (0 if none)
    static constexpr CaptivityMode kConfiguredMode = CaptivityMode::Captive;

    //JavaScript Listeners:
    static void OnWedgeHover(const char* argument);
    static void OnCloseFromJS(const char* argument);
    static void OnDispositionChanged(const char* argument);
    static void OnBribe(const char* argument);

    static void SendBribeResult(bool success, int price, int gain, float disposition, const char* reason);
    static void SetDisposition(RE::Actor* actor, float value);
};

}
