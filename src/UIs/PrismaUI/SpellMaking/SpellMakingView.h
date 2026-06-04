#pragma once

#include "../PrismaFeatureView.h"
#include "SpellMakingContract.h"

class PrismaViewHandle;

namespace SpellMaking {

class SpellMakingView : public PrismaFeatureView<SpellMakingView> {
public:
    static void Initialize(const PrismaUIService& service);
    static void Open();
    static void Close();

private:
    static constexpr const char* kHtmlPath = "SpellMaking/index.html";
    static constexpr const char* kVerifyJsFunc = SpellMakingContract::JsFunc::SpellMakingVerifyBridges;
    static void RegisterListeners(const PrismaViewHandle& view);

    // JS listener: the player confirmed a purchase. `argument` is the JSON buy payload
    // (consumed by SpellRecipeParser).
    static void OnBuy(const char* argument);
    // JS listener: the player closed the screen without buying.
    static void OnCloseFromJS(const char* argument);
};

}
