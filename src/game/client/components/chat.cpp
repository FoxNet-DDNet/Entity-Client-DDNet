/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "chat.h"

#include "tclient/bindchat.h"
#include "tclient/warlist.h"

#include <base/color.h>
#include <base/io.h>
#include <base/log.h>
#include <base/log_color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>
#include <base/time.h>
#include <base/types.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/client/client.h>
#include <engine/console.h>
#include <engine/editor.h>
#include <engine/external/tinyexpr.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/csv.h>
#include <engine/shared/protocol.h>
#include <engine/shared/video.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/animstate.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/translate.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui_rect.h>
#include <game/localization.h>
#include <game/teamscore.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

static constexpr float POPUP_WIDTH = 170.0f;
static constexpr float POPUP_LANGUAGE_WIDTH = 200.0f;
static constexpr float POPUP_LANGUAGE_MAX_HEIGHT = 320.0f;
static constexpr float POPUP_FONT_SIZE = 10.0f;
static constexpr float POPUP_ENTRY_HEIGHT = 16.0f;
static constexpr float POPUP_ENTRY_SPACING = 2.0f;
static constexpr float POPUP_ENTRY_PADDING = 4.0f;
// The chat menu button, sized off the input line it sits next to
static constexpr float MENU_BUTTON_SCALE = 1.3f;
static constexpr float MENU_BUTTON_ROUNDING = 3.0f;

char CChat::ms_aDisplayText[MAX_CHAT_LENGTH] = "";

CChat::CLine::CLine()
{
	m_TextContainerIndex.Reset();
	m_QuadContainerIndex = -1;
	m_RenderedOffsetType = -1;
	m_RenderWidth = 0.0f;
}

void CChat::CLine::Reset(CChat &This)
{
	This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	This.Graphics()->DeleteQuadContainer(m_QuadContainerIndex);
	m_Initialized = false;
	m_Time = 0;
	m_aText[0] = '\0';
	m_aName[0] = '\0';
	m_Friend = false;
	m_TimesRepeated = 0;
	m_pManagedTeeRenderInfo = nullptr;

	// EClient
	m_pTranslateResponse = nullptr;
	m_Paused = false; // EClient
	m_RenderedOffsetType = -1;

	// Selection text
	m_RenderedName.clear();
	m_RenderedText.clear();
	m_RenderWidth = 0.0f;
}

CChat::CChat()
{
	m_Mode = MODE_NONE;
	m_BacklogCurLine = 0;
	m_LinesRendered = 0;

	// Selection state initialization
	m_Selecting = false;
	m_SelectionMousePress = vec2(0.0f, 0.0f);
	m_SelectionMouseRelease = vec2(0.0f, 0.0f);
	m_HasSelection = false;
	m_SelectionText.clear();
	m_NewLineCounter = 0;
	m_HoveringMessage = false;
	m_SelectorMouse = vec2(-1.0f, -1.0f);

	m_Input.SetClipboardLineCallback([this](const char *pStr) {
		if(Client()->m_FoxNetVersion != 0 && Client()->RconAuthed())
		{
			if(Client()->m_FoxNetVersion != 0 && pStr[0] && Client()->RconAuthed())
			{
				SendChat(m_Mode == MODE_ALL ? 0 : 1, pStr);
				AddHistoryEntry(pStr);
			}
		}
	});
	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = std::min(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vServerCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vServerCommands.emplace_back(pName, pParams, pHelpText);
	m_ServerCommandsNeedSorting = true;

	GameClient()->m_Bindchat.CacheChatCommands();
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vServerCommands.erase(std::remove_if(m_vServerCommands.begin(), m_vServerCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vServerCommands.end());

	GameClient()->m_Bindchat.CacheChatCommands();
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized)
			continue;
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		Line.m_RenderedOffsetType = -1;
		// recalculate sizes
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
	}
}

void CChat::ClearLines()
{
	for(auto &Line : m_aLines)
		Line.Reset(*this);
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
	m_BacklogCurLine = 0;
	m_LinesRendered = 0;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = nullptr;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_ServerSupportsCommandInfo = false;
	m_ServerCommandsNeedSorting = false;
	m_aCurrentInputText[0] = '\0';
	m_BacklogCurLine = 0;
	m_LinesRendered = 0;
	DisableMode();
	m_vServerCommands.clear();
	GameClient()->m_Bindchat.CacheChatCommands();
	GameClient()->m_Bindchat.SortChatBinds();

	// Reset selection state
	m_Selecting = false;
	m_SelectionMousePress = vec2(0.0f, 0.0f);
	m_SelectionMouseRelease = vec2(0.0f, 0.0f);
	m_HasSelection = false;
	m_SelectionText.clear();
	m_NewLineCounter = 0;
	m_HoveringMessage = false;

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(!str_comp(pMode, "all"))
		((CChat *)pUserData)->EnableMode(0);
	else if(!str_comp(pMode, "team"))
		((CChat *)pUserData)->EnableMode(1);
	else if(!str_comp(pMode, "silent"))
		((CChat *)pUserData)->EnableMode(2);
	else
		log_error("chat", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat *)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConClientMessage(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->AddLine(ECLIENT_MSG, TEAM_ALL, pResult->GetString(0));
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::Echo(const char *pString)
{
	AddLine(CLIENT_MSG, 0, pString);
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'|'silent'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("message", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClientMessage, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");

	Console()->Register("set_input", "r[input]", CFGFLAG_CLIENT, ConSetChatInput, this, "Opens chat and sets the input as the message"); // EClient [Player Actions]
	Console()->Register("say_queued", "r[message]", CFGFLAG_CLIENT, ConSayQueued, this, "Say in queue chat"); // EClient
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

bool CChat::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(m_Mode == MODE_NONE)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);

	m_SelectorMouse.x = std::clamp(m_SelectorMouse.x, 0.0f, (float)Graphics()->WindowWidth() - 1.0f);
	m_SelectorMouse.y = std::clamp(m_SelectorMouse.y, 0.0f, (float)Graphics()->WindowHeight() - 1.0f);

	return true;
}

int CChat::GetLinesToScroll(int Direction, int LinesToScroll) const
{
	const int RenderableLines = NumInitializedLines();

	int LinesToSkip = Direction == -1 ? m_BacklogCurLine + m_LinesRendered : m_BacklogCurLine - 1;
	LinesToSkip = std::clamp(LinesToSkip, 0, RenderableLines);

	int RemainingAbove = std::max(0, RenderableLines - LinesToSkip);
	int Amount = Direction == -1 ? std::min(RemainingAbove, std::max(LinesToScroll, 0)) : std::min(m_BacklogCurLine, std::max(LinesToScroll, 0));
	if(LinesToScroll <= 0)
		Amount = Direction == -1 ? RemainingAbove : m_BacklogCurLine;
	return std::max(0, Amount);
}

int CChat::NumInitializedLines() const
{
	int Count = 0;
	for(int i = 0; i < MAX_LINES; i++)
	{
		const CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		Count++;
	}
	return Count;
}

void CChat::AnchorPausedLines()
{
	// While the view is paused, incoming lines are only hidden, the view itself is still at
	// the bottom. Turn them into a real backlog offset before scrolling, otherwise they would
	// all pop in at once and the view would jump.
	const int PendingLines = GetLinesToSkipWhilePaused();
	if(PendingLines <= 0)
		return;

	m_BacklogCurLine = std::clamp(m_BacklogCurLine + PendingLines, 0, GetMaxBacklogCurLine());
	m_NewLineCounter = 0;
}

void CChat::ScrollToTop()
{
	AnchorPausedLines();
	m_BacklogCurLine += GetLinesToScroll(-1, -1);
}

void CChat::ScrollToBottom()
{
	m_BacklogCurLine = 0;
	m_NewLineCounter = 0;
}

void CChat::ScrollPageUp()
{
	AnchorPausedLines();
	m_BacklogCurLine += GetLinesToScroll(-1, std::max(1, m_LinesRendered));
}

void CChat::ScrollPageDown()
{
	AnchorPausedLines();
	m_BacklogCurLine -= GetLinesToScroll(1, std::max(1, m_LinesRendered));
	if(m_BacklogCurLine < 0)
		m_BacklogCurLine = 0;
}

// <EClient
static bool ContainsMathOperator(const char *pStr)
{
	for(const char *pChar = pStr; *pChar != '\0'; ++pChar)
	{
		if(*pChar == '+' || *pChar == '-' || *pChar == '*' || *pChar == '/' || *pChar == '^' || *pChar == '%' || *pChar == '(')
			return true;
	}
	return false;
}

bool CChat::MathSuggestion(char *pSuggestion, size_t SuggestionSize) const
{
	if(!g_Config.m_ClChatMath)
		return false;

	const char *pInput = m_Input.GetString();
	int Length = str_length(pInput);

	while(Length > 0 && pInput[Length - 1] == ' ')
		--Length;
	if(Length < 2 || pInput[Length - 1] != '=')
		return false;
	--Length;

	char aExpression[MAX_LINE_LENGTH];
	str_truncate(aExpression, sizeof(aExpression), pInput, Length);

	double Value = 0.0;
	bool Found = false;
	for(const char *pStart = aExpression; *pStart != '\0'; ++pStart)
	{
		if(*pStart == ' ' || (pStart != aExpression && *(pStart - 1) != ' '))
			continue;

		if(!ContainsMathOperator(pStart))
			continue;

		int Error = 0;
		Value = te_interp(pStart, &Error);
		if(Error == 0 && std::isfinite(Value))
		{
			Found = true;
			break;
		}
	}
	if(!Found)
		return false;

	char aValue[64];
	str_format(aValue, sizeof(aValue), "%.10g", Value);
	str_format(pSuggestion, (int)SuggestionSize, "%s%s", pInput[str_length(pInput) - 1] == ' ' ? "" : " ", aValue);
	return true;
}
// EClient>

bool CChat::OnInput(const IInput::CEvent &Event)
{
	if(m_Mode == MODE_NONE)
		return false;

	const int BacklogPrevLine = m_BacklogCurLine;

	// <EClient: while the chat is open it swallows every event, so the ui never hears the keys the
	// menus opened from it care about. They are handed over here, ahead of everything the chat
	// does with the same keys.
	if(Event.m_Flags & IInput::FLAG_PRESS && PopupOpen())
	{
		if(Event.m_Key == KEY_ESCAPE)
		{
			CloseTopPopup();
			return true;
		}
		if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			// Raised rather than scrolled by hand. A scroll region reached through ScrollRelative
			// restarts its animation from where it currently is, so ticks arriving faster than one
			// animation lands cut each other short and the list creeps instead of racing; taking
			// the hotkey it makes them stack the way a wheel is supposed to. It also picks up the
			// region under the cursor on its own, and gets alt-held page scrolling for free.
			Ui()->SetHotkey(Event.m_Key == KEY_MOUSE_WHEEL_UP ? CUi::HOTKEY_SCROLL_UP : CUi::HOTKEY_SCROLL_DOWN);
			// The wheel belongs to the menu either way, it must not move the chat behind it
			return true;
		}
	}
	// EClient>

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		DisableMode();
		GameClient()->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_ServerCommandsNeedSorting)
		{
			std::sort(m_vServerCommands.begin(), m_vServerCommands.end());
			GameClient()->m_Bindchat.SortChatBinds(); // EClient
			m_ServerCommandsNeedSorting = false;
		}

		bool SilentMessage = false;
		bool SendMessage = true;

		if(m_Mode == MODE_SILENT)
			SilentMessage = true;

		const char *pInput = m_Input.GetString();

		if(GameClient()->m_Bindchat.ChatDoBinds(pInput))
			SendMessage = false;

		// EClient: a practice command is run against the local practice world and swallowed, so the
		// server never sees a slash command it would answer for a team we are not in. It was typed
		// into the chat box like any other line, so it belongs in the history like any other line.
		if(SendMessage && GameClient()->m_LocalPractice.OnChatCommand(pInput))
		{
			AddHistoryEntry(pInput);
			SendMessage = false;
		}

		if(SendMessage)
		{
			if(SilentMessage)
			{
				if(g_Config.m_ClSilentMessages)
					AddLine(SILENT_MSG, TEAM_ALL, pInput);
			}
			else if(Client()->m_FoxNetVersion != 0 && pInput[0] && Client()->RconAuthed())
			{
				SendChat(m_Mode == MODE_ALL ? 0 : 1, pInput);
				AddHistoryEntry(pInput);
			}
			else
				SendChatQueued(pInput);
		}

		m_pHistoryEntry = nullptr;
		DisableMode();
		GameClient()->OnRelease();
		m_Input.Clear();
	}
	else if(Input()->ModifierIsPressed() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_C)
	{
		// Copy selection to clipboard
		if(m_HasSelection && !m_SelectionText.empty())
		{
			Input()->SetClipboardText(m_SelectionText.c_str());
			m_HasSelection = false;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_PAGEUP)
	{
		if(!m_Selecting && !m_HasSelection)
			ScrollPageUp();
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_PAGEDOWN)
	{
		if(!m_Selecting && !m_HasSelection)
			ScrollPageDown();
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_UP)
	{
		if(!m_Selecting && !m_HasSelection)
		{
			AnchorPausedLines();
			m_BacklogCurLine += GetLinesToScroll(-1, 1);
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	{
		if(!m_Selecting && !m_HasSelection)
		{
			AnchorPausedLines();
			m_BacklogCurLine -= GetLinesToScroll(1, 1);
			if(m_BacklogCurLine < 0)
				m_BacklogCurLine = 0;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_HOME && m_Input.IsEmpty())
	{
		ScrollToTop();
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_END && m_Input.IsEmpty())
	{
		ScrollToBottom();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		// <EClient
		char aMathSuggestion[64];
		if(MathSuggestion(aMathSuggestion, sizeof(aMathSuggestion)))
		{
			char aBuf[MAX_LINE_LENGTH];
			str_copy(aBuf, m_Input.GetString());
			str_append(aBuf, aMathSuggestion);
			m_Input.Set(aBuf);
			m_Input.SetCursorOffset(str_length(aBuf));
			return true;
		}
		// EClient>

		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : GameClient()->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = GameClient()->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != nullptr)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &Player1, const CRateablePlayer &Player2) -> bool {
					return Player1.m_Score < Player2.m_Score;
				});
		}

		if(GameClient()->m_Bindchat.ChatDoAutocomplete(ShiftPressed))
			;
		else
		{
			// find next possible name
			const char *pCompletionString = nullptr;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &GameClient()->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].m_ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_CHAT_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if((m_Input.GetString()[0] == '/' || GameClient()->m_Bindchat.CheckBindChat(m_Input.GetString())) && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"") || str_startswith(pCompletionString, "#")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
		{
			m_pHistoryEntry = m_History.Last();
		}

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	if(m_BacklogCurLine != BacklogPrevLine)
		m_BacklogCurLine = std::max(0, m_BacklogCurLine);

	const int MaxBacklogCurLine = std::max(0, NumInitializedLines() - 1);
	if(m_BacklogCurLine > MaxBacklogCurLine)
		m_BacklogCurLine = MaxBacklogCurLine;

	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode == MODE_NONE)
	{
		if(Team == 1)
			m_Mode = MODE_TEAM;
		else if(Team == 2)
			m_Mode = MODE_SILENT;
		else
			m_Mode = MODE_ALL;

		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_BacklogCurLine = 0;
		m_Input.Activate(EInputPriority::CHAT);
		if(m_SelectorMouse == vec2(-1.0f, -1.0f))
			m_SelectorMouse = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight()) * 0.5f;
	}
}

