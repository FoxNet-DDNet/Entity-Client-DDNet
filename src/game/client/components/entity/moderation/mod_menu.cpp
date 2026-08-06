#include "mod_menu.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/console.h>
#include <game/client/components/menus.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

static void CollectPossibleCommandsCallback(int Index, const char *pCmd, void *pUser)
{
	static_cast<std::vector<const char *> *>(pUser)->push_back(pCmd);
}

static void SortCompletions(std::vector<const char *> &vCompletions, const char *pSearch)
{
	if(pSearch[0] == '\0')
		return;

	std::sort(vCompletions.begin(), vCompletions.end(), [pSearch](const char *pA, const char *pB) {
		const char *pMatchA = str_find_nocase(pA, pSearch);
		const char *pMatchB = str_find_nocase(pB, pSearch);
		const int MatchPosA = pMatchA ? pMatchA - pA : -1;
		const int MatchPosB = pMatchB ? pMatchB - pB : -1;

		if(MatchPosA != MatchPosB)
			return MatchPosA < MatchPosB;

		const int LenA = str_length(pA);
		const int LenB = str_length(pB);
		if(LenA != LenB)
			return LenA < LenB;

		return str_comp_nocase(pA, pB) < 0;
	});
}

static std::string_view GetCommandName(std::string_view CommandLine)
{
	const size_t Start = CommandLine.find_first_not_of(" \t");
	if(Start == std::string_view::npos)
		return {};

	const size_t End = CommandLine.find_first_of(" \t;", Start);
	if(End == std::string_view::npos)
		return CommandLine.substr(Start);
	return CommandLine.substr(Start, End - Start);
}

static std::string ReplaceClientIdPlaceholder(std::string_view Command, int ClientId)
{
	char aClientId[16];
	str_format(aClientId, sizeof(aClientId), "%d", ClientId);

	std::string Result;
	Result.reserve(Command.size() + sizeof(aClientId));

	size_t Start = 0;
	while(Start < Command.size())
	{
		const size_t PlaceholderPos = Command.find("%d", Start);
		if(PlaceholderPos == std::string_view::npos)
		{
			Result.append(Command.substr(Start));
			break;
		}

		Result.append(Command.substr(Start, PlaceholderPos - Start));
		Result.append(aClientId);
		Start = PlaceholderPos + 2;
	}

	return Result;
}

static bool CommandTargetsPlayers(const char *pCommandTemplate)
{
	return str_find(pCommandTemplate, "%d") != nullptr;
}

static char NextCommandParam(const char *&pFormat)
{
	if(*pFormat)
	{
		pFormat++;

		if(*pFormat == '[')
		{
			for(; *pFormat != ']'; pFormat++)
			{
				if(!*pFormat)
					return *pFormat;
			}

			pFormat++;
			if(*pFormat == ' ')
				pFormat++;
		}
	}
	return *pFormat;
}

static bool IsIntegerArgument(std::string_view Argument)
{
	if(Argument == "%d")
		return true;

	char aBuf[IConsole::CMDLINE_LENGTH];
	str_copy(aBuf, std::string(Argument).c_str(), sizeof(aBuf));
	int Value;
	return str_toint(aBuf, &Value) && Value != std::numeric_limits<int>::max() && Value != std::numeric_limits<int>::min();
}

static bool IsFloatArgument(std::string_view Argument)
{
	if(Argument == "%d")
		return true;

	char aBuf[IConsole::CMDLINE_LENGTH];
	str_copy(aBuf, std::string(Argument).c_str(), sizeof(aBuf));
	float Value;
	return str_tofloat(aBuf, &Value) && Value != std::numeric_limits<float>::max() && Value != std::numeric_limits<float>::min();
}

static std::vector<std::string_view> TokenizeCommandArguments(std::string_view CommandLine)
{
	std::vector<std::string_view> vTokens;
	const size_t CommandStart = CommandLine.find_first_not_of(" \t");
	if(CommandStart == std::string_view::npos)
		return vTokens;

	size_t Pos = CommandLine.find_first_of(" \t", CommandStart);
	if(Pos == std::string_view::npos)
		return vTokens;

	while(Pos < CommandLine.size())
	{
		Pos = CommandLine.find_first_not_of(" \t", Pos);
		if(Pos == std::string_view::npos)
			break;

		const size_t Start = Pos;
		if(CommandLine[Pos] == '"')
		{
			++Pos;
			bool Escaping = false;
			while(Pos < CommandLine.size())
			{
				if(Escaping)
					Escaping = false;
				else if(CommandLine[Pos] == '\\')
					Escaping = true;
				else if(CommandLine[Pos] == '"')
				{
					++Pos;
					break;
				}
				++Pos;
			}
			vTokens.push_back(CommandLine.substr(Start, Pos - Start));
		}
		else
		{
			Pos = CommandLine.find_first_of(" \t", Pos);
			if(Pos == std::string_view::npos)
			{
				vTokens.push_back(CommandLine.substr(Start));
				break;
			}
			vTokens.push_back(CommandLine.substr(Start, Pos - Start));
		}
	}

	return vTokens;
}

static bool ValidateCommandSyntax(std::string_view CommandLine, const IConsole::ICommandInfo *pCommandInfo)
{
	if(pCommandInfo == nullptr)
		return false;

	const std::vector<std::string_view> vArguments = TokenizeCommandArguments(CommandLine);
	const char *pFormat = pCommandInfo->Params();
	size_t ArgumentIndex = 0;
	bool Optional = false;

	for(char Param = *pFormat; Param != '\0'; Param = NextCommandParam(pFormat))
	{
		if(Param == '?')
		{
			Optional = true;
			continue;
		}

		if(ArgumentIndex >= vArguments.size())
			return Optional;

		const std::string_view Argument = vArguments[ArgumentIndex];
		switch(Param)
		{
		case 'i':
			if(!IsIntegerArgument(Argument))
				return false;
			break;
		case 'f':
			if(!IsFloatArgument(Argument))
				return false;
			break;
		case 'c':
			if(Argument.empty())
				return false;
			break;
		case 'r':
			return true;
		case 'v':
		case 's':
		default:
			break;
		}

		++ArgumentIndex;
		Optional = false;
	}

	return ArgumentIndex == vArguments.size();
}

