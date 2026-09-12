#pragma once

#include "PrismaUI_API.h"

#include <filesystem>

class PrismaUIService;

class ViewUtility {
public:
	static PrismaView CreateHiddenView(const PrismaUIService& service, const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReady);

	//Absolute path of a file under Data/PrismaUI/views, e.g. "Stats/perk-trees.js".
	//The file need not exist: this also resolves where to write one.
	static std::filesystem::path ResolveViewFile(const char* relativePath);
};