void CChat::DisableMode()
{
	if(m_Mode != MODE_NONE)
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
		m_BacklogCurLine = 0;
		// EClient: the menus belong to the open chat, they have nothing to act on without it
		Ui()->ClosePopupMenu(&m_MessagePopupContext);
		Ui()->ClosePopupMenu(&m_ChatPopupContext, true);
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		/*
		if(g_Config.m_ClCensorChat)
		{
			char aMessage[MAX_CHAT_LENGTH];
			str_copy(aMessage, pMsg->m_pMessage);
			GameClient()->m_Censor.CensorMessage(aMessage);
			AddLine(pMsg->m_ClientId, pMsg->m_Team, aMessage);
		}
		else
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
		*/

		AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pMsg->m_ClientId == SERVER_MSG)
		{
			StoreSave(pMsg->m_pMessage);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vServerCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

static constexpr const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

// TODO: remove this in a few releases (in 2027 or later)
//       it got deprecated by CGameClient::StoreSave
void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		GameClient()->Map()->BaseName(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

bool CChat::LineHighlighted(int ClientId, const char *pLine)
{
	bool Highlighted = false;

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && ClientId != GameClient()->m_aLocalIds[1])
		{
			for(int LocalId : GameClient()->m_aLocalIds)
			{
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
			}
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}

	return Highlighted;
}

void CChat::AddLine(int ClientId, int Team, const char *pLine)
{
	if(ChatDetection(ClientId, Team, pLine))
		return;

	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
	if(ClientId >= 0 && GameClient()->m_Snap.m_LocalClientId != ClientId)
	{
		if(g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend)
		{
			char Message[MAX_LINE_LENGTH];
			str_format(Message, sizeof(Message), "%s", GameClient()->m_aClients[ClientId].m_aName);
			if(Team == 3)
				str_format(Message, sizeof(Message), "← %s", GameClient()->m_aClients[ClientId].m_aName);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Message, pLine, Color);
			return;
		}
		else if(GameClient()->m_WarList.m_WarPlayers[ClientId].m_IsMuted)
		{
			char Message[MAX_LINE_LENGTH];
			str_format(Message, sizeof(Message), "%s", GameClient()->m_aClients[ClientId].m_aName);
			if(Team == 3)
				str_format(Message, sizeof(Message), "← %s", GameClient()->m_aClients[ClientId].m_aName);

			if(g_Config.m_ClMutedConsoleColor)
				Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMutedColor));

			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Message, pLine, Color);
			return;
		}
		else if(g_Config.m_ClWarList && g_Config.m_ClHideEnemyChat && GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1])
		{
			char Message[MAX_LINE_LENGTH];
			str_format(Message, sizeof(Message), "%s", GameClient()->m_aClients[ClientId].m_aName);
			if(Team == 3)
				str_format(Message, sizeof(Message), "← %s", GameClient()->m_aClients[ClientId].m_aName);

			if(g_Config.m_ClMutedConsoleColor)
				Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMutedColor));

			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, Message, pLine, Color);
			return;
		}
	}

	if(*pLine == 0 || (ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_aName[0] == '\0' || // unknown client
					  GameClient()->m_aClients[ClientId].m_ChatIgnore ||
					  // (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && GameClient()->m_aClients[ClientId].m_Foe))))
		return;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = nullptr;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == nullptr)
		{
			pEnd = pStrOld;
		}

		if(++Length >= MAX_CHAT_LENGTH)
		{
			*(const_cast<char *>(pStr)) = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*(const_cast<char *>(pEnd)) = 0;

	if(*pLine == 0)
		return;

	bool Highlighted = LineHighlighted(ClientId, pLine);

	auto &&FChatMsgCheckAndPrint = [](const CLine &Line) {
		ColorRGBA ChatLogColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(Line.m_Highlighted)
		{
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		}
		else
		{
			if(Line.m_Friend && g_Config.m_ClMessageFriend)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFriendColor));
			else if(Line.m_Team)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			else if(Line.m_ClientId == SERVER_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			else if(Line.m_ClientId == CLIENT_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			else // regular message
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		}

		const char *pFrom;
		if(Line.m_Whisper)
			pFrom = "whisper";
		else if(Line.m_Team)
			pFrom = "teamchat";
		else if(Line.m_ClientId == SERVER_MSG)
			pFrom = "server";
		else if(Line.m_ClientId == CLIENT_MSG)
			pFrom = "client";
		else
			pFrom = "chat";

		log_info_color(color_cast<LOG_COLOR>(ChatLogColor), pFrom, "%s%s%s", Line.m_aName, Line.m_ClientId >= 0 ? ": " : "", Line.m_aText);
	};

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));

	CLine &PreviousLine = m_aLines[m_CurrentLine];

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	// If it's a client message, m_aText will have ": " prepended so we have to work around it.
	if(PreviousLine.m_Initialized &&
		PreviousLine.m_TeamNumber == Team &&
		PreviousLine.m_ClientId == ClientId &&
		str_comp(PreviousLine.m_aText, pLine) == 0 &&
		PreviousLine.m_CustomColor == CustomColor)
	{
		PreviousLine.m_TimesRepeated++;
		TextRender()->DeleteTextContainer(PreviousLine.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(PreviousLine.m_QuadContainerIndex);
		PreviousLine.m_RenderedOffsetType = -1;
		PreviousLine.m_Time = time();
		PreviousLine.m_aYOffset[0] = -1.0f;
		PreviousLine.m_aYOffset[1] = -1.0f;

		FChatMsgCheckAndPrint(PreviousLine);
		return;
	}

	// Keep the visible line mapping stable while the view is paused, i.e. while dragging
	// a selection, while a finished selection exists or while the cursor hovers a message.
	// Once a finished selection exists, any new incoming line invalidates it because
	// the cached selection coordinates no longer match the live chat backlog.
	if(IsScrollPaused())
	{
		m_NewLineCounter++;
	}
	else
	{
		m_SelectionText.clear();
		m_NewLineCounter = 0;
	}

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;

	CLine &CurrentLine = m_aLines[m_CurrentLine];
	CurrentLine.Reset(*this);
	CurrentLine.m_Initialized = true;
	CurrentLine.m_Time = time();
	CurrentLine.m_aYOffset[0] = -1.0f;
	CurrentLine.m_aYOffset[1] = -1.0f;
	CurrentLine.m_ClientId = ClientId;
	CurrentLine.m_TeamNumber = Team;
	CurrentLine.m_Team = Team == 1;
	CurrentLine.m_Whisper = Team >= 2;
	CurrentLine.m_NameColor = -2;
	CurrentLine.m_CustomColor = CustomColor;
	CurrentLine.m_Highlighted = Highlighted;

	str_copy(CurrentLine.m_aText, pLine);

	if(CurrentLine.m_ClientId == SERVER_MSG)
	{
		str_copy(CurrentLine.m_aName, "*** ");
		if(g_Config.m_ClChatServerPrefix)
			str_copy(CurrentLine.m_aName, g_Config.m_ClServerPrefix);
	}
	else if(CurrentLine.m_ClientId == CLIENT_MSG || CurrentLine.m_ClientId == ECLIENT_MSG)
	{
		str_copy(CurrentLine.m_aName, "— ");
		if(g_Config.m_ClChatClientPrefix)
			str_copy(CurrentLine.m_aName, g_Config.m_ClClientPrefix);
	}
	else if(CurrentLine.m_ClientId == SILENT_MSG)
	{
		auto &LineAuthor = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId];

		str_copy(CurrentLine.m_aName, LineAuthor.m_aName);
		str_append(CurrentLine.m_aName, ": ");
	}
	else
	{
		const auto &LineAuthor = GameClient()->m_aClients[CurrentLine.m_ClientId];

		if(LineAuthor.m_Active)
		{
			if(LineAuthor.m_Team == TEAM_SPECTATORS)
				CurrentLine.m_NameColor = TEAM_SPECTATORS;

			if(GameClient()->IsTeamPlay())
			{
				if(LineAuthor.m_Team == TEAM_RED)
					CurrentLine.m_NameColor = TEAM_RED;
				else if(LineAuthor.m_Team == TEAM_BLUE)
					CurrentLine.m_NameColor = TEAM_BLUE;
			}
		}

		if(Team == TEAM_WHISPER_SEND)
		{
			str_copy(CurrentLine.m_aName, "→");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_BLUE;
			CurrentLine.m_Highlighted = false;
			Highlighted = false;
		}
		else if(Team == TEAM_WHISPER_RECV)
		{
			str_copy(CurrentLine.m_aName, "←");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_RED;
			CurrentLine.m_Highlighted = true;
			Highlighted = true;
		}
		else
		{
			str_copy(CurrentLine.m_aName, LineAuthor.m_aName);
		}

		if(LineAuthor.m_Active)
		{
			CurrentLine.m_Friend = LineAuthor.m_Friend;
			CurrentLine.m_Paused = LineAuthor.m_Paused; // EClient
			CurrentLine.m_pManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(LineAuthor);
		}
	}

	FChatMsgCheckAndPrint(CurrentLine);

	if(m_BacklogCurLine > 0)
		m_BacklogCurLine = std::min(m_BacklogCurLine + 1, GetMaxBacklogCurLine());

	// play sound
	int64_t Now = time();
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", CurrentLine.m_aName, CurrentLine.m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = CurrentLine.m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
#endif
			if(PlaySound)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
			else if(g_Config.m_SndFriendChat && (GameClient()->Friends()->IsFriend(m_aLines[m_CurrentLine].m_aName, "\0", true)))
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 0.8f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}

	// TClient
	GameClient()->m_Translate.AutoTranslate(CurrentLine);
}

