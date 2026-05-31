#pragma once

#include "SpellMakingStore.h"

namespace SpellMaking {

class SpellRecipeParser {
public:
    [[nodiscard]] static bool Parse(const char* payload, SpellMakingStore::SpellRecipe& recipe, int& goldPrice);
};

}
