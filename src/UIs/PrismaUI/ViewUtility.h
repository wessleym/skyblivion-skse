#pragma once

#include "PrismaUI_API.h"

class PrismaUIService;

class ViewUtility {
public:
	static PrismaView CreateHiddenView(const PrismaUIService& service, const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReady);
};
