#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_MENU_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_MENU_H

#include <base/str.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <string>
#include <vector>

class IConfigManager;

// The moderation menu page, only reachable while authenticated to rcon. It runs an
// rcon command for the players selected from its online player list, either the
// command that is typed into its input or one of the user defined quick actions.
// Commands are templates that are run for every selected player, with "%d" replaced
// by their client id.
//
// This is a component so that the quick actions can register their console commands
// and be written to their config file. Only the page itself is rendered by CMenus.
class CMenusModeration : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnInit() override;
	void OnWindowResize() override;

	void Render(CUIRect MainView);

private:
	enum
	{
		QUICKACTION_MAX_NAME = 32,
		QUICKACTION_MAX_CMD = IConsole::CMDLINE_LENGTH,
		QUICKACTION_MAX_ACTIONS = 64
	};

	class CQuickAction
	{
	public:
		char m_aName[QUICKACTION_MAX_NAME] = "";
		char m_aCommand[QUICKACTION_MAX_CMD] = "";

		bool operator==(const CQuickAction &Other) const
		{
			return !str_comp(m_aName, Other.m_aName) && !str_comp(m_aCommand, Other.m_aCommand);
		}
	};

	// Online players, one entry per row of the list, addressed by client id
	class CPlayerEntry
	{
	public:
		int m_ItemId = 0;
		bool m_Selected = false;
		// The name and clan the player had when they were selected, see SelectPlayer
		char m_aSelectedName[MAX_NAME_LENGTH] = "";
		char m_aSelectedClan[MAX_CLAN_LENGTH] = "";
		CCachedText m_Score;
		CCachedText m_ScoreMillis;
	};

	enum EPlayerSortMode
	{
		SORT_NAME = 0,
		SORT_CLIENT_ID,
		SORT_CLAN,
		SORT_RANK,
		SORT_SKIN,
		NUM_SORT_MODES,
	};
	CPlayerEntry m_aPlayers[MAX_CLIENTS];
	int m_PlayerSortMode = SORT_NAME;
	int m_LastSelectedClientId = -1;
	CScrollRegion m_PlayerScrollRegion;
	CButtonContainer m_SortButton;
	CButtonContainer m_DeselectAllButton;
	void SelectPlayer(int ClientId);
	int CountSelectedPlayers() const;
	// The rcon lines that run the command template for all selected players, joining
	// as many commands as fit into a single line
	std::vector<std::string> BuildRconCommandChunks(const char *pCommandTemplate) const;

	// Command
	CLineInputBuffered<IConsole::CMDLINE_LENGTH> m_CommandInput;
	CButtonContainer m_ExecuteCommandButton;
	CButtonContainer m_CopyToRconButton;

	// Quick actions
	std::vector<CQuickAction> m_vQuickActions;
	bool m_QuickActionsEditMode = false;
	int m_SelectedAction = -1;
	int m_LoadedAction = -1;
	int m_DraggedAction = -1;
	bool m_Dragging = false;
	vec2 m_DragStartPos = vec2(0.0f, 0.0f);
	char m_aEditName[QUICKACTION_MAX_NAME] = "";
	char m_aEditCommand[QUICKACTION_MAX_CMD] = "";
	// Tooltips only keep the pointer to their text, so it must outlive the frame
	char m_aHoveredCommand[QUICKACTION_MAX_CMD] = "";
	char m_aCommandTooltip[IConsole::CMDLINE_LENGTH + IConsole::TEMPCMD_PARAMS_LENGTH + IConsole::TEMPCMD_HELP_LENGTH + 8] = "";
	CLineInput m_ActionNameInput;
	CLineInput m_ActionCommandInput;
	CScrollRegion m_ActionScrollRegion;
	CButtonContainer m_aActionButtons[QUICKACTION_MAX_ACTIONS];
	CButtonContainer m_EditModeButton;
	CButtonContainer m_AddActionButton;
	CButtonContainer m_DeleteActionButton;
	void RenderQuickActions(CUIRect View);

	int AddQuickAction(const char *pName, const char *pCommand);
	void RemoveQuickAction(const char *pName, const char *pCommand);
	void RemoveQuickAction(int Index);
	void RemoveAllQuickActions();
	void MoveQuickAction(int Index, int NewIndex);

	static void ConAddModAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveModAction(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllModActions(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_MOD_MENU_H
