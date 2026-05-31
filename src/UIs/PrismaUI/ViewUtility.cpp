#include "PrismaUIService.h"
#include "ViewUtility.h"

PrismaView ViewUtility::CreateHiddenView(const PrismaUIService& service, const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReady) {
	PrismaView view = service.Api()->CreateView(htmlPath, onDomReady);
	service.RegisterConsoleLogging(view);
	//Hide immediately. PrismaUI views are visible by default after CreateView, which would flash the page on screen before OnDomReady fires.
	service.Api()->Hide(view);
	return view;
}