static const IConsole::ICommandInfo *FindDisplayedCommandInfo(IConsole *pConsole, IClient *pClient, std::string_view CommandLine, bool &ExactMatch, std::string &CommandName)
{
	ExactMatch = false;
	CommandName.clear();

	const std::string_view CommandToken = GetCommandName(CommandLine);
	if(CommandToken.empty())
		return nullptr;

	const bool UseTempCommands = pClient->RconAuthed() && pClient->UseTempRconCommands();
	const int FlagMask = CFGFLAG_SERVER;

	char aCommand[IConsole::CMDLINE_LENGTH];
	str_copy(aCommand, std::string(CommandToken).c_str(), sizeof(aCommand));

	if(const IConsole::ICommandInfo *pInfo = pConsole->GetCommandInfo(aCommand, FlagMask, UseTempCommands))
	{
		ExactMatch = true;
		CommandName = pInfo->Name();
		return pInfo;
	}

	std::vector<const char *> vSuggestions;
	pConsole->PossibleCommands(aCommand, FlagMask, UseTempCommands, CollectPossibleCommandsCallback, &vSuggestions);
	if(vSuggestions.empty())
		return nullptr;
	SortCompletions(vSuggestions, aCommand);

	CommandName = vSuggestions.front();
	return pConsole->GetCommandInfo(CommandName.c_str(), FlagMask, UseTempCommands);
}

static void AutocompleteCommandInput(CLineInput *pInput, std::string_view SuggestedCommand, const IConsole::ICommandInfo *pCommandInfo)
{
	if(pInput == nullptr || SuggestedCommand.empty())
		return;

	const std::string_view CurrentInput = pInput->GetString();
	const size_t Start = CurrentInput.find_first_not_of(" \t");
	const size_t End = Start == std::string_view::npos ? std::string_view::npos : CurrentInput.find_first_of(" \t;", Start);

	std::string NewInput;
	NewInput.reserve(CurrentInput.size() + SuggestedCommand.size() + 1);

	if(Start != std::string_view::npos)
		NewInput.append(CurrentInput.substr(0, Start));
	NewInput.append(SuggestedCommand);

	if(End != std::string_view::npos)
		NewInput.append(CurrentInput.substr(End));
	else if(pCommandInfo != nullptr && pCommandInfo->Params()[0] != '\0')
		NewInput.push_back(' ');

	pInput->Set(NewInput.c_str());
	pInput->SetCursorOffset(NewInput.size());
	pInput->SelectNothing();
}

void CMenusModeration::ConAddModAction(IConsole::IResult *pResult, void *pUserData)
{
	CMenusModeration *pThis = static_cast<CMenusModeration *>(pUserData);
	pThis->AddQuickAction(pResult->GetString(0), pResult->GetString(1));
}

void CMenusModeration::ConRemoveModAction(IConsole::IResult *pResult, void *pUserData)
{
	CMenusModeration *pThis = static_cast<CMenusModeration *>(pUserData);
	pThis->RemoveQuickAction(pResult->GetString(0), pResult->GetString(1));
}

void CMenusModeration::ConRemoveAllModActions(IConsole::IResult *pResult, void *pUserData)
{
	CMenusModeration *pThis = static_cast<CMenusModeration *>(pUserData);
	pThis->RemoveAllQuickActions();
}

void CMenusModeration::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYMODACTIONS);

	Console()->Register("add_mod_action", "s[name] r[command]", CFGFLAG_CLIENT, ConAddModAction, this, "Add a quick action to the moderation menu");
	Console()->Register("remove_mod_action", "s[name] r[command]", CFGFLAG_CLIENT, ConRemoveModAction, this, "Remove a quick action from the moderation menu");
	Console()->Register("delete_all_mod_actions", "", CFGFLAG_CLIENT, ConRemoveAllModActions, this, "Removes all moderation menu quick actions");
}

void CMenusModeration::OnInit()
{
	m_CommandInput.Set("ban %d 20160 Bot Client.");
	m_ActionNameInput.SetBuffer(m_aEditName, sizeof(m_aEditName));
	m_ActionCommandInput.SetBuffer(m_aEditCommand, sizeof(m_aEditCommand));
}

void CMenusModeration::OnWindowResize()
{
	for(CPlayerEntry &Player : m_aPlayers)
	{
		Player.m_Score.Reset(TextRender());
		Player.m_ScoreMillis.Reset(TextRender());
	}
}

int CMenusModeration::AddQuickAction(const char *pName, const char *pCommand)
{
	if(m_vQuickActions.size() >= QUICKACTION_MAX_ACTIONS)
		return -1;

	CQuickAction Action;
	str_copy(Action.m_aName, pName);
	str_copy(Action.m_aCommand, pCommand);
	m_vQuickActions.push_back(Action);
	return static_cast<int>(m_vQuickActions.size()) - 1;
}

void CMenusModeration::RemoveQuickAction(const char *pName, const char *pCommand)
{
	CQuickAction Action;
	str_copy(Action.m_aName, pName);
	str_copy(Action.m_aCommand, pCommand);
	auto It = std::find(m_vQuickActions.begin(), m_vQuickActions.end(), Action);
	if(It != m_vQuickActions.end())
		m_vQuickActions.erase(It);
}

void CMenusModeration::RemoveQuickAction(int Index)
{
	if(Index < 0 || Index >= static_cast<int>(m_vQuickActions.size()))
		return;
	m_vQuickActions.erase(m_vQuickActions.begin() + Index);
}

