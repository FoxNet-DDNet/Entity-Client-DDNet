#ifndef GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H
#include <game/client/component.h>

#include <engine/console.h>

#include <base/system.h>
#include <vector>

class IConfigManager;

enum
{
	QUICKACTIONS_MAX_NAME = 32,
	QUICKACTIONS_MAX_CMD = 1024,
	QUICKACTIONS_MAX_BINDS = 16
};

class CQuickActions : public CComponent
{
	float m_AnimationTime = 0.0f;
	float m_aAnimationTimeItems[QUICKACTIONS_MAX_BINDS] = {0};

	bool m_Active = false;
	bool m_WasActive = false;

	int m_QuickActionId;
	int m_LastQuickActionId;

	int m_SelectedBind;

	vec2 GetCursorWorldPos() const;
	int GetClosetClientId(vec2 Pos);

	static void ConOpenQuickActionMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConAddQuickAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveQuickAction(IConsole::IResult *pResult, void *pUserData);
	static void ConResetAllQuickActions(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllQuickActions(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	void DrawDebugLines();

public:
	class CBind
	{
	public:
		char m_aName[QUICKACTIONS_MAX_NAME] = "*";
		char m_aCommand[QUICKACTIONS_MAX_CMD] = "";

		bool operator==(const CBind &Other) const
		{
			return !str_comp(m_aName, Other.m_aName) && !str_comp(m_aCommand, Other.m_aCommand);
		}
	};

	std::vector<CBind> m_vBinds;

	CQuickActions();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnRender() override;
	void OnConsoleInit() override;
	void OnInit() override;
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

#endif // GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H