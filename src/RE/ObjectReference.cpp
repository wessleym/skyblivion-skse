#include "RE/ObjectReference.h"

namespace RE
{
    bool IsAnimPlaying(RE::TESObjectREFR* target)
    {
        if (!target)
            return false;


        RE::NiAVObject* ni_3d = target->Get3D2();

        if (!ni_3d)
            return false;

        RE::NiNode* ni_node = ni_3d->AsNode();

        if (!ni_node || !ni_node->controllers)
            return false;

        RE::NiControllerManager* ni_manager = ni_node->controllers->AsNiControllerManager();

        if (!ni_manager)
            return false;


        if (ni_manager->sequenceArray.size() != 0)
        {
            for (auto& sequence : ni_manager->sequenceArray)
            {
                if (sequence && sequence->state.underlying() != 0)
                {
                    return true;
                }
            }
        }

        return false;

    }
}
