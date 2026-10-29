#ifndef GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_PLAYER_ACTIONS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_PLAYER_ACTIONS_H
#include "wheel.h"

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/input.h>

#include <game/client/component.h>

class IConfigManager;

enum
{
	PLAYERACTIONS_MAX_NAME = 32,
	PLAYERACTIONS_MAX_CMD = 1024,
	PLAYERACTIONS_MAX_BINDS = 64
};

class CPlayerActions : public CComponent
{
	int m_PlayerActionId;

	int GetClosestClientId(vec2 Pos);

	static void ConOpenPlayerActionMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConAddPlayerAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemovePlayerAction(IConsole::IResult *pResult, void *pUserData);
	static void ConResetAllPlayerActions(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllPlayerActions(IConsole::IResult *pResult, void *pUserData);
	// Legacy alias for ec_reset_playeraction_mouse, deregistered after config load
	static void ConLegacyResetPlayerActionMouse(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void DrawDebugLines();

public:
	CPlayerActions();
	int Sizeof() const override { return sizeof(*this); }
	int PlayerActionId() const { return m_PlayerActionId; }

	CWheel m_Wheel;

	void OnReset() override;
	void OnRender() override;
	void OnConsoleInit() override;
	void OnInit() override;
	void OnRelease() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	void AddDefaultBinds();
	void AddBind(const char *pName, const char *pCommand);
	void UpdateBind(int Index, const char *pName, const char *pCommand);

	bool IsActive() const { return m_Wheel.m_Active; }
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_PLAYER_ACTIONS_H