void CChat::OnPrepareLines(float y)
{
	float x = 5.0f;
	float FontSize = this->FontSize();
	const int LinesToSkipWhilePaused = GetLinesToSkipWhilePaused();

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat;
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	int64_t Now = time();
	float LineWidth = (IsScoreBoardOpen ? std::max(85.0f, FontSize * 85.0f / 6.0f) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;
	m_LinesRendered = 0;

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		if(i < LinesToSkipWhilePaused)
			continue;

		const int AdjustedIndex = i - LinesToSkipWhilePaused;
		if(AdjustedIndex < m_BacklogCurLine)
			continue;

		const bool NeedsYOffsetRecalc = Line.m_aYOffset[OffsetType] < 0.0f;
		const bool NeedsContainerRecreate = ForceRecreate || !Line.m_TextContainerIndex.Valid() || Line.m_RenderedOffsetType != OffsetType;

		if(!NeedsContainerRecreate && !NeedsYOffsetRecalc)
		{
			// Even if we skip recreating the container, we still need to update y position
			y -= Line.m_aYOffset[OffsetType];

			// cut off if msgs waste too much space
			if(y < HeightLimit)
				break;

			m_LinesRendered++;
			continue;
		}

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		Line.m_RenderedOffsetType = -1;

		char aClientId[16] = "";
		if(g_Config.m_ClShowIdsChat && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load *** *** ***' to continue";
			}
		}

		const char *pTranslatedError = nullptr;
		const char *pTranslatedText = nullptr;
		const char *pTranslatedLanguage = nullptr;
		if(Line.m_pTranslateResponse != nullptr)
		{
			if(Line.m_pTranslateResponse->m_Text[0])
			{
				// If hidden and there is translated text
				if(pText != Line.m_aText)
				{
					pTranslatedError = Localize("Translated text hidden due to streamer mode");
				}
				else if(Line.m_pTranslateResponse->m_Error)
				{
					pTranslatedError = Line.m_pTranslateResponse->m_Text;
				}
				else
				{
					pTranslatedText = Line.m_pTranslateResponse->m_Text;
					if(Line.m_pTranslateResponse->m_Language[0] != '\0')
						pTranslatedLanguage = Line.m_pTranslateResponse->m_Language;
					else
						pTranslatedLanguage = "?";
				}
			}
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			CTextCursor MeasureCursor;
			MeasureCursor.SetPosition(vec2(TextBegin, 0.0f));
			MeasureCursor.m_FontSize = FontSize;
			MeasureCursor.m_Flags = 0;
			MeasureCursor.m_LineWidth = LineWidth;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				MeasureCursor.m_X += RealMsgPaddingTee;

				if(Line.m_Paused && g_Config.m_ClSpectatePrefix)
				{
					TextRender()->TextEx(&MeasureCursor, g_Config.m_ClSpecPrefix);
				}
				if(g_Config.m_ClWarList && g_Config.m_ClWarlistPrefixes && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId) && !Line.m_Whisper && !GameClient()->m_WarList.m_WarPlayers[Line.m_ClientId].m_IsMuted) // EClient
				{
					TextRender()->TextEx(&MeasureCursor, g_Config.m_ClWarlistPrefix);
				}
				else if(Line.m_Friend && g_Config.m_ClMessageFriend)
				{
					TextRender()->TextEx(&MeasureCursor, g_Config.m_ClFriendPrefix);
				}
			}

			TextRender()->TextEx(&MeasureCursor, aClientId);
			TextRender()->TextEx(&MeasureCursor, Line.m_aName);
			if(Line.m_TimesRepeated > 0)
			{
				TextRender()->TextEx(&MeasureCursor, aCount);
			}

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&MeasureCursor, ": ");
			}

			CTextCursor AppendCursor = MeasureCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = MeasureCursor.m_X;
				AppendCursor.m_LineWidth -= MeasureCursor.m_LongestLineWidth;
			}

			if(pTranslatedText)
			{
				// This is here so that the background has the correct size
				if(g_Config.m_ClChatColorParsing && Line.m_ClientId != SERVER_MSG)
					TextRender()->ColorParsing(pTranslatedText, &AppendCursor, ColorRGBA(1, 1, 1, 1), &Line.m_TextContainerIndex);
				else
					TextRender()->TextEx(&AppendCursor, pTranslatedText);

				if(pTranslatedLanguage)
				{
					TextRender()->TextEx(&AppendCursor, " [");
					TextRender()->TextEx(&AppendCursor, pTranslatedLanguage);
					TextRender()->TextEx(&AppendCursor, "]");
				}
			}
			else if(pTranslatedError)
			{
				TextRender()->TextEx(&AppendCursor, pText);
				TextRender()->TextEx(&AppendCursor, " [ERR]");
			}
			else
			{
				// This is here so that the background has the correct size
				if(g_Config.m_ClChatColorParsing && Line.m_ClientId != SERVER_MSG)
					TextRender()->ColorParsing(pText, &AppendCursor, ColorRGBA(1, 1, 1, 1), &Line.m_TextContainerIndex);
				else
					TextRender()->TextEx(&AppendCursor, pText);
			}

			Line.m_aYOffset[OffsetType] = AppendCursor.Height() + RealMsgPaddingY;
		}

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		m_LinesRendered++;

		// the position the text was created
		Line.m_TextYOffset = y + RealMsgPaddingY / 2.0f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		// reset the cursor
		CTextCursor LineCursor;
		LineCursor.SetPosition(vec2(TextBegin, Line.m_TextYOffset));
		LineCursor.m_FontSize = FontSize;
		LineCursor.m_LineWidth = LineWidth;

		std::string RawName;
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			LineCursor.m_X += RealMsgPaddingTee;

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId == SILENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSilentColor));
		else if(Line.m_ClientId == ECLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClECMessageColor));
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Team)
			NameColor = CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else if(Line.m_NameColor == TEAM_RED)
			NameColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
		else if(Line.m_NameColor == TEAM_BLUE)
			NameColor = ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
		else if(g_Config.m_ClWarList && g_Config.m_ClWarListChat && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId)) // TClient
			NameColor = GameClient()->m_WarList.GetPriorityColor(Line.m_ClientId);
		else if(Line.m_Friend && g_Config.m_ClChatFriendColor)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFriendColor));
		else if(Line.m_NameColor == TEAM_SPECTATORS)
			NameColor = ColorRGBA(0.75f, 0.5f, 0.75f, 1.0f);
		else if(Line.m_ClientId >= 0 && g_Config.m_ClChatTeamColors && GameClient()->m_Teams.Team(Line.m_ClientId))
			NameColor = GameClient()->GetDDTeamColor(GameClient()->m_Teams.Team(Line.m_ClientId), 0.75f);
		else
			NameColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);

		TextRender()->TextColor(NameColor);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aClientId);
		RawName += aClientId;
		// Message is from valid player
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			if(g_Config.m_ClSpectatePrefix && Line.m_Paused && !Line.m_Whisper)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSpecColor)));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, g_Config.m_ClSpecPrefix);
				RawName += g_Config.m_ClSpecPrefix;
			}

			if(g_Config.m_ClWarList && g_Config.m_ClWarlistPrefixes && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId) && !Line.m_Whisper) // TClient
			{
				TextRender()->TextColor(GameClient()->m_WarList.GetPriorityColor(Line.m_ClientId));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, g_Config.m_ClWarlistPrefix);
				RawName += g_Config.m_ClWarlistPrefix;
			}
			else if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFriendColor)).WithAlpha(1.0f));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, g_Config.m_ClFriendPrefix);
				RawName += g_Config.m_ClFriendPrefix;
			}
		}

		TextRender()->TextColor(NameColor);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, Line.m_aName);
		RawName += std::string(Line.m_aName);

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aCount);
			RawName += aCount;
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, ": ");
			RawName += ": ";
		}

		ColorRGBA Color;
		if(Line.m_CustomColor)
			Color = *Line.m_CustomColor;
		else if(Line.m_ClientId == SILENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSilentColor));
		else if(Line.m_ClientId == ECLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClECMessageColor));
		else if(Line.m_ClientId == SERVER_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Highlighted)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Line.m_Team)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else // regular message
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = LineCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LineCursor.m_X;
			AppendCursor.m_LineWidth -= LineCursor.m_LongestLineWidth;
			Line.m_StartX = LineCursor.m_X;
			Line.m_LineWidth = AppendCursor.m_LineWidth;
		}

		std::string RawMessage;

		if(pTranslatedText)
		{
			if(g_Config.m_ClChatColorParsing && Line.m_ClientId != SERVER_MSG)
				TextRender()->ColorParsing(pTranslatedText, &AppendCursor, Color, &Line.m_TextContainerIndex);
			else
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedText);
			RawMessage += TextRender()->RemoveColorCodes(pTranslatedText);

			if(pTranslatedLanguage)
			{
				std::string Lang = " [" + std::string(pTranslatedLanguage) + "]";
				ColorRGBA ColorLang = Color;
				ColorLang.r *= 0.8f;
				ColorLang.g *= 0.8f;
				ColorLang.b *= 0.8f;
				TextRender()->TextColor(ColorLang);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, Lang.c_str());
				RawMessage += Lang;
			}
		}
		else if(pTranslatedError)
		{
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);

			std::string Lang = " [ERR]";
			ColorRGBA ColorLang = Color;
			ColorLang.r *= 0.8f;
			ColorLang.g *= 0.8f;
			ColorLang.b *= 0.8f;
			TextRender()->TextColor(ColorLang);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, Lang.c_str());
			RawMessage += pText;
			RawMessage += Lang;
			if(g_Config.m_EcTranslateLogErrors)
				log_error("translate", "%s", pTranslatedError);
		}
		else
		{
			ColorizeLine(Line, AppendCursor);
			if(g_Config.m_ClChatColorParsing && Line.m_ClientId != SERVER_MSG)
				TextRender()->ColorParsing(pText, &AppendCursor, Color, &Line.m_TextContainerIndex);
			else
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			RawMessage += TextRender()->RemoveColorCodes(pText);
		}

		AppendCursor.m_vColorSplits.clear();

		float FullWidth = RealMsgPaddingX * 1.5f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			FullWidth += LineCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
		}
		else
		{
			FullWidth += std::max(LineCursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
		}
		Line.m_RenderWidth = FullWidth; // EClient: needed to detect the cursor hovering this message

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, y, FullWidth, Line.m_aYOffset[OffsetType], MessageRounding(), IGraphics::CORNER_ALL);
		}

		// EClient
		Line.m_RenderedName = RawName;
		Line.m_RenderedText = RawMessage;
		Line.m_RenderedOffsetType = OffsetType;

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

