#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/entity/mediaplayer/media_player_impl.h>
#include <game/client/components/entity/moderation/mod_quick_actions.h>
#include <game/client/components/menus.h>
#include <game/client/components/skins.h>
#include <game/client/components/tclient/statusbar.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;

void CollectPossibleCommandsCallback(int Index, const char *pCmd, void *pUser)
{
	static_cast<std::vector<const char *> *>(pUser)->push_back(pCmd);
}

void SortCompletions(std::vector<const char *> &vCompletions, const char *pSearch)
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

std::string_view GetCommandName(std::string_view CommandLine)
{
	const size_t Start = CommandLine.find_first_not_of(" \t");
	if(Start == std::string_view::npos)
		return {};

	const size_t End = CommandLine.find_first_of(" \t;", Start);
	if(End == std::string_view::npos)
		return CommandLine.substr(Start);
	return CommandLine.substr(Start, End - Start);
}

std::string ReplaceClientIdPlaceholder(std::string_view Command, int ClientId)
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

bool CommandTargetsPlayers(const char *pCommandTemplate)
{
	return str_find(pCommandTemplate, "%d") != nullptr;
}

std::vector<std::string> BuildRconCommandChunks(const char *pCommandTemplate, const bool (&aSelectedPlayers)[MAX_CLIENTS])
{
	std::vector<std::string> vChunks;
	if(pCommandTemplate == nullptr || pCommandTemplate[0] == '\0')
		return vChunks;

	constexpr size_t MaxCommandLength = IConsole::CMDLINE_LENGTH - 1;
	std::string CurrentChunk;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!aSelectedPlayers[ClientId])
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

int CountSelectedPlayers(const bool (&aSelectedPlayers)[MAX_CLIENTS])
{
	int SelectedCount = 0;
	for(bool Selected : aSelectedPlayers)
	{
		if(Selected)
			++SelectedCount;
	}
	return SelectedCount;
}

char NextCommandParam(const char *&pFormat)
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

bool IsIntegerArgument(std::string_view Argument)
{
	if(Argument == "%d")
		return true;

	char aBuf[IConsole::CMDLINE_LENGTH];
	str_copy(aBuf, std::string(Argument).c_str(), sizeof(aBuf));
	int Value;
	return str_toint(aBuf, &Value) && Value != std::numeric_limits<int>::max() && Value != std::numeric_limits<int>::min();
}

bool IsFloatArgument(std::string_view Argument)
{
	if(Argument == "%d")
		return true;

	char aBuf[IConsole::CMDLINE_LENGTH];
	str_copy(aBuf, std::string(Argument).c_str(), sizeof(aBuf));
	float Value;
	return str_tofloat(aBuf, &Value) && Value != std::numeric_limits<float>::max() && Value != std::numeric_limits<float>::min();
}

std::vector<std::string_view> TokenizeCommandArguments(std::string_view CommandLine)
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

bool ValidateCommandSyntax(std::string_view CommandLine, const IConsole::ICommandInfo *pCommandInfo)
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

const IConsole::ICommandInfo *FindDisplayedCommandInfo(IConsole *pConsole, IClient *pClient, std::string_view CommandLine, bool &ExactMatch, std::string &CommandName)
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

void AutocompleteCommandInput(CLineInput *pInput, std::string_view SuggestedCommand, const IConsole::ICommandInfo *pCommandInfo)
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

