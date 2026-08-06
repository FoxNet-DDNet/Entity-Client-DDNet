#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_QUICK_ACTIONS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_QUICK_ACTIONS_H

#include <base/system.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <vector>

class IConfigManager;

enum
{
	MODQUICKACTION_MAX_NAME = 32,
	MODQUICKACTION_MAX_CMD = IConsole::CMDLINE_LENGTH,
	MODQUICKACTION_MAX_ACTIONS = 64
};

// User defined rcon shortcuts shown in the moderation menu. Every action is a
// command template that is run for the players selected in that menu, the same
// way the command input of the menu works, so "%d" is replaced by the client id.
class CModQuickActions : public CComponent
{
	static void ConAddModAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveModAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllModActions(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	class CAction
	{
	public:
		char m_aName[MODQUICKACTION_MAX_NAME] = "";
		char m_aCommand[MODQUICKACTION_MAX_CMD] = "";

		bool operator==(const CAction &Other) const
		{
			return !str_comp(m_aName, Other.m_aName) && !str_comp(m_aCommand, Other.m_aCommand);
		}
	};

	std::vector<CAction> m_vActions;

	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;

	int AddAction(const char *pName, const char *pCommand);
	void RemoveAction(const char *pName, const char *pCommand);
	void RemoveAction(int Index);
	void RemoveAllActions();
	void MoveAction(int Index, int NewIndex);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_QUICK_ACTIONS_H