int CChat::GetLinesToSkipWhilePaused() const
{
	if(!IsScrollPaused() || m_BacklogCurLine != 0 || m_NewLineCounter <= 0)
		return 0;

	return std::min(m_NewLineCounter, std::max(0, NumInitializedLines() - 1));
}

int CChat::GetMaxBacklogCurLine() const
{
	return std::max(0, NumInitializedLines() - std::max(1, m_LinesRendered));
}

void CChat::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode != MODE_NONE)
	{
		SyncUiMouse(); // EClient
		Ui()->StartCheck();
		Ui()->Update(true);
	}

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				SendChat(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(Width, Height);

	// EClient
	CHudLayout &Layout = GameClient()->m_Hud.HudLayout();
	const CHudLayout::CScope LayoutScope(&Layout, EHudElement::CHAT);

	float x = 5.0f;
	float y = 300.0f - 20.0f * FontSize() / 6.0f;
	float ScaledFontSize = FontSize() * (8.0f / 6.0f);

	// EClient: where the lines start from, for the element rect reported further down
	const float ChatBottom = y;

	// EClient: cursor position in chat space, used for selection and hover detection. The layout
	// may have moved and scaled the chat away from the base screen, so the cursor is brought into
	// the chat's own coordinates rather than compared against them directly.
	const vec2 MousePos = Layout.ToElementSpace(EHudElement::CHAT,
		m_SelectorMouse / vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight()) * vec2(Width, Height));

	// EClient: the menus live on the ui screen, so anything the chat draws that they have to line
	// up with has to make the trip out of the chat's own coordinates and onto that screen
	const CUIRect *pUiScreen = Ui()->Screen();
	const vec2 UiScale = vec2(pUiScreen->w / Width, pUiScreen->h / Height);
	const auto ToUiRect = [&](vec2 Pos, vec2 Size) {
		const vec2 Base = Layout.ToBaseSpace(EHudElement::CHAT, Pos);
		const float Scale = Layout.ElementScale(EHudElement::CHAT);
		return CUIRect{Base.x * UiScale.x, Base.y * UiScale.y, Size.x * Scale * UiScale.x, Size.y * Scale * UiScale.y};
	};
	CUIRect MenuButtonRect = {0.0f, 0.0f, 0.0f, 0.0f};

	// EClient: right clicking a message opens its menu. Taken on the press rather than through the
	// ui, so that a message can be right clicked while another one's menu is still open.
	const bool RightClicked = m_Mode != MODE_NONE && Ui()->MouseButtonClicked(1) && !Ui()->IsPopupHovered();

	// Handle mouse selection for chat when chat mode is active
	if(m_Mode != MODE_NONE)
	{
		// Chat input area bounds (rough estimate - below the input line)
		const float ChatInputAreaY = y;

		// Use KeyIsPressed for mouse button state (works with UI mouse system)
		const bool MousePressed = Input()->KeyIsPressed(KEY_MOUSE_1);

		// Check if mouse is pressed (start selection) - only if above chat input (lower Y value)
		if(!m_Selecting && MousePressed && MousePos.y < ChatInputAreaY && !Ui()->IsPopupOpen())
		{
			m_Selecting = true;
			m_NewLineCounter = 0;
			m_SelectionMousePress = MousePos;
			m_SelectionMouseRelease = m_SelectionMousePress;
			m_HasSelection = false;
			m_SelectionText.clear();
		}

		// Update release position while selecting
		if(m_Selecting)
		{
			m_SelectionMouseRelease = MousePos;
		}

		// Check if mouse is released (end selection)
		if(m_Selecting && !MousePressed)
		{
			m_Selecting = false;
			// Keep selection state if we have a valid selection
		}

		// Clear selection if clicking in the input area
		if(MousePressed && MousePos.y >= ChatInputAreaY && m_HasSelection)
		{
			m_HasSelection = false;
			m_SelectionText.clear();
			m_NewLineCounter = 0;
		}
	}
	else
	{
		// Clear selection when chat mode is disabled
		if(m_Selecting || m_HasSelection)
		{
			m_Selecting = false;
			m_HasSelection = false;
			m_SelectionText.clear();
			m_NewLineCounter = 0;
		}
		m_HoveringMessage = false; // EClient
	}

	if(m_Mode != MODE_NONE)
	{
		// EClient: the button that opens the chat menu. It rides along with the input line instead
		// of being an element of its own, so the hud layout has nothing to say about it. Taller
		// than the line it sits next to, so it is centred on the text rather than hung off its top.
		const float MenuButtonSize = ScaledFontSize * MENU_BUTTON_SCALE;
		const float MenuButtonSpacing = ScaledFontSize / 3.0f;
		MenuButtonRect = ToUiRect(vec2(x, y + (ScaledFontSize - MenuButtonSize) / 2.0f), vec2(MenuButtonSize, MenuButtonSize));

		// render chat input
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(x + MenuButtonSize + MenuButtonSpacing, y));
		InputCursor.m_FontSize = ScaledFontSize;
		InputCursor.m_LineWidth = Width - 195.0f;

		TextRender()->TextColor(TextRender()->DefaultTextColor());

		if(m_Mode == MODE_ALL)
		{
			TextRender()->TextEx(&InputCursor, Localize("All"));
		}
		else if(m_Mode == MODE_TEAM)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor)));
			TextRender()->TextEx(&InputCursor, Localize("Team"));
		}
		else if(m_Mode == MODE_SILENT)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSilentColor)));
			TextRender()->TextEx(&InputCursor, Localize("Silent"));
		}
		else
		{
			TextRender()->TextEx(&InputCursor, Localize("Chat"));
		}

		TextRender()->TextEx(&InputCursor, ": ");

		const float MessageMaxWidth = InputCursor.m_LineWidth - (InputCursor.m_X - InputCursor.m_StartX);
		const CUIRect ClippingRect = {InputCursor.m_X, InputCursor.m_Y, MessageMaxWidth, 2.25f * InputCursor.m_FontSize};
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		// EClient: ClipEnable wants screen pixels, so unlike the drawing it cannot ride the screen
		// mapping the layout set up and has to be taken back to base coordinates by hand
		const vec2 ClipTopLeft = Layout.ToBaseSpace(EHudElement::CHAT, vec2(ClippingRect.x, ClippingRect.y));
		const float ClipScale = Layout.ElementScale(EHudElement::CHAT);
		Graphics()->ClipEnable((int)(ClipTopLeft.x * XScale), (int)(ClipTopLeft.y * YScale),
			(int)(ClippingRect.w * ClipScale * XScale), (int)(ClippingRect.h * ClipScale * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();

		m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
		const CUIRect InputCursorRect = {InputCursor.m_X, InputCursor.m_Y - ScrollOffset, 0.0f, 0.0f};
		const bool WasChanged = m_Input.WasChanged();
		const bool WasCursorChanged = m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		const STextBoundingBox BoundingBox = m_Input.Render(&InputCursorRect, InputCursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);

		Graphics()->ClipDisable();

		// Scroll up or down to keep the caret inside the clipping rect
		const float CaretPositionY = m_Input.GetCaretPosition().y - ScrollOffsetChange;
		if(CaretPositionY < ClippingRect.y)
			ScrollOffsetChange -= ClippingRect.y - CaretPositionY;
		else if(CaretPositionY + InputCursor.m_FontSize > ClippingRect.y + ClippingRect.h)
			ScrollOffsetChange += CaretPositionY + InputCursor.m_FontSize - (ClippingRect.y + ClippingRect.h);

		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, ClippingRect.h, BoundingBox.m_H);

		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollOffsetChange);

		const std::vector<CCommand> &vChatCommands = GameClient()->m_Bindchat.m_vChatCommands;
		const float HintStartX = InputCursor.m_X; // EClient

		// Autocompletion hint
		if(GameClient()->m_Bindchat.ValidPrefix(m_Input.GetString()[0]) && m_Input.GetString()[1] != '\0' && !vChatCommands.empty())
		{
			for(const auto &Command : vChatCommands)
			{
				if(str_startswith_nocase(Command.m_aName, m_Input.GetString()))
				{
					InputCursor.m_X = HintStartX + TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
					InputCursor.m_Y = m_Input.GetCaretPosition().y;
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
					TextRender()->TextEx(&InputCursor, Command.m_aName + str_length(m_Input.GetString()));
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					break;
				}
			}
		}

		// <EClient
		// Math expression hint
		char aMathSuggestion[64];
		if(MathSuggestion(aMathSuggestion, sizeof(aMathSuggestion)))
		{
			InputCursor.m_X = HintStartX + TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
			InputCursor.m_Y = m_Input.GetCaretPosition().y;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
			TextRender()->TextEx(&InputCursor, aMathSuggestion);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		// EClient>
	}

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
	{
		m_HoveringMessage = false; // EClient: no messages are rendered, so nothing can be hovered
		RenderChatUi(MenuButtonRect); // EClient
		return;
	}

	m_BacklogCurLine = std::clamp(m_BacklogCurLine, 0, GetMaxBacklogCurLine());

	y -= ScaledFontSize;

	OnPrepareLines(y);

	bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)

	int64_t Now = time();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	m_LinesRendered = 0;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	// For selection handling
	const bool IsSelecting = m_Mode != MODE_NONE && (m_Selecting || m_HasSelection);

	// While the view is paused, skip rendering new lines to keep it stable
	// Instead of adjusting mouse positions, we simply don't show the new messages until the pause ends
	const int LinesToSkipWhilePaused = GetLinesToSkipWhilePaused();
	// Only reset counter while the view is not paused
	if(!IsScrollPaused())
		m_NewLineCounter = 0;

	// EClient: the cursor pauses the chat while it rests on a message, recomputed below
	bool HoveringMessage = false;

	// Determine selection Y range
	float SelectionMinY = std::min(m_SelectionMousePress.y, m_SelectionMouseRelease.y);
	float SelectionMaxY = std::max(m_SelectionMousePress.y, m_SelectionMouseRelease.y);

	// Track lines for building selection text (we process from newest to oldest, so we need to reverse later)
	struct SLineSelectionInfo
	{
		int m_LineIndex;
		float m_Y;
		float m_Height;
		std::string m_Text;
		int m_SelectionStart;
		int m_SelectionEnd;
	};
	std::vector<SLineSelectionInfo> vSelectedLines;

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		// Skip new lines that arrived while the view is paused to keep it stable
		if(i < LinesToSkipWhilePaused)
			continue;

		// Adjust index for skipped lines when checking backlog
		const int AdjustedIndex = i - LinesToSkipWhilePaused;
		if(AdjustedIndex < m_BacklogCurLine)
			continue;

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit)
			break;

		++m_LinesRendered;

		float Blend = Now > Line.m_Time + 14 * time_freq() && !m_PrevShowChat ? 1.0f - (Now - Line.m_Time - 14 * time_freq()) / (2.0f * time_freq()) : 1.0f;

		const float LineTop = y;
		const float LineBottom = y + Line.m_aYOffset[OffsetType];

		const bool LineHovered = m_Mode != MODE_NONE &&
					 MousePos.x >= x && MousePos.x <= x + Line.m_RenderWidth &&
					 MousePos.y >= LineTop && MousePos.y <= LineBottom;
		if(LineHovered)
		{
			HoveringMessage = true;
			if(RightClicked)
			{
				OpenMessagePopup(Line, ((m_CurrentLine - i) + MAX_LINES) % MAX_LINES);
			}
		}

		// Draw backgrounds for messages in one batch
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				ColorRGBA BackgroundColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true)).WithMultipliedAlpha(Blend);
				if(LineHovered && !Ui()->IsPopupHovered()) // EClient
				{
					static constexpr float gs_MessageHoverHighlightAlpha = 0.38f;
					if(BackgroundColor.a <= 0.1f)
						BackgroundColor.a = 0.1f;
					if(BackgroundColor.a > 0.66f)
						BackgroundColor.a *= gs_MessageHoverHighlightAlpha;
					else
						BackgroundColor.a /= gs_MessageHoverHighlightAlpha;
				}

				Graphics()->SetColor(BackgroundColor);
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, 0, ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset));
			}
		}

		if(IsSelecting && LineBottom >= SelectionMinY && LineTop <= SelectionMaxY)
		{
			// This line overlaps with selection - store info for later rendering
			SLineSelectionInfo Info;
			Info.m_LineIndex = i;
			Info.m_Y = y;
			Info.m_Height = Line.m_aYOffset[OffsetType];
			// Text will be built in the selection loop below where we have all the same logic
			Info.m_Text = "";
			Info.m_SelectionStart = -1;
			Info.m_SelectionEnd = -1;

			vSelectedLines.push_back(Info);
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			if(!g_Config.m_ClChatOld && Line.m_pManagedTeeRenderInfo != nullptr)
			{
				CTeeRenderInfo &TeeRenderInfo = Line.m_pManagedTeeRenderInfo->TeeRenderInfo();
				const int TeeSize = MessageTeeSize();
				TeeRenderInfo.m_Size = TeeSize;

				float RowHeight = FontSize() + RealMsgPaddingY;
				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + (RealMsgPaddingX + TeeSize) / 2.0f, y + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, Blend);
			}

			const ColorRGBA TextColor = TextRender()->DefaultTextColor().WithMultipliedAlpha(Blend);
			const ColorRGBA TextOutlineColor = TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend);
			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, 0, (y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset);
		}
	}

	m_HoveringMessage = HoveringMessage; // EClient

	if(IsSelecting && !vSelectedLines.empty())
	{
		const int TeeSize = MessageTeeSize();
		float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

		float Begin = x;
		float TextBegin = Begin + RealMsgPaddingX / 2.0f;

		float LineWidth = (IsScoreBoardOpen ? std::max(85.0f, (FontSize() * 85.0f / 6.0f)) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

		m_SelectionText.clear();
		bool AnySelection = false;

		TextRender()->TextSelectionColor(TextRender()->DefaultTextSelectionColor());

		for(auto &Info : vSelectedLines)
		{
			CLine &Line = m_aLines[((m_CurrentLine - Info.m_LineIndex) + MAX_LINES) % MAX_LINES];

			CTextCursor LineCursor;
			LineCursor.SetPosition(vec2(TextBegin, Info.m_Y + RealMsgPaddingY / 2.0f));
			LineCursor.m_FontSize = FontSize();
			LineCursor.m_LineWidth = LineWidth;
			LineCursor.m_Flags = TEXTFLAG_RENDER;
			LineCursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_CALCULATE;
			LineCursor.m_PressMouse = m_SelectionMousePress;
			LineCursor.m_ReleaseMouse = m_SelectionMouseRelease;
			LineCursor.m_SelectionHeightFactor = 1.0f;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
				LineCursor.m_X += RealMsgPaddingTee;

			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				LineCursor.m_StartX = Line.m_StartX;
				LineCursor.m_LineWidth = Line.m_LineWidth;
			}

			std::string FullText = Line.m_RenderedName + Line.m_RenderedText;

			TextRender()->TextColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
			TextRender()->TextEx(&LineCursor, FullText.c_str(), -1);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			Info.m_Text = FullText;

			int SelStart = -1;
			int SelEnd = -1;

			int LineSelStart = std::min(LineCursor.m_SelectionStart, LineCursor.m_SelectionEnd);
			int LineSelEnd = std::max(LineCursor.m_SelectionStart, LineCursor.m_SelectionEnd);

			// Combine selections
			if(LineSelStart >= 0 && LineSelEnd > LineSelStart)
			{
				SelStart = LineSelStart;
				SelEnd = LineSelEnd;
			}

			if(SelStart >= 0 && SelEnd >= 0 && SelStart < SelEnd)
			{
				Info.m_SelectionStart = SelStart;
				Info.m_SelectionEnd = SelEnd;
			}
			else
			{
				Info.m_SelectionStart = -1;
				Info.m_SelectionEnd = -1;
			}
		}

		// Build selection text (lines are in reverse order, so reverse to get correct order)
		for(auto &vSelectedLine : std::ranges::reverse_view(vSelectedLines))
		{
			if(vSelectedLine.m_SelectionStart >= 0 && vSelectedLine.m_SelectionEnd >= 0 && vSelectedLine.m_SelectionStart != vSelectedLine.m_SelectionEnd)
			{
				AnySelection = true;
				if(!m_SelectionText.empty())
					m_SelectionText += "\n";

				// Extract selected portion of text (Info.m_Text contains name: message)
				const size_t OffStart = str_utf8_offset_chars_to_bytes(vSelectedLine.m_Text.c_str(), vSelectedLine.m_SelectionStart);
				const size_t OffEnd = str_utf8_offset_chars_to_bytes(vSelectedLine.m_Text.c_str(), vSelectedLine.m_SelectionEnd);
				if(OffEnd > OffStart && OffEnd <= vSelectedLine.m_Text.length())
					m_SelectionText += vSelectedLine.m_Text.substr(OffStart, OffEnd - OffStart);
			}
		}

		m_HasSelection = AnySelection;
		if(!AnySelection)
			m_SelectionText.clear();
	}
	else if(!m_Selecting)
	{
		// Clear selection when not selecting and no selection exists
		if(!m_HasSelection)
		{
			m_SelectionText.clear();
		}
	}

	// EClient: the box is the chat at its fullest, which is what it looks like with chat open and
	// nothing else in the way. HeightLimit and the wrap width both tighten while the scoreboard is
	// up or chat is closed, and following those would make the element resize itself underneath
	// whoever is trying to place it.
	const float FullHeightLimit = 50.0f;
	Layout.ReportNaturalRect(EHudElement::CHAT,
		vec2(x, FullHeightLimit), vec2((float)g_Config.m_ClChatWidth, ChatBottom - FullHeightLimit));

	RenderChatUi(MenuButtonRect); // EClient
}

