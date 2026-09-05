/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CHAT_H
#define GAME_CLIENT_COMPONENTS_CHAT_H

#include <base/str.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/ringbuffer.h>

#include <generated/protocol7.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <string>
#include <vector>

// TClient
class CTranslateResponse
{
public:
	bool m_Error = false;
	char m_Text[1024] = "";
	char m_Language[16] = "";

	bool m_Auto = false;
};

constexpr auto SAVES_FILE = "ddnet-saves.txt";

class CHud;

enum
{
	MAX_LINES = 384,
	MAX_LINE_LENGTH = 256
};

class CChat : public CComponent
{
	static constexpr float CHAT_HEIGHT_FULL = 200.0f;
	static constexpr float CHAT_HEIGHT_MIN = 50.0f;
	static constexpr float CHAT_FONTSIZE_WIDTH_RATIO = 2.5f;

	CLineInputBuffered<MAX_CHAT_LENGTH> m_Input;
	class CLine
	{
	public:
		CLine();
		void Reset(CChat &This);

		bool m_Initialized;
		int64_t m_Time;
		float m_aYOffset[2];
		int m_ClientId;
		int m_TeamNumber;
		bool m_Team;
		bool m_Whisper;
		int m_NameColor;
		char m_aName[64];
		char m_aText[MAX_CHAT_LENGTH];
		bool m_Friend;

		bool m_Paused;

		bool m_Highlighted;
		std::optional<ColorRGBA> m_CustomColor;

		STextContainerIndex m_TextContainerIndex;
		int m_QuadContainerIndex;
		int m_RenderedOffsetType;

		std::shared_ptr<CManagedTeeRenderInfo> m_pManagedTeeRenderInfo;

		float m_TextYOffset;

		int m_TimesRepeated;

		std::shared_ptr<CTranslateResponse> m_pTranslateResponse;

		// EClient
		std::string m_RenderedName;
		std::string m_RenderedText;
		float m_StartX;
		float m_LineWidth;
		float m_RenderWidth; // width of the rendered message background, used for hover detection
	};

	bool LineShouldHighlight(const char *pLine, const char *pName);

	bool m_PrevScoreBoardShowed;
	bool m_PrevShowChat;
	int m_BacklogCurLine;
	int m_LinesRendered;
	vec2 m_SelectorMouse;

	// Selection state for copying from chat
	bool m_Selecting;
	vec2 m_SelectionMousePress;
	vec2 m_SelectionMouseRelease;
	bool m_HasSelection;
	std::string m_SelectionText;
	int m_NewLineCounter; // Track new lines while the view is paused to keep it stable
	bool m_HoveringMessage; // EClient: cursor rests on a rendered message
	// EClient: a message menu opens on the release of a right click that both began and ended on
	// the same message, this is the message its press claimed
	int m_RightClickLine = -1;

	CLine m_aLines[MAX_LINES];
	int m_CurrentLine;

	enum
	{
		// client IDs for special messages
		SILENT_MSG = -4, // EClient
		ECLIENT_MSG = -3, // EClient
		CLIENT_MSG = -2,
		SERVER_MSG = -1,
	};

	enum
	{
		MODE_NONE = 0,
		MODE_ALL,
		MODE_TEAM,
		MODE_SILENT, // EClient
	};

	enum
	{
		CHAT_SERVER = 0,
		CHAT_HIGHLIGHT,
		CHAT_CLIENT,
		CHAT_NUM,
	};

	// <EClient: the menus reachable from the chat, one on a message and one on the button that
	// sits next to the input
	class CMessagePopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;
		// Where the message sits in m_aLines, plus the time it carried when the menu was opened.
		// The menu outlives the frame it was opened on and the slot may be reused underneath it,
		// so anything acting on the line has to check that it is still the same message.
		int m_LineIndex = -1;
		int64_t m_LineTime = 0;
		int m_ClientId = -1;
		// Source language the message was translated from, empty unless it was translated
		char m_aLanguage[16] = "";
		// The message as it reads on screen, prefixes and name included
		std::string m_Text;

		// Which entries the menu was opened with. Settled once, so that the height the menu was
		// given and what it draws into it cannot disagree while it is open.
		bool m_ShowFriend = false;
		bool m_ShowTranslate = false;
		bool m_ShowLanguage = false;

