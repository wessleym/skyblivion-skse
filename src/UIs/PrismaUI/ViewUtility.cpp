#include "PrismaUIService.h"
#include "ViewUtility.h"

#include <filesystem>

namespace {
	std::filesystem::path ResolveViewFile(const char* htmlPath) {
		wchar_t exePath[MAX_PATH];
		const auto exePathLength = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
		std::filesystem::path gameRoot = (exePathLength > 0 && exePathLength < MAX_PATH)
			? std::filesystem::path(exePath).parent_path()
			: std::filesystem::current_path();
		return gameRoot / "Data" / "PrismaUI" / "views" / htmlPath;
	}
}

PrismaView ViewUtility::CreateHiddenView(const PrismaUIService& service, const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReady) {
	const std::filesystem::path htmlFile = ResolveViewFile(htmlPath);
	if (!std::filesystem::exists(htmlFile)) {
		Log::ERROR("ViewUtility: .html Missing: {}. View will not be created.", htmlFile.string());
		return 0;
	}

	const std::filesystem::path scriptFile = htmlFile.parent_path() / "script.js";
	if (!std::filesystem::exists(scriptFile)) {
		Log::ERROR("ViewUtility: .js Missing: {}. View will not be created.", scriptFile.string());
		return 0;
	}

	const std::filesystem::path styleFile = htmlFile.parent_path() / "style.css";
	if (!std::filesystem::exists(styleFile)) {
		Log::WARN("ViewUtility: .css Missing: {}. View will render unstyled.", styleFile.string());
	}

	PrismaView view = service.Api()->CreateView(htmlPath, onDomReady);
	service.RegisterConsoleLogging(view);
	//Hide immediately. PrismaUI views are visible by default after CreateView, which would flash the page on screen before OnDomReady fires.
	service.Api()->Hide(view);
	return view;
}
