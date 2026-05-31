#include "PrismaUIService.h"

namespace {
    void OnConsoleMessage(PrismaView, PRISMA_UI_API::ConsoleMessageLevel level, const char* message) {
        const char* msg = message ? message : "";
        const auto format = "[JS] {}";
        switch (level) {
        case PRISMA_UI_API::ConsoleMessageLevel::Error:
            Log::ERROR(format, msg);
            break;
        case PRISMA_UI_API::ConsoleMessageLevel::Warning:
            Log::WARN(format, msg);
            break;
        case PRISMA_UI_API::ConsoleMessageLevel::Debug:
            Log::DEBUG(format, msg);
            break;
        default:  // Log, Info
            Log::INFO(format, msg);
            break;
        }
    }
}

std::optional<PrismaUIService> PrismaUIService::Initialize() {
    //V2 is a subclass of V1 and additionally allows receiving console messages.
    if (auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
        Log::DEBUG("PrismaUIService: PrismaUI V2 Acquired");
        return PrismaUIService(apiV2, apiV2);
    }
    if (auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>()) {
        Log::WARN("PrismaUIService: PrismaUI V1 Acquired (V2 Not Found)");
        return PrismaUIService(api, nullptr);
    }
    Log::ERROR("PrismaUIService: PrismaUI Plugin API Not Found");
    return std::nullopt;
}

void PrismaUIService::RegisterConsoleLogging(PrismaView view) const {
    if (m_apiV2) {
        m_apiV2->RegisterConsoleCallback(view, &OnConsoleMessage);
    }
}