		CButtonContainer m_FriendButton;
		CButtonContainer m_CopyButton;
		CButtonContainer m_TranslateButton;
		CButtonContainer m_WhitelistButton;
		CButtonContainer m_BlacklistButton;

		CLine *Line() const;

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	};
	CMessagePopupContext m_MessagePopupContext;

	class CChatPopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;

		CButtonContainer m_AutoTranslateButton;
		CButtonContainer m_BackendButton;
		CButtonContainer m_LanguagesButton;
		CButtonContainer m_CopyChatButton;
		CButtonContainer m_ClearChatButton;

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	};
	CChatPopupContext m_ChatPopupContext;

	class CBackendPopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;

		// Room for every ETranslateBackend there is, which chat.h cannot name without including
		// its way back around into translate.h. chat.cpp holds the assert that keeps the two in
		// step.
		static constexpr int MAX_BACKENDS = 8;
		CButtonContainer m_aBackendButtons[MAX_BACKENDS];

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	};
	CBackendPopupContext m_BackendPopupContext;

	class CLanguagePopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;
		CScrollRegion m_ScrollRegion;

		// Built when the menu opens: every language worth offering, plus any code already sitting
		// in one of the lists that is not among them, so a hand written entry can still be taken
		// back out. Left alone afterwards, the buttons below are addressed by their position.
		std::vector<std::string> m_vCodes;
		std::vector<std::string> m_vNames;
		std::vector<CButtonContainer> m_vWhitelistButtons;
		std::vector<CButtonContainer> m_vBlacklistButtons;

		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);
	};
	CLanguagePopupContext m_LanguagePopupContext;

	CButtonContainer m_ChatMenuButton;

	static float PopupHeight(int Entries);

	// Keeps the ui cursor on top of the one the chat steers, so the menus can hit test against it
	void SyncUiMouse();
	void OpenMessagePopup(const CLine &Line, int LineIndex);
	void OpenChatPopup(const CUIRect &ButtonRect);
	void OpenBackendPopup(const CUIRect &FromRect);
	void OpenLanguagePopup(const CUIRect &FromRect);
	// The topmost menu the chat has open, taken down the way the ui would take it down if it were
	// the one hearing the key
	bool CloseTopPopup();
	// The menu button and the menus themselves, drawn on the ui screen once the chat is done with
	// its own. Taken care of on every path that renders the chat input, including the one that
	// draws no messages at all.
	void RenderChatUi(const CUIRect &MenuButtonRect);
	std::string ChatText() const;
	// EClient>

	int m_Mode;
	bool m_Show;
	bool m_CompletionUsed;
	int m_CompletionChosen;
	char m_aCompletionBuffer[MAX_CHAT_LENGTH];
	int m_PlaceholderOffset;
	int m_PlaceholderLength;
	static char ms_aDisplayText[MAX_CHAT_LENGTH];
	class CRateablePlayer
	{
	public:
		int m_ClientId;
		int m_Score;
	};
	CRateablePlayer m_aPlayerCompletionList[MAX_CLIENTS];
	int m_PlayerCompletionListLength;

	struct CCommand
	{
		char m_aName[IConsole::TEMPCMD_NAME_LENGTH];
		char m_aParams[IConsole::TEMPCMD_PARAMS_LENGTH];
		char m_aHelpText[IConsole::TEMPCMD_HELP_LENGTH];
		char m_Prefix; // EClient

		CCommand() = default;
		CCommand(const char *pName, const char *pParams, const char *pHelpText)
		{
			str_copy(m_aName, pName);
			str_copy(m_aParams, pParams);
			str_copy(m_aHelpText, pHelpText);
			m_Prefix = m_aName[0]; // EClient
		}

		bool operator<(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) == 0; }
	};

	std::vector<CCommand> m_vServerCommands;
	bool m_ServerCommandsNeedSorting;

	struct CHistoryEntry
	{
		int m_Team;
		char m_aText[1];
	};
	CHistoryEntry *m_pHistoryEntry;
	CStaticRingBuffer<CHistoryEntry, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	int m_PendingChatCounter;
	int64_t m_LastChatSend;
	int64_t m_aLastSoundPlayed[CHAT_NUM];
	bool m_IsInputCensored;
	char m_aCurrentInputText[MAX_CHAT_LENGTH];
	bool m_EditingNewLine;

	bool m_ServerSupportsCommandInfo;

	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConChat(IConsole::IResult *pResult, void *pUserData);
	static void ConShowChat(IConsole::IResult *pResult, void *pUserData);
	static void ConEcho(IConsole::IResult *pResult, void *pUserData);
	static void ConClearChat(IConsole::IResult *pResult, void *pUserData);

	static void ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void StoreSave(const char *pText);

	friend class CBindChat;
	friend class CTranslate;
	friend class CChatBubbles;
	friend class CHud;
	friend class CLocalPractice; // EClient