void CMenusModeration::RemoveAllQuickActions()
{
	m_vQuickActions.clear();
}

void CMenusModeration::MoveQuickAction(int Index, int NewIndex)
{
	const int NumActions = static_cast<int>(m_vQuickActions.size());
	if(Index < 0 || Index >= NumActions || NewIndex < 0 || NewIndex >= NumActions || Index == NewIndex)
		return;

	const CQuickAction Action = m_vQuickActions[Index];
	m_vQuickActions.erase(m_vQuickActions.begin() + Index);
	m_vQuickActions.insert(m_vQuickActions.begin() + NewIndex, Action);
}

void CMenusModeration::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CMenusModeration *pThis = static_cast<CMenusModeration *>(pUserData);

	for(const CQuickAction &Action : pThis->m_vQuickActions)
	{
		char aBuf[(QUICKACTION_MAX_NAME + QUICKACTION_MAX_CMD) * 2 + 32] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_mod_action \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Action.m_aName, pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Action.m_aCommand, pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYMODACTIONS);
	}
}

// Remember the identity of a selected player so the selection can be dropped once
// the client id is recycled. Client ids are reused when a player leaves and another
// joins, and this menu is only rendered while the moderation page is open, so we
// cannot rely on observing an empty slot: otherwise a stale selection would silently
// move onto whoever reuses the id and a command could be executed on an innocent
// player.
void CMenusModeration::SelectPlayer(int ClientId)
{
	m_aPlayers[ClientId].m_Selected = true;
	str_copy(m_aPlayers[ClientId].m_aSelectedName, GameClient()->m_aClients[ClientId].m_aName);
	str_copy(m_aPlayers[ClientId].m_aSelectedClan, GameClient()->m_aClients[ClientId].m_aClan);
}

std::vector<std::string> CMenusModeration::BuildRconCommandChunks(const char *pCommandTemplate) const
{
	std::vector<std::string> vChunks;
	if(pCommandTemplate == nullptr || pCommandTemplate[0] == '\0')
		return vChunks;

	constexpr size_t MaxCommandLength = IConsole::CMDLINE_LENGTH - 1;
	std::string CurrentChunk;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!m_aPlayers[ClientId].m_Selected)
			continue;

		std::string Command = ReplaceClientIdPlaceholder(pCommandTemplate, ClientId);
		if(Command.empty())
			continue;

		if(Command.size() > MaxCommandLength)
			continue;

		const size_t AddedLength = CurrentChunk.empty() ? Command.size() : CurrentChunk.size() + 1 + Command.size();
		if(!CurrentChunk.empty() && AddedLength > MaxCommandLength)
		{
			vChunks.push_back(CurrentChunk);
			CurrentChunk.clear();
		}

		if(!CurrentChunk.empty())
			CurrentChunk.append(";");
		CurrentChunk.append(Command);
	}

	if(!CurrentChunk.empty())
		vChunks.push_back(CurrentChunk);

	return vChunks;
}

int CMenusModeration::CountSelectedPlayers() const
{
	int SelectedCount = 0;
	for(const CPlayerEntry &Player : m_aPlayers)
	{
		if(Player.m_Selected)
			++SelectedCount;
	}
	return SelectedCount;
}

