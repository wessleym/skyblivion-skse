class MouseUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBMouseUtility", "SetCursorPosition", SetCursorPosition);
    }

private:
    static void SetCursorPosition(RE::StaticFunctionTag*, float x, float y) {
        if (auto cursor = RE::MenuCursor::GetSingleton()) {
            cursor->cursorPosX = x;
            cursor->cursorPosY = y;
        }
    }
};