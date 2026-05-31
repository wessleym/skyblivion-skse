#pragma once

#include "PrismaUIService.h"

#include <nlohmann/json.hpp>
#include <format>
#include <string>

//A combination of PrismaUIService and a PrismaView
class PrismaViewHandle {
public:
	PrismaViewHandle() = default;
	PrismaViewHandle(const PrismaUIService* service, PrismaView view) :
		m_service(service), m_view(view) {
	}

	[[nodiscard]] bool IsValid() const { return m_service != nullptr && m_view != 0; }
	[[nodiscard]] PrismaView View() const { return m_view; }

	void Show() const { m_service->Api()->Show(m_view); }
	void Hide() const { m_service->Api()->Hide(m_view); }
	void Focus(bool pauseGame = false) const { m_service->Api()->Focus(m_view, pauseGame); }
	void Unfocus() const { m_service->Api()->Unfocus(m_view); }

	void UnfocusAndHide() const {
		Unfocus();
		Hide();
	}

	void RegisterListener(const char* functionName, PRISMA_UI_API::JSListenerCallback callback) const {
		m_service->Api()->RegisterJSListener(m_view, functionName, callback);
	}

	void Invoke(const std::string& script) const { m_service->Api()->Invoke(m_view, script.c_str()); }

	void InvokeByFunctionName(const char* jsFunc) const { Invoke(std::format("{}();", jsFunc)); }

	void InvokeByFunctionName(const char* jsFunc, const nlohmann::json& argument) const {
		Invoke(std::format("{}({});", jsFunc, argument.dump()));
	}

private:
	const PrismaUIService* m_service = nullptr;
	PrismaView m_view = 0;
};