void CMenusModeration::Render(CUIRect MainView)
{
	MainView.Draw(CMenus::ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.Margin(10.0f, &MainView);

	CUIRect LeftView, RightView, Header, Label, Button, PlayerList;
	const float RightWidth = std::min(360.0f, MainView.w * 0.4f);
	MainView.VSplitRight(RightWidth, &LeftView, &RightView);
	LeftView.VSplitRight(10.0f, &LeftView, nullptr);

	RightView.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_ALL, 10.0f);
	RightView.Margin(10.0f, &RightView);

	RightView.HSplitTop(24.0f, &Header, &RightView);
	Header.VSplitRight(140.0f, &Label, &Button);
	Ui()->DoLabel(&Label, EcLocalize("Online Players"), 18.0f, TEXTALIGN_ML);
	RightView.HSplitTop(10.0f, nullptr, &RightView);

	const char *pSortLabel = "";
	switch(m_PlayerSortMode)
	{
	case SORT_CLIENT_ID:
		pSortLabel = EcLocalize("Client Id");
		break;
	case SORT_CLAN:
		pSortLabel = EcLocalize("Clan");
		break;
	case SORT_RANK:
		pSortLabel = EcLocalize("Rank");
		break;
	case SORT_SKIN:
		pSortLabel = EcLocalize("Skin Name");
		break;
	case SORT_NAME:
	default:
		pSortLabel = EcLocalize("Name");
		break;
	}

	char aSortButton[64];
	str_format(aSortButton, sizeof(aSortButton), "%s: %s", EcLocalize("Sort"), pSortLabel);
	if(GameClient()->m_Menus.DoButton_Menu(&m_SortButton, aSortButton, 0, &Button))
		m_PlayerSortMode = (m_PlayerSortMode + 1) % NUM_SORT_MODES;

	const bool Race7 = Client()->IsSixup() && GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE);
	const bool MillisecondScore = GameClient()->m_ReceivedDDNetPlayerFinishTimes;
	const bool TimeScore = GameClient()->m_GameInfo.m_TimeScore;

	CUIRect Footer;
	RightView.HSplitBottom(24.0f, &PlayerList, &Footer);
	Footer.HSplitTop(4.0f, nullptr, &Footer);

	std::vector<int> vOnlinePlayers;
	vOnlinePlayers.reserve(MAX_CLIENTS);
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameClient()->m_Snap.m_apPlayerInfos[i])
			vOnlinePlayers.push_back(i);
	}

	auto NameCompare = [&](int a, int b) {
		const int NameComp = str_comp_nocase(GameClient()->m_aClients[a].m_aName, GameClient()->m_aClients[b].m_aName);
		if(NameComp != 0)
			return NameComp < 0;
		return a < b;
	};

	auto HasRankTime = [&](int ClientId) {
		if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
			return GameClient()->m_aClients[ClientId].m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS && GameClient()->m_aClients[ClientId].m_FinishTimeSeconds != FinishTime::UNSET;

		const CNetObj_PlayerInfo *pPlayerInfo = GameClient()->m_Snap.m_apPlayerInfos[ClientId];
		return pPlayerInfo != nullptr && GameClient()->m_GameInfo.m_TimeScore && pPlayerInfo->m_Score != FinishTime::NOT_FINISHED_TIMESCORE && pPlayerInfo->m_Score != FinishTime::UNSET;
	};

	std::sort(vOnlinePlayers.begin(), vOnlinePlayers.end(), [&](int a, int b) {
		switch(m_PlayerSortMode)
		{
		case SORT_CLIENT_ID:
			return a < b;
		case SORT_CLAN:
		{
			const bool ClanlessA = GameClient()->m_aClients[a].m_aClan[0] == '\0';
			const bool ClanlessB = GameClient()->m_aClients[b].m_aClan[0] == '\0';
			if(ClanlessA != ClanlessB)
				return !ClanlessA;

			const int ClanComp = str_comp_nocase(GameClient()->m_aClients[a].m_aClan, GameClient()->m_aClients[b].m_aClan);
			if(ClanComp != 0)
				return ClanComp < 0;
			return NameCompare(a, b);
		}
		case SORT_RANK:
		{
			const bool HasRankA = HasRankTime(a);
			const bool HasRankB = HasRankTime(b);
			if(HasRankA != HasRankB)
				return HasRankA;
			if(!HasRankA)
				return NameCompare(a, b);

			if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
			{
				if(GameClient()->m_aClients[a].m_FinishTimeSeconds != GameClient()->m_aClients[b].m_FinishTimeSeconds)
					return GameClient()->m_aClients[a].m_FinishTimeSeconds < GameClient()->m_aClients[b].m_FinishTimeSeconds;
				if(GameClient()->m_aClients[a].m_FinishTimeMillis != GameClient()->m_aClients[b].m_FinishTimeMillis)
					return GameClient()->m_aClients[a].m_FinishTimeMillis < GameClient()->m_aClients[b].m_FinishTimeMillis;
			}
			else
			{
				const int ScoreA = GameClient()->m_Snap.m_apPlayerInfos[a]->m_Score;
				const int ScoreB = GameClient()->m_Snap.m_apPlayerInfos[b]->m_Score;
				if(ScoreA != ScoreB)
					return ScoreA > ScoreB;
			}

			return NameCompare(a, b);
		}
		case SORT_SKIN:
		{
			const int SkinComp = str_comp_nocase(GameClient()->m_aClients[a].m_aSkinName, GameClient()->m_aClients[b].m_aSkinName);
			if(SkinComp != 0)
				return SkinComp < 0;
			return NameCompare(a, b);
		}
		case SORT_NAME:
		default:
			return NameCompare(a, b);
		}
	});

	// Drop the selection of players that left or changed identity, see SelectPlayer
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_aPlayers[i].m_Selected)
			continue;
		if(!GameClient()->m_Snap.m_apPlayerInfos[i] ||
			str_comp(GameClient()->m_aClients[i].m_aName, m_aPlayers[i].m_aSelectedName) != 0 ||
			str_comp(GameClient()->m_aClients[i].m_aClan, m_aPlayers[i].m_aSelectedClan) != 0)
		{
			m_aPlayers[i].m_Selected = false;
		}
	}
	if(m_LastSelectedClientId >= 0 && !GameClient()->m_Snap.m_apPlayerInfos[m_LastSelectedClientId])
		m_LastSelectedClientId = -1;

	const int SelectedCountBeforeActions = CountSelectedPlayers();
	const bool HasSelectedPlayers = SelectedCountBeforeActions > 0;
	const bool HasCommandTemplate = m_CommandInput.GetString()[0] != '\0';
	bool ExactCommandMatch = false;
	std::string DisplayedCommandName;
	const IConsole::ICommandInfo *pDisplayedCommandInfo = FindDisplayedCommandInfo(Console(), Client(), m_CommandInput.GetString(), ExactCommandMatch, DisplayedCommandName);
	const bool CommandSyntaxValid = HasCommandTemplate && ExactCommandMatch && ValidateCommandSyntax(m_CommandInput.GetString(), pDisplayedCommandInfo);
	const std::vector<std::string> vCommandChunks = BuildRconCommandChunks(m_CommandInput.GetString());

	LeftView.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_ALL, 10.0f);
	LeftView.Margin(10.0f, &LeftView);

	CUIRect CommandLabel, CommandInput, CommandMeta, CommandHelp, ButtonsRow, ExecuteButton, CopyButton;
	LeftView.HSplitTop(24.0f, &CommandLabel, &LeftView);
	Ui()->DoLabel(&CommandLabel, EcLocalize("Command"), 18.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(10.0f, nullptr, &LeftView);
	LeftView.HSplitTop(24.0f, &CommandInput, &LeftView);
	Ui()->DoClearableEditBox(&m_CommandInput, &CommandInput, 12.0f);
	if(CLineInput::GetActiveInput() == &m_CommandInput && Input()->KeyPress(KEY_TAB) && pDisplayedCommandInfo != nullptr)
		AutocompleteCommandInput(&m_CommandInput, DisplayedCommandName, pDisplayedCommandInfo);

	LeftView.HSplitTop(16.0f, &CommandMeta, &LeftView);
	if(pDisplayedCommandInfo)
	{
		char aCommandMeta[IConsole::CMDLINE_LENGTH + IConsole::TEMPCMD_PARAMS_LENGTH + 8];
		if(pDisplayedCommandInfo->Params()[0] != '\0')
			str_format(aCommandMeta, sizeof(aCommandMeta), "%s %s", DisplayedCommandName.c_str(), pDisplayedCommandInfo->Params());
		else
			str_copy(aCommandMeta, DisplayedCommandName.c_str(), sizeof(aCommandMeta));

		TextRender()->TextColor(ExactCommandMatch ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.55f) : ColorRGBA(1.0f, 0.85f, 0.45f, 0.75f));
		Ui()->DoLabel(&CommandMeta, aCommandMeta, 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else if(HasCommandTemplate)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.4f, 0.4f, 0.9f));
		Ui()->DoLabel(&CommandMeta, EcLocalize("Unknown rcon command"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	LeftView.HSplitTop(16.0f, &CommandHelp, &LeftView);
	if(!CommandSyntaxValid && HasCommandTemplate)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.4f, 0.4f, 0.9f));
		Ui()->DoLabel(&CommandHelp, EcLocalize("Invalid command syntax"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else if(pDisplayedCommandInfo && pDisplayedCommandInfo->Help()[0] != '\0')
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.45f));
		Ui()->DoLabel(&CommandHelp, pDisplayedCommandInfo->Help(), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	LeftView.HSplitTop(28.0f, &ButtonsRow, &LeftView);
	ButtonsRow.VSplitMid(&ExecuteButton, &CopyButton, 5.0f);

	const bool ExecuteDisabled = !Client()->RconAuthed() || !HasSelectedPlayers || !HasCommandTemplate || !CommandSyntaxValid || vCommandChunks.empty();
	if(GameClient()->m_Menus.DoButtonForceFontSize_Menu(&m_ExecuteCommandButton, EcLocalize("Execute Command"), 0, &ExecuteButton, 11.0f, ExecuteDisabled))
	{
		for(const std::string &Chunk : vCommandChunks)
			Client()->Rcon(Chunk.c_str());
	}

	const bool CopyDisabled = !HasSelectedPlayers || !HasCommandTemplate || vCommandChunks.empty();
	if(GameClient()->m_Menus.DoButtonForceFontSize_Menu(&m_CopyToRconButton, EcLocalize("Copy to Rcon"), 0, &CopyButton, 11.0f, CopyDisabled))
	{
		GameClient()->m_GameConsole.SetRemoteConsoleInput(vCommandChunks.front().c_str());
	}

	LeftView.HSplitTop(12.0f, nullptr, &LeftView);
	RenderQuickActions(LeftView);

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 90.0f;
	m_PlayerScrollRegion.Begin(&PlayerList, &ScrollParams);

	for(int Index = 0; Index < (int)vOnlinePlayers.size(); ++Index)
	{
		const int ClientId = vOnlinePlayers[Index];
		CPlayerEntry &Player = m_aPlayers[ClientId];

		CUIRect Row;
		PlayerList.HSplitTop(30.0f, &Row, &PlayerList);
		const bool Visible = m_PlayerScrollRegion.AddRect(Row);
		if(!Visible)
			continue;

		const int ClickResult = Ui()->DoButtonLogic(&Player.m_ItemId, 0, &Row, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
		if(ClickResult == 1)
		{
			if(Input()->ShiftIsPressed() && m_LastSelectedClientId != -1)
			{
				auto AnchorIt = std::find(vOnlinePlayers.begin(), vOnlinePlayers.end(), m_LastSelectedClientId);
				if(AnchorIt != vOnlinePlayers.end())
				{
					const int AnchorIndex = AnchorIt - vOnlinePlayers.begin();
					const int Start = std::min(AnchorIndex, Index);
					const int End = std::max(AnchorIndex, Index);
					for(int RangeIndex = Start; RangeIndex <= End; ++RangeIndex)
						SelectPlayer(vOnlinePlayers[RangeIndex]);
				}
				else
				{
					SelectPlayer(ClientId);
				}
			}
			else
			{
				SelectPlayer(ClientId);
			}

			m_LastSelectedClientId = ClientId;
		}
		else if(ClickResult == 2)
		{
			if(Input()->ShiftIsPressed() && m_LastSelectedClientId != -1)
			{
				auto AnchorIt = std::find(vOnlinePlayers.begin(), vOnlinePlayers.end(), m_LastSelectedClientId);
				if(AnchorIt != vOnlinePlayers.end())
				{
					const int AnchorIndex = AnchorIt - vOnlinePlayers.begin();
					const int Start = std::min(AnchorIndex, Index);
					const int End = std::max(AnchorIndex, Index);
					for(int RangeIndex = Start; RangeIndex <= End; ++RangeIndex)
						m_aPlayers[vOnlinePlayers[RangeIndex]].m_Selected = false;
				}
				else
				{
					Player.m_Selected = false;
				}
			}
			else
			{
				Player.m_Selected = false;
			}

			m_LastSelectedClientId = ClientId;
		}

		const bool Selected = Player.m_Selected;
		const bool Hovered = Ui()->HotItem() == &Player.m_ItemId;
		ColorRGBA RowColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f);
		if(Selected)
			RowColor = Hovered ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.22f);
		else if(Hovered)
			RowColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f);
		Row.Draw(RowColor, IGraphics::CORNER_ALL, 5.0f);

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;
		TeeInfo.m_Size = 24.0f;

		CUIRect RowInner, TeeRect, TextRect, InfoRect, SideRect, IdRect, RankRect, NameRect, ClanRect;
		Row.Margin(2.0f, &RowInner);
		RowInner.VSplitLeft(28.0f, &TeeRect, &TextRect);
		TextRect.VSplitRight(120.0f, &InfoRect, &SideRect);
		SideRect.VSplitLeft(38.0f, &IdRect, &RankRect);
		InfoRect.HSplitTop(InfoRect.h / 2.0f, &NameRect, &ClanRect);

		Ui()->DoLabel(&NameRect, GameClient()->m_aClients[ClientId].m_aName, 14.0f, TEXTALIGN_ML);
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
		Ui()->DoLabel(&ClanRect, GameClient()->m_aClients[ClientId].m_aClan, 11.0f, TEXTALIGN_ML);
		char aClientIdBuf[16];
		str_format(aClientIdBuf, sizeof(aClientIdBuf), "%d", ClientId);
		Ui()->DoLabel(&IdRect, aClientIdBuf, 10.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		RankRect.HMargin(2.0f, &RankRect);
		if(Race7)
		{
			const int Score = GameClient()->m_Snap.m_apPlayerInfos[ClientId]->m_Score;
			Ui()->RenderTime(RankRect, 10.0f, Score / 1000, Score == protocol7::FinishTime::NOT_FINISHED, Score % 1000, true,
				Player.m_Score, Player.m_ScoreMillis, TextRender()->DefaultTextColor());
		}
		else if(MillisecondScore)
		{
			Ui()->RenderTime(RankRect, 10.0f, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS, GameClient()->m_aClients[ClientId].m_FinishTimeMillis, true,
				Player.m_Score, Player.m_ScoreMillis, TextRender()->DefaultTextColor());
		}
		else if(TimeScore)
		{
			const int Score = GameClient()->m_Snap.m_apPlayerInfos[ClientId]->m_Score;
			Ui()->RenderTime(RankRect, 10.0f, Score, Score == FinishTime::NOT_FINISHED_TIMESCORE, -1, false,
				Player.m_Score, Player.m_ScoreMillis, TextRender()->DefaultTextColor());
		}
		else
		{
			TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f));
			Ui()->DoLabel(&RankRect, "-", 10.0f, TEXTALIGN_MR);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		const vec2 TeeEyeDir = GameClient()->m_Menus.TeeEyeDirection(TeeRect.Center());
		const bool Paused = GameClient()->m_aClients[ClientId].m_Paused || GameClient()->m_aClients[ClientId].m_Spec;
		CAnimState AnimState;
		AnimState.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		if(Paused)
			AnimState.Add(&g_pData->m_aAnimations[TeeEyeDir.x < 0 ? ANIM_SIT_LEFT : ANIM_SIT_RIGHT], 0.0f, 1.0f);
		else
			AnimState.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);

		GameClient()->m_Menus.RenderTee(TeeRect.Center() + vec2(-1.0f, 2.5f), TeeEyeDir, &AnimState, &TeeInfo, Paused ? EMOTE_BLINK : EMOTE_NORMAL);
	}

	m_PlayerScrollRegion.End();

	const int SelectedCount = CountSelectedPlayers();

	CUIRect SelectedLabel, DeselectButton;
	Footer.VSplitRight(110.0f, &SelectedLabel, &DeselectButton);
	char aSelectedBuf[64];
	str_format(aSelectedBuf, sizeof(aSelectedBuf), "%d %s", SelectedCount, EcLocalize("selected"));
	Ui()->DoLabel(&SelectedLabel, aSelectedBuf, 12.0f, TEXTALIGN_ML);

	if(GameClient()->m_Menus.DoButton_Menu(&m_DeselectAllButton, EcLocalize("Deselect All"), 0, &DeselectButton))
	{
		for(CPlayerEntry &Player : m_aPlayers)
			Player.m_Selected = false;
		m_LastSelectedClientId = -1;
	}
}

