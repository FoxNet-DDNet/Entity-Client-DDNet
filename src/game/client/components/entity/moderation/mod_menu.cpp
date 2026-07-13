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
			Ui()->RenderTime(RankRect, 10.0f, Score / 1000, Score == protocol7::FinishTime::NOT_FINISHED, Score % 1000, true);
		}
		else if(MillisecondScore)
		{
			Ui()->RenderTime(RankRect, 10.0f, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds, GameClient()->m_aClients[ClientId].m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS, GameClient()->m_aClients[ClientId].m_FinishTimeMillis, true);
		}
		else if(TimeScore)
		{
			const int Score = GameClient()->m_Snap.m_apPlayerInfos[ClientId]->m_Score;
			Ui()->RenderTime(RankRect, 10.0f, Score, Score == FinishTime::NOT_FINISHED_TIMESCORE, -1, false);
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