// <EClient
// What CUi::DoPopupMenu takes off a popup's rect before handing the rest to the render function,
// its border plus its margin. Kept private over there, so the height a menu asks for has to know.
static constexpr float POPUP_PADDING = 5.0f;

static void RenderPopupIcon(CUi *pUi, const CUIRect &Rect, const char *pIcon, ColorRGBA Color)
{
	pUi->TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	pUi->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	pUi->TextRender()->TextColor(Color);
	pUi->DoLabel(&Rect, pIcon, POPUP_FONT_SIZE, TEXTALIGN_MC);
	pUi->TextRender()->TextColor(pUi->TextRender()->DefaultTextColor());
	pUi->TextRender()->SetRenderFlags(0);
	pUi->TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

float CChat::PopupHeight(int Entries)
{
	if(Entries <= 0)
		return POPUP_PADDING * 2.0f;
	return Entries * POPUP_ENTRY_HEIGHT + (Entries - 1) * POPUP_ENTRY_SPACING + POPUP_PADDING * 2.0f;
}

void CChat::SyncUiMouse()
{
	// The chat steers a cursor of its own while it is open, the ui knows nothing of it. Walking
	// the ui cursor onto it every frame is what lets the menus hit test against what the player
	// sees, without the chat having to give up the cursor it already tracks for text selection.
	const vec2 Delta = m_SelectorMouse - Ui()->UpdatedMousePos();
	if(Delta != vec2(0.0f, 0.0f))
		Ui()->OnCursorMove(Delta.x, Delta.y);
}

CChat::CLine *CChat::CMessagePopupContext::Line() const
{
	if(m_pChat == nullptr || m_LineIndex < 0 || m_LineIndex >= MAX_LINES)
		return nullptr;

	CLine *pLine = &m_pChat->m_aLines[m_LineIndex];
	// The chat holds still while a menu is open, but the slot can still be handed to a newer
	// message, and acting on the wrong one is worse than doing nothing
	if(!pLine->m_Initialized || pLine->m_Time != m_LineTime)
		return nullptr;
	return pLine;
}

void CChat::OpenMessagePopup(const CLine &Line, int LineIndex)
{
	Ui()->ClosePopupMenu(&m_ChatPopupContext, true);
	Ui()->ClosePopupMenu(&m_MessagePopupContext);

	m_MessagePopupContext.m_pChat = this;
	m_MessagePopupContext.m_LineIndex = LineIndex;
	m_MessagePopupContext.m_LineTime = Line.m_Time;
	m_MessagePopupContext.m_ClientId = Line.m_ClientId;
	m_MessagePopupContext.m_Text = Line.m_RenderedName + Line.m_RenderedText;
	m_MessagePopupContext.m_aLanguage[0] = '\0';
	if(Line.m_pTranslateResponse && !Line.m_pTranslateResponse->m_Error)
		str_copy(m_MessagePopupContext.m_aLanguage, Line.m_pTranslateResponse->m_Language);

	bool IsLocal = false;
	for(const int LocalId : GameClient()->m_aLocalIds)
		IsLocal |= LocalId >= 0 && LocalId == Line.m_ClientId;

	m_MessagePopupContext.m_ShowFriend = !IsLocal && Line.m_ClientId >= 0 && GameClient()->m_aClients[Line.m_ClientId].m_Active;
	m_MessagePopupContext.m_ShowTranslate = !IsLocal && Line.m_ClientId >= SERVER_MSG && Line.m_aText[0] != '\0';
	m_MessagePopupContext.m_ShowLanguage = m_MessagePopupContext.m_aLanguage[0] != '\0';

	const int Entries = 1 + (m_MessagePopupContext.m_ShowFriend ? 1 : 0) +
			    (m_MessagePopupContext.m_ShowTranslate ? 1 : 0) +
			    (m_MessagePopupContext.m_ShowLanguage ? 2 : 0);
	Ui()->DoPopupMenu(&m_MessagePopupContext, Ui()->MouseX(), Ui()->MouseY(), POPUP_WIDTH, PopupHeight(Entries),
		&m_MessagePopupContext, CMessagePopupContext::Render);
}

void CChat::OpenChatPopup(const CUIRect &ButtonRect)
{
	Ui()->ClosePopupMenu(&m_MessagePopupContext);
	Ui()->ClosePopupMenu(&m_ChatPopupContext, true);

	m_ChatPopupContext.m_pChat = this;
	// Anchored on the button's top edge rather than below it. The input line sits low enough that
	// the menu is always folded upwards, and this way it stops at the button instead of covering
	// it.
	Ui()->DoPopupMenu(&m_ChatPopupContext, ButtonRect.x, ButtonRect.y, POPUP_WIDTH, PopupHeight(5),
		&m_ChatPopupContext, CChatPopupContext::Render);
}

void CChat::OpenBackendPopup(const CUIRect &FromRect)
{
	Ui()->ClosePopupMenu(&m_BackendPopupContext);
	m_BackendPopupContext.m_pChat = this;
	// Opened alongside the entry it came from rather than on top of it
	Ui()->DoPopupMenu(&m_BackendPopupContext, FromRect.x + FromRect.w, FromRect.y, POPUP_WIDTH,
		PopupHeight(CTranslate::NumBackends()), &m_BackendPopupContext, CBackendPopupContext::Render);
}

void CChat::OpenLanguagePopup(const CUIRect &FromRect)
{
	Ui()->ClosePopupMenu(&m_LanguagePopupContext);
	m_LanguagePopupContext.m_pChat = this;
	m_LanguagePopupContext.m_vCodes.clear();
	m_LanguagePopupContext.m_vNames.clear();

	for(int i = 0; i < CTranslate::NumLanguages(); i++)
	{
		m_LanguagePopupContext.m_vCodes.emplace_back(CTranslate::LanguageCode(i));
		m_LanguagePopupContext.m_vNames.emplace_back(CTranslate::LanguageName(i));
	}

	// Whatever the lists hold that the table does not still has to be reachable, or a code typed
	// into the config by hand could never be taken back out from here
	const char *apLists[] = {g_Config.m_EcTranslateLanguageWhitelist, g_Config.m_EcTranslateLanguageBlacklist};
	for(const char *pList : apLists)
	{
		char aToken[16];
		char aCode[16];
		while((pList = str_next_token(pList, ", ", aToken, sizeof(aToken))) != nullptr)
		{
			CTranslate::NormalizeLanguage(aToken, aCode, sizeof(aCode));
			if(aCode[0] == '\0')
				continue;
			if(std::find(m_LanguagePopupContext.m_vCodes.begin(), m_LanguagePopupContext.m_vCodes.end(), aCode) != m_LanguagePopupContext.m_vCodes.end())
				continue;
			m_LanguagePopupContext.m_vCodes.emplace_back(aCode);
			m_LanguagePopupContext.m_vNames.emplace_back("");
		}
	}

	// Sized once, here, because the ui addresses a button by where it lives
	m_LanguagePopupContext.m_vWhitelistButtons.clear();
	m_LanguagePopupContext.m_vBlacklistButtons.clear();
	m_LanguagePopupContext.m_vWhitelistButtons.resize(m_LanguagePopupContext.m_vCodes.size());
	m_LanguagePopupContext.m_vBlacklistButtons.resize(m_LanguagePopupContext.m_vCodes.size());

	const float Height = std::min(PopupHeight((int)m_LanguagePopupContext.m_vCodes.size()), POPUP_LANGUAGE_MAX_HEIGHT);
	Ui()->DoPopupMenu(&m_LanguagePopupContext, FromRect.x + FromRect.w, FromRect.y, POPUP_LANGUAGE_WIDTH, Height,
		&m_LanguagePopupContext, CLanguagePopupContext::Render);
}

bool CChat::CloseTopPopup()
{
	// Innermost first, the same order the ui itself would take them down in
	if(Ui()->IsPopupOpen(&m_LanguagePopupContext))
		Ui()->ClosePopupMenu(&m_LanguagePopupContext);
	else if(Ui()->IsPopupOpen(&m_BackendPopupContext))
		Ui()->ClosePopupMenu(&m_BackendPopupContext);
	else if(Ui()->IsPopupOpen(&m_MessagePopupContext))
		Ui()->ClosePopupMenu(&m_MessagePopupContext);
	else if(Ui()->IsPopupOpen(&m_ChatPopupContext))
		Ui()->ClosePopupMenu(&m_ChatPopupContext, true);
	else
		return false;
	return true;
}

void CChat::RenderChatUi(const CUIRect &MenuButtonRect)
{
	if(m_Mode == MODE_NONE)
		return;

	Ui()->MapScreen();

	if(MenuButtonRect.h > 0.0f)
	{
		// CUi::DoButton_FontIcon would do all of this, but it rounds its corners off harder than
		// this button wants, and the amount is not something it takes
		const bool Open = Ui()->IsPopupOpen(&m_ChatPopupContext);
		MenuButtonRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, (Open ? 0.5f : 0.25f) * Ui()->ButtonColorMul(&m_ChatMenuButton)),
			IGraphics::CORNER_ALL, MENU_BUTTON_ROUNDING);

		CUIRect Label;
		MenuButtonRect.HMargin(MenuButtonRect.h / 6.0f, &Label);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		Ui()->DoLabel(&Label, FontIcon::LIST_UL, Label.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		if(Ui()->DoButtonLogic(&m_ChatMenuButton, 0, &MenuButtonRect, BUTTONFLAG_LEFT))
			OpenChatPopup(MenuButtonRect);
	}

	Ui()->RenderPopupMenus();

	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pUiScreen = Ui()->Screen();
	RenderTools()->RenderCursor(m_SelectorMouse / WindowSize * vec2(pUiScreen->w, pUiScreen->h), 24.0f);
	Ui()->FinishCheck();
}

