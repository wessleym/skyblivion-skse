#pragma once

#include "PrismaUI_API.h"

#include <optional>
class PrismaUIService {
public:
	static std::optional<PrismaUIService> Initialize();
	PRISMA_UI_API::IVPrismaUI1* Api() const { return m_api; }
	void RegisterConsoleLogging(PrismaView view) const;

private:
	PrismaUIService(PRISMA_UI_API::IVPrismaUI1* api, PRISMA_UI_API::IVPrismaUI2* apiV2) : m_api(api), m_apiV2(apiV2) {}

	PRISMA_UI_API::IVPrismaUI1* m_api;//Ultimately stores IVPrismaUI2 (or IVPrismaUI1 as a fallback)
	PRISMA_UI_API::IVPrismaUI2* m_apiV2;
};
