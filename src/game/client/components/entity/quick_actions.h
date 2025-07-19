#ifndef GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H
#include <game/client/component.h>
class IConfigManager;

enum
{
	QUICKACTIONS_MAX_NAME = 32,
	QUICKACTIONS_MAX_CMD = 1024,
	QUICKACTIONS_MAX_BINDS = 16
};

class CQuickActions : public CComponent
{
	void DrawCircle(float x, float y, float r, int Segments);

	bool m_WasActive;
	bool m_Active;

	int m_QuickActionId;

	vec2 m_SelectorMouse;
	int m_SelectedBind;

	vec2 GetCursorWorldPos() const;

	static void ConOpenQuickActionMenu(IConsole::IResult *pResult, void *pUserData);
	static void ConAddQuickAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveQuickAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllQuickActions(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	class CBind
	{
	public:
		char m_aName[QUICKACTIONS_MAX_NAME];
		char m_aCommand[QUICKACTIONS_MAX_CMD];

		bool operator==(const CBind &Other) const
		{
			return str_comp(m_aName, Other.m_aName) == 0 && str_comp(m_aCommand, Other.m_aCommand) == 0;
		}
	};

	std::vector<CBind> m_vBinds;

	CQuickActions();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;
	virtual void OnRelease() override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	int GetClosetClientId(vec2 Pos);

	void AddBind(const char *Name, const char *Command);
	void RemoveBind(const char *Name, const char *Command);
	void RemoveBind(int Index);
	void RemoveAllBinds();

	void AddDefaultBinds();

	void ExecuteBind(int Bind);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_QUICKACTIONS_H