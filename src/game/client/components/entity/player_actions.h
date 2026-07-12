#ifndef GAME_CLIENT_COMPONENTS_ENTITY_PLAYER_ACTIONS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_PLAYER_ACTIONS_H
#include <base/system.h>
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <vector>

class IConfigManager;

enum
{
	PLAYERACTIONS_MAX_NAME = 32,
	PLAYERACTIONS_MAX_CMD = 1024,
	PLAYERACTIONS_MAX_BINDS = 16
};

class CPlayerActions : public CComponent
{
	float m_AnimationTime = 0.0f;
	float m_aAnimationTimeItems[PLAYERACTIONS_MAX_BINDS] = {0};

	bool m_Active = false;
	bool m_WasActive = false;

	int m_PlayerActionId;

	int m_SelectedBind;

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
	class CBind
	{
	public:
		char m_aName[PLAYERACTIONS_MAX_NAME] = "*";
		char m_aCommand[PLAYERACTIONS_MAX_CMD] = "";

		bool operator==(const CBind &Other) const
		{
			return !str_comp(m_aName, Other.m_aName) && !str_comp(m_aCommand, Other.m_aCommand);
		}
	};

	std::vector<CBind> m_vBinds;

	CPlayerActions();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnConsoleInit() override;
	void OnInit() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRelease() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	bool IsActive() const { return m_Active; }

	void AddBind(const char *Name, const char *Command);
	void RemoveBind(const char *Name, const char *Command);
	void RemoveBind(int Index);
	void RemoveAllBinds();

	void AddDefaultBinds();

	void ExecuteBind(int Bind);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_PLAYER_ACTIONS_H