public:
	CChat();
	int Sizeof() const override { return sizeof(*this); }

	static constexpr float MESSAGE_TEE_PADDING_RIGHT = 0.5f;

	bool IsActive() const { return m_Mode != MODE_NONE; }
	void AddLine(int ClientId, int Team, const char *pLine);
	void EnableMode(int Team);
	void DisableMode();
	void RegisterCommand(const char *pName, const char *pParams, const char *pHelpText);
	void UnregisterCommand(const char *pName);
	void Echo(const char *pString);

	void OnWindowResize() override;
	void OnConsoleInit() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	void OnPrepareLines(float y);
	// EClient: the view is frozen while selecting text or while hovering a message,
	// so messages don't move away under the cursor
	bool IsScrollPaused() const { return m_Mode != MODE_NONE && (m_Selecting || m_HasSelection || m_HoveringMessage || PopupOpen()); }
	// EClient: a menu opened from the chat holds the view still, the message it acts on must not
	// scroll out from under it
	bool PopupOpen() const
	{
		return Ui()->IsPopupOpen(&m_MessagePopupContext) || Ui()->IsPopupOpen(&m_ChatPopupContext) ||
		       Ui()->IsPopupOpen(&m_BackendPopupContext) || Ui()->IsPopupOpen(&m_LanguagePopupContext);
	}
	int GetLinesToSkipWhilePaused() const; // EClient
	int GetMaxBacklogCurLine() const;
	void Reset();
	void OnRelease() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnInit() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	void RebuildChat();
	void ClearLines();
	int GetLinesToScroll(int Direction, int LinesToScroll) const;
	int NumInitializedLines() const;

	// EClient
	void AnchorPausedLines();
	void ScrollToTop();
	void ScrollToBottom();
	void ScrollPageUp();
	void ScrollPageDown();

	void EnsureCoherentFontSize() const;
	void EnsureCoherentWidth() const;

	float FontSize() const { return g_Config.m_ClChatFontSize / 10.0f; }
	float MessagePaddingX() const { return FontSize() * (5 / 6.f); }
	float MessagePaddingY() const { return FontSize() * (1 / 6.f); }
	float MessageTeeSize() const { return FontSize() * (7 / 6.f); }
	float MessageRounding() const { return FontSize() * (1 / 2.f); }

	// ----- send functions -----

	// Sends a chat message to the server.
	//
	// @param Team MODE_ALL=0 MODE_TEAM=1
	// @param pLine the chat message
	// @param Conn the connection to send on, -1 for the active one
	void SendChat(int Team, const char *pLine, int Conn = -1);

	// Sends a chat message to the server.
	//
	// It uses a queue with a maximum of 3 entries
	// that ensures there is a minimum delay of one second
	// between sent messages.
	//
	// It uses team or public chat depending on m_Mode.
	void SendChatQueued(const char *pLine);

	// EClient
	bool MathSuggestion(char *pSuggestion, size_t SuggestionSize) const;

	bool LineHighlighted(int ClientId, const char *pLine);
	bool ChatDetection(int ClientId, int Team, const char *pLine);
	void ColorizeLine(const CLine &Line, CTextCursor &Cursor);
	void AddHistoryEntry(const char *pLine);

private:
	static void ConClientMessage(IConsole::IResult *pResult, void *pUserData);
	static void ConSetChatInput(IConsole::IResult *pResult, void *pUserData);
	static void ConSayQueued(IConsole::IResult *pResult, void *pUserData);
	// EClient>
};
#endif