// The quick action section, rendered below the command input of the menu. A right
// click on an action fills that input with the command of the action.
void CMenusModeration::RenderQuickActions(CUIRect View)
{
	// Set every frame, the localized strings are replaced on language change
	m_ActionNameInput.SetEmptyText(EcLocalize("Button name"));
	m_ActionCommandInput.SetEmptyText(EcLocalize("Command, %d is replaced by the client id"));

	if(m_SelectedAction >= (int)m_vQuickActions.size())
		m_SelectedAction = -1;

	CUIRect Header, Hint, EditButton;
	View.HSplitTop(24.0f, &Header, &View);
	View.HSplitTop(2.0f, nullptr, &View);
	View.HSplitTop(14.0f, &Hint, &View);
	View.HSplitTop(6.0f, nullptr, &View);

	Header.VSplitRight(70.0f, &Header, &EditButton);
	Header.VSplitRight(5.0f, &Header, nullptr);
	Ui()->DoLabel(&Header, EcLocalize("Quick Actions"), 18.0f, TEXTALIGN_ML);

	// The moderation page keeps rendering when the rcon authentication is lost, so
	// the quick actions are gated here as well and not only by the menu tab.
	if(!Client()->RconAuthed())
	{
		m_QuickActionsEditMode = false;
		m_SelectedAction = -1;
		m_DraggedAction = -1;
		m_Dragging = false;
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.45f));
		Ui()->DoLabel(&Hint, EcLocalize("Quick actions require rcon authentication"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	if(GameClient()->m_Menus.DoButton_Menu(&m_EditModeButton, m_QuickActionsEditMode ? EcLocalize("Done") : EcLocalize("Edit"), m_QuickActionsEditMode, &EditButton))
	{
		m_QuickActionsEditMode = !m_QuickActionsEditMode;
		m_SelectedAction = -1;
		m_DraggedAction = -1;
		m_Dragging = false;
	}

	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.45f));
	Ui()->DoLabel(&Hint, m_QuickActionsEditMode ? EcLocalize("Click to edit an action, drag it to move it somewhere else") : EcLocalize("Left click runs the action on the selection, right click copies it above"), 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// Load the selected action into the edit buffers before anything can change the
	// selection again, so that editing never writes into the previously selected one.
	if(m_SelectedAction != m_LoadedAction)
	{
		if(m_SelectedAction >= 0)
		{
			m_ActionNameInput.Set(m_vQuickActions[m_SelectedAction].m_aName);
			m_ActionCommandInput.Set(m_vQuickActions[m_SelectedAction].m_aCommand);
		}
		else
		{
			m_ActionNameInput.Clear();
			m_ActionCommandInput.Clear();
		}
		m_LoadedAction = m_SelectedAction;
	}

	if(m_QuickActionsEditMode)
	{
		CUIRect Editor, Row, Label, Input, AddButton, DeleteButton;
		View.HSplitBottom(68.0f, &View, &Editor);
		View.HSplitBottom(8.0f, &View, nullptr);

		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitLeft(70.0f, &Label, &Input);
		Ui()->DoLabel(&Label, EcLocalize("Name"), 12.0f, TEXTALIGN_ML);
		Ui()->DoEditBox(&m_ActionNameInput, &Input, 11.0f);

		// The syntax check is only a hint, a command is never rejected: servers can
		// have commands that this client does not know about.
		bool ExactCommandMatch = false;
		std::string DisplayedCommandName;
		const IConsole::ICommandInfo *pCommandInfo = FindDisplayedCommandInfo(Console(), Client(), m_aEditCommand, ExactCommandMatch, DisplayedCommandName);
		const bool CommandSyntaxValid = m_aEditCommand[0] == '\0' || (ExactCommandMatch && ValidateCommandSyntax(m_aEditCommand, pCommandInfo));

		Editor.HSplitTop(4.0f, nullptr, &Editor);
		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitLeft(70.0f, &Label, &Input);
		Ui()->DoLabel(&Label, EcLocalize("Command"), 12.0f, TEXTALIGN_ML);
		std::vector<STextColorSplit> vCommandColorSplits;
		if(!CommandSyntaxValid)
			vCommandColorSplits.emplace_back(0, -1, ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoEditBox(&m_ActionCommandInput, &Input, 11.0f, IGraphics::CORNER_ALL, vCommandColorSplits);

		if(m_aEditCommand[0] != '\0')
		{
			if(pCommandInfo == nullptr)
			{
				str_copy(m_aCommandTooltip, EcLocalize("Unknown rcon command"));
			}
			else
			{
				if(pCommandInfo->Params()[0] != '\0')
					str_format(m_aCommandTooltip, sizeof(m_aCommandTooltip), "%s %s", DisplayedCommandName.c_str(), pCommandInfo->Params());
				else
					str_copy(m_aCommandTooltip, DisplayedCommandName.c_str());
				if(pCommandInfo->Help()[0] != '\0')
				{
					str_append(m_aCommandTooltip, "\n");
					str_append(m_aCommandTooltip, pCommandInfo->Help());
				}
			}
			GameClient()->m_Tooltips.DoToolTip(&m_ActionCommandInput, &Input, m_aCommandTooltip, 200.0f);
		}

		if(m_SelectedAction >= 0)
		{
			str_copy(m_vQuickActions[m_SelectedAction].m_aName, m_aEditName);
			str_copy(m_vQuickActions[m_SelectedAction].m_aCommand, m_aEditCommand);
		}

		Editor.HSplitTop(4.0f, nullptr, &Editor);
		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitMid(&AddButton, &DeleteButton, 5.0f);

		// Adding takes over whatever is in the input fields, so an action can be
		// written out first and then added without losing what was typed.
		const bool AddDisabled = (int)m_vQuickActions.size() >= QUICKACTION_MAX_ACTIONS;
		if(GameClient()->m_Menus.DoButtonForceFontSize_Menu(&m_AddActionButton, EcLocalize("Add Action"), 0, &AddButton, 11.0f, AddDisabled))
			m_SelectedAction = AddQuickAction(m_aEditName[0] != '\0' ? m_aEditName : "New Action", m_aEditCommand);

		if(GameClient()->m_Menus.DoButtonForceFontSize_Menu(&m_DeleteActionButton, EcLocalize("Delete Action"), 0, &DeleteButton, 11.0f, m_SelectedAction < 0))
		{
			RemoveQuickAction(m_SelectedAction);
			m_SelectedAction = -1;
			m_DraggedAction = -1;
			m_Dragging = false;
		}
	}

	if(View.h < 20.0f)
		return;

	if(m_vQuickActions.empty())
	{
		CUIRect EmptyLabel;
		View.HSplitTop(20.0f, &EmptyLabel, nullptr);
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f));
		Ui()->DoLabel(&EmptyLabel, m_QuickActionsEditMode ? EcLocalize("Use \"Add Action\" to create a quick action") : EcLocalize("No quick actions yet, use \"Edit\" to add one"), 11.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	m_ActionScrollRegion.Begin(&View, &ScrollParams);

	// The tiles are laid out into as many columns as the available width fits, so
	// the grid grows with the amount of actions instead of using a fixed layout.
	const float Spacing = 4.0f;
	const float TileHeight = 22.0f;
	const float MinTileWidth = 92.0f;
	const int NumActions = (int)m_vQuickActions.size();
	const int Columns = std::max(1, (int)((View.w + Spacing) / (MinTileWidth + Spacing)));
	const float TileWidth = (View.w - (Columns - 1) * Spacing) / Columns;

	std::vector<CUIRect> vTiles;
	vTiles.reserve(NumActions);
	CUIRect Row = View;
	for(int Index = 0; Index < NumActions; ++Index)
	{
		if(Index % Columns == 0)
		{
			CUIRect SpacedRow;
			View.HSplitTop(TileHeight + Spacing, &SpacedRow, &View);
			m_ActionScrollRegion.AddRect(SpacedRow);
			SpacedRow.HSplitTop(TileHeight, &Row, nullptr);
		}

		CUIRect Tile;
		Row.VSplitLeft(TileWidth, &Tile, &Row);
		Row.VSplitLeft(Spacing, nullptr, &Row);
		vTiles.push_back(Tile);
	}

	int HoveredAction = -1;
	for(int Index = 0; Index < NumActions; ++Index)
	{
		if(Ui()->MouseHovered(&vTiles[Index]))
		{
			HoveredAction = Index;
			break;
		}
	}

	const bool HasSelectedPlayers = CountSelectedPlayers() > 0;
	int MoveFrom = -1;
	int MoveTo = -1;

	for(int Index = 0; Index < NumActions; ++Index)
	{
		const CUIRect &Tile = vTiles[Index];
		if(m_ActionScrollRegion.RectClipped(Tile))
			continue;

		const CQuickAction &Action = m_vQuickActions[Index];
		const bool TargetsPlayers = CommandTargetsPlayers(Action.m_aCommand);
		const bool CanExecute = Action.m_aCommand[0] != '\0' && (!TargetsPlayers || HasSelectedPlayers);

		if(m_QuickActionsEditMode)
		{
			bool Clicked = false;
			bool Abrupted = false;
			Ui()->DoDraggableButtonLogic(&m_aActionButtons[Index], 0, &Tile, &Clicked, &Abrupted);

			// DoDraggableButtonLogic returns 0 on the frame the tile is pressed, so the
			// active item is used to detect the start of a possible drag.
			if(m_DraggedAction != Index && Ui()->ActiveItem() == &m_aActionButtons[Index])
			{
				m_DraggedAction = Index;
				m_DragStartPos = Ui()->MousePos();
				m_Dragging = false;
			}

			if(m_DraggedAction == Index)
			{
				if(distance(Ui()->MousePos(), m_DragStartPos) > 5.0f)
					m_Dragging = true;

				if(Abrupted)
				{
					m_DraggedAction = -1;
					m_Dragging = false;
				}
				else if(Clicked)
				{
					if(m_Dragging)
					{
						if(HoveredAction >= 0 && HoveredAction != Index)
						{
							MoveFrom = Index;
							MoveTo = HoveredAction;
						}
					}
					else
					{
						m_SelectedAction = m_SelectedAction == Index ? -1 : Index;
					}
					m_DraggedAction = -1;
					m_Dragging = false;
				}
			}
		}
		else
		{
			const int Result = Ui()->DoButtonLogic(&m_aActionButtons[Index], 0, &Tile, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
			if(Result == 1 && CanExecute)
			{
				if(TargetsPlayers)
				{
					for(const std::string &Chunk : BuildRconCommandChunks(Action.m_aCommand))
						Client()->Rcon(Chunk.c_str());
				}
				else
				{
					Client()->Rcon(Action.m_aCommand);
				}
			}
			else if(Result == 2 && Action.m_aCommand[0] != '\0')
			{
				m_CommandInput.Set(Action.m_aCommand);
				m_CommandInput.SetCursorOffset(str_length(Action.m_aCommand));
				m_CommandInput.SelectNothing();
			}
		}

		const bool Hovered = Ui()->HotItem() == &m_aActionButtons[Index];
		const bool Selected = m_QuickActionsEditMode && m_SelectedAction == Index;
		const bool DropTarget = m_Dragging && HoveredAction == Index && m_DraggedAction != Index;

		// Same base color and hover/press response as the other menu buttons
		ColorRGBA TileColor(1.0f, 1.0f, 1.0f, 0.5f);
		if(Selected)
			TileColor = ColorRGBA(0.5f, 0.95f, 0.7f, 0.5f);
		else if(DropTarget)
			TileColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f);
		TileColor.a *= Ui()->ButtonColorMul(&m_aActionButtons[Index]);
		if(!m_QuickActionsEditMode && !CanExecute)
			TileColor.a *= 0.5f;
		if(m_Dragging && m_DraggedAction == Index)
			TileColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f);
		Tile.Draw(TileColor, IGraphics::CORNER_ALL, 5.0f);

		if(Hovered && Action.m_aCommand[0] != '\0')
		{
			// The tooltip only keeps the pointer around, and only the hovered tile can
			// show one, so a single shared buffer is enough to keep it valid.
			str_copy(m_aHoveredCommand, Action.m_aCommand);
			GameClient()->m_Tooltips.DoToolTip(&m_aActionButtons[Index], &Tile, m_aHoveredCommand);
		}

		CUIRect TileLabel;
		Tile.VMargin(4.0f, &TileLabel);
		SLabelProperties LabelProps;
		LabelProps.m_MaxWidth = TileLabel.w;
		LabelProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&TileLabel, Action.m_aName[0] != '\0' ? Action.m_aName : Action.m_aCommand, 11.0f, TEXTALIGN_MC, LabelProps);
	}

	// Stop dragging when the tile stopped being the active element without being
	// released on top of the grid, for example when it was scrolled out of view.
	if(m_DraggedAction >= 0 && (m_DraggedAction >= NumActions || Ui()->ActiveItem() != &m_aActionButtons[m_DraggedAction]))
	{
		m_DraggedAction = -1;
		m_Dragging = false;
	}

	if(m_Dragging && m_DraggedAction >= 0)
	{
		const CQuickAction &Action = m_vQuickActions[m_DraggedAction];
		CUIRect Floating = vTiles[m_DraggedAction];
		Floating.x = Ui()->MousePos().x - Floating.w / 2.0f;
		Floating.y = Ui()->MousePos().y - Floating.h / 2.0f;
		Floating.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f), IGraphics::CORNER_ALL, 5.0f);

		CUIRect FloatingLabel;
		Floating.VMargin(4.0f, &FloatingLabel);
		SLabelProperties LabelProps;
		LabelProps.m_MaxWidth = FloatingLabel.w;
		LabelProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&FloatingLabel, Action.m_aName[0] != '\0' ? Action.m_aName : Action.m_aCommand, 11.0f, TEXTALIGN_MC, LabelProps);
	}

	m_ActionScrollRegion.End();

	// Moving shifts the indices of the other actions, so a selection that is not the
	// moved one is dropped instead of pointing at a different action afterwards.
	if(MoveFrom >= 0)
	{
		MoveQuickAction(MoveFrom, MoveTo);
		m_SelectedAction = m_SelectedAction == MoveFrom ? MoveTo : -1;
	}
}