void CMenus::RenderModerationMenu(CUIRect MainView)
{
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

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

	enum EPlayerSortMode
	{
		SORT_NAME = 0,
		SORT_CLIENT_ID,
		SORT_CLAN,
		SORT_RANK,
		SORT_SKIN,
		NUM_SORT_MODES,
	};

	static int s_PlayerSortMode = SORT_NAME;
	const char *pSortLabel = "";
	switch(s_PlayerSortMode)
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
	static CButtonContainer s_SortButton;
	if(DoButton_Menu(&s_SortButton, aSortButton, 0, &Button))
		s_PlayerSortMode = (s_PlayerSortMode + 1) % NUM_SORT_MODES;

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
		switch(s_PlayerSortMode)
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

	static bool s_aSelectedPlayers[MAX_CLIENTS] = {false};
	static char s_aaSelectedNames[MAX_CLIENTS][MAX_NAME_LENGTH] = {{0}};
	static char s_aaSelectedClans[MAX_CLIENTS][MAX_CLAN_LENGTH] = {{0}};
	static int s_aPlayerItemIds[MAX_CLIENTS] = {0};
	static int s_LastSelectedClientId = -1;
	static CScrollRegion s_PlayerScrollRegion;
	static CLineInputBuffered<IConsole::CMDLINE_LENGTH> s_CommandInput;
	static bool s_CommandInitialized = false;
	if(!s_CommandInitialized)
	{
		s_CommandInput.Set("ban %d 20160 Bot Client.");
		s_CommandInitialized = true;
	}

	// Remember the identity of a selected player so the selection can be dropped
	// once the client id is recycled. Client ids are reused when a player leaves
	// and another joins, and this menu is only rendered while the moderation page
	// is open, so we cannot rely on observing an empty slot: otherwise a stale
	// selection would silently move onto whoever reuses the id and a command
	// could be executed on an innocent player.
	auto SelectPlayer = [&](int ClientId) {
		s_aSelectedPlayers[ClientId] = true;
		str_copy(s_aaSelectedNames[ClientId], GameClient()->m_aClients[ClientId].m_aName);
		str_copy(s_aaSelectedClans[ClientId], GameClient()->m_aClients[ClientId].m_aClan);
	};

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!s_aSelectedPlayers[i])
			continue;
		if(!GameClient()->m_Snap.m_apPlayerInfos[i] ||
			str_comp(GameClient()->m_aClients[i].m_aName, s_aaSelectedNames[i]) != 0 ||
			str_comp(GameClient()->m_aClients[i].m_aClan, s_aaSelectedClans[i]) != 0)
		{
			s_aSelectedPlayers[i] = false;
		}
	}
	if(s_LastSelectedClientId >= 0 && !GameClient()->m_Snap.m_apPlayerInfos[s_LastSelectedClientId])
		s_LastSelectedClientId = -1;

	const int SelectedCountBeforeActions = CountSelectedPlayers(s_aSelectedPlayers);
	const bool HasSelectedPlayers = SelectedCountBeforeActions > 0;
	const bool HasCommandTemplate = s_CommandInput.GetString()[0] != '\0';
	bool ExactCommandMatch = false;
	std::string DisplayedCommandName;
	const IConsole::ICommandInfo *pDisplayedCommandInfo = FindDisplayedCommandInfo(Console(), Client(), s_CommandInput.GetString(), ExactCommandMatch, DisplayedCommandName);
	const bool CommandSyntaxValid = HasCommandTemplate && ExactCommandMatch && ValidateCommandSyntax(s_CommandInput.GetString(), pDisplayedCommandInfo);
	const std::vector<std::string> vCommandChunks = BuildRconCommandChunks(s_CommandInput.GetString(), s_aSelectedPlayers);

	LeftView.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_ALL, 10.0f);
	LeftView.Margin(10.0f, &LeftView);

	CUIRect CommandLabel, CommandInput, CommandMeta, CommandHelp, ButtonsRow, ExecuteButton, CopyButton;
	LeftView.HSplitTop(24.0f, &CommandLabel, &LeftView);
	Ui()->DoLabel(&CommandLabel, EcLocalize("Command"), 18.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(10.0f, nullptr, &LeftView);
	LeftView.HSplitTop(24.0f, &CommandInput, &LeftView);
	Ui()->DoClearableEditBox(&s_CommandInput, &CommandInput, 12.0f);
	if(CLineInput::GetActiveInput() == &s_CommandInput && Input()->KeyPress(KEY_TAB) && pDisplayedCommandInfo != nullptr)
		AutocompleteCommandInput(&s_CommandInput, DisplayedCommandName, pDisplayedCommandInfo);

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

	static CButtonContainer s_ExecuteCommandButton;
	const bool ExecuteDisabled = !Client()->RconAuthed() || !HasSelectedPlayers || !HasCommandTemplate || !CommandSyntaxValid || vCommandChunks.empty();
	if(DoButtonForceFontSize_Menu(&s_ExecuteCommandButton, EcLocalize("Execute Command"), 0, &ExecuteButton, 11.0f, ExecuteDisabled))
	{
		for(const std::string &Chunk : vCommandChunks)
			Client()->Rcon(Chunk.c_str());
	}

	static CButtonContainer s_CopyToRconButton;
	const bool CopyDisabled = !HasSelectedPlayers || !HasCommandTemplate || vCommandChunks.empty();
	if(DoButtonForceFontSize_Menu(&s_CopyToRconButton, EcLocalize("Copy to Rcon"), 0, &CopyButton, 11.0f, CopyDisabled))
	{
		GameClient()->m_GameConsole.SetRemoteConsoleInput(vCommandChunks.front().c_str());
	}

	LeftView.HSplitTop(12.0f, nullptr, &LeftView);
	RenderModerationQuickActions(LeftView, &s_CommandInput, s_aSelectedPlayers);

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 90.0f;
	s_PlayerScrollRegion.Begin(&PlayerList, &ScrollParams);

	for(int Index = 0; Index < (int)vOnlinePlayers.size(); ++Index)
	{
		const int ClientId = vOnlinePlayers[Index];

		CUIRect Row;
		PlayerList.HSplitTop(30.0f, &Row, &PlayerList);
		const bool Visible = s_PlayerScrollRegion.AddRect(Row);
		if(!Visible)
			continue;

		const int ClickResult = Ui()->DoButtonLogic(&s_aPlayerItemIds[ClientId], 0, &Row, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
		if(ClickResult == 1)
		{
			if(Input()->ShiftIsPressed() && s_LastSelectedClientId != -1)
			{
				auto AnchorIt = std::find(vOnlinePlayers.begin(), vOnlinePlayers.end(), s_LastSelectedClientId);
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

			s_LastSelectedClientId = ClientId;
		}
		else if(ClickResult == 2)
		{
			if(Input()->ShiftIsPressed() && s_LastSelectedClientId != -1)
			{
				auto AnchorIt = std::find(vOnlinePlayers.begin(), vOnlinePlayers.end(), s_LastSelectedClientId);
				if(AnchorIt != vOnlinePlayers.end())
				{
					const int AnchorIndex = AnchorIt - vOnlinePlayers.begin();
					const int Start = std::min(AnchorIndex, Index);
					const int End = std::max(AnchorIndex, Index);
					for(int RangeIndex = Start; RangeIndex <= End; ++RangeIndex)
						s_aSelectedPlayers[vOnlinePlayers[RangeIndex]] = false;
				}
				else
				{
					s_aSelectedPlayers[ClientId] = false;
				}
			}
			else
			{
				s_aSelectedPlayers[ClientId] = false;
			}

			s_LastSelectedClientId = ClientId;
		}

		const bool Selected = s_aSelectedPlayers[ClientId];
		const bool Hovered = Ui()->HotItem() == &s_aPlayerItemIds[ClientId];
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
				m_aModMenuScore[ClientId], m_aModMenuScoreMillis[ClientId], TextRender()->DefaultTextColor());
		}
		else if(MillisecondScore)
		{
			Ui()->RenderTime(RankRect, 10.0f, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS, GameClient()->m_aClients[ClientId].m_FinishTimeMillis, true,
				m_aModMenuScore[ClientId], m_aModMenuScoreMillis[ClientId], TextRender()->DefaultTextColor());
		}
		else if(TimeScore)
		{
			const int Score = GameClient()->m_Snap.m_apPlayerInfos[ClientId]->m_Score;
			Ui()->RenderTime(RankRect, 10.0f, Score, Score == FinishTime::NOT_FINISHED_TIMESCORE, -1, false,
				m_aModMenuScore[ClientId], m_aModMenuScoreMillis[ClientId], TextRender()->DefaultTextColor());
		}
		else
		{
			TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f));
			Ui()->DoLabel(&RankRect, "-", 10.0f, TEXTALIGN_MR);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		const vec2 TeeEyeDir = TeeEyeDirection(TeeRect.Center());
		const bool Paused = GameClient()->m_aClients[ClientId].m_Paused || GameClient()->m_aClients[ClientId].m_Spec;
		CAnimState AnimState;
		AnimState.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		if(Paused)
			AnimState.Add(&g_pData->m_aAnimations[TeeEyeDir.x < 0 ? ANIM_SIT_LEFT : ANIM_SIT_RIGHT], 0.0f, 1.0f);
		else
			AnimState.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);

		RenderTee(TeeRect.Center() + vec2(-1.0f, 2.5f), TeeEyeDir, &AnimState, &TeeInfo, Paused ? EMOTE_BLINK : EMOTE_NORMAL);
	}

	s_PlayerScrollRegion.End();

	const int SelectedCount = CountSelectedPlayers(s_aSelectedPlayers);

	CUIRect SelectedLabel, DeselectButton;
	Footer.VSplitRight(110.0f, &SelectedLabel, &DeselectButton);
	char aSelectedBuf[64];
	str_format(aSelectedBuf, sizeof(aSelectedBuf), "%d %s", SelectedCount, EcLocalize("selected"));
	Ui()->DoLabel(&SelectedLabel, aSelectedBuf, 12.0f, TEXTALIGN_ML);

	static CButtonContainer s_DeselectAllButton;
	if(DoButton_Menu(&s_DeselectAllButton, EcLocalize("Deselect All"), 0, &DeselectButton))
	{
		for(bool &Selected : s_aSelectedPlayers)
			Selected = false;
		s_LastSelectedClientId = -1;
	}
}