std::string CChat::ChatText() const
{
	std::string Result;
	for(int i = NumInitializedLines() - 1; i >= 0; i--)
	{
		const CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		// A line only gains its rendered form once it has been drawn, one that never was falls
		// back to the parts it was built from
		std::string Text = Line.m_RenderedName + Line.m_RenderedText;
		if(Text.empty())
		{
			if(Line.m_aName[0] != '\0')
				Text = std::string(Line.m_aName) + ": ";
			Text += Line.m_aText;
		}
		if(Text.empty())
			continue;
		if(!Result.empty())
			Result += "\n";
		Result += Text;
	}
	return Result;
}

CUi::EPopupMenuFunctionResult CChat::CMessagePopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CMessagePopupContext *pPopup = static_cast<CMessagePopupContext *>(pContext);
	CChat *pChat = pPopup->m_pChat;
	CUi *pUi = pChat->Ui();

	CUi::EPopupMenuFunctionResult Result = CUi::POPUP_KEEP_OPEN;
	CUIRect Slot;
	bool First = true;
	const auto NextSlot = [&]() {
		if(!First)
			View.HSplitTop(POPUP_ENTRY_SPACING, nullptr, &View);
		First = false;
		View.HSplitTop(POPUP_ENTRY_HEIGHT, &Slot, &View);
		return &Slot;
	};

	if(pPopup->m_ShowFriend)
	{
		const CGameClient::CClientData &Client = pChat->GameClient()->m_aClients[pPopup->m_ClientId];
		if(pUi->DoButton_PopupMenu(&pPopup->m_FriendButton, Client.m_Friend ? Localize("Remove friend") : Localize("Add friend"),
			   NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true, Client.m_Active))
		{
			if(Client.m_Friend)
				pChat->GameClient()->Friends()->RemoveFriend(Client.m_aName, Client.m_aClan);
			else
				pChat->GameClient()->Friends()->AddFriend(Client.m_aName, Client.m_aClan);
			Result = CUi::POPUP_CLOSE_CURRENT;
		}
	}

	if(pUi->DoButton_PopupMenu(&pPopup->m_CopyButton, Localize("Copy message"), NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		pChat->Input()->SetClipboardText(pPopup->m_Text.c_str());
		Result = CUi::POPUP_CLOSE_CURRENT;
	}

	if(pPopup->m_ShowTranslate)
	{
		CLine *pLine = pPopup->Line();
		if(pUi->DoButton_PopupMenu(&pPopup->m_TranslateButton, Localize("Translate"), NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true, pLine != nullptr) &&
			pLine != nullptr)
		{
			pChat->GameClient()->m_Translate.Translate(*pLine, true);
			Result = CUi::POPUP_CLOSE_CURRENT;
		}
	}

	if(pPopup->m_ShowLanguage)
	{
		char aBuf[64];

		const bool Whitelisted = CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageWhitelist, pPopup->m_aLanguage);
		str_format(aBuf, sizeof(aBuf), Whitelisted ? Localize("Unwhitelist %s") : Localize("Whitelist %s"), pPopup->m_aLanguage);
		if(pUi->DoButton_PopupMenu(&pPopup->m_WhitelistButton, aBuf, NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
		{
			CTranslate::SetLanguageInList(g_Config.m_EcTranslateLanguageWhitelist, sizeof(g_Config.m_EcTranslateLanguageWhitelist), pPopup->m_aLanguage, !Whitelisted);
		}

		const bool Blacklisted = CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageBlacklist, pPopup->m_aLanguage);
		str_format(aBuf, sizeof(aBuf), Blacklisted ? Localize("Unblacklist %s") : Localize("Blacklist %s"), pPopup->m_aLanguage);
		if(pUi->DoButton_PopupMenu(&pPopup->m_BlacklistButton, aBuf, NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
		{
			CTranslate::SetLanguageInList(g_Config.m_EcTranslateLanguageBlacklist, sizeof(g_Config.m_EcTranslateLanguageBlacklist), pPopup->m_aLanguage, !Blacklisted);
		}
	}

	return Result;
}

CUi::EPopupMenuFunctionResult CChat::CBackendPopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CBackendPopupContext *pPopup = static_cast<CBackendPopupContext *>(pContext);
	CUi *pUi = pPopup->m_pChat->Ui();

	static_assert((int)ETranslateBackend::NUM <= MAX_BACKENDS, "CBackendPopupContext has no button for every backend");

	CUi::EPopupMenuFunctionResult Result = CUi::POPUP_KEEP_OPEN;
	CUIRect Slot;
	for(int i = 0; i < CTranslate::NumBackends(); i++)
	{
		if(i != 0)
			View.HSplitTop(POPUP_ENTRY_SPACING, nullptr, &View);
		View.HSplitTop(POPUP_ENTRY_HEIGHT, &Slot, &View);

		const int Value = CTranslate::BackendConfigValue(i);
		const ETranslateBackend Backend = (ETranslateBackend)Value;
		const bool Current = g_Config.m_EcTranslateBackend == Value;
		// The one in use is drawn filled in rather than only on hover, so the list says which it is
		if(pUi->DoButton_PopupMenu(&pPopup->m_aBackendButtons[i], CTranslate::BackendName(Backend),
			   &Slot, POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, !Current))
		{
			g_Config.m_EcTranslateBackend = Value;
			Result = CUi::POPUP_CLOSE_CURRENT;
		}

		// What the backend still wants before it can do anything, marked on the right of its row.
		// Google asks for nothing and so carries no mark, which is the whole point of showing them.
		const CTranslate::EBackendRequirement Requirement = CTranslate::BackendRequirement(Backend);
		const char *pTooltip = Localize("Works as is, nothing to set up");
		if(Requirement != CTranslate::EBackendRequirement::NONE)
		{
			const bool Ready = CTranslate::BackendReady(Backend);
			const bool NeedsKey = Requirement == CTranslate::EBackendRequirement::API_KEY;
			if(NeedsKey)
				pTooltip = Ready ? Localize("Uses the api key from ec_translate_key") : Localize("Needs an api key, set ec_translate_key");
			else
				pTooltip = Ready ? Localize("Uses the server from ec_translate_endpoint") : Localize("Needs a server of your own, set ec_translate_endpoint. Requests go to localhost:5000 until you do");

			CUIRect Icon;
			Slot.VSplitRight(POPUP_ENTRY_HEIGHT, nullptr, &Icon);
			RenderPopupIcon(pUi, Icon, NeedsKey ? FontIcon::KEY : FontIcon::NETWORK_WIRED,
				Ready ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f) : ColorRGBA(1.0f, 0.75f, 0.2f, 0.9f));
		}
		pPopup->m_pChat->GameClient()->m_Tooltips.DoToolTip(&pPopup->m_aBackendButtons[i], &Slot, pTooltip, POPUP_LANGUAGE_WIDTH);
	}

	return Result;
}

