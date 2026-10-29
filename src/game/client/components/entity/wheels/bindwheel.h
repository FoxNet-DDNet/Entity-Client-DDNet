#ifndef GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_BINDWHEEL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_BINDWHEEL_H

#include "wheel.h"

#include <engine/console.h>
#include <engine/input.h>

#include <game/client/component.h>

class IConfigManager;

enum
{
	BINDWHEEL_MAX_NAME = 64,
	BINDWHEEL_MAX_CMD = 1024,
	BINDWHEEL_MAX_BINDS = 64
};

class CBindWheel : public CComponent
{
	static void ConOpenBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConAddBindwheelLegacy(IConsole::IResult *pResult, void *pUserData);
	static void ConAddBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllBindwheelBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConBindwheelExecuteHover(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CBindWheel();
	int Sizeof() const override { return sizeof(*this); }

	CWheel m_Wheel;

	void AddBind(const char *pName, const char *pCommand);
	void UpdateBind(int Index, const char *pName, const char *pCommand);

	void OnReset() override;
	void OnRender() override;
	void OnConsoleInit() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const { return m_Wheel.m_Active; }
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_BINDWHEEL_H