void CMenus::RenderModerationQuickActions(CUIRect View, CLineInput *pCommandInput, const bool (&aSelectedPlayers)[MAX_CLIENTS])
{
	static bool s_EditMode = false;
	static int s_SelectedAction = -1;
	static int s_LoadedAction = -1;
	static char s_aEditName[MODQUICKACTION_MAX_NAME] = "";
	static char s_aEditCommand[MODQUICKACTION_MAX_CMD] = "";
	static char s_aHoveredCommand[MODQUICKACTION_MAX_CMD] = "";
	static int s_DraggedAction = -1;
	static bool s_Dragging = false;
	static vec2 s_DragStartPos = vec2(0.0f, 0.0f);
	static CScrollRegion s_ActionScrollRegion;
	static CButtonContainer s_aActionButtons[MODQUICKACTION_MAX_ACTIONS];

	static CLineInput s_NameInput;
	static CLineInput s_ActionCommandInput;
	s_NameInput.SetBuffer(s_aEditName, sizeof(s_aEditName));
	s_NameInput.SetEmptyText(EcLocalize("Button name"));
	s_ActionCommandInput.SetBuffer(s_aEditCommand, sizeof(s_aEditCommand));
	s_ActionCommandInput.SetEmptyText(EcLocalize("Command, %d is replaced by the client id"));

	CModQuickActions *pQuickActions = &GameClient()->m_ModQuickActions;
	if(s_SelectedAction >= (int)pQuickActions->m_vActions.size())
		s_SelectedAction = -1;

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
		s_EditMode = false;
		s_SelectedAction = -1;
		s_DraggedAction = -1;
		s_Dragging = false;
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.45f));
		Ui()->DoLabel(&Hint, EcLocalize("Quick actions require rcon authentication"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	static CButtonContainer s_EditModeButton;
	if(DoButton_Menu(&s_EditModeButton, s_EditMode ? EcLocalize("Done") : EcLocalize("Edit"), s_EditMode, &EditButton))
	{
		s_EditMode = !s_EditMode;
		s_SelectedAction = -1;
		s_DraggedAction = -1;
		s_Dragging = false;
	}

	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.45f));
	Ui()->DoLabel(&Hint, s_EditMode ? EcLocalize("Click to edit an action, drag it to move it somewhere else") : EcLocalize("Left click runs the action on the selection, right click copies it above"), 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// Load the selected action into the edit buffers before anything can change the
	// selection again, so that editing never writes into the previously selected one.
	if(s_SelectedAction != s_LoadedAction)
	{
		if(s_SelectedAction >= 0)
		{
			s_NameInput.Set(pQuickActions->m_vActions[s_SelectedAction].m_aName);
			s_ActionCommandInput.Set(pQuickActions->m_vActions[s_SelectedAction].m_aCommand);
		}
		else
		{
			s_NameInput.Clear();
			s_ActionCommandInput.Clear();
		}
		s_LoadedAction = s_SelectedAction;
	}

	if(s_EditMode)
	{
		CUIRect Editor, Row, Label, Input, AddButton, DeleteButton;
		View.HSplitBottom(68.0f, &View, &Editor);
		View.HSplitBottom(8.0f, &View, nullptr);

		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitLeft(70.0f, &Label, &Input);
		Ui()->DoLabel(&Label, EcLocalize("Name"), 12.0f, TEXTALIGN_ML);
		Ui()->DoEditBox(&s_NameInput, &Input, 11.0f);

		// The syntax check is only a hint, a command is never rejected: servers can
		// have commands that this client does not know about.
		bool ExactCommandMatch = false;
		std::string DisplayedCommandName;
		const IConsole::ICommandInfo *pCommandInfo = FindDisplayedCommandInfo(Console(), Client(), s_aEditCommand, ExactCommandMatch, DisplayedCommandName);
		const bool CommandSyntaxValid = s_aEditCommand[0] == '\0' || (ExactCommandMatch && ValidateCommandSyntax(s_aEditCommand, pCommandInfo));

		Editor.HSplitTop(4.0f, nullptr, &Editor);
		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitLeft(70.0f, &Label, &Input);
		Ui()->DoLabel(&Label, EcLocalize("Command"), 12.0f, TEXTALIGN_ML);
		std::vector<STextColorSplit> vCommandColorSplits;
		if(!CommandSyntaxValid)
			vCommandColorSplits.emplace_back(0, -1, ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoEditBox(&s_ActionCommandInput, &Input, 11.0f, IGraphics::CORNER_ALL, vCommandColorSplits);

		if(s_aEditCommand[0] != '\0')
		{
			// The tooltip only keeps the pointer around, so the text has to outlive the frame.
			static char s_aCommandTooltip[IConsole::CMDLINE_LENGTH + IConsole::TEMPCMD_PARAMS_LENGTH + IConsole::TEMPCMD_HELP_LENGTH + 8];
			if(pCommandInfo == nullptr)
			{
				str_copy(s_aCommandTooltip, EcLocalize("Unknown rcon command"));
			}
			else
			{
				if(pCommandInfo->Params()[0] != '\0')
					str_format(s_aCommandTooltip, sizeof(s_aCommandTooltip), "%s %s", DisplayedCommandName.c_str(), pCommandInfo->Params());
				else
					str_copy(s_aCommandTooltip, DisplayedCommandName.c_str());
				if(pCommandInfo->Help()[0] != '\0')
				{
					str_append(s_aCommandTooltip, "\n");
					str_append(s_aCommandTooltip, pCommandInfo->Help());
				}
			}
			GameClient()->m_Tooltips.DoToolTip(&s_ActionCommandInput, &Input, s_aCommandTooltip, 200.0f);
		}

		if(s_SelectedAction >= 0)
		{
			str_copy(pQuickActions->m_vActions[s_SelectedAction].m_aName, s_aEditName);
			str_copy(pQuickActions->m_vActions[s_SelectedAction].m_aCommand, s_aEditCommand);
		}

		Editor.HSplitTop(4.0f, nullptr, &Editor);
		Editor.HSplitTop(20.0f, &Row, &Editor);
		Row.VSplitMid(&AddButton, &DeleteButton, 5.0f);

		// Adding takes over whatever is in the input fields, so an action can be
		// written out first and then added without losing what was typed.
		static CButtonContainer s_AddActionButton;
		const bool AddDisabled = (int)pQuickActions->m_vActions.size() >= MODQUICKACTION_MAX_ACTIONS;
		if(DoButtonForceFontSize_Menu(&s_AddActionButton, EcLocalize("Add Action"), 0, &AddButton, 11.0f, AddDisabled))
			s_SelectedAction = pQuickActions->AddAction(s_aEditName[0] != '\0' ? s_aEditName : "New Action", s_aEditCommand);

		static CButtonContainer s_DeleteActionButton;
		if(DoButtonForceFontSize_Menu(&s_DeleteActionButton, EcLocalize("Delete Action"), 0, &DeleteButton, 11.0f, s_SelectedAction < 0))
		{
			pQuickActions->RemoveAction(s_SelectedAction);
			s_SelectedAction = -1;
			s_DraggedAction = -1;
			s_Dragging = false;
		}
	}

	if(View.h < 20.0f)
		return;

	if(pQuickActions->m_vActions.empty())
	{
		CUIRect EmptyLabel;
		View.HSplitTop(20.0f, &EmptyLabel, nullptr);
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f));
		Ui()->DoLabel(&EmptyLabel, s_EditMode ? EcLocalize("Use \"Add Action\" to create a quick action") : EcLocalize("No quick actions yet, use \"Edit\" to add one"), 11.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		return;
	}

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	s_ActionScrollRegion.Begin(&View, &ScrollParams);

	// The tiles are laid out into as many columns as the available width fits, so
	// the grid grows with the amount of actions instead of using a fixed layout.
	const float Spacing = 4.0f;
	const float TileHeight = 22.0f;
	const float MinTileWidth = 92.0f;
	const int NumActions = (int)pQuickActions->m_vActions.size();
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
			s_ActionScrollRegion.AddRect(SpacedRow);
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

	const bool HasSelectedPlayers = CountSelectedPlayers(aSelectedPlayers) > 0;
	int MoveFrom = -1;
	int MoveTo = -1;

	for(int Index = 0; Index < NumActions; ++Index)
	{
		const CUIRect &Tile = vTiles[Index];
		if(s_ActionScrollRegion.RectClipped(Tile))
			continue;

		const CModQuickActions::CAction &Action = pQuickActions->m_vActions[Index];
		const bool TargetsPlayers = CommandTargetsPlayers(Action.m_aCommand);
		const bool CanExecute = Action.m_aCommand[0] != '\0' && (!TargetsPlayers || HasSelectedPlayers);

		if(s_EditMode)
		{
			bool Clicked = false;
			bool Abrupted = false;
			Ui()->DoDraggableButtonLogic(&s_aActionButtons[Index], 0, &Tile, &Clicked, &Abrupted);

			// DoDraggableButtonLogic returns 0 on the frame the tile is pressed, so the
			// active item is used to detect the start of a possible drag.
			if(s_DraggedAction != Index && Ui()->ActiveItem() == &s_aActionButtons[Index])
			{
				s_DraggedAction = Index;
				s_DragStartPos = Ui()->MousePos();
				s_Dragging = false;
			}

			if(s_DraggedAction == Index)
			{
				if(distance(Ui()->MousePos(), s_DragStartPos) > 5.0f)
					s_Dragging = true;

				if(Abrupted)
				{
					s_DraggedAction = -1;
					s_Dragging = false;
				}
				else if(Clicked)
				{
					if(s_Dragging)
					{
						if(HoveredAction >= 0 && HoveredAction != Index)
						{
							MoveFrom = Index;
							MoveTo = HoveredAction;
						}
					}
					else
					{
						s_SelectedAction = s_SelectedAction == Index ? -1 : Index;
					}
					s_DraggedAction = -1;
					s_Dragging = false;
				}
			}
		}
		else
		{
			const int Result = Ui()->DoButtonLogic(&s_aActionButtons[Index], 0, &Tile, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
			if(Result == 1 && CanExecute)
			{
				if(TargetsPlayers)
				{
					for(const std::string &Chunk : BuildRconCommandChunks(Action.m_aCommand, aSelectedPlayers))
						Client()->Rcon(Chunk.c_str());
				}
				else
				{
					Client()->Rcon(Action.m_aCommand);
				}
			}
			else if(Result == 2 && Action.m_aCommand[0] != '\0')
			{
				pCommandInput->Set(Action.m_aCommand);
				pCommandInput->SetCursorOffset(str_length(Action.m_aCommand));
				pCommandInput->SelectNothing();
			}
		}

		const bool Hovered = Ui()->HotItem() == &s_aActionButtons[Index];
		const bool Selected = s_EditMode && s_SelectedAction == Index;
		const bool DropTarget = s_Dragging && HoveredAction == Index && s_DraggedAction != Index;

		// Same base color and hover/press response as the other menu buttons
		ColorRGBA TileColor(1.0f, 1.0f, 1.0f, 0.5f);
		if(Selected)
			TileColor = ColorRGBA(0.5f, 0.95f, 0.7f, 0.5f);
		else if(DropTarget)
			TileColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f);
		TileColor.a *= Ui()->ButtonColorMul(&s_aActionButtons[Index]);
		if(!s_EditMode && !CanExecute)
			TileColor.a *= 0.5f;
		if(s_Dragging && s_DraggedAction == Index)
			TileColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f);
		Tile.Draw(TileColor, IGraphics::CORNER_ALL, 5.0f);

		if(Hovered && Action.m_aCommand[0] != '\0')
		{
			// The tooltip only keeps the pointer around, and only the hovered tile can
			// show one, so a single shared buffer is enough to keep it valid.
			str_copy(s_aHoveredCommand, Action.m_aCommand);
			GameClient()->m_Tooltips.DoToolTip(&s_aActionButtons[Index], &Tile, s_aHoveredCommand);
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
	if(s_DraggedAction >= 0 && (s_DraggedAction >= NumActions || Ui()->ActiveItem() != &s_aActionButtons[s_DraggedAction]))
	{
		s_DraggedAction = -1;
		s_Dragging = false;
	}

	if(s_Dragging && s_DraggedAction >= 0)
	{
		const CModQuickActions::CAction &Action = pQuickActions->m_vActions[s_DraggedAction];
		CUIRect Floating = vTiles[s_DraggedAction];
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

	s_ActionScrollRegion.End();

	// Moving shifts the indices of the other actions, so a selection that is not the
	// moved one is dropped instead of pointing at a different action afterwards.
	if(MoveFrom >= 0)
	{
		pQuickActions->MoveAction(MoveFrom, MoveTo);
		s_SelectedAction = s_SelectedAction == MoveFrom ? MoveTo : -1;
	}
}