CUi::EPopupMenuFunctionResult CChat::CLanguagePopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CLanguagePopupContext *pPopup = static_cast<CLanguagePopupContext *>(pContext);
	CUi *pUi = pPopup->m_pChat->Ui();

	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarThickness = 8.0f;
	ScrollParams.m_ScrollbarMargin = POPUP_ENTRY_SPACING;
	ScrollParams.m_ScrollbarNoOuterMargin = true;
	ScrollParams.m_ScrollUnit = 3 * (POPUP_ENTRY_HEIGHT + POPUP_ENTRY_SPACING);
	pPopup->m_ScrollRegion.Begin(&View, &ScrollParams);

	const ColorRGBA WhitelistColor = ColorRGBA(0.4f, 0.9f, 0.4f, 0.75f);
	const ColorRGBA BlacklistColor = ColorRGBA(0.95f, 0.35f, 0.35f, 0.75f);

	for(size_t i = 0; i < pPopup->m_vCodes.size(); ++i)
	{
		CUIRect Slot;
		if(i != 0)
			View.HSplitTop(POPUP_ENTRY_SPACING, nullptr, &View);
		View.HSplitTop(POPUP_ENTRY_HEIGHT, &Slot, &View);
		if(!pPopup->m_ScrollRegion.AddRect(Slot))
			continue;

		const char *pCode = pPopup->m_vCodes[i].c_str();

		CUIRect Whitelist, Blacklist, Label;
		Slot.VSplitRight(POPUP_ENTRY_HEIGHT, &Slot, &Blacklist);
		Slot.VSplitRight(POPUP_ENTRY_SPACING, &Slot, nullptr);
		Slot.VSplitRight(POPUP_ENTRY_HEIGHT, &Label, &Whitelist);

		// A code the table does not name is shown on its own, there is nothing to put beside it
		char aBuf[64];
		if(pPopup->m_vNames[i].empty())
			str_copy(aBuf, pCode);
		else
			str_format(aBuf, sizeof(aBuf), "%s (%s)", pPopup->m_vNames[i].c_str(), pCode);
		pUi->DoLabel(&Label, aBuf, POPUP_FONT_SIZE, TEXTALIGN_ML);

		const bool Whitelisted = CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageWhitelist, pCode);
		if(pUi->DoButton_FontIcon(&pPopup->m_vWhitelistButtons[i], FontIcon::STAR, Whitelisted, &Whitelist, BUTTONFLAG_LEFT,
			   IGraphics::CORNER_ALL, true, Whitelisted ? std::optional(WhitelistColor.WithMultipliedAlpha(pUi->ButtonColorMul(&pPopup->m_vWhitelistButtons[i]))) : std::nullopt))
		{
			CTranslate::SetLanguageInList(g_Config.m_EcTranslateLanguageWhitelist, sizeof(g_Config.m_EcTranslateLanguageWhitelist), pCode, !Whitelisted);
		}
		pPopup->m_pChat->GameClient()->m_Tooltips.DoToolTip(&pPopup->m_vWhitelistButtons[i], &Whitelist, Localize("Auto translate this language"));

		const bool Blacklisted = CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageBlacklist, pCode);
		if(pUi->DoButton_FontIcon(&pPopup->m_vBlacklistButtons[i], FontIcon::BAN, Blacklisted, &Blacklist, BUTTONFLAG_LEFT,
			   IGraphics::CORNER_ALL, true, Blacklisted ? std::optional(BlacklistColor.WithMultipliedAlpha(pUi->ButtonColorMul(&pPopup->m_vBlacklistButtons[i]))) : std::nullopt))
		{
			CTranslate::SetLanguageInList(g_Config.m_EcTranslateLanguageBlacklist, sizeof(g_Config.m_EcTranslateLanguageBlacklist), pCode, !Blacklisted);
		}
		pPopup->m_pChat->GameClient()->m_Tooltips.DoToolTip(&pPopup->m_vBlacklistButtons[i], &Blacklist, Localize("Never auto translate this language"));
	}

	pPopup->m_ScrollRegion.End();

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CChat::CChatPopupContext::Render(void *pContext, CUIRect View, bool Active)
{
	CChatPopupContext *pPopup = static_cast<CChatPopupContext *>(pContext);
	CChat *pChat = pPopup->m_pChat;
	CUi *pUi = pChat->Ui();

	CUi::EPopupMenuFunctionResult Result = CUi::POPUP_KEEP_OPEN;
	CUIRect Slot;
	bool First = true;
	const auto NextSlot = [&]() {
		if(!First)
			View.HSplitTop(POPUP_ENTRY_SPACING, nullptr, &View);
		First = false;
		View.HSplitTop(POPUP_ENTRY_HEIGHT, &Slot, &View);
		return &Slot;
	};

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Auto translate"), g_Config.m_EcTranslateAuto ? Localize("On") : Localize("Off"));
	// Left open, the label it flips is the only thing that reports what happened
	if(pUi->DoButton_PopupMenu(&pPopup->m_AutoTranslateButton, aBuf, NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		g_Config.m_EcTranslateAuto ^= 1;
	}

	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Backend"), CTranslate::BackendName(CTranslate::Backend()));
	if(pUi->DoButton_PopupMenu(&pPopup->m_BackendButton, aBuf, NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		pChat->OpenBackendPopup(Slot);
	}

	if(pUi->DoButton_PopupMenu(&pPopup->m_LanguagesButton, Localize("Languages"), NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		pChat->OpenLanguagePopup(Slot);
	}

	if(pUi->DoButton_PopupMenu(&pPopup->m_CopyChatButton, Localize("Copy chat"), NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		const std::string Text = pChat->ChatText();
		if(!Text.empty())
			pChat->Input()->SetClipboardText(Text.c_str());
		Result = CUi::POPUP_CLOSE_CURRENT;
	}

	if(pUi->DoButton_PopupMenu(&pPopup->m_ClearChatButton, Localize("Clear chat"), NextSlot(), POPUP_FONT_SIZE, TEXTALIGN_ML, POPUP_ENTRY_PADDING, true))
	{
		pChat->ClearLines();
		Result = CUi::POPUP_CLOSE_CURRENT;
	}

	return Result;
}
// EClient>

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

void CChat::SendChat(int Team, const char *pLine, int Conn)
{
	// EClient: every outgoing line funnels through here, which is what catches the ones that never
	// went through the chat box -- "say /r" from a bind or the console above all
	if(GameClient()->m_LocalPractice.OnChatCommand(pLine))
		return;

	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	if(!g_Config.m_ClSendDotsChat && pLine[0] == '.')
		return;

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		if(Conn < 0)
			Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		else
			Client()->SendPackMsg(Conn, &Msg7, MSGFLAG_VITAL, true);
		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	if(Conn < 0)
		Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
	else
		Client()->SendPackMsg(Conn, &Msg, MSGFLAG_VITAL);
}

void CChat::SendChatQueued(const char *pLine)
{
	// EClient: caught before it is queued, so a practice command does not sit waiting on the chat
	// flood timer to be run
	if(GameClient()->m_LocalPractice.OnChatCommand(pLine))
		return;

	if(!pLine || str_length(pLine) < 1)
		return;

	bool AddEntry = false;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(m_Mode == MODE_ALL ? 0 : 1, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		AddHistoryEntry(pLine);
	}
}

// EClient
bool CChat::ChatDetection(int ClientId, int Team, const char *pLine)
{
	if(Client()->State() == CClient::STATE_DEMOPLAYBACK)
		return false;

	auto FindName = [&](int Idx = 0) -> std::string {
		if(Idx < 0)
			return "";

		const char *pSearch = pLine;
		for(int i = 0; i <= Idx; ++i)
		{
			const char *pFindName = str_find_nocase(pSearch, "'");
			if(!pFindName)
				return "";

			const char *pNameEnd = str_find_nocase(pFindName + 1, "'");
			if(!pNameEnd || pNameEnd <= pFindName + 1)
				return "";

			if(i == Idx)
				return std::string(pFindName + 1, pNameEnd);

			pSearch = pNameEnd + 1;
		}

		return "";
	};

	if(ClientId == SERVER_MSG)
	{
		if(g_Config.m_ClAutoAddOnNameChange)
		{
			if(str_find_nocase(pLine, "changed name to"))
			{
				std::string NameBefore = FindName(0);
				std::string NameAfter = FindName(1);

				int PlayerCid = GameClient()->GetClientId(NameBefore.c_str());

				if(PlayerCid >= 0)
				{
					const CWarDataCache Cache = GameClient()->m_WarList.GetWarData(PlayerCid);
					const CWarEntry *ExistingEntry = GameClient()->m_WarList.FindWarEntryWithName(NameAfter.c_str());
					const CWarEntry *OldEntry = GameClient()->m_WarList.FindWarEntryWithName(NameBefore.c_str());

					if(ExistingEntry && OldEntry && ExistingEntry->m_pWarType == OldEntry->m_pWarType && str_comp(ExistingEntry->m_aName, NameAfter.c_str()) == 0)
						return false; // Already exists with the new name

					char aBuf[128];
					char aReason[128] = "";
					str_copy(aReason, NameBefore.c_str());
					if(OldEntry && OldEntry->m_aReason[0] != '\0')
						str_copy(aReason, OldEntry->m_aReason);

					if(ExistingEntry && ExistingEntry->m_pWarType->m_Index == 2)
					{
						str_format(aBuf, sizeof(aBuf), "'%s' changed their name to a Teammates ['%s']", NameBefore.c_str(), NameAfter.c_str());
						if(g_Config.m_ClAutoAddOnNameChange == 2)
							GameClient()->ClientMessage(aBuf);
					}
					// Skip Wartype None
					for(size_t WarlistType = 1; WarlistType < GameClient()->m_WarList.m_WarTypes.size(); ++WarlistType)
					{
						if(IsFlagSet(g_Config.m_ClWarlistAutoAddFlags, WarlistType))
							continue;
						const char *pWarName = GameClient()->m_WarList.m_WarTypes[WarlistType]->m_aWarName;

						if(Cache.m_WarGroupMatches[WarlistType])
						{
							GameClient()->m_WarList.AddWarEntry(NameAfter.c_str(), "", aReason, pWarName, true);
							str_format(aBuf, sizeof(aBuf), "Auto Added \"%s\" to Temp '%s' list", NameAfter.c_str(), pWarName);
							if(g_Config.m_ClAutoAddOnNameChange == 2)
								GameClient()->ClientMessage(aBuf);
						}
					}
					if(Cache.m_IsMuted)
					{
						GameClient()->m_WarList.AddMute(NameAfter.c_str(), true, true);
						str_format(aBuf, sizeof(aBuf), "Auto Added \"%s\" to Temp Mute list", NameAfter.c_str());
						if(g_Config.m_ClAutoAddOnNameChange == 2)
							GameClient()->ClientMessage(aBuf);
					}
				}
			}
		}

		if(g_Config.m_ClAutoJoinTeam && g_Config.m_ClAutoJoinTeamName[0] != '\0')
		{
			std::string Name = FindName();
			if(str_find_nocase(pLine, "joined team "))
			{
				if(!str_comp(Name.c_str(), g_Config.m_ClAutoJoinTeamName))
				{
					char aBuf[2048] = "/Join ";
					str_append(aBuf, Name.c_str());
					GameClient()->m_Chat.SendChat(0, aBuf);
					char Joined[2048] = "Auto Joined ";
					str_append(Joined, Name.c_str());

					GameClient()->ClientMessage(Joined);
				}
			}
		}

		if(g_Config.m_ClNotifyOnJoin && g_Config.m_ClAutoNotifyName[0] != '\0')
		{
			std::string Name = FindName();
			if(str_find_nocase(pLine, "entered and joined the game"))
			{
				if(!str_comp(Name.c_str(), g_Config.m_ClAutoNotifyName))
				{
					GameClient()->ClientMessage(g_Config.m_ClAutoNotifyMsg);
					GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CTF_CAPTURE, 0.3f);
				}
			}
		}

		if(g_Config.m_ClAntiSpawnBlock)
		{
			// anti spawn block runs on both connections, so hide the messages of either of them
			for(const char *pName : {g_Config.m_PlayerName, g_Config.m_ClDummyName})
			{
				char aBuf[255];
				str_format(aBuf, sizeof(aBuf), "'%s' joined team", pName);
				if(str_find_nocase(pLine, aBuf))
				{
					return true;
				}
				str_format(aBuf, sizeof(aBuf), "'%s' locked your team. After the race starts, killing will kill everyone in your team.", pName);
				if(str_find_nocase(pLine, aBuf))
				{
					return true;
				}
			}
		}
	}
	else if(ClientId >= 0) // Player Message
	{
		if(g_Config.m_ClDismissAdBots > 0 && !GameClient()->m_aClients[ClientId].m_Friend)
		{
			bool AdBotFound = false;

			// generic message
			if(str_find_nocase(pLine, "bro, check out this client") && Team == TEAM_WHISPER_RECV) // whisper advertising
				AdBotFound = true;

			if(str_find_nocase(pLine, "you could do better") && str_find_nocase(pLine, "Not without"))
			{
				// try to not remove their message if they are just trying to be funny
				if(!str_find_nocase(pLine, "github.com") && !str_find_nocase(pLine, "ddnet") &&
					!str_find_nocase(pLine, "tater") && !str_find_nocase(pLine, "tclient") && !str_find_nocase(pLine, "t-client") && !str_find_nocase(pLine, "tclient.app") &&
					!str_find_nocase(pLine, "aiodob") && !str_find_nocase(pLine, "a-client") && !str_find(pLine, "A Client") && !str_find(pLine, "A client") &&
					!str_find_nocase(pLine, "entity") && !str_find_nocase(pLine, "EClient") && !str_find_nocase(pLine, "eclient") &&
					!str_find_nocase(pLine, "chillerbot") && !str_find_nocase(pLine, "cactus"))
					AdBotFound = true;
				if(str_find(pLine, " ")) // This is the little white space it uses between some letters
					AdBotFound = true;
			}

			if(AdBotFound == true)
			{
				// Forwarding the message
				if(str_find_nocase(pLine, "← "))
					return false;

				char Text[265] = "";
				str_format(Text, sizeof(Text), "← %s: %s", GameClient()->m_aClients[ClientId].m_aName, pLine);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whisper", Text, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor)));

				// Chat Response
				if(g_Config.m_ClDismissAdBots == 1)
					return true; // just dont show their messages
			}
		}
	}
	return false;
}

void CChat::ColorizeLine(const CLine &Line, CTextCursor &Cursor)
{
	if(!g_Config.m_ClWarListColorJoinLeave)
		return;

	const char *pLine = Line.m_aText;

	int ClientId = Line.m_ClientId;
	if(ClientId != SERVER_MSG)
		return;

	if(!str_find_nocase(pLine, "entered and joined the game") && !str_find_nocase(pLine, "has left the game"))
		return;

	const char *pNameBeginQuote = str_find(pLine, "'");
	if(!pNameBeginQuote)
		return;

	const char *pNameStart = pNameBeginQuote + 1;
	const char *pNameEndQuote = str_find(pNameStart, "'");
	if(!pNameEndQuote || pNameEndQuote <= pNameStart)
		return;

	std::string Name(pNameStart, pNameEndQuote);
	if(Name.empty())
		return;

	const CWarEntry *ExistingEntry = GameClient()->m_WarList.FindWarEntryWithName(Name.c_str());
	if(!ExistingEntry || ExistingEntry->m_pWarType == nullptr)
		return;

	const int SplitIndex = Cursor.m_CharCount + (int)(pNameStart - 1 - pLine);
	const int SplitLength = (int)(pNameEndQuote + 2 - pNameStart);
	if(SplitLength <= 0)
		return;

	Cursor.m_vColorSplits.emplace_back(SplitIndex, SplitLength, ExistingEntry->m_pWarType->m_Color);
}

void CChat::ConSetChatInput(IConsole::IResult *pResult, void *pUserData)
{
	CChat *pChat = (CChat *)pUserData;
	pChat->EnableMode(TEAM_FLOCK);
	pChat->m_Input.Set(pResult->GetString(0));
}

void CChat::ConSayQueued(IConsole::IResult *pResult, void *pUserData)
{
	CChat *pChat = (CChat *)pUserData;
	pChat->m_Mode = MODE_ALL;
	pChat->SendChatQueued(pResult->GetString(0));
	pChat->m_Mode = MODE_NONE;
}

void CChat::AddHistoryEntry(const char *pLine)
{
	const int Length = str_length(pLine);
	CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
	pEntry->m_Team = m_Mode == MODE_ALL ? 0 : 1;
	str_copy(pEntry->m_aText, pLine, Length + 1);
}
