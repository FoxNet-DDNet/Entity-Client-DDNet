#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
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

#include <string>
#include <vector>

using namespace std::chrono_literals;

enum
{
	ENTITY_TAB_SETTINGS = 0,
	ENTITY_TAB_VISUAL,
	ENTITY_TAB_WARLIST,
	ENTITY_TAB_STATUSBAR,
	ENTITY_TAB_BINDWHEEL,
	ENTITY_TAB_QUICKACTION,
	ENTITY_TAB_INFO,
	NUMBER_OF_ENTITY_TABS,
};

typedef struct
{
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
} CKeyInfo;

static float s_Time = 0.0f;
static bool s_StartedTime = false;

const float ScrollSpeed = 100.0f;

const float FontSize = 14.0f;
const float EditBoxFontSize = 12.0f;
const float LineSize = 20.0f;
const float HeadlineFontSize = 20.0f;
const float StandardFontSize = 14.0f;

const float LineMargin = 20.0f;
const float Margin = 10.0f;
const float MarginSmall = 5.0f;
const float MarginExtraSmall = 2.5f;
const float MarginBetweenSections = 30.0f;
const float MarginBetweenViews = 30.0f;

const float HeadlineHeight = HeadlineFontSize + 0.0f;

const float HeaderHeight = FontSize + 5.0f + Margin;

const float ColorPickerLineSize = 25.0f;
const float ColorPickerLabelSize = 13.0f;
const float ColorPickerLineSpacing = 5.0f;

const float CornerRoundness = 15.0f;

const float HeaderSize = 20.0f;
const int HeaderAlignment = TEXTALIGN_MC;
const ColorRGBA BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

void CMenus::RenderSettingsEntity(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	static int s_CurTab = 0;

	CUIRect TabBar, Button;

	int TabCount = NUMBER_OF_ENTITY_TABS;
	for(int Tab = 0; Tab < NUMBER_OF_ENTITY_TABS; ++Tab)
	{
		if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, Tab))
		{
			TabCount--;
			if(s_CurTab == Tab)
				s_CurTab++;
		}
	}

	if(TabCount <= 0)
	{
		RenderEClientInfoPage(MainView);
		return;
	}

	MainView.HSplitTop(LineSize * 1.1f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / TabCount;
	static CButtonContainer s_aPageTabs[NUMBER_OF_ENTITY_TABS] = {};
	const char *apTabNames[NUMBER_OF_ENTITY_TABS] = {
		Localize("Settings"),
		Localize("Visuals"),
		Localize("Warlist"),
		Localize("Status bar"),
		Localize("Bindwheel"),
		Localize("Quick actions"),
		Localize("Info"),
	};

	for(int Tab = 0; Tab < NUMBER_OF_ENTITY_TABS; ++Tab)
	{
		int LeftTab = 0;
		int RightTab = NUMBER_OF_ENTITY_TABS - 1;

		if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, Tab))
			continue;

		for(int i = 0; i < Tab; ++i)
		{
			if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, i) && IsFlagSet(g_Config.m_ClEClientSettingsTabs, LeftTab))
				LeftTab++;
		}
		for(int i = NUMBER_OF_ENTITY_TABS - 1; i > 0; --i)
		{
			if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, i) && IsFlagSet(g_Config.m_ClEClientSettingsTabs, RightTab))
				RightTab--;
		}

		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);

		int Corners = Tab == LeftTab ? IGraphics::CORNER_L : Tab == RightTab ? IGraphics::CORNER_R :
										       IGraphics::CORNER_NONE;
		if(LeftTab == RightTab)
			Corners = IGraphics::CORNER_ALL;

		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	if(s_CurTab == ENTITY_TAB_SETTINGS)
	{
		RenderSettingsEClient(MainView);
	}
	if(s_CurTab == ENTITY_TAB_VISUAL)
	{
		RenderSettingsVisual(MainView);
	}
	if(s_CurTab == ENTITY_TAB_WARLIST)
	{
		RenderSettingsWarList(MainView);
	}
	if(s_CurTab == ENTITY_TAB_STATUSBAR)
	{
		RenderSettingsStatusbar(MainView);
	}
	if(s_CurTab == ENTITY_TAB_BINDWHEEL)
	{
		RenderSettingsBindwheel(MainView);
	}
	if(s_CurTab == ENTITY_TAB_QUICKACTION)
	{
		RenderSettingsQuickActions(MainView);
	}
	if(s_CurTab == ENTITY_TAB_INFO)
	{
		RenderEClientInfoPage(MainView);
	}
}

void CMenus::RenderEClientNewsPage(CUIRect MainView)
{
	class CNewsLine
	{
	public:
		int m_Index = -1;
		CUIRect m_Rect;
		std::string m_Text;
		float m_FontSize = 15.0f;
	};

	class CSelectedNewsLine
	{
	public:
		int m_LineIndex = -1;
		int m_SelectionStart = -1;
		int m_SelectionEnd = -1;
	};

	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_NEWS);

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.VSplitLeft(15.0f, nullptr, &MainView);

	static bool s_Selecting = false;
	static bool s_HasSelection = false;
	static vec2 s_SelectionMousePress = vec2(0.0f, 0.0f);
	static vec2 s_SelectionMouseRelease = vec2(0.0f, 0.0f);
	static std::string s_SelectionText;
	static std::string s_LastNews;
	static std::vector<CSelectedNewsLine> s_vSelectedLines;

	const char *pNews = GameClient()->m_EntityInfo.m_aNews;
	if(s_LastNews != pNews)
	{
		s_LastNews = pNews;
		s_Selecting = false;
		s_HasSelection = false;
		s_SelectionText.clear();
		s_vSelectedLines.clear();
	}

	const CUIRect InteractionRect = MainView;
	const bool HoveredNews = Ui()->MouseHovered(&InteractionRect) && !Ui()->IsPopupOpen();

	if(HoveredNews && Ui()->MouseButtonClicked(0))
	{
		s_Selecting = true;
		s_HasSelection = false;
		s_SelectionText.clear();
		s_vSelectedLines.clear();
		s_SelectionMousePress = Ui()->MousePos();
		s_SelectionMouseRelease = Ui()->MousePos();
	}
	else if(!HoveredNews && Ui()->MouseButtonClicked(0) && !s_Selecting)
	{
		s_HasSelection = false;
		s_SelectionText.clear();
		s_vSelectedLines.clear();
	}

	if(s_Selecting)
	{
		s_SelectionMouseRelease = Ui()->MousePos();
		if(!Ui()->MouseButton(0))
			s_Selecting = false;
	}

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = Ui()->IsPopupOpen() ? 0.0f : ScrollSpeed;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	CUIRect ContentView = MainView;
	CUIRect Label;

	std::vector<CNewsLine> vVisibleLines;

	auto PrepareSelectionCursor = [&](const CNewsLine &Line) {
		float TextHeight = 0.0f;
		float MaxCharacterHeight = 0.0f;
		int LineCount = 0;
		STextSizeProperties TextSizeProps;
		TextSizeProps.m_pHeight = &TextHeight;
		TextSizeProps.m_pMaxCharacterHeightInLine = &MaxCharacterHeight;
		TextSizeProps.m_pLineCount = &LineCount;

		const float TextWidth = TextRender()->TextWidth(Line.m_FontSize, Line.m_Text.c_str(), -1, -1.0f, 0, TextSizeProps);

		CTextCursor Cursor;
		Cursor.SetPosition(CUi::CalcAlignedCursorPos(&Line.m_Rect, vec2(TextWidth, TextHeight), TEXTALIGN_ML, LineCount == 1 ? &MaxCharacterHeight : nullptr));
		Cursor.m_FontSize = Line.m_FontSize;
		Cursor.m_Flags = TEXTFLAG_RENDER;
		Cursor.m_SelectionHeightFactor = 1.0f;
		return Cursor;
	};

	auto RenderSelectionForLine = [&](const CNewsLine &Line, int SelectionStart, int SelectionEnd) {
		if(SelectionStart < 0 || SelectionEnd <= SelectionStart || Line.m_Text.empty())
			return;

		CTextCursor Cursor = PrepareSelectionCursor(Line);
		Cursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_SET;
		Cursor.m_SelectionStart = SelectionStart;
		Cursor.m_SelectionEnd = SelectionEnd;

		TextRender()->TextColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
		TextRender()->TextEx(&Cursor, Line.m_Text.c_str(), -1);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	char aLine[256];
	int LineIndex = 0;
	while((pNews = str_next_token(pNews, "\n", aLine, sizeof(aLine))))
	{
		float LineHeight = 20.0f;
		float LineFontSize = 15.0f;
		ColorRGBA TextColor = TextRender()->DefaultTextColor();
		std::string LineText = aLine;

		if(!LineText.empty())
		{
			if(LineText.size() >= 3 && LineText[0] == '#' && LineText[1] == '#' && LineText[2] == '#')
			{
				LineText = LineText.substr(3);
				LineHeight = 21.0f;
				LineFontSize = 17.5f;
			}
			else if(LineText.size() >= 2 && LineText[0] == '#' && LineText[1] == '#')
			{
				LineText = LineText.substr(2);
				LineHeight = 23.5f;
				LineFontSize = 20.0f;
			}
			else if(LineText.size() >= 2 && LineText[0] == '#' && str_isnum(LineText[1]))
			{
				const int Code = (LineText[1] - '0') * 2;
				LineText = LineText.substr(2);
				LineHeight = 31.5f - Code;
				LineFontSize = 28.5f - Code;
			}
			else if(LineText[0] == '#')
			{
				LineText = LineText.substr(1);
				LineHeight = 26.0f;
				LineFontSize = 22.5f;
			}
			else if(LineText.size() >= 2 && LineText[0] == '-' && LineText[1] == '#')
			{
				TextColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
				LineText = LineText.substr(2);
				LineHeight = 13.5f;
				LineFontSize = 10.5f;
			}
			else if(LineText.size() >= 2 && LineText[0] == '-' && LineText[1] == ' ')
			{
				LineText = std::string("•") + LineText.substr(1);
			}
		}

		ContentView.HSplitTop(LineHeight, &Label, &ContentView);

		if(s_ScrollRegion.AddRect(Label))
		{
			TextRender()->TextColor(TextColor);
			Ui()->DoLabel(&Label, LineText.c_str(), LineFontSize, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			CNewsLine &Line = vVisibleLines.emplace_back();
			Line.m_Index = LineIndex;
			Line.m_Rect = Label;
			Line.m_Text = LineText;
			Line.m_FontSize = LineFontSize;
		}

		++LineIndex;
	}

	if(s_Selecting)
	{
		const float SelectionMinY = minimum(s_SelectionMousePress.y, s_SelectionMouseRelease.y);
		const float SelectionMaxY = maximum(s_SelectionMousePress.y, s_SelectionMouseRelease.y);

		std::vector<CSelectedNewsLine> vSelectedLines;
		std::string SelectionText;
		bool AnySelection = false;

		for(const CNewsLine &Line : vVisibleLines)
		{
			const float LineTop = Line.m_Rect.y;
			const float LineBottom = Line.m_Rect.y + Line.m_Rect.h;
			if(LineBottom < SelectionMinY || LineTop > SelectionMaxY || Line.m_Text.empty())
				continue;

			CTextCursor Cursor = PrepareSelectionCursor(Line);
			Cursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_CALCULATE;
			Cursor.m_PressMouse = s_SelectionMousePress;
			Cursor.m_ReleaseMouse = s_SelectionMouseRelease;

			TextRender()->TextColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
			TextRender()->TextEx(&Cursor, Line.m_Text.c_str(), -1);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			const int SelectionStart = minimum(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);
			const int SelectionEnd = maximum(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);

			if(SelectionStart < 0 || SelectionEnd <= SelectionStart)
				continue;

			CSelectedNewsLine &SelectedLine = vSelectedLines.emplace_back();
			SelectedLine.m_LineIndex = Line.m_Index;
			SelectedLine.m_SelectionStart = SelectionStart;
			SelectedLine.m_SelectionEnd = SelectionEnd;

			const size_t StartOffset = str_utf8_offset_chars_to_bytes(Line.m_Text.c_str(), SelectionStart);
			const size_t EndOffset = str_utf8_offset_chars_to_bytes(Line.m_Text.c_str(), SelectionEnd);
			if(EndOffset > StartOffset && EndOffset <= Line.m_Text.length())
			{
				if(AnySelection)
					SelectionText += "\n";
				SelectionText += Line.m_Text.substr(StartOffset, EndOffset - StartOffset);
				AnySelection = true;
			}
		}

		s_HasSelection = AnySelection;
		s_SelectionText = AnySelection ? SelectionText : "";
		s_vSelectedLines = AnySelection ? vSelectedLines : std::vector<CSelectedNewsLine>();
	}
	else if(s_HasSelection)
	{
		for(const CNewsLine &Line : vVisibleLines)
		{
			for(const CSelectedNewsLine &SelectedLine : s_vSelectedLines)
			{
				if(SelectedLine.m_LineIndex == Line.m_Index)
				{
					RenderSelectionForLine(Line, SelectedLine.m_SelectionStart, SelectedLine.m_SelectionEnd);
					break;
				}
			}
		}
	}

	if(s_HasSelection && GameClient()->Input()->ModifierIsPressed() && GameClient()->Input()->KeyPress(KEY_C))
	{
		GameClient()->Input()->SetClipboardText(s_SelectionText.c_str());
	}

	CUIRect Space;
	ContentView.HSplitTop(10.0f, &Space, &ContentView);
	s_ScrollRegion.AddRect(Space);

	s_ScrollRegion.End();
}

void CMenus::RenderEClientInfoPage(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, LowerRightView;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	RightView.VSplitLeft(MarginSmall, nullptr, &RightView);
	LeftView.VSplitRight(MarginSmall, &LeftView, nullptr);
	RightView.HSplitMid(&RightView, &LowerRightView, 0.0f);

	const float TeeSize = 75.0f;
	const float CardSize = TeeSize + MarginSmall;
	CUIRect TeeRect, DevCardRect;

	// Left Side

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Code Stealer:"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	LeftView.HSplitTop(CardSize, &DevCardRect, &LeftView);
	DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);

	static CButtonContainer s_LinkButton;
	{
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, "qxdFox"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, "qxdFox", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, BUTTONFLAG_LEFT))
			Client()->ViewLink("https://github.com/qxdFox");
	}

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Hide Settings Tabs", LineSize, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &LeftView, &LeftView);

	static int s_ShowSettings = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_SETTINGS);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowSettings, Localize("Settings"), &s_ShowSettings, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_SETTINGS, s_ShowSettings);

	static int s_ShowVisal = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowVisal, Localize("Visual"), &s_ShowVisal, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL, s_ShowVisal);

	static int s_ShowWarlist = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowWarlist, Localize("Warlist"), &s_ShowWarlist, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST, s_ShowWarlist);

	static int s_ShowStatusBar = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_STATUSBAR);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowStatusBar, Localize("Status Bar"), &s_ShowStatusBar, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_STATUSBAR, s_ShowStatusBar);

	static int s_ShowBindwheel = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_BINDWHEEL);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowBindwheel, Localize("Bindwheel"), &s_ShowBindwheel, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_BINDWHEEL, s_ShowBindwheel);

	static int s_ShowQuickActions = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_QUICKACTION);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowQuickActions, Localize("Quick Actions"), &s_ShowQuickActions, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_QUICKACTION, s_ShowQuickActions);

	char DeathCounter[32];
	str_format(DeathCounter, sizeof(DeathCounter), "%d death%s (all time)", GameClient()->m_EClient.m_KillCount, GameClient()->m_EClient.m_KillCount == 1 ? "" : "s");
	LeftView.HSplitTop(LineSize, &LeftView, &LeftView);
	Ui()->DoLabel(&LeftView, DeathCounter, FontSize, TEXTALIGN_ML);

	// Right Side
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("Config Files"), HeadlineFontSize, TEXTALIGN_MR);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	char aBuf[128 + IO_MAX_PATH_LENGTH];

	CUIRect FilesLeft, FilesRight;

	RightView.HSplitTop(LineSize * 2.0f, &Button, &RightView);
	Button.VSplitMid(&FilesLeft, &FilesRight, MarginSmall);

	static CButtonContainer s_EClientConfig, s_Config, s_Warlist, s_Profiles, s_Chatbinds, s_FontFolder;
	if(DoButtonLineSize_Menu(&s_EClientConfig, Localize("E-Client Setting"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::ENTITY].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Config, Localize("DDNet Settings"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::DDNET].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Profiles, Localize("Profiles"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);

	if(DoButtonLineSize_Menu(&s_Warlist, Localize("Warlist"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTWARLIST].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Chatbinds, Localize("Chatbinds"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTCHATBINDS].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_FontFolder, Localize("Fonts Folder"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->CreateFolder("data/entity", IStorage::TYPE_ABSOLUTE);
		Storage()->CreateFolder("data/entity/fonts", IStorage::TYPE_ABSOLUTE);
		Client()->ViewFile("data/entity/fonts");
	}

	// Render Tee Above everything else
	{
		CTeeRenderInfo TeeRenderInfo;
		TeeRenderInfo.Apply(GameClient()->m_Skins.Find("Catnoa"));
		TeeRenderInfo.ApplyColors(true, 10784768, 15269690);
		TeeRenderInfo.m_Size = TeeSize;

		RenderDraggableTee(MainView, TeeRect.Center(), TeeEyeDirection(TeeRect.Center()), CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL);
	}
}

void CMenus::RenderChatPreview(CUIRect MainView)
{
	CChat &Chat = GameClient()->m_Chat;

	ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClFriendColor));
	ColorRGBA SpecColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClSpecColor));
	ColorRGBA EnemyColor = GameClient()->m_WarList.m_WarTypes[1]->m_Color;
	ColorRGBA HelperColor = GameClient()->m_WarList.m_WarTypes[3]->m_Color;
	ColorRGBA TeammateColor = GameClient()->m_WarList.m_WarTypes[2]->m_Color;
	ColorRGBA NormalColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageColor));
	ColorRGBA ClientColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	ColorRGBA DefaultNameColor(0.8f, 0.8f, 0.8f, 1.0f);

	const float RealFontSize = 10.0f;
	const float RealMsgPaddingX = 12;
	const float RealMsgPaddingY = 4;
	const float RealMsgPaddingTee = 16;
	const float RealOffsetY = RealFontSize + RealMsgPaddingY;

	const float X = RealMsgPaddingX / 2.0f + MainView.x;
	float Y = MainView.y;
	float LineWidth = g_Config.m_ClChatWidth * 2 - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;
	char aBuf[128];

	str_copy(aBuf, Client()->PlayerName());

	const CAnimState *pIdleState = CAnimState::GetIdle();
	const float RealTeeSize = 16;
	const float RealTeeSizeHalved = 8;
	constexpr float TWSkinUnreliableOffset = -0.25f;
	const float OffsetTeeY = RealTeeSizeHalved;
	const float FullHeightMinusTee = RealOffsetY - RealTeeSize;

	class CPreviewLine
	{
	public:
		int m_ClShowIdsChat = 0;
		bool m_Team = false;
		char m_aName[64] = "";
		char m_aText[256] = "";
		bool m_Spec = false;
		bool m_Enemy = false;
		bool m_Helper = false;
		bool m_Teammate = false;
		bool m_Friend = false;
		bool m_Player = false;
		bool m_Client = false;
		bool m_Highlighted = false;
		int m_TimesRepeated = 0;

		CTeeRenderInfo m_RenderInfo;
	};

	static std::vector<CPreviewLine> s_vLines;

	enum ELineFlag
	{
		FLAG_TEAM = 1 << 0,
		FLAG_FRIEND = 1 << 1,
		FLAG_HIGHLIGHT = 1 << 2,
		FLAG_CLIENT = 1 << 3,
		FLAG_ENEMY = 1 << 4,
		FLAG_HELPER = 1 << 5,
		FLAG_TEAMMATE = 1 << 6,
		FLAG_SPEC = 1 << 7
	};
	enum
	{
		PREVIEW_SYS,
		PREVIEW_HIGHLIGHT,
		PREVIEW_TEAM,
		PREVIEW_FRIEND,
		PREVIEW_SPAMMER,
		PREVIEW_ENEMY,
		PREVIEW_HELPER,
		PREVIEW_TEAMMATE,
		PREVIEW_SPEC,
		PREVIEW_CLIENT
	};
	auto &&SetPreviewLine = [](int Index, int ClientId, const char *pName, const char *pText, int Flag, int Repeats) {
		CPreviewLine *pLine;
		if((int)s_vLines.size() <= Index)
		{
			s_vLines.emplace_back();
			pLine = &s_vLines.back();
		}
		else
		{
			pLine = &s_vLines[Index];
		}
		pLine->m_ClShowIdsChat = ClientId;
		pLine->m_Team = Flag & FLAG_TEAM;
		pLine->m_Friend = Flag & FLAG_FRIEND;
		pLine->m_Player = ClientId >= 0;
		pLine->m_Highlighted = Flag & FLAG_HIGHLIGHT;
		pLine->m_Client = Flag & FLAG_CLIENT;
		pLine->m_Spec = Flag & FLAG_SPEC;
		pLine->m_Enemy = Flag & FLAG_ENEMY;
		pLine->m_Helper = Flag & FLAG_HELPER;
		pLine->m_Teammate = Flag & FLAG_TEAMMATE;
		str_copy(pLine->m_aName, pName);
		str_copy(pLine->m_aText, pText);
	};
	auto &&SetLineSkin = [RealTeeSize](int Index, const CSkin *pSkin) {
		if(Index >= (int)s_vLines.size())
			return;
		s_vLines[Index].m_RenderInfo.m_Size = RealTeeSize;
		s_vLines[Index].m_RenderInfo.Apply(pSkin);
	};

	auto &&RenderPreview = [&](int LineIndex, int x, int y, bool Render = true) {
		if(LineIndex >= (int)s_vLines.size())
			return vec2(0, 0);
		CTextCursor LocalCursor;
		LocalCursor.SetPosition(vec2(x, y));
		LocalCursor.m_FontSize = RealFontSize;
		LocalCursor.m_Flags = Render ? TEXTFLAG_RENDER : 0;
		LocalCursor.m_LineWidth = LineWidth;
		const auto &Line = s_vLines[LineIndex];

		char aClientId[16] = "";
		if(g_Config.m_ClShowIdsChat && Line.m_ClShowIdsChat >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClShowIdsChat, aClientId, EClientIdFormat::INDENT_FORCE);
		}

		char aCount[12];
		if(Line.m_ClShowIdsChat < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		if(Line.m_Player)
		{
			LocalCursor.m_X += RealMsgPaddingTee;

			if(Line.m_Enemy && g_Config.m_ClWarlistPrefixes)
			{
				if(Render)
					TextRender()->TextColor(EnemyColor);
				TextRender()->TextEx(&LocalCursor, "♦ ", -1);
			}
			if(Line.m_Helper && g_Config.m_ClWarlistPrefixes)
			{
				if(Render)
					TextRender()->TextColor(HelperColor);
				TextRender()->TextEx(&LocalCursor, "♦ ", -1);
			}
			if(Line.m_Teammate && g_Config.m_ClWarlistPrefixes)
			{
				if(Render)
					TextRender()->TextColor(TeammateColor);
				TextRender()->TextEx(&LocalCursor, "♦ ", -1);
			}
			if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				if(Render)
					TextRender()->TextColor(FriendColor);
				TextRender()->TextEx(&LocalCursor, g_Config.m_ClFriendPrefix, -1);
			}
			if(Line.m_Spec && g_Config.m_ClSpectatePrefix)
			{
				if(Render)
					TextRender()->TextColor(SpecColor);
				TextRender()->TextEx(&LocalCursor, g_Config.m_ClSpecPrefix, -1);
			}
		}

		ColorRGBA NameColor;
		if(Line.m_Friend)
			NameColor = FriendColor;
		else if(Line.m_Team)
			NameColor = CalculateNameColor(color_cast<ColorHSLA>(TeamColor));
		else if(Line.m_Player)
			NameColor = DefaultNameColor;
		else if(Line.m_Client)
			NameColor = ClientColor;
		else
			NameColor = SystemColor;

		if(Render)
			TextRender()->TextColor(NameColor);

		TextRender()->TextEx(&LocalCursor, aClientId);
		TextRender()->TextEx(&LocalCursor, Line.m_aName);

		if(Line.m_TimesRepeated > 0)
		{
			if(Render)
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->TextEx(&LocalCursor, aCount, -1);
		}

		if(Line.m_ClShowIdsChat >= 0 && Line.m_aName[0] != '\0')
		{
			if(Render)
				TextRender()->TextColor(NameColor);
			TextRender()->TextEx(&LocalCursor, ": ", -1);
		}

		CTextCursor AppendCursor = LocalCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LocalCursor.m_X;
			AppendCursor.m_LineWidth -= LocalCursor.m_LongestLineWidth;
		}

		if(Render)
		{
			if(Line.m_Highlighted)
				TextRender()->TextColor(HighlightedColor);
			else if(Line.m_Team)
				TextRender()->TextColor(TeamColor);
			else if(Line.m_Player)
				TextRender()->TextColor(NormalColor);
		}

		TextRender()->TextEx(&AppendCursor, Line.m_aText, -1);
		if(Render)
			TextRender()->TextColor(TextRender()->DefaultTextColor());

		return vec2{LocalCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth, AppendCursor.Height() + RealMsgPaddingY};
	};

	// Set preview lines
	{
		char aLineBuilder[128];

		str_format(aLineBuilder, sizeof(aLineBuilder), "'%s' entered and joined the game", aBuf);
		if(g_Config.m_ClChatServerPrefix)
			SetPreviewLine(PREVIEW_SYS, -1, g_Config.m_ClServerPrefix, aLineBuilder, 0, 0);
		else
			SetPreviewLine(PREVIEW_SYS, -1, "*** ", aLineBuilder, 0, 0);
		str_format(aLineBuilder, sizeof(aLineBuilder), "Hey, how are you %s?", aBuf);
		SetPreviewLine(PREVIEW_HIGHLIGHT, 7, "Random Tee", aLineBuilder, FLAG_HIGHLIGHT, 0);

		SetPreviewLine(PREVIEW_TEAM, 11, "Your Teammate", "Let's speedrun this!", FLAG_TEAM, 0);
		SetPreviewLine(PREVIEW_FRIEND, 8, "Friend", "Hello there", FLAG_FRIEND, 0);
		SetPreviewLine(PREVIEW_SPAMMER, 9, "Spammer", "Hey fools, I'm spamming here!", 0, 5);
		if(g_Config.m_ClChatClientPrefix)
			SetPreviewLine(PREVIEW_CLIENT, -1, g_Config.m_ClClientPrefix, "Echo command executed", FLAG_CLIENT, 0);
		else
			SetPreviewLine(PREVIEW_CLIENT, -1, "— ", "Echo command executed", FLAG_CLIENT, 0);
		SetPreviewLine(PREVIEW_ENEMY, 6, "Enemy", "Nobo", FLAG_ENEMY, 0);
		SetPreviewLine(PREVIEW_HELPER, 3, "Helper", "Ima help this random :>", FLAG_HELPER, 0);
		SetPreviewLine(PREVIEW_TEAMMATE, 10, "Teammate", "Help me!", FLAG_TEAMMATE, 0);
		SetPreviewLine(PREVIEW_SPEC, 11, "Random Spectator", "Crazy gameplay dude", FLAG_SPEC, 0);
	}

	SetLineSkin(PREVIEW_HIGHLIGHT, GameClient()->m_Skins.Find("pinky"));
	SetLineSkin(PREVIEW_TEAM, GameClient()->m_Skins.Find("default_flower"));
	SetLineSkin(PREVIEW_FRIEND, GameClient()->m_Skins.Find("cammostripes"));
	SetLineSkin(PREVIEW_SPAMMER, GameClient()->m_Skins.Find("beast"));
	SetLineSkin(PREVIEW_ENEMY, GameClient()->m_Skins.Find("default"));
	SetLineSkin(PREVIEW_HELPER, GameClient()->m_Skins.Find("Robot"));
	SetLineSkin(PREVIEW_TEAMMATE, GameClient()->m_Skins.Find("Catnoa"));
	SetLineSkin(PREVIEW_SPEC, GameClient()->m_Skins.Find("turtle"));

	// Backgrounds first
	if(!g_Config.m_ClChatOld)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.12f);

		float TempY = Y;
		const float RealBackgroundRounding = Chat.MessageRounding() * 2.0f;

		auto &&RenderMessageBackground = [&](int LineIndex) {
			auto Size = RenderPreview(LineIndex, 0, 0, false);
			Graphics()->DrawRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Size.x + RealMsgPaddingX * 1.5f, Size.y, RealBackgroundRounding, IGraphics::CORNER_ALL);
			return Size.y;
		};

		if(g_Config.m_ClShowChatSystem)
		{
			TempY += RenderMessageBackground(PREVIEW_SYS);
		}

		if(!g_Config.m_ClShowChatFriends)
		{
			if(!g_Config.m_ClShowChatTeamMembersOnly)
				TempY += RenderMessageBackground(PREVIEW_HIGHLIGHT);
			TempY += RenderMessageBackground(PREVIEW_TEAM);
		}

		if(!g_Config.m_ClShowChatTeamMembersOnly)
			TempY += RenderMessageBackground(PREVIEW_FRIEND);

		if(!g_Config.m_ClShowChatFriends && !g_Config.m_ClShowChatTeamMembersOnly)
		{
			TempY += RenderMessageBackground(PREVIEW_SPAMMER);
		}

		TempY += RenderMessageBackground(PREVIEW_ENEMY);
		TempY += RenderMessageBackground(PREVIEW_HELPER);
		TempY += RenderMessageBackground(PREVIEW_TEAMMATE);
		TempY += RenderMessageBackground(PREVIEW_SPEC);

		TempY += RenderMessageBackground(PREVIEW_CLIENT);

		Graphics()->QuadsEnd();
	}

	// System
	if(g_Config.m_ClShowChatSystem)
	{
		Y += RenderPreview(PREVIEW_SYS, X, Y).y;
	}

	if(!g_Config.m_ClShowChatFriends)
	{
		// Highlighted
		if(!g_Config.m_ClChatOld && !g_Config.m_ClShowChatTeamMembersOnly)
			RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_HIGHLIGHT].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
		if(!g_Config.m_ClShowChatTeamMembersOnly)
			Y += RenderPreview(PREVIEW_HIGHLIGHT, X, Y).y;

		// Team
		if(!g_Config.m_ClChatOld)
			RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_TEAM].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
		Y += RenderPreview(PREVIEW_TEAM, X, Y).y;
	}

	// Friend
	if(!g_Config.m_ClChatOld && !g_Config.m_ClShowChatTeamMembersOnly)
		RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_FRIEND].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	if(!g_Config.m_ClShowChatTeamMembersOnly)
		Y += RenderPreview(PREVIEW_FRIEND, X, Y).y;

	// Normal
	if(!g_Config.m_ClShowChatFriends && !g_Config.m_ClShowChatTeamMembersOnly)
	{
		if(!g_Config.m_ClChatOld)
			RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_SPAMMER].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
		Y += RenderPreview(PREVIEW_SPAMMER, X, Y).y;
	}
	// Enemy
	RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_ENEMY].m_RenderInfo, EMOTE_ANGRY, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	Y += RenderPreview(PREVIEW_ENEMY, X, Y).y;
	// Helper
	RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_HELPER].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	Y += RenderPreview(PREVIEW_HELPER, X, Y).y;
	// Teammate
	RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_TEAMMATE].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	Y += RenderPreview(PREVIEW_TEAMMATE, X, Y).y;
	// Spectating
	RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_SPEC].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	Y += RenderPreview(PREVIEW_SPEC, X, Y).y;
	// Client
	RenderPreview(PREVIEW_CLIENT, X, Y);

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
void CMenus::RenderSettingsStatusbar(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, StatusBar;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitBottom(100.0f, &MainView, &StatusBar);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Status Bar"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBar, Localize("Show status bar"), &g_Config.m_ClStatusBar, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBarLabels, Localize("Show labels on status bar items"), &g_Config.m_ClStatusBarLabels, &LeftView, LineSize);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarHeight, &g_Config.m_ClStatusBarHeight, &Button, Localize("Status bar height"), 1, 16);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Local Time"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBar12HourClock, Localize("Use 12 hour clock"), &g_Config.m_ClStatusBar12HourClock, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBarLocalTimeSeocnds, Localize("Show seconds on clock"), &g_Config.m_ClStatusBarLocalTimeSeocnds, &LeftView, LineSize);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Colors"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	static CButtonContainer s_StatusbarColor, s_StatusbarTextColor;

	DoLine_ColorPicker(&s_StatusbarColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Status bar color"), &g_Config.m_ClStatusBarColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClStatusBarColor)), false);
	DoLine_ColorPicker(&s_StatusbarTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Text color"), &g_Config.m_ClStatusBarTextColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClStatusBarTextColor)), false);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarAlpha, &g_Config.m_ClStatusBarAlpha, &Button, Localize("Status bar alpha"), 0, 100);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarTextAlpha, &g_Config.m_ClStatusBarTextAlpha, &Button, Localize("Text alpha"), 0, 100);

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("Status Bar Codes:"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("a = Angle"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("p = Ping"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("d = Prediction"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("c = Position"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("l = Local Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("r = Race Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("f = FPS"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("v = Velocity"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("z = Zoom"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("_ or \' \' = Space"), FontSize, TEXTALIGN_ML);
	static int s_SelectedItem = -1;
	static int s_TypeSelectedOld = -1;

	CUIRect StatusScheme, StatusButtons, ItemLabel;
	static CButtonContainer s_ApplyButton, s_AddButton, s_RemoveButton;
	StatusBar.HSplitBottom(LineSize + MarginSmall, &StatusBar, &StatusScheme);
	StatusBar.HSplitTop(LineSize + MarginSmall, &ItemLabel, &StatusBar);
	StatusScheme.HSplitTop(MarginSmall, nullptr, &StatusScheme);

	if(s_TypeSelectedOld >= 0)
		Ui()->DoLabel(&ItemLabel, GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld].m_aDesc, FontSize, TEXTALIGN_ML);

	StatusScheme.VSplitMid(&StatusButtons, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&Label, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&StatusScheme, &Button, MarginSmall);
	if(DoButton_Menu(&s_ApplyButton, Localize("Apply"), 0, &Button))
	{
		GameClient()->m_StatusBar.ApplyStatusBarScheme(g_Config.m_ClStatusBarScheme);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		s_SelectedItem = -1;
	}
	Ui()->DoLabel(&Label, Localize("Status Scheme:"), FontSize, TEXTALIGN_MR);
	static CLineInput s_StatusScheme(g_Config.m_ClStatusBarScheme, sizeof(g_Config.m_ClStatusBarScheme));
	s_StatusScheme.SetEmptyText("");
	Ui()->DoEditBox(&s_StatusScheme, &StatusScheme, EditBoxFontSize);

	static std::vector<const char *> s_DropDownNames = {};
	for(const CStatusItem &StatusItemType : GameClient()->m_StatusBar.m_StatusItemTypes)
		if(s_DropDownNames.size() != GameClient()->m_StatusBar.m_StatusItemTypes.size())
			s_DropDownNames.push_back(StatusItemType.m_aName);

	static CUi::SDropDownState s_DropDownState;
	static CScrollRegion s_DropDownScrollRegion;
	s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
	CUIRect DropDownRect;

	StatusButtons.VSplitMid(&DropDownRect, &StatusButtons, MarginSmall);
	const int TypeSelectedNew = Ui()->DoDropDown(&DropDownRect, s_TypeSelectedOld, s_DropDownNames.data(), s_DropDownNames.size(), s_DropDownState);
	if(s_TypeSelectedOld != TypeSelectedNew)
	{
		s_TypeSelectedOld = TypeSelectedNew;
		if(s_SelectedItem >= 0)
		{
			GameClient()->m_StatusBar.m_StatusBarItems[s_SelectedItem] = &GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld];
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		}
	}
	CUIRect ButtonL, ButtonR;
	StatusButtons.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	if(DoButton_Menu(&s_AddButton, Localize("Add Item"), 0, &ButtonL) && s_TypeSelectedOld >= 0)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.push_back(&GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld]);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		s_SelectedItem = (int)GameClient()->m_StatusBar.m_StatusBarItems.size() - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, Localize("Remove Item"), 0, &ButtonR) && s_SelectedItem >= 0)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.erase(GameClient()->m_StatusBar.m_StatusBarItems.begin() + s_SelectedItem);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		s_SelectedItem = -1;
	}

	// color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClStatusBarColor)).WithAlpha(0.5f)
	StatusBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 5.0f);
	int ItemCount = GameClient()->m_StatusBar.m_StatusBarItems.size();
	float AvailableWidth = StatusBar.w;
	// AvailableWidth -= (ItemCount - 1) * MarginSmall;
	AvailableWidth -= MarginSmall;
	StatusBar.VSplitLeft(MarginExtraSmall, nullptr, &StatusBar);
	if(ItemCount <= 0)
	{
		Ui()->DoLabel(&StatusBar, Localize("No status bar items configured"), FontSize, TEXTALIGN_MC);
		return;
	}

	float ItemWidth = AvailableWidth / (float)ItemCount;
	CUIRect StatusItemButton;
	static std::vector<CButtonContainer *> s_pItemButtons;
	static std::vector<CButtonContainer> s_ItemButtons;
	static vec2 s_ActivePos = vec2(0.0f, 0.0f);
	class CSwapItem
	{
	public:
		vec2 m_InitialPosition = vec2(0.0f, 0.0f);
		float m_Duration = 0.0f;
	};

	static std::vector<CSwapItem> s_ItemSwaps;

	if((int)s_ItemButtons.size() != ItemCount)
	{
		s_ItemSwaps.resize(ItemCount);
		s_pItemButtons.resize(ItemCount);
		s_ItemButtons.resize(ItemCount);
		for(int i = 0; i < ItemCount; ++i)
		{
			s_pItemButtons[i] = &s_ItemButtons[i];
		}
	}
	bool StatusItemActive = false;
	int HotStatusIndex = 0;
	for(int i = 0; i < ItemCount; ++i)
	{
		if(Ui()->ActiveItem() == s_pItemButtons[i])
		{
			StatusItemActive = true;
			HotStatusIndex = i;
		}
	}

	for(int i = 0; i < ItemCount; ++i)
	{
		// if(i > 0)
		//	StatusBar.VSplitLeft(MarginSmall, nullptr, &StatusBar);
		StatusBar.VSplitLeft(ItemWidth, &StatusItemButton, &StatusBar);
		StatusItemButton.HMargin(MarginSmall, &StatusItemButton);
		StatusItemButton.VMargin(MarginExtraSmall, &StatusItemButton);
		CStatusItem *StatusItem = GameClient()->m_StatusBar.m_StatusBarItems[i];
		ColorRGBA Col = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
		if(s_SelectedItem == i)
			Col = ColorRGBA(1.0f, 0.35f, 0.35f, 0.75f);
		CUIRect TempItemButton = StatusItemButton;
		TempItemButton.y = 0, TempItemButton.h = 10000.0f;
		if(StatusItemActive && Ui()->ActiveItem() != s_pItemButtons[i] && Ui()->MouseInside(&TempItemButton))
		{
			std::swap(s_pItemButtons[i], s_pItemButtons[HotStatusIndex]);
			std::swap(GameClient()->m_StatusBar.m_StatusBarItems[i], GameClient()->m_StatusBar.m_StatusBarItems[HotStatusIndex]);
			s_SelectedItem = -2;
			s_ItemSwaps[HotStatusIndex].m_InitialPosition = vec2(StatusItemButton.x, StatusItemButton.y);
			s_ItemSwaps[HotStatusIndex].m_Duration = 0.15f;
			s_ItemSwaps[i].m_InitialPosition = vec2(s_ActivePos.x, s_ActivePos.y);
			s_ItemSwaps[i].m_Duration = 0.15f;
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		}
		TempItemButton = StatusItemButton;
		s_ItemSwaps[i].m_Duration = std::max(0.0f, s_ItemSwaps[i].m_Duration - Client()->RenderFrameTime());
		if(s_ItemSwaps[i].m_Duration > 0.0f)
		{
			float Progress = std::pow(2.0, -5.0 * (1.0 - s_ItemSwaps[i].m_Duration / 0.15f));
			TempItemButton.x = mix(TempItemButton.x, s_ItemSwaps[i].m_InitialPosition.x, Progress);
		}
		if(DoButtonLineSize_Menu(s_pItemButtons[i], StatusItem->m_aDisplayName, 0, &TempItemButton, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, Col))
		{
			if(s_SelectedItem == -2)
				s_SelectedItem++;
			else if(s_SelectedItem != i)
			{
				s_SelectedItem = i;
				for(int t = 0; t < (int)GameClient()->m_StatusBar.m_StatusItemTypes.size(); ++t)
					if(str_comp(GameClient()->m_StatusBar.m_StatusItemTypes[t].m_aName, StatusItem->m_aName) == 0)
						s_TypeSelectedOld = t;
			}
			else
			{
				s_SelectedItem = -1;
				s_TypeSelectedOld = -1;
			}
		}
		if(Ui()->ActiveItem() == s_pItemButtons[i])
			s_ActivePos = vec2(StatusItemButton.x, StatusItemButton.y);
	}
	if(!StatusItemActive)
		s_SelectedItem = std::max(-1, s_SelectedItem);
}
void CMenus::RenderSettingsQuickActions(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	const float Radius = minimum(RightView.w, RightView.h) / 2.0f;
	vec2 Pos{RightView.x + RightView.w / 2.0f, RightView.y + RightView.h / 2.0f};
	// Draw Circle
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 64);
	Graphics()->QuadsEnd();

	static char s_aBindName[QUICKACTIONS_MAX_NAME];
	static char s_aBindCommand[QUICKACTIONS_MAX_CMD];

	static int s_SelectedBindIndex = -1;
	int HoveringIndex = -1;

	float MouseDist = distance(Pos, Ui()->MousePos());
	if(GameClient()->m_QuickActions.m_vBinds.empty())
	{
		float Size = 20.0f;
		TextRender()->Text(Pos.x - TextRender()->TextWidth(Size, "Empty") / 2.0f, Pos.y - Size / 2, Size, "Empty");
	}
	else if(MouseDist < Radius && MouseDist > Radius * 0.25f)
	{
		int SegmentCount = GameClient()->m_QuickActions.m_vBinds.size();
		float SegmentAngle = 2 * pi / SegmentCount;

		float HoveringAngle = angle(Ui()->MousePos() - Pos) + SegmentAngle / 2;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;

		HoveringIndex = (int)(HoveringAngle / (2 * pi) * SegmentCount);
		if(Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = HoveringIndex;
			str_copy(s_aBindName, GameClient()->m_QuickActions.m_vBinds[HoveringIndex].m_aName);
			str_copy(s_aBindCommand, GameClient()->m_QuickActions.m_vBinds[HoveringIndex].m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
		{
			CQuickActions::CBind BindA = GameClient()->m_QuickActions.m_vBinds[s_SelectedBindIndex];
			CQuickActions::CBind BindB = GameClient()->m_QuickActions.m_vBinds[HoveringIndex];
			str_copy(GameClient()->m_QuickActions.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
			str_copy(GameClient()->m_QuickActions.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
			str_copy(GameClient()->m_QuickActions.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
			str_copy(GameClient()->m_QuickActions.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(2))
		{
			s_SelectedBindIndex = HoveringIndex;
		}
	}
	else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
	{
		s_SelectedBindIndex = -1;
		str_copy(s_aBindName, "");
		str_copy(s_aBindCommand, "");
	}

	if(!GameClient()->m_QuickActions.m_vBinds.empty())
	{
		const float Theta = pi * 2.0f / GameClient()->m_QuickActions.m_vBinds.size();
		for(int i = 0; i < static_cast<int>(GameClient()->m_QuickActions.m_vBinds.size()); i++)
		{
			float FontSizes = 12.0f;
			if(i == s_SelectedBindIndex)
			{
				FontSizes = 20.0f;
				TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
			}
			else if(i == HoveringIndex)
				FontSizes = 14.0f;

			const CQuickActions::CBind Bind = GameClient()->m_QuickActions.m_vBinds[i];
			const float Angle = Theta * i;
			vec2 TextPos = direction(Angle);
			TextPos *= Radius * 0.75f;

			float Width = TextRender()->TextWidth(FontSizes, Bind.m_aName);
			TextPos += Pos;
			TextPos.x -= Width / 2.0f;
			TextRender()->Text(TextPos.x, TextPos.y, FontSizes, Bind.m_aName);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Name:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText("Name");
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Command:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(Localize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, Localize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0)
	{
		CQuickActions::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		str_copy(GameClient()->m_QuickActions.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
		str_copy(GameClient()->m_QuickActions.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	CUIRect ButtonAdd, ButtonRemove;
	Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
	if(DoButton_Menu(&s_AddButton, Localize("Add Bind"), 0, &ButtonAdd))
	{
		CQuickActions::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_QuickActions.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_QuickActions.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, Localize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_QuickActions.RemoveBind(s_SelectedBindIndex);
		s_SelectedBindIndex = -1;
	}

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Use left mouse to select", 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Use right mouse to swap with selected", 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Use middle mouse select without copy", 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, "To use the name of the target player do \"%s\"", 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, "To get the Client Id do \"%d\"", 14.0f, TEXTALIGN_ML);

	// RenderTee
	{
		const char *pSkinName = g_Config.m_ClPlayerSkin;
		int pUseCustomColor = g_Config.m_ClPlayerUseCustomColor;
		unsigned pColorBody = g_Config.m_ClPlayerColorBody;
		unsigned pColorFeet = g_Config.m_ClPlayerColorFeet;

		const CSkin *pDefaultSkin = GameClient()->m_Skins.Find("default");
		const CSkins::CSkinContainer *pOwnSkinContainer = GameClient()->m_Skins.FindContainerOrNullptr(pSkinName[0] == '\0' ? "default" : pSkinName);
		if(pOwnSkinContainer != nullptr && pOwnSkinContainer->IsSpecial())
		{
			pOwnSkinContainer = nullptr; // Special skins cannot be selected, show as missing due to invalid name
		}

		CTeeRenderInfo OwnSkinInfo;
		OwnSkinInfo.Apply(pOwnSkinContainer == nullptr || pOwnSkinContainer->Skin() == nullptr ? pDefaultSkin : pOwnSkinContainer->Skin().get());
		OwnSkinInfo.ApplyColors(pUseCustomColor, pColorBody, pColorFeet);
		OwnSkinInfo.m_Size = 72.0f;

		// Tee
		{
			vec2 DeltaPosition = Ui()->MousePos() - Pos;
			vec2 TeeDirection = normalize(DeltaPosition);

			const vec2 TeeRenderPos = vec2(Pos.x, Pos.y);
			RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, EMOTE_NORMAL, TeeDirection, TeeRenderPos);
		}
	}

	CUIRect KeyLabel;
	LeftView.HSplitBottom(LineSize, &LeftView, &Button);
	Button.VSplitLeft(250.0f, &KeyLabel, &Button);

	static CButtonContainer s_ReaderButtonWheel, s_ClearButtonWheel;
	DoLine_KeyReader(KeyLabel, s_ReaderButtonWheel, s_ClearButtonWheel, Localize("Quick Actions Key:"), "+quickactions");

	LeftView.HSplitBottom(LineSize, &LeftView, &Button);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetQuickActionMouse, Localize("Reset position of mouse when opening the quick actions menu"), &g_Config.m_ClResetQuickActionMouse, &Button, LineSize);
}

void CMenus::RenderSettingsBindwheel(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	const float Radius = minimum(RightView.w, RightView.h) / 2.0f;
	vec2 Pos{RightView.x + RightView.w / 2.0f, RightView.y + RightView.h / 2.0f};
	// Draw Circle
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 64);
	Graphics()->QuadsEnd();

	static char s_aBindName[BINDWHEEL_MAX_NAME];
	static char s_aBindCommand[BINDWHEEL_MAX_CMD];

	static int s_SelectedBindIndex = -1;
	int HoveringIndex = -1;

	float MouseDist = distance(Pos, Ui()->MousePos());
	if(GameClient()->m_Bindwheel.m_vBinds.empty()) // EClient -> Fixes a Crash
	{
		float Size = 20.0f;
		TextRender()->Text(Pos.x - TextRender()->TextWidth(Size, "Empty") / 2.0f, Pos.y - Size / 2, Size, "Empty");
	}
	else if(MouseDist < Radius && MouseDist > Radius * 0.25f)
	{
		int SegmentCount = GameClient()->m_Bindwheel.m_vBinds.size();
		float SegmentAngle = 2 * pi / SegmentCount;

		float HoveringAngle = angle(Ui()->MousePos() - Pos) + SegmentAngle / 2;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;

		HoveringIndex = (int)(HoveringAngle / (2 * pi) * SegmentCount);
		if(Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = HoveringIndex;
			str_copy(s_aBindName, GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aName);
			str_copy(s_aBindCommand, GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
		{
			CBindWheel::CBind BindA = GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex];
			CBindWheel::CBind BindB = GameClient()->m_Bindwheel.m_vBinds[HoveringIndex];
			str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
			str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
			str_copy(GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
			str_copy(GameClient()->m_Bindwheel.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(2))
		{
			s_SelectedBindIndex = HoveringIndex;
		}
	}
	else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
	{
		s_SelectedBindIndex = -1;
		str_copy(s_aBindName, "");
		str_copy(s_aBindCommand, "");
	}

	if(!GameClient()->m_Bindwheel.m_vBinds.empty())
	{
		const float Theta = pi * 2.0f / GameClient()->m_Bindwheel.m_vBinds.size();
		for(int i = 0; i < static_cast<int>(GameClient()->m_Bindwheel.m_vBinds.size()); i++)
		{
			float FontSizes = 12.0f;
			if(i == s_SelectedBindIndex)
			{
				FontSizes = 20.0f;
				TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
			}
			else if(i == HoveringIndex)
				FontSizes = 14.0f;

			const CBindWheel::CBind Bind = GameClient()->m_Bindwheel.m_vBinds[i];
			const float Angle = Theta * i;
			vec2 TextPos = direction(Angle);
			TextPos *= Radius * 0.75f;

			float Width = TextRender()->TextWidth(FontSizes, Bind.m_aName);
			TextPos += Pos;
			TextPos.x -= Width / 2.0f;
			TextRender()->Text(TextPos.x, TextPos.y, FontSizes, Bind.m_aName);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Name:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText("Name");
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Command:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(Localize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, Localize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0)
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
		str_copy(GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	CUIRect ButtonAdd, ButtonRemove;
	Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
	if(DoButton_Menu(&s_AddButton, Localize("Add Bind"), 0, &ButtonAdd))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_Bindwheel.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_Bindwheel.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, Localize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_Bindwheel.RemoveBind(s_SelectedBindIndex);
		s_SelectedBindIndex = -1;
	}

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Use left mouse to select"), 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Use right mouse to swap with selected"), 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, Localize("Use middle mouse select without copy"), 14.0f, TEXTALIGN_ML);

	CUIRect KeyLabel;
	LeftView.HSplitBottom(LineSize, &LeftView, &Button);
	Button.VSplitLeft(250.0f, &KeyLabel, &Button);

	static CButtonContainer s_ReaderButtonWheel, s_ClearButtonWheel;
	DoLine_KeyReader(KeyLabel, s_ReaderButtonWheel, s_ClearButtonWheel, Localize("Bind Wheel Key:"), "+bindwheel");

	LeftView.HSplitBottom(LineSize, &LeftView, &Button);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetBindWheelMouse, Localize("Reset position of mouse when opening bindwheel"), &g_Config.m_ClResetBindWheelMouse, &Button, LineSize);

	CUIRect CopyRight;
	MainView.HSplitBottom(20.0f, 0, &CopyRight);
	CopyRight.VSplitLeft(255.0f, &CopyRight, &CopyRight);
	Ui()->DoLabel(&CopyRight, "© Tater", 14.0f, TEXTALIGN_ML);
}

void CMenus::RenderSettingsWarList(CUIRect MainView)
{
	CUIRect RightView, LeftView, Column1, Column2, Column3, Column4, Button, ButtonL, ButtonR, Label;

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, Margin);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	// WAR LIST will have 4 columns
	//  [War entries] - [Entry Editing] - [Group Types] - [Recent Players]
	//									 [Group Editing]

	// putting this here so it can be updated by the entry list
	static char s_aEntryName[MAX_NAME_LENGTH];
	static char s_aEntryClan[MAX_CLAN_LENGTH];
	static char s_aEntryReason[MAX_WARLIST_REASON_LENGTH];
	static int s_IsClan = 0;
	static int s_IsName = 1;

	LeftView.VSplitMid(&Column1, &Column2, Margin);
	RightView.VSplitMid(&Column3, &Column4, Margin);

	// ======WAR ENTRIES======
	Column1.HSplitTop(HeadlineHeight, &Label, &Column1);
	Label.VSplitRight(25.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("War Entries"), HeadlineFontSize, TEXTALIGN_ML);
	Column1.HSplitTop(MarginSmall, nullptr, &Column1);

	static CButtonContainer s_ReverseEntries;
	static bool s_Reversed = true;
	if(Ui()->DoButton_FontIcon(&s_ReverseEntries, FontIcon::CHEVRON_DOWN, 0, &Button, BUTTONFLAG_LEFT))
	{
		s_Reversed = !s_Reversed;
	}

	CUIRect EntriesSearch;
	Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
	EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);

	static CWarEntry *s_pSelectedEntry = nullptr;
	static CWarType *s_pSelectedType = GameClient()->m_WarList.m_WarTypes[0];

	// Filter the list
	static CLineInputBuffered<128> s_EntriesFilterInput;
	std::vector<CWarEntry *> vpFilteredEntries;
	for(CWarEntry &Entry : GameClient()->m_WarList.m_vWarEntries)
	{
		CWarEntry *pEntry = &Entry;
		bool Matches = false;
		if(str_find_nocase(pEntry->m_aName, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(str_find_nocase(pEntry->m_aClan, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(str_find_nocase(pEntry->m_pWarType->m_aWarName, s_EntriesFilterInput.GetString()))
			Matches = true;
		if(Matches)
			vpFilteredEntries.push_back(pEntry);
	}

	if(s_Reversed)
	{
		std::reverse(vpFilteredEntries.begin(), vpFilteredEntries.end());
	}
	int SelectedOldEntry = -1;
	static CListBox s_EntriesListBox;
	s_EntriesListBox.DoStart(35.0f, vpFilteredEntries.size(), 1, 2, SelectedOldEntry, &Column1);

	static std::vector<unsigned char> s_vItemIds;
	static std::vector<CButtonContainer> s_vDeleteButtons;

	const int MaxEntries = GameClient()->m_WarList.m_vWarEntries.size();
	s_vItemIds.resize(MaxEntries);
	s_vDeleteButtons.resize(MaxEntries);

	for(size_t i = 0; i < vpFilteredEntries.size(); i++)
	{
		CWarEntry *pEntry = vpFilteredEntries[i];

		// idk why it wants this, it was complaining
		if(!pEntry)
			continue;

		if(s_pSelectedEntry && pEntry == s_pSelectedEntry)
			SelectedOldEntry = i;

		const CListboxItem Item = s_EntriesListBox.DoNextItem(&s_vItemIds[i], SelectedOldEntry >= 0 && (size_t)SelectedOldEntry == i);
		if(!Item.m_Visible)
			continue;

		CUIRect EntryRect, DeleteButton, EntryTypeRect, WarType, ToolTip;
		Item.m_Rect.Margin(0.0f, &EntryRect);
		EntryRect.VSplitLeft(26.0f, &DeleteButton, &EntryRect);
		DeleteButton.HMargin(7.5f, &DeleteButton);
		DeleteButton.VSplitLeft(MarginSmall, nullptr, &DeleteButton);
		DeleteButton.VSplitRight(MarginExtraSmall, &DeleteButton, nullptr);

		if(Ui()->DoButton_FontIcon(&s_vDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, BUTTONFLAG_LEFT))
			GameClient()->m_WarList.RemoveWarEntry(pEntry);

		bool IsClan = false;
		char aBuf[32];
		if(str_comp(pEntry->m_aClan, "") != 0)
		{
			str_copy(aBuf, pEntry->m_aClan);
			IsClan = true;
		}
		else
		{
			str_copy(aBuf, pEntry->m_aName);
		}
		EntryRect.VSplitLeft(35.0f, &EntryTypeRect, &EntryRect);

		if(IsClan)
		{
			RenderFontIcon(EntryTypeRect, FontIcon::USERS, 18.0f, TEXTALIGN_MC);
		}
		else
		{
			// TODO: render the real skin with skin remembering component (to be added)
			CTeeRenderInfo TeeInfo;
			TeeInfo.Apply(GameClient()->m_Skins.Find("default"));
			TeeInfo.m_Size = 35.0f;

			RenderTee(EntryTypeRect.Center(), TeeEyeDirection(EntryTypeRect.Center()), CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL);
		}

		if(str_comp(pEntry->m_aReason, "") != 0)
		{
			EntryRect.VSplitRight(20.0f, &EntryRect, &ToolTip);
			RenderFontIcon(ToolTip, FontIcon::COMMENT, 18.0f, TEXTALIGN_MC);
			GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &ToolTip, pEntry->m_aReason);
			GameClient()->m_Tooltips.SetFadeTime(&s_vItemIds[i], 0.0f);
		}

		EntryRect.HMargin(MarginExtraSmall, &EntryRect);
		EntryRect.HSplitMid(&EntryRect, &WarType, MarginSmall);

		if(pEntry->m_TempEntry)
			str_append(aBuf, " ⏳");

		Ui()->DoLabel(&EntryRect, aBuf, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(pEntry->m_pWarType->m_Color);
		Ui()->DoLabel(&WarType, pEntry->m_pWarType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		if(pEntry->m_TempEntry)
			GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &EntryRect, "Temporary Entry");
	}

	const int NewSelectedEntry = s_EntriesListBox.DoEnd();
	const bool HasValidNewSelectedEntry = NewSelectedEntry >= 0 && NewSelectedEntry < (int)vpFilteredEntries.size();
	if(HasValidNewSelectedEntry && (SelectedOldEntry != NewSelectedEntry || (SelectedOldEntry >= 0 && Ui()->HotItem() == &s_vItemIds[NewSelectedEntry] && Ui()->MouseButtonClicked(0))))
	{
		s_pSelectedEntry = vpFilteredEntries[NewSelectedEntry];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aEntryName, s_pSelectedEntry->m_aName);
			str_copy(s_aEntryClan, s_pSelectedEntry->m_aClan);
			str_copy(s_aEntryReason, s_pSelectedEntry->m_aReason);
			if(str_comp(s_pSelectedEntry->m_aClan, "") != 0)
			{
				s_IsName = 0;
				s_IsClan = 1;
			}
			else
			{
				s_IsName = 1;
				s_IsClan = 0;
			}
			s_pSelectedType = s_pSelectedEntry->m_pWarType;
		}
	}
	else if(vpFilteredEntries.empty())
	{
		s_pSelectedEntry = nullptr;
	}

	Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	// ======WAR ENTRY EDITING======
	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Label.VSplitRight(25.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Edit Entry"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);

	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aEntryName, sizeof(s_aEntryName));
	s_NameInput.SetEmptyText("Name");
	if(s_IsName)
		Ui()->DoEditBox(&s_NameInput, &ButtonL, EditBoxFontSize);
	else
	{
		ButtonL.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonL);
		ButtonL.VMargin(2.0f, &ButtonL);
		s_NameInput.Render(&ButtonL, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	static CLineInput s_ClanInput;
	s_ClanInput.SetBuffer(s_aEntryClan, sizeof(s_aEntryClan));
	s_ClanInput.SetEmptyText("Clan");
	if(s_IsClan)
		Ui()->DoEditBox(&s_ClanInput, &ButtonR, EditBoxFontSize);
	else
	{
		ButtonR.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonR);
		ButtonR.VMargin(2.0f, &ButtonR);
		s_ClanInput.Render(&ButtonR, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static unsigned char s_NameRadio, s_ClanRadio;
	if(DoButton_CheckBox_Common(&s_NameRadio, "Name", s_IsName ? "X" : "", &ButtonL, BUTTONFLAG_ALL))
	{
		s_IsName = 1;
		s_IsClan = 0;
	}
	if(DoButton_CheckBox_Common(&s_ClanRadio, "Clan", s_IsClan ? "X" : "", &ButtonR, BUTTONFLAG_ALL))
	{
		s_IsName = 0;
		s_IsClan = 1;
	}
	if(!s_IsName)
		str_copy(s_aEntryName, "");
	if(!s_IsClan)
		str_copy(s_aEntryClan, "");

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);
	static CLineInput s_ReasonInput;
	s_ReasonInput.SetBuffer(s_aEntryReason, sizeof(s_aEntryReason));
	s_ReasonInput.SetEmptyText("Reason");
	Ui()->DoEditBox(&s_ReasonInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_OverrideButton;

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize * 2.0f, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);

	if(DoButtonLineSize_Menu(&s_OverrideButton, Localize("Override Entry"), 0, &ButtonL, LineSize) && s_pSelectedEntry)
	{
		if(s_pSelectedEntry && s_pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(s_pSelectedEntry->m_aName, s_aEntryName);
			str_copy(s_pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(s_pSelectedEntry->m_aReason, s_aEntryReason);
			s_pSelectedEntry->m_pWarType = s_pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, Localize("Add Entry"), 0, &ButtonR, LineSize))
	{
		if(s_pSelectedType)
			GameClient()->m_WarList.AddWarEntry(s_aEntryName, s_aEntryClan, s_aEntryReason, s_pSelectedType->m_aWarName);
	}
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column2);
	if(s_pSelectedType)
	{
		float Shade = 0.0f;
		Button.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), 15, 3.0f);
		TextRender()->TextColor(s_pSelectedType->m_Color);
		Ui()->DoLabel(&Button, s_pSelectedType->m_aWarName, HeadlineFontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Column2.HSplitBottom(200.0f, nullptr, &Column2);

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, Localize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, Localize("Enable warlist"), &g_Config.m_ClWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixes, Localize("Warlist Chat Prefixes"), &g_Config.m_ClWarlistPrefixes, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixesServerInfo, Localize("Warlist Prefix Server Info"), &g_Config.m_ClWarlistPrefixesServerInfo, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListChat, Localize("Colors in chat"), &g_Config.m_ClWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListScoreboard, Localize("Colors in scoreboard"), &g_Config.m_ClWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSpecMenu, Localize("Colors in specmenu"), &g_Config.m_ClWarListSpecMenu, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListShowClan, Localize("Show clan if war"), &g_Config.m_ClWarListShowClan, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSwapNameReason, Localize("Swap Reason with Name"), &g_Config.m_ClWarListSwapNameReason, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListAllowDuplicates, Localize("Allow Duplicate Entries"), &g_Config.m_ClWarListAllowDuplicates, &Column2, LineSize);

	// ======WAR TYPE EDITING======

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, Localize("War Groups"), HeadlineFontSize, TEXTALIGN_ML);
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);

	static char s_aTypeName[MAX_WARLIST_TYPE_LENGTH];
	static ColorRGBA s_GroupColor = ColorRGBA(1, 1, 1, 1);

	CUIRect WarTypeList;
	Column3.HSplitBottom(180.0f, &WarTypeList, &Column3);
	m_pRemoveWarType = nullptr;
	int SelectedOldType = -1;
	static CListBox s_WarTypeListBox;
	s_WarTypeListBox.DoStart(25.0f, GameClient()->m_WarList.m_WarTypes.size(), 1, 2, SelectedOldType, &WarTypeList);

	static std::vector<unsigned char> s_vTypeItemIds;
	static std::vector<CButtonContainer> s_vTypeDeleteButtons;
	static std::vector<CButtonContainer> s_vTypeBrowserHideButtons;
	static std::vector<std::string> s_vTypeEntriesText;

	const int MaxTypes = GameClient()->m_WarList.m_WarTypes.size();
	s_vTypeItemIds.resize(MaxTypes);
	s_vTypeDeleteButtons.resize(MaxTypes);
	s_vTypeBrowserHideButtons.resize(MaxTypes);
	s_vTypeEntriesText.resize(MaxTypes);

	for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
	{
		CWarType *pType = GameClient()->m_WarList.m_WarTypes[i];
		if(!pType)
			continue;

		if(s_pSelectedType && pType == s_pSelectedType)
			SelectedOldType = i;

		const CListboxItem Item = s_WarTypeListBox.DoNextItem(&s_vTypeItemIds[i], SelectedOldType >= 0 && SelectedOldType == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TypeRect, DeleteButton, HideButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);

		if(Ui()->ActiveItem() == &s_vTypeItemIds[i])
		{
			// If dragging up/down the list, swap war types
			if(Ui()->MouseButton(0))
			{
				int SwapIndex = -1;
				if(Ui()->MouseY() < Item.m_Rect.y)
					SwapIndex = i - 1;
				else if(Ui()->MouseY() > Item.m_Rect.y + Item.m_Rect.h)
					SwapIndex = i + 1;
				if(SwapIndex >= 0 && SwapIndex < (int)GameClient()->m_WarList.m_WarTypes.size())
				{
					CWarType *pSwapType = GameClient()->m_WarList.m_WarTypes[SwapIndex];
					CWarType *pThisType = GameClient()->m_WarList.m_WarTypes[i];
					if(pSwapType->m_Removable && pThisType->m_Removable)
					{
						std::swap(GameClient()->m_WarList.m_WarTypes[i], GameClient()->m_WarList.m_WarTypes[SwapIndex]);
						Ui()->SetActiveItem(&s_vTypeItemIds[SwapIndex]);
					}
				}
			}
		}

		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vTypeDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				m_pRemoveWarType = pType;
		}
		if(g_Config.m_ClWarlistBrowser && pType != GameClient()->m_WarList.m_pWarTypeNone)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &HideButton);
			HideButton.HSplitTop(20.0f, &HideButton, nullptr);
			HideButton.Margin(2.0f, &HideButton);

			const bool BrowserFlagSet = IsFlagSet(g_Config.m_ClWarlistBrowserFlags, i);

			if(DoButtonNoRect_FontIcon(&s_vTypeBrowserHideButtons[i], BrowserFlagSet ? FontIcon::EYE_SLASH : FontIcon::EYE, 0, &HideButton, IGraphics::CORNER_ALL))
			{
				SetFlag(g_Config.m_ClWarlistBrowserFlags, i, !BrowserFlagSet);
			}
			GameClient()->m_Tooltips.DoToolTip(&s_vTypeBrowserHideButtons[i], &HideButton,
				BrowserFlagSet ? Localize("Hidden in server browser") : Localize("Shown in server browser"));
		}

		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d %s", pType->m_NumEntries, Localize("entries"));
		s_vTypeEntriesText[i] = aBuf;
		GameClient()->m_Tooltips.DoToolTip(&s_vTypeItemIds[i], &TypeRect, s_vTypeEntriesText[i].c_str());
	}
	const int NewSelectedType = s_WarTypeListBox.DoEnd();
	if((SelectedOldType != NewSelectedType && NewSelectedType >= 0) || (NewSelectedType >= 0 && Ui()->HotItem() == &s_vTypeItemIds[NewSelectedType] && Ui()->MouseButtonClicked(0)))
	{
		s_pSelectedType = GameClient()->m_WarList.m_WarTypes[NewSelectedType];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aTypeName, s_pSelectedType->m_aWarName);
			s_GroupColor = s_pSelectedType->m_Color;
		}
	}
	if(m_pRemoveWarType != nullptr)
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage),
			Localize("Are you sure that you want to remove '%s' from your war groups?"),
			m_pRemoveWarType->m_aWarName);
		PopupConfirm(Localize("Remove War Group"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmRemoveWarType);
	}

	static CLineInput s_TypeNameInput;
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	Column3.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column3);
	s_TypeNameInput.SetBuffer(s_aTypeName, sizeof(s_aTypeName));
	s_TypeNameInput.SetEmptyText("Group Name");
	Ui()->DoEditBox(&s_TypeNameInput, &Button, EditBoxFontSize);
	static CButtonContainer s_AddGroupButton, s_OverrideGroupButton, s_GroupColorPicker;

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int s_ColorValue = 0;
	s_ColorValue = color_cast<ColorHSLA>(s_GroupColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, Localize("Color"), &s_ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, Localize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && s_pSelectedType)
	{
		if(s_pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(s_pSelectedType->m_aWarName, s_aTypeName);
			s_pSelectedType->m_Color = s_GroupColor;
		}
	}
	bool AddDisabled = str_comp(GameClient()->m_WarList.FindWarType(s_aTypeName)->m_aWarName, "none") != 0 || str_comp(s_aTypeName, "none") == 0;
	if(DoButtonLineSize_Menu(&s_AddGroupButton, Localize("Add Group"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_WarList.AddWarType(s_aTypeName, s_GroupColor);
	}

	// ======ONLINE PLAYER LIST======

	Column4.HSplitTop(HeadlineHeight, &Label, &Column4);
	Ui()->DoLabel(&Label, Localize("Online Players"), HeadlineFontSize, TEXTALIGN_ML);
	Column4.HSplitTop(MarginSmall, nullptr, &Column4);

	CUIRect PlayerList;
	Column4.HSplitBottom(0.0f, &PlayerList, &Column4);
	static CListBox s_PlayerListBox;
	s_PlayerListBox.DoStart(30.0f, MAX_CLIENTS, 1, 2, -1, &PlayerList);

	static std::vector<unsigned char> s_vPlayerItemIds;
	static std::vector<CButtonContainer> s_vNameButtons;
	static std::vector<CButtonContainer> s_vClanButtons;

	s_vPlayerItemIds.resize(MAX_CLIENTS);
	s_vNameButtons.resize(MAX_CLIENTS);
	s_vClanButtons.resize(MAX_CLIENTS);

	std::vector<int> vOnlinePlayers;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameClient()->m_Snap.m_apPlayerInfos[i])
			vOnlinePlayers.push_back(i);
	}

	std::sort(vOnlinePlayers.begin(), vOnlinePlayers.end(), [&](int a, int b) {
		return str_comp(GameClient()->m_aClients[a].m_aName, GameClient()->m_aClients[b].m_aName) < 0;
	});

	for(int ClientId : vOnlinePlayers)
	{
		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;

		const CListboxItem Item = s_PlayerListBox.DoNextItem(&s_vPlayerItemIds[ClientId], false);
		if(!Item.m_Visible)
			continue;

		CUIRect PlayerRect, TeeRect, NameRect, ClanRect;
		Item.m_Rect.Margin(0.0f, &PlayerRect);
		PlayerRect.VSplitLeft(25.0f, &TeeRect, &PlayerRect);

		PlayerRect.VSplitMid(&NameRect, &ClanRect, 0);
		PlayerRect = NameRect;
		PlayerRect.x = TeeRect.x;
		PlayerRect.w += TeeRect.w;
		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(ClientId).m_NameColor);
		ColorRGBA NameButtonColor = Ui()->CheckActiveItem(&s_vNameButtons[ClientId]) ? ColorRGBA(1, 1, 1, 0.75f) :
											       (Ui()->HotItem() == &s_vNameButtons[ClientId] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		PlayerRect.Draw(NameButtonColor, IGraphics::CORNER_L, 5.0f);
		Ui()->DoLabel(&NameRect, GameClient()->m_aClients[ClientId].m_aName, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vNameButtons[ClientId], false, &PlayerRect, BUTTONFLAG_ALL))
		{
			s_IsName = 1;
			s_IsClan = 0;
			str_copy(s_aEntryName, GameClient()->m_aClients[ClientId].m_aName);
		}

		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(ClientId).m_ClanColor);
		ColorRGBA ClanButtonColor = Ui()->CheckActiveItem(&s_vClanButtons[ClientId]) ? ColorRGBA(1, 1, 1, 0.75f) :
											       (Ui()->HotItem() == &s_vClanButtons[ClientId] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		ClanRect.Draw(ClanButtonColor, IGraphics::CORNER_R, 5.0f);
		Ui()->DoLabel(&ClanRect, GameClient()->m_aClients[ClientId].m_aClan, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vClanButtons[ClientId], false, &ClanRect, BUTTONFLAG_ALL))
		{
			s_IsName = 0;
			s_IsClan = 1;
			str_copy(s_aEntryClan, GameClient()->m_aClients[ClientId].m_aClan);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		TeeInfo.m_Size = 25.0f;
		vec2 TeeEyeDir = TeeEyeDirection(TeeRect.Center());
		bool Paused = GameClient()->m_aClients[ClientId].m_Paused || GameClient()->m_aClients[ClientId].m_Spec;
		CAnimState AnimState;
		AnimState.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		if(Paused)
			AnimState.Add(&g_pData->m_aAnimations[TeeEyeDir.x < 0 ? ANIM_SIT_LEFT : ANIM_SIT_RIGHT], 0.0f, 1.0f);
		else
			AnimState.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);
		RenderTee(TeeRect.Center() + vec2(-1.0f, 2.5f), TeeEyeDir, &AnimState, &TeeInfo, Paused ? EMOTE_BLINK : EMOTE_NORMAL);
	}
	s_PlayerListBox.DoEnd();
}

void CMenus::RenderSettingsProfiles(CUIRect MainView)

{
	int *pCurrentUseCustomColor = m_Dummy ? &g_Config.m_ClDummyUseCustomColor : &g_Config.m_ClPlayerUseCustomColor;

	const char *pCurrentSkinName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
	const unsigned CurrentColorBody = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody) : -1;
	const unsigned CurrentColorFeet = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet) : -1;
	const int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
	const int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	const char *pCurrentName = m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName;
	const char *pCurrentClan = m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan;

	const CProfile CurrentProfile(
		CurrentColorBody,
		CurrentColorFeet,
		CurrentFlag,
		Emote,
		pCurrentSkinName,
		pCurrentName,
		pCurrentClan);

	static int s_SelectedProfile = -1;

	CUIRect Label, Button;

	auto FRenderProfile = [&](CUIRect Rect, const CProfile &Profile, bool Main) {
		auto FRenderCross = [&](CUIRect Cross) {
			float MaxExtent = std::max(Cross.w, Cross.h);
			TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			const auto TextBoundingBox = TextRender()->TextBoundingBox(MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->Text(Cross.x + (Cross.w - TextBoundingBox.m_W) / 2.0f, Cross.y + (Cross.h - TextBoundingBox.m_H) / 2.0f, MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		};
		{
			CUIRect Skin;
			Rect.VSplitLeft(50.0f, &Skin, &Rect);
			if(!Main && Profile.m_SkinName[0] == '\0')
			{
				FRenderCross(Skin);
			}
			else
			{
				CTeeRenderInfo TeeRenderInfo;
				TeeRenderInfo.Apply(GameClient()->m_Skins.Find(Profile.m_SkinName));
				TeeRenderInfo.ApplyColors(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0, Profile.m_BodyColor, Profile.m_FeetColor);
				TeeRenderInfo.m_Size = 50.0f;
				const vec2 Pos = Skin.Center() + vec2(0.0f, TeeRenderInfo.m_Size / 10.0f); // Prevent overflow from hats
				vec2 Dir = vec2(1.0f, 0.0f);
				if(Main)
				{
					Dir = Ui()->MousePos() - Pos;
					Dir /= TeeRenderInfo.m_Size;
					const float Length = length(Dir);
					if(Length > 1.0f)
						Dir /= Length;
				}
				RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, std::max(0, Profile.m_Emote), Dir, Pos);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Colors;
			Rect.VSplitLeft(10.0f, &Colors, &Rect);
			CUIRect BodyColor{Colors.Center().x - 5.0f, Colors.Center().y - 11.0f, 10.0f, 10.0f};
			CUIRect FeetColor{Colors.Center().x - 5.0f, Colors.Center().y + 1.0f, 10.0f, 10.0f};
			if(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0)
			{
				// Body Color
				Graphics()->DrawRect(BodyColor.x, BodyColor.y, BodyColor.w, BodyColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
				// Feet Color;
				Graphics()->DrawRect(FeetColor.x, FeetColor.y, FeetColor.w, FeetColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
			}
			else
			{
				FRenderCross(BodyColor);
				FRenderCross(FeetColor);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Flag;
			Rect.VSplitRight(50.0f, &Rect, &Flag);
			Flag = {Flag.x, Flag.y + (Flag.h - 25.0f) / 2.0f, Flag.w, 25.0f};
			if(Profile.m_CountryFlag == -2)
				FRenderCross(Flag);
			else
				GameClient()->m_CountryFlags.Render(Profile.m_CountryFlag, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Flag.x, Flag.y, Flag.w, Flag.h);
		}
		Rect.VSplitRight(5.0f, &Rect, nullptr);
		{
			const float Height = Rect.h / 3.0f;
			if(Main)
			{
				char aBuf[256];
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), Localize("Name: %s"), Profile.m_Name);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), Localize("Clan: %s"), Profile.m_Clan);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), Localize("Skin: %s"), Profile.m_SkinName);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
			else
			{
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Name, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Clan, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
		}
	};

	{
		CUIRect Top;
		MainView.HSplitTop(160.0f, &Top, &MainView);
		CUIRect Profiles, Settings, Actions;
		Top.VSplitLeft(300.0f, &Profiles, &Top);
		{
			CUIRect Skin;
			Profiles.HSplitTop(LineSize, &Label, &Profiles);
			Ui()->DoLabel(&Label, Localize("Your profile"), FontSize, TEXTALIGN_ML);
			Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
			Profiles.HSplitTop(50.0f, &Skin, &Profiles);
			FRenderProfile(Skin, CurrentProfile, true);

			// After load
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(LineSize, &Label, &Profiles);
				Ui()->DoLabel(&Label, Localize("After Load"), FontSize, TEXTALIGN_ML);
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(50.0f, &Skin, &Profiles);

				CProfile LoadProfile = CurrentProfile;
				const CProfile &Profile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
				if(g_Config.m_ClProfileSkin && strlen(Profile.m_SkinName) != 0)
					str_copy(LoadProfile.m_SkinName, Profile.m_SkinName);
				if(g_Config.m_ClProfileColors && Profile.m_BodyColor != -1 && Profile.m_FeetColor != -1)
				{
					LoadProfile.m_BodyColor = Profile.m_BodyColor;
					LoadProfile.m_FeetColor = Profile.m_FeetColor;
				}
				if(g_Config.m_ClProfileEmote && Profile.m_Emote != -1)
					LoadProfile.m_Emote = Profile.m_Emote;
				if(g_Config.m_ClProfileName && strlen(Profile.m_Name) != 0)
					str_copy(LoadProfile.m_Name, Profile.m_Name);
				if(g_Config.m_ClProfileClan && (strlen(Profile.m_Clan) != 0 || g_Config.m_ClProfileOverwriteClanWithEmpty))
					str_copy(LoadProfile.m_Clan, Profile.m_Clan);
				if(g_Config.m_ClProfileFlag && Profile.m_CountryFlag != -2)
					LoadProfile.m_CountryFlag = Profile.m_CountryFlag;

				FRenderProfile(Skin, LoadProfile, true);
			}
		}
		Top.VSplitLeft(20.0f, nullptr, &Top);
		Top.VSplitMid(&Settings, &Actions, 20.0f);
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileSkin, Localize("Save/Load Skin"), &g_Config.m_ClProfileSkin, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileColors, Localize("Save/Load Colors"), &g_Config.m_ClProfileColors, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileEmote, Localize("Save/Load Emote"), &g_Config.m_ClProfileEmote, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileName, Localize("Save/Load Name"), &g_Config.m_ClProfileName, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileClan, Localize("Save/Load Clan"), &g_Config.m_ClProfileClan, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileFlag, Localize("Save/Load Flag"), &g_Config.m_ClProfileFlag, &Settings, LineSize);
		}
		{
			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_LoadButton;
			if(DoButton_Menu(&s_LoadButton, Localize("Load"), 0, &Button))
			{
				if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
				{
					CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
					GameClient()->m_SkinProfiles.ApplyProfile(m_Dummy, LoadProfile);
				}
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_SaveButton;
			if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button))
			{
				GameClient()->m_SkinProfiles.AddProfile(
					g_Config.m_ClProfileColors ? CurrentColorBody : -1,
					g_Config.m_ClProfileColors ? CurrentColorFeet : -1,
					g_Config.m_ClProfileFlag ? CurrentFlag : -2,
					g_Config.m_ClProfileEmote ? Emote : -1,
					g_Config.m_ClProfileSkin ? pCurrentSkinName : "",
					g_Config.m_ClProfileName ? pCurrentName : "",
					g_Config.m_ClProfileClan ? pCurrentClan : "");
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			static int s_AllowDelete;
			DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, Localize("Enable Deleting"), &s_AllowDelete, &Actions, LineSize);
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			if(s_AllowDelete)
			{
				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_DeleteButton;
				if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
						GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + s_SelectedProfile);
				Actions.HSplitTop(5.0f, nullptr, &Actions);

				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_OverrideButton;
				if(DoButton_Menu(&s_OverrideButton, Localize("Override"), 0, &Button))
				{
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
					{
						GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile] = CProfile(
							g_Config.m_ClProfileColors ? CurrentColorBody : -1,
							g_Config.m_ClProfileColors ? CurrentColorFeet : -1,
							g_Config.m_ClProfileFlag ? CurrentFlag : -2,
							g_Config.m_ClProfileEmote ? Emote : -1,
							g_Config.m_ClProfileSkin ? pCurrentSkinName : "",
							g_Config.m_ClProfileName ? pCurrentName : "",
							g_Config.m_ClProfileClan ? pCurrentClan : "");
					}
				}
			}
		}
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect Options;
		MainView.HSplitTop(LineSize, &Options, &MainView);

		Options.VSplitLeft(150.0f, &Button, &Options);
		if(DoButton_CheckBox(&m_Dummy, Localize("Dummy"), m_Dummy, &Button))
			m_Dummy = 1 - m_Dummy;

		Options.VSplitLeft(150.0f, &Button, &Options);
		static int s_CustomColorId = 0;
		if(DoButton_CheckBox(&s_CustomColorId, Localize("Custom colors"), *pCurrentUseCustomColor, &Button))
		{
			*pCurrentUseCustomColor = *pCurrentUseCustomColor ? 0 : 1;
			SetNeedSendInfo();
		}

		Button = Options;
		if(DoButton_CheckBox(&g_Config.m_ClProfileOverwriteClanWithEmpty, Localize("Overwrite clan even if empty"), g_Config.m_ClProfileOverwriteClanWithEmpty, &Button))
			g_Config.m_ClProfileOverwriteClanWithEmpty = 1 - g_Config.m_ClProfileOverwriteClanWithEmpty;
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect SelectorRect;
		MainView.HSplitBottom(LineSize + MarginSmall, &MainView, &SelectorRect);
		SelectorRect.HSplitTop(MarginSmall, nullptr, &SelectorRect);

		static CButtonContainer s_ProfilesFile;
		SelectorRect.VSplitLeft(130.0f, &Button, &SelectorRect);
		if(DoButton_Menu(&s_ProfilesFile, Localize("Profiles file"), 0, &Button))
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
	}

	const std::vector<CProfile> &ProfileList = GameClient()->m_SkinProfiles.m_Profiles;
	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, ProfileList.size(), MainView.w / 200.0f, 3, s_SelectedProfile, &MainView, true, IGraphics::CORNER_ALL, true);

	static bool s_Indexes[1024];

	for(size_t i = 0; i < ProfileList.size(); ++i)
	{
		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexes[i], s_SelectedProfile >= 0 && (size_t)s_SelectedProfile == i);
		if(!Item.m_Visible)
			continue;

		FRenderProfile(Item.m_Rect, ProfileList[i], false);
	}

	s_SelectedProfile = s_ListBox.DoEnd();

	CUIRect Tater;
	MainView.HSplitBottom(-30.0f, 0, &Tater);
	Tater.VSplitLeft(135.0f, &Tater, &Tater);
	Ui()->DoLabel(&Tater, "© Tater", 14.0f, TEXTALIGN_ML);
}

void CMenus::RenderSettingsEClient(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = Ui()->IsPopupOpen() ? 0.0f : ScrollSpeed;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	CUIRect Label, Button;
	// left side in settings menu

	CUIRect Automation, FreezeKill, ChatSettings,
		Performance, GoresMode, MenuSettings, FastInput, AntiLatency, FrozenTeeHud, AntiPingSmoothing, GhostTools;
	MainView.VSplitMid(&Automation, &Performance);

	/* Automation */
	{
		static float s_Offset = 0.0f;

		Automation.VMargin(5.0f, &Automation);
		Automation.HSplitTop(245.0f + s_Offset, &Automation, &ChatSettings);
		if(s_ScrollRegion.AddRect(Automation))
		{
			s_Offset = 0.0f;

			Automation.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			Automation.VMargin(Margin, &Automation);

			Automation.HSplitTop(HeaderHeight, &Button, &Automation);
			Ui()->DoLabel(&Button, Localize("Automation"), HeaderSize, HeaderAlignment);
			{
				// group em up
				{
					std::array<float, 2> Sizes = {
						TextRender()->TextBoundingBox(FontSize, "Tabbed reply").m_W,
						TextRender()->TextBoundingBox(FontSize, "Muted Reply").m_W};
					float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 23.5f;

					{
						Automation.HSplitTop(20.0f, &Button, &MainView);

						Button.VSplitLeft(0.0f, 0, &Automation);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClAutoReplyMsg, sizeof(g_Config.m_ClAutoReplyMsg));
						s_ReplyMsg.SetEmptyText("I'm Currently Tabbed Out");

						if(DoButton_CheckBox(&g_Config.m_ClTabbedOutMsg, "Tabbed reply", g_Config.m_ClTabbedOutMsg, &Automation))
							g_Config.m_ClTabbedOutMsg ^= 1;

						if(g_Config.m_ClTabbedOutMsg)
							Ui()->DoEditBox(&s_ReplyMsg, &Button, EditBoxFontSize);
					}
					Automation.HSplitTop(21.0f, &Button, &Automation);
					{
						Automation.HSplitTop(20.0f, &Button, &MainView);

						Button.VSplitLeft(0.0f, 0, &Automation);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClAutoReplyMutedMsg, sizeof(g_Config.m_ClAutoReplyMutedMsg));
						s_ReplyMsg.SetEmptyText("You're muted, I can't see your messages");

						if(DoButton_CheckBox(&g_Config.m_ClReplyMuted, "Muted Reply", g_Config.m_ClReplyMuted, &Automation))
							g_Config.m_ClReplyMuted ^= 1;
						if(g_Config.m_ClReplyMuted)
							Ui()->DoEditBox(&s_ReplyMsg, &Button, EditBoxFontSize);
					}
				}
				Automation.HSplitTop(25.0f, &Button, &Automation);
				{
					{
						const char *Name = g_Config.m_ClNotifyOnJoin ? "Notify on Join Name" : "Notify on Join";
						float Length = TextRender()->TextBoundingBox(FontSize, "Notify on Join Name").m_W + 7.5f; // Give it some breathing room

						Automation.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, 0, &Automation);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_NotifyName;
						s_NotifyName.SetBuffer(g_Config.m_ClAutoNotifyName, sizeof(g_Config.m_ClAutoNotifyName));
						s_NotifyName.SetEmptyText("qxdFox");

						if(DoButton_CheckBox(&g_Config.m_ClNotifyOnJoin, Name, g_Config.m_ClNotifyOnJoin, &Automation))
							g_Config.m_ClNotifyOnJoin ^= 1;

						if(g_Config.m_ClNotifyOnJoin)
							Ui()->DoEditBox(&s_NotifyName, &Button, EditBoxFontSize);
					}

					if(g_Config.m_ClNotifyOnJoin)
					{
						static CLineInput s_NotifyMsg;
						s_NotifyMsg.SetBuffer(g_Config.m_ClAutoNotifyMsg, sizeof(g_Config.m_ClAutoNotifyMsg));
						s_NotifyMsg.SetEmptyText("Your Fav Person Has Joined!");

						float Length = TextRender()->TextBoundingBox(12.5f, "Notify Message").m_W + 3.5f; // Give it some breathing room

						Automation.HSplitTop(21.0f, &Button, &Automation);
						Automation.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						Ui()->DoEditBox(&s_NotifyMsg, &Button, EditBoxFontSize);

						Automation.HSplitTop(3.0f, &Button, &Automation);
						Ui()->DoLabel(&Automation, "Notify Message", 12.5f, TEXTALIGN_LEFT);
						Automation.HSplitTop(-3.0f, &Button, &Automation);
						s_Offset += 20.0f;
					}
				}
				Automation.HSplitTop(25.0f, &Button, &Automation);
				{
					const char *pN = "Execute before connect";
					float Length = TextRender()->TextBoundingBox(12.5f, pN).m_W + 3.5f; // Give it some breathing room

					Automation.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &Automation);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					static CLineInput s_ReplyMsg;
					s_ReplyMsg.SetBuffer(g_Config.m_ClExecuteOnConnect, sizeof(g_Config.m_ClExecuteOnConnect));
					s_ReplyMsg.SetEmptyText("Any Console Command");

					Ui()->DoEditBox(&s_ReplyMsg, &Button, EditBoxFontSize);

					Automation.HSplitTop(3.0f, &Button, &Automation);
					Ui()->DoLabel(&Automation, pN, 12.5f, TEXTALIGN_LEFT);
					Automation.HSplitTop(-3.0f, &Button, &Automation);
				}
				Automation.HSplitTop(25.0f, &Button, &Automation);
				{
					const char *pN = "Execute on join";
					float Length = TextRender()->TextBoundingBox(12.5f, pN).m_W + 3.5f; // Give it some breathing room

					Automation.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &Automation);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					static CLineInput s_ReplyMsg;
					s_ReplyMsg.SetBuffer(g_Config.m_ClRunOnJoinConsole, sizeof(g_Config.m_ClRunOnJoinConsole));
					s_ReplyMsg.SetEmptyText("Any Console Command");

					Ui()->DoEditBox(&s_ReplyMsg, &Button, EditBoxFontSize);

					Automation.HSplitTop(3.0f, &Button, &Automation);
					Ui()->DoLabel(&Automation, pN, 12.5f, TEXTALIGN_LEFT);
					Automation.HSplitTop(-3.0f, &Button, &Automation);
				}
				Automation.HSplitTop(25.0f, &Button, &Automation);
				{
					Automation.HSplitTop(20.0f, &Button, &Automation);

					Button.VSplitLeft(0.0f, 0, &Automation);

					if(DoButton_CheckBox(&g_Config.m_ClNotifyWhenLast, "Show when you're the last player", g_Config.m_ClNotifyWhenLast, &Automation))
						g_Config.m_ClNotifyWhenLast ^= 1;

					if(g_Config.m_ClNotifyWhenLast)
					{
						static CLineInput s_LastMessage;
						s_LastMessage.SetBuffer(g_Config.m_ClNotifyWhenLastText, sizeof(g_Config.m_ClNotifyWhenLastText));
						s_LastMessage.SetEmptyText("Last!");

						float Length = TextRender()->TextBoundingBox(12.5f, "Text to Show").m_W + 3.5f; // Give it some breathing room

						Automation.HSplitTop(21.0f, &Button, &Automation);
						Automation.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(100.0f, &Button, &MainView);

						Ui()->DoEditBox(&s_LastMessage, &Button, EditBoxFontSize);

						Automation.HSplitTop(20.0f, &Button, &Automation);
						Ui()->DoLabel(&Automation, "Text to Show", 12.5f, TEXTALIGN_ML);
						Automation.HSplitTop(-20.0f, &Button, &Automation);

						static CButtonContainer s_LastColor;
						Automation.HSplitTop(-3.0f, &Button, &Automation);
						DoLine_ColorPicker(&s_LastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Automation, Localize(""), &g_Config.m_ClNotifyWhenLastColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClNotifyWhenLastColor)), true);

						Automation.HSplitTop(-25.0f, &Button, &Automation);
						s_Offset += 20.0f;
					}
				}
				Automation.HSplitTop(20.0f, &Button, &Automation);

				Automation.HSplitTop(2.5f, &Button, &Automation);
				Automation.HSplitTop(LineSize, &Button, &Automation);
				if(DoButton_CheckBox(&g_Config.m_ClAutoAddOnNameChange, Localize("Auto Add to Default Lists on Name Change"), g_Config.m_ClAutoAddOnNameChange, &Button))
				{
					g_Config.m_ClAutoAddOnNameChange = g_Config.m_ClAutoAddOnNameChange ? 0 : 1;
				}
				if(g_Config.m_ClAutoAddOnNameChange)
				{
					Automation.HSplitTop(LineSize, &Button, &Automation);
					static int s_NamePlatesStrong = 0;
					if(DoButton_CheckBox(&s_NamePlatesStrong, "Notify you everytime someone gets auto added", g_Config.m_ClAutoAddOnNameChange == 2, &Button))
						g_Config.m_ClAutoAddOnNameChange = g_Config.m_ClAutoAddOnNameChange != 2 ? 2 : 1;
					s_Offset += 20.0f;
				}
				Automation.HSplitTop(2.5f, &Button, &Automation);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNotifyOnMove, "Notify When Player is Being Moved", &g_Config.m_ClNotifyOnMove, &Automation, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiSpawnBlock, "Anti Spawn Block", &g_Config.m_ClAntiSpawnBlock, &Automation, LineSize);
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClAntiSpawnBlock, &Button, "Puts you into a random Team when you Kill and get frozen");
			}
		}
	}

	/* Chat Settings */
	{
		ChatSettings.HSplitTop(Margin, nullptr, &ChatSettings);
		ChatSettings.HSplitTop(395.0f, &ChatSettings, &GhostTools);
		if(s_ScrollRegion.AddRect(ChatSettings))
		{
			ChatSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ChatSettings.VMargin(Margin, &ChatSettings);

			ChatSettings.HSplitTop(HeaderHeight, &Button, &ChatSettings);
			Ui()->DoLabel(&Button, Localize("Chat Settings"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatBubble, ("Show Chat Bubble"), &g_Config.m_ClChatBubble, &ChatSettings, LineSize);
				ChatSettings.HSplitTop(2.5f, &Button, &ChatSettings);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowMutedInConsole, ("Show Messages of Muted People in The Console"), &g_Config.m_ClShowMutedInConsole, &ChatSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideEnemyChat, ("Hide Enemy Chat (Shows in Console)"), &g_Config.m_ClHideEnemyChat, &ChatSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowIdsChat, ("Show Client Ids in Chat"), &g_Config.m_ClShowIdsChat, &ChatSettings, LineSize);

				ChatSettings.HSplitTop(10.0f, &Button, &ChatSettings);

				// Please no one ask me.
				std::array<float, 5> Sizes = {
					TextRender()->TextBoundingBox(FontSize, "Friend Prefix").m_W,
					TextRender()->TextBoundingBox(FontSize, "Spec Prefix").m_W,
					TextRender()->TextBoundingBox(FontSize, "Server Prefix").m_W,
					TextRender()->TextBoundingBox(FontSize, "Client Prefix").m_W,
					TextRender()->TextBoundingBox(FontSize, "Warlist Prefix").m_W,
				};
				float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 20.0f;

				{
					ChatSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChatSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(85.0f, &Button, 0);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClFriendPrefix, sizeof(g_Config.m_ClFriendPrefix));
					s_PrefixMsg.SetEmptyText("alt + num3");
					if(DoButton_CheckBox(&g_Config.m_ClMessageFriend, "Friend Prefix", g_Config.m_ClMessageFriend, &ChatSettings))
					{
						g_Config.m_ClMessageFriend ^= 1;
					}
					Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize);
				}

				// spectate prefix
				ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
				{
					ChatSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChatSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(85.0f, &Button, 0);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClSpecPrefix, sizeof(g_Config.m_ClSpecPrefix));
					s_PrefixMsg.SetEmptyText("alt+7");
					if(DoButton_CheckBox(&g_Config.m_ClSpectatePrefix, "Spec Prefix", g_Config.m_ClSpectatePrefix, &ChatSettings))
					{
						g_Config.m_ClSpectatePrefix ^= 1;
					}
					Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize);
				}

				// server prefix
				ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
				{
					ChatSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChatSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(85.0f, &Button, 0);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClServerPrefix, sizeof(g_Config.m_ClServerPrefix));
					s_PrefixMsg.SetEmptyText("*** ");
					if(DoButton_CheckBox(&g_Config.m_ClChatServerPrefix, "Server Prefix", g_Config.m_ClChatServerPrefix, &ChatSettings))
					{
						g_Config.m_ClChatServerPrefix ^= 1;
					}
					Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize);
				}

				// client prefix
				ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
				{
					ChatSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChatSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(85.0f, &Button, 0);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClClientPrefix, sizeof(g_Config.m_ClClientPrefix));
					s_PrefixMsg.SetEmptyText("alt0151");
					if(DoButton_CheckBox(&g_Config.m_ClChatClientPrefix, "Client Prefix", g_Config.m_ClChatClientPrefix, &ChatSettings))
					{
						g_Config.m_ClChatClientPrefix ^= 1;
					}
					Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize);
				}
				ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
				{
					ChatSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChatSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(85.0f, &Button, 0);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClWarlistPrefix, sizeof(g_Config.m_ClWarlistPrefix));
					s_PrefixMsg.SetEmptyText("alt4");
					if(DoButton_CheckBox(&g_Config.m_ClWarlistPrefixes, "Warlist Prefix", g_Config.m_ClWarlistPrefixes, &ChatSettings))
					{
						g_Config.m_ClWarlistPrefixes ^= 1;
					}
					Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize);
				}
				ChatSettings.HSplitTop(55.0f, &Button, &ChatSettings);
				Ui()->DoLabel(&Button, Localize("Chat Preview"), FontSize + 3, TEXTALIGN_ML);
				ChatSettings.HSplitTop(-15.0f, &Button, &ChatSettings);

				RenderChatPreview(ChatSettings);
			}
		}
	}

	/* Ghost Tools */
	{
		GhostTools.HSplitTop(Margin, nullptr, &GhostTools);
		GhostTools.HSplitTop(180.0f, &GhostTools, nullptr);
		if(s_ScrollRegion.AddRect(GhostTools))
		{
			GhostTools.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			GhostTools.VMargin(Margin, &GhostTools);

			GhostTools.HSplitTop(HeaderHeight, &Button, &GhostTools);
			Ui()->DoLabel(&Button, Localize("Ghost Tools"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowOthersGhosts, Localize("Show unpredicted ghosts for other players"), &g_Config.m_TcShowOthersGhosts, &GhostTools, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSwapGhosts, Localize("Swap ghosts and normal players"), &g_Config.m_TcSwapGhosts, &GhostTools, LineSize);
				GhostTools.HSplitTop(LineSize, &Button, &GhostTools);
				Ui()->DoScrollbarOption(&g_Config.m_TcPredGhostsAlpha, &g_Config.m_TcPredGhostsAlpha, &Button, Localize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
				GhostTools.HSplitTop(LineSize, &Button, &GhostTools);
				Ui()->DoScrollbarOption(&g_Config.m_TcUnpredGhostsAlpha, &g_Config.m_TcUnpredGhostsAlpha, &Button, Localize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHideFrozenGhosts, Localize("Hide ghosts of frozen players"), &g_Config.m_TcHideFrozenGhosts, &GhostTools, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderGhostAsCircle, Localize("Render ghosts as circles"), &g_Config.m_TcRenderGhostAsCircle, &GhostTools, LineSize);

				static CButtonContainer s_ReaderButtonGhost, s_ClearButtonGhost;
				DoLine_KeyReader(GhostTools, s_ReaderButtonGhost, s_ClearButtonGhost, Localize("Toggle ghosts key"), "toggle tc_show_others_ghosts 0 1");
			}
		}
	}
	// right side

	/* Performance */
	{
		Performance.VMargin(5.0f, &Performance);
		Performance.HSplitTop(80.0f, &Performance, &GoresMode);
		if(s_ScrollRegion.AddRect(Performance))
		{
			Performance.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			Performance.VMargin(Margin, &Performance);

			Performance.HSplitTop(HeaderHeight, &Button, &Performance);
			Ui()->DoLabel(&Button, Localize("Performance"), HeaderSize, HeaderAlignment);

			if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHighProcessPriority, ("High DDNet Process Priority"), &g_Config.m_ClHighProcessPriority, &Performance, LineSize))
				GameClient()->m_EClient.SetDDNetProcessPriority(g_Config.m_ClHighProcessPriority);

			if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordNormalProcessPriority, ("Lower Discords Process Priority"), &g_Config.m_ClDiscordNormalProcessPriority, &Performance, LineSize))
			{
				if(g_Config.m_ClDiscordNormalProcessPriority)
					GameClient()->m_EClient.StartDiscordPriorityThread();
			}
		}
	}

	/* Gores Mode */
	{
		GoresMode.HSplitTop(Margin, nullptr, &GoresMode);
		GoresMode.HSplitTop(100.0f, &GoresMode, &FastInput);
		if(s_ScrollRegion.AddRect(GoresMode))
		{
			GoresMode.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			GoresMode.VMargin(Margin, &GoresMode);

			GoresMode.HSplitTop(HeaderHeight, &Button, &GoresMode);
			Ui()->DoLabel(&Button, Localize("Gores Mode"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresMode, ("\"advanced\" Gores Mode"), &g_Config.m_ClGoresMode, &GoresMode, LineSize);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresModeDisableIfWeapons, ("Disable if You Have Any Weapon"), &g_Config.m_ClGoresModeDisableIfWeapons, &GoresMode, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoEnableGoresMode, ("Auto Enable if Gametype is \"Gores\""), &g_Config.m_ClAutoEnableGoresMode, &GoresMode, LineSize);
		}
	}

	/* Fast Input */
	{
		FastInput.HSplitTop(Margin, nullptr, &FastInput);
		FastInput.HSplitTop(g_Config.m_TcFastInput ? 125.0f : 100.0f, &FastInput, &MenuSettings);
		if(s_ScrollRegion.AddRect(FastInput))
		{
			FastInput.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			FastInput.VMargin(Margin, &FastInput);

			FastInput.HSplitTop(HeaderHeight, &Button, &FastInput);
			Ui()->DoLabel(&Button, Localize("Input"), HeaderSize, HeaderAlignment);
			{
				if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, Localize("Fast Input (reduced visual delay)"), &g_Config.m_TcFastInput, &FastInput, LineSize))
					Client()->SendFastInputsInfo(g_Config.m_ClDummy);

				FastInput.HSplitTop(LineSize, &Button, &FastInput);
				if(Ui()->DoScrollbarOption(&g_Config.m_TcFastInputAmount, &g_Config.m_TcFastInputAmount, &Button, "Amount", 1, 40, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE, "ms"))
					Client()->SendFastInputsInfo(g_Config.m_ClDummy);

				if(g_Config.m_TcFastInput)
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, Localize("Extra tick other tees (increases other tees latency, \nmakes dragging slightly easier when using fast input)"), &g_Config.m_TcFastInputOthers, &FastInput, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, "Sub-Tick aiming", &g_Config.m_ClSubTickAiming, &FastInput, LineSize);
			}
		}
	}

	/* Menu Settings */
	{
		MenuSettings.HSplitTop(Margin, nullptr, &MenuSettings);
		MenuSettings.HSplitTop(100.0f, &MenuSettings, &FreezeKill);
		if(s_ScrollRegion.AddRect(MenuSettings))
		{
			MenuSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			MenuSettings.VMargin(Margin, &MenuSettings);

			MenuSettings.HSplitTop(HeaderHeight, &Button, &MenuSettings);
			Ui()->DoLabel(&Button, Localize("Menu Settings"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersInMenu, Localize("Show Settings Icon When Tee's in a Menu"), &g_Config.m_ClShowOthersInMenu, &MenuSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuFriendColor, Localize("Friend Color in Spectate Menu"), &g_Config.m_ClSpecMenuFriendColor, &MenuSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuPrefixes, Localize("Player Prefixes in Spectate Menu"), &g_Config.m_ClSpecMenuPrefixes, &MenuSettings, LineSize);
			}
		}
	}

	/* Freeze Kill */
	{
		static float s_Offset = 0.0f;
		FreezeKill.HSplitTop(Margin, nullptr, &FreezeKill);
		FreezeKill.HSplitTop(75.0f + s_Offset, &FreezeKill, &FrozenTeeHud);
		if(s_ScrollRegion.AddRect(FreezeKill))
		{
			s_Offset = 0.0f;

			FreezeKill.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			FreezeKill.VMargin(Margin, &FreezeKill);

			FreezeKill.HSplitTop(HeaderHeight, &Button, &FreezeKill);
			Ui()->DoLabel(&Button, Localize("Freeze Kill"), HeaderSize, HeaderAlignment);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKill, Localize("Kill on Freeze"), &g_Config.m_ClFreezeKill, &FreezeKill, LineSize);

			if(g_Config.m_ClFreezeKill)
			{
				s_Offset += 95.0f;

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillIgnoreKillProt, Localize("Ignore Kill Protection"), &g_Config.m_ClFreezeKillIgnoreKillProt, &FreezeKill, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillNotMoving, Localize("Don't Kill if Moving"), &g_Config.m_ClFreezeKillNotMoving, &FreezeKill, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillOnlyFullFrozen, Localize("Only Kill if Fully Frozen"), &g_Config.m_ClFreezeKillOnlyFullFrozen, &FreezeKill, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillTeamInView, Localize("Dont Kill if Teammate is in View"), &g_Config.m_ClFreezeKillTeamInView, &FreezeKill, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillWaitMs, Localize("Wait Until Kill"), &g_Config.m_ClFreezeKillWaitMs, &FreezeKill, LineSize);
				if(g_Config.m_ClFreezeKillWaitMs)
				{
					s_Offset += 25.0f;
					FreezeKill.HSplitTop(2 * LineSize, &Button, &FreezeKill);
					Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillMs, &g_Config.m_ClFreezeKillMs, &Button, Localize("Milliseconds to Wait For"), 1, 5000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "ms");
				}
			}
		}
	}

	/* Frozen Tee Hud */
	{
		FrozenTeeHud.HSplitTop(Margin, nullptr, &FrozenTeeHud);
		int Size = 160.0f;
		if(g_Config.m_ClShowFrozenText)
			Size += 20.0f;
		if(g_Config.m_ClWarList && g_Config.m_ClShowFrozenHud)
			Size += 64.0f;

		static int s_Offset = 0;

		FrozenTeeHud.HSplitTop(Size + s_Offset, &FrozenTeeHud, &AntiLatency);
		if(s_ScrollRegion.AddRect(FrozenTeeHud))
		{
			s_Offset = 0;
			FrozenTeeHud.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			FrozenTeeHud.VMargin(Margin, &FrozenTeeHud);

			FrozenTeeHud.HSplitTop(HeaderHeight, &Button, &FrozenTeeHud);
			Ui()->DoLabel(&Button, Localize("Frozen Tee Display"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHud, Localize("Show frozen tee display"), &g_Config.m_ClShowFrozenHud, &FrozenTeeHud, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHudSkins, Localize("Use skins instead of ninja tees"), &g_Config.m_ClShowFrozenHudSkins, &FrozenTeeHud, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFrozenHudTeamOnly, Localize("Only show after joining a team"), &g_Config.m_ClFrozenHudTeamOnly, &FrozenTeeHud, LineSize);

				FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
				Ui()->DoScrollbarOption(&g_Config.m_ClFrozenMaxRows, &g_Config.m_ClFrozenMaxRows, &Button, Localize("Max Rows"), 1, 6);
				FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
				Ui()->DoScrollbarOption(&g_Config.m_ClFrozenHudTeeSize, &g_Config.m_ClFrozenHudTeeSize, &Button, Localize("Tee Size"), 8, 27);

				FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
				if(DoButton_CheckBox(&g_Config.m_ClShowFrozenText, Localize("Tees left alive text"), g_Config.m_ClShowFrozenText >= 1, &Button))
					g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText >= 1 ? 0 : 1;
				if(g_Config.m_ClShowFrozenText)
				{
					FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
					static int s_CountFrozenText = 0;
					if(DoButton_CheckBox(&s_CountFrozenText, Localize("Count frozen tees"), g_Config.m_ClShowFrozenText == 2, &Button))
						g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText != 2 ? 2 : 1;
				}

				// This would be fancy asf as a popup ngl, like generally speaking any of these weird extra
				// settings for other features could* (should) be inside popups cause they dont fucking fit
				// Also, fuck UI coding
				if(g_Config.m_ClWarList && g_Config.m_ClShowFrozenHud)
				{
					FrozenTeeHud.HSplitTop(10.0f, nullptr, &FrozenTeeHud);
					FrozenTeeHud.HSplitTop(FontSize, &Button, &FrozenTeeHud);
					Ui()->DoLabel(&Button, Localize("Only show certain Wartypes in frozen tee display"), FontSize, TEXTALIGN_TL);
					FrozenTeeHud.HSplitTop(FontSize, &Button, &FrozenTeeHud);
					Ui()->DoLabel(&Button, Localize("Select none to show all"), FontSize, TEXTALIGN_TL);
					FrozenTeeHud.HSplitTop(5.0f, nullptr, &FrozenTeeHud);
					FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);

					static CButtonContainer s_OpenedEntries;
					static bool s_Open = false;
					if(Ui()->DoButton_FontIcon(&s_OpenedEntries, s_Open ? FontIcon::CHEVRON_DOWN : FontIcon::CHEVRON_RIGHT, 0, &Button, BUTTONFLAG_LEFT))
						s_Open = !s_Open;

					CUIRect Button2;
					FrozenTeeHud.HSplitTop(0, &Button2, &FrozenTeeHud);

					if(s_Open)
					{
						const size_t WarTypes = GameClient()->m_WarList.m_WarTypes.size();
						s_Offset += WarTypes * LineSize;

						static std::vector<int> s_vFrozenWarTypeIds;
						if(s_vFrozenWarTypeIds.size() < WarTypes)
							s_vFrozenWarTypeIds.resize(WarTypes);

						FrozenTeeHud.HSplitTop(LineSize, &Button2, &FrozenTeeHud);
						if(DoButton_CheckBox(s_vFrozenWarTypeIds.data(), "Show players without entry", IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, 0), &Button2))
							SetFlag(g_Config.m_ClWarlistFrozenTeeFlags, 0, !IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, 0));

						for(size_t Type = 1; Type < WarTypes; Type++)
						{
							char aBuf[64];
							str_format(aBuf, sizeof(aBuf), "%s '%s'", "Show", GameClient()->m_WarList.m_WarTypes.at(Type)->m_aWarName);
							const bool FlagSet = IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, Type);

							FrozenTeeHud.HSplitTop(LineSize, &Button2, &FrozenTeeHud);
							if(DoButton_CheckBox(&s_vFrozenWarTypeIds[Type], aBuf, FlagSet, &Button2))
								SetFlag(g_Config.m_ClWarlistFrozenTeeFlags, Type, !FlagSet);
						}
					}
				}
			}
		}
	}

	/* Anti Latency Tools */
	{
		static float s_Offset = 0.0f;
		AntiLatency.HSplitTop(Margin, nullptr, &AntiLatency);
		AntiLatency.HSplitTop(120.0f + s_Offset, &AntiLatency, &AntiPingSmoothing);
		if(s_ScrollRegion.AddRect(AntiLatency))
		{
			s_Offset = 0.0f;
			AntiLatency.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			AntiLatency.VMargin(Margin, &AntiLatency);

			AntiLatency.HSplitTop(HeaderHeight, &Button, &AntiLatency);
			Ui()->DoLabel(&Button, Localize("Anti Latency Tools"), HeaderSize, HeaderAlignment);
			{
				AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
				Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, Localize("Prediction Margin"), 10, 75, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRemoveAnti, Localize("Remove prediction & antiping in freeze"), &g_Config.m_TcRemoveAnti, &AntiLatency, LineSize);
				if(g_Config.m_TcRemoveAnti)
				{
					if(g_Config.m_TcUnfreezeLagDelayTicks < g_Config.m_TcUnfreezeLagTicks)
						g_Config.m_TcUnfreezeLagDelayTicks = g_Config.m_TcUnfreezeLagTicks;
					AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
					Ui()->DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagTicks, &g_Config.m_TcUnfreezeLagTicks, &Button, Localize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
					Ui()->DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagDelayTicks, &g_Config.m_TcUnfreezeLagDelayTicks, &Button, Localize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					s_Offset += 40.0f;
				}
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcUnpredOthersInFreeze, Localize("Dont predict other players if you are frozen"), &g_Config.m_TcUnpredOthersInFreeze, &AntiLatency, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredMarginInFreeze, Localize("Adjust your prediction margin while frozen"), &g_Config.m_TcPredMarginInFreeze, &AntiLatency, LineSize);
				AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
				if(g_Config.m_TcPredMarginInFreeze)
				{
					Ui()->DoScrollbarOption(&g_Config.m_TcPredMarginInFreezeAmount, &g_Config.m_TcPredMarginInFreezeAmount, &Button, Localize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
					s_Offset += 20.0f;
				}
			}
		}
	}

	/* Anti Ping Smoothing */
	{
		AntiPingSmoothing.HSplitTop(Margin, nullptr, &AntiPingSmoothing);
		AntiPingSmoothing.HSplitTop(120.0f, &AntiPingSmoothing, nullptr);
		if(s_ScrollRegion.AddRect(AntiPingSmoothing))
		{
			AntiPingSmoothing.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			AntiPingSmoothing.VMargin(Margin, &AntiPingSmoothing);

			AntiPingSmoothing.HSplitTop(HeaderHeight, &Button, &AntiPingSmoothing);
			Ui()->DoLabel(&Button, Localize("Anti Ping Smoothing"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingImproved, Localize("Use new smoothing algorithm"), &g_Config.m_TcAntiPingImproved, &AntiPingSmoothing, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingStableDirection, Localize("Optimistic prediction in stable direction"), &g_Config.m_TcAntiPingStableDirection, &AntiPingSmoothing, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingNegativeBuffer, Localize("Remember instability for longer"), &g_Config.m_TcAntiPingNegativeBuffer, &AntiPingSmoothing, LineSize);
				AntiPingSmoothing.HSplitTop(LineSize, &Button, &AntiPingSmoothing);
				Ui()->DoScrollbarOption(&g_Config.m_TcAntiPingUncertaintyScale, &g_Config.m_TcAntiPingUncertaintyScale, &Button, Localize("Uncertainty duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
			}
		}
	}
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsVisual(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = Ui()->IsPopupOpen() ? 0.0f : ScrollSpeed;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	CUIRect Label, Button;

	// left side in settings menu
	CUIRect Cosmetics, Trails, PhysicBalls, ServerRainbow, TileOutlines,
		Miscellaneous, MapOverview, DiscordRpc, ChatBubbles, PlayerIndicator, BgDraw, SweatMode;
	MainView.VSplitMid(&Cosmetics, &Miscellaneous);

	/* Cosmetics */
	{
		bool RainbowOn = g_Config.m_ClRainbowHook || g_Config.m_ClRainbowTees || g_Config.m_ClRainbowWeapon || g_Config.m_ClRainbowOthers;
		static float s_Offset = 0.0f;

		Cosmetics.VMargin(5.0f, &Cosmetics);
		Cosmetics.HSplitTop(235.0f + s_Offset, &Cosmetics, &Trails);
		if(s_ScrollRegion.AddRect(Cosmetics))
		{
			s_Offset = 0.0f;
			Cosmetics.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			Cosmetics.VMargin(Margin, &Cosmetics);

			Cosmetics.HSplitTop(HeaderHeight, &Button, &Cosmetics);
			Ui()->DoLabel(&Button, Localize("Cosmetic Settings"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSmallSkins, ("Small Skins"), &g_Config.m_ClSmallSkins, &Cosmetics, LineMargin);

			static std::vector<const char *> s_EffectDropDownNames;
			s_EffectDropDownNames = {Localize("No Effect"), Localize("Sparkle effect"), Localize("Fire Trail Effect"), Localize("Switch Effect")};
			static CUi::SDropDownState s_EffectDropDownState;
			static CScrollRegion s_EffectDropDownScrollRegion;
			s_EffectDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_EffectDropDownScrollRegion;
			int EffectSelectedOld = g_Config.m_ClEffect;
			CUIRect EffectDropDownRect;
			Cosmetics.HSplitTop(LineSize, &EffectDropDownRect, &Cosmetics);
			const int EffectSelectedNew = Ui()->DoDropDown(&EffectDropDownRect, EffectSelectedOld, s_EffectDropDownNames.data(), s_EffectDropDownNames.size(), s_EffectDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_EffectDropDownState.m_SelectionPopupContext, EffectDropDownRect.x, EffectDropDownRect.y);

			if(s_ScrollRegion.ClipRect())
			{
				const float PosY = EffectDropDownRect.y + 20.0f;
				if(PosY < s_ScrollRegion.ClipRect()->y || PosY > (s_ScrollRegion.ClipRect()->y + s_ScrollRegion.ClipRect()->h))
				{
					Ui()->ClosePopupMenu(&s_EffectDropDownState.m_SelectionPopupContext);
				}
			}

			if(EffectSelectedOld != EffectSelectedNew)
			{
				g_Config.m_ClEffect = EffectSelectedNew;
				if(g_Config.m_ClEffectSpeedOverride)
				{
					if(g_Config.m_ClEffect == EFFECT_SPARKLE)
						g_Config.m_ClEffectSpeed = 75;
					else if(g_Config.m_ClEffect == EFFECT_FIRETRAIL)
						g_Config.m_ClEffectSpeed = 125;
					else if(g_Config.m_ClEffect == EFFECT_SWITCH)
						g_Config.m_ClEffectSpeed = 150;
				}
			}

			Cosmetics.HSplitTop(5.0f, &Button, &Cosmetics);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectColors, ("Effect Color"), &g_Config.m_ClEffectColors, &Cosmetics, LineMargin);

			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClEffectColors, &Cosmetics, "Doesn't work if the sprite already has a set color\nMake the sprite the color you want if it doesn't work");
			if(g_Config.m_ClEffectColors)
			{
				static CButtonContainer s_EffectR;
				Cosmetics.HSplitTop(-3.0f, &Label, &Cosmetics);
				Cosmetics.HSplitTop(-17.0f, &Button, &Cosmetics);
				DoLine_ColorPicker(&s_EffectR, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Cosmetics, Localize(""), &g_Config.m_ClEffectColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClEffectColor)), true);
				Cosmetics.HSplitTop(-10.0f, &Button, &Cosmetics);
			}

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectOthers, ("Effect Others"), &g_Config.m_ClEffectOthers, &Cosmetics, LineMargin);

			Cosmetics.HSplitTop(MarginSmall, &Button, &Cosmetics);

			// ***** Rainbow ***** //
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowTees, Localize("Rainbow Tees"), &g_Config.m_ClRainbowTees, &Cosmetics, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowWeapon, Localize("Rainbow weapons"), &g_Config.m_ClRainbowWeapon, &Cosmetics, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowHook, Localize("Rainbow hook"), &g_Config.m_ClRainbowHook, &Cosmetics, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowOthers, Localize("Rainbow others"), &g_Config.m_ClRainbowOthers, &Cosmetics, LineSize);

			Cosmetics.HSplitTop(MarginExtraSmall, nullptr, &Cosmetics);
			static std::vector<const char *> s_RainbowDropDownNames;
			s_RainbowDropDownNames = {Localize("Rainbow"), Localize("Pulse"), Localize("Black"), Localize("Random")};
			static CUi::SDropDownState s_RainbowDropDownState;
			static CScrollRegion s_RainbowDropDownScrollRegion;
			s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
			int RainbowSelectedOld = g_Config.m_ClRainbowMode - 1;
			CUIRect RainbowDropDownRect;
			Cosmetics.HSplitTop(LineSize, &RainbowDropDownRect, &Cosmetics);
			const int RainbowSelectedNew = Ui()->DoDropDown(&RainbowDropDownRect, RainbowSelectedOld, s_RainbowDropDownNames.data(), s_RainbowDropDownNames.size(), s_RainbowDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_RainbowDropDownState.m_SelectionPopupContext, RainbowDropDownRect.x, RainbowDropDownRect.y);

			if(s_ScrollRegion.ClipRect())
			{
				const float PosY = RainbowDropDownRect.y + 20.0f;
				if(PosY < s_ScrollRegion.ClipRect()->y || PosY > (s_ScrollRegion.ClipRect()->y + s_ScrollRegion.ClipRect()->h))
				{
					Ui()->ClosePopupMenu(&s_RainbowDropDownState.m_SelectionPopupContext);
				}
			}

			if(RainbowSelectedOld != RainbowSelectedNew)
			{
				g_Config.m_ClRainbowMode = RainbowSelectedNew + 1;
			}
			Cosmetics.HSplitTop(MarginExtraSmall, nullptr, &Cosmetics);
			Cosmetics.HSplitTop(LineSize, &Button, &Cosmetics);
			if(RainbowOn)
			{
				s_Offset += 20.0f;
				Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Button, Localize("Rainbow speed"), 0, 200, &CUi::ms_LogarithmicScrollbarScale, 0, "%");
			}
			Cosmetics.HSplitTop(MarginExtraSmall, nullptr, &Cosmetics);
			Cosmetics.HSplitTop(MarginSmall, nullptr, &Cosmetics);
		}
	}

	/* Trails */
	{
		static float s_Offset = 0.0f;
		Trails.HSplitTop(Margin, nullptr, &Trails);
		Trails.HSplitTop(205.0f + s_Offset, &Trails, &PhysicBalls);
		if(s_ScrollRegion.AddRect(Trails))
		{
			s_Offset = 0.0f;
			Trails.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			Trails.VMargin(Margin, &Trails);

			Trails.HSplitTop(HeaderHeight, &Button, &Trails);
			Ui()->DoLabel(&Button, Localize("Tee Trails"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrail, Localize("Enable tee trails"), &g_Config.m_EcTeeTrail, &Trails, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailOthers, Localize("Show other tees' trails"), &g_Config.m_EcTeeTrailOthers, &Trails, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailFade, Localize("Fade trail alpha"), &g_Config.m_EcTeeTrailFade, &Trails, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailTaper, Localize("Taper trail width"), &g_Config.m_EcTeeTrailTaper, &Trails, LineSize);

			Trails.HSplitTop(MarginExtraSmall, nullptr, &Trails);
			std::vector<const char *> vTrailDropDownNames;
			vTrailDropDownNames = {Localize("Solid"), Localize("Tee"), Localize("Rainbow"), Localize("Speed")};
			static CUi::SDropDownState s_TrailDropDownState;
			static CScrollRegion s_TrailDropDownScrollRegion;
			s_TrailDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailDropDownScrollRegion;
			int TrailSelectedOld = g_Config.m_EcTeeTrailColorMode - 1;
			CUIRect TrailDropDownRect;
			Trails.HSplitTop(LineSize, &TrailDropDownRect, &Trails);
			const int TrailSelectedNew = Ui()->DoDropDown(&TrailDropDownRect, TrailSelectedOld, vTrailDropDownNames.data(), vTrailDropDownNames.size(), s_TrailDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_TrailDropDownState.m_SelectionPopupContext, TrailDropDownRect.x, TrailDropDownRect.y);

			if(s_ScrollRegion.ClipRect())
			{
				const float PosY = TrailDropDownRect.y + 20.0f;
				if(PosY < s_ScrollRegion.ClipRect()->y || PosY > (s_ScrollRegion.ClipRect()->y + s_ScrollRegion.ClipRect()->h))
				{
					Ui()->ClosePopupMenu(&s_TrailDropDownState.m_SelectionPopupContext);
				}
			}

			if(TrailSelectedOld != TrailSelectedNew)
			{
				g_Config.m_EcTeeTrailColorMode = TrailSelectedNew + 1;
			}
			Trails.HSplitTop(MarginSmall, nullptr, &Trails);

			static CButtonContainer s_TeeTrailColor;
			if(g_Config.m_EcTeeTrailColorMode == CTrails::COLORMODE_SOLID)
			{
				DoLine_ColorPicker(&s_TeeTrailColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Trails, Localize("Tee trail color"), &g_Config.m_EcTeeTrailColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
				s_Offset = ColorPickerLineSize + ColorPickerLineSpacing;
			}

			Trails.HSplitTop(LineSize, &Button, &Trails);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailWidth, &g_Config.m_EcTeeTrailWidth, &Button, Localize("Trail width"), 0, 20);
			Trails.HSplitTop(LineSize, &Button, &Trails);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailLength, &g_Config.m_EcTeeTrailLength, &Button, Localize("Trail length"), 0, 200);
			Trails.HSplitTop(LineSize, &Button, &Trails);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailAlpha, &g_Config.m_EcTeeTrailAlpha, &Button, Localize("Trail alpha"), 0, 100);
		}
	}
	/* Physic Balls */
	{
		PhysicBalls.HSplitTop(Margin, nullptr, &PhysicBalls);
		PhysicBalls.HSplitTop(120.0f, &PhysicBalls, &ServerRainbow);
		if(s_ScrollRegion.AddRect(PhysicBalls))
		{
			PhysicBalls.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			PhysicBalls.VMargin(Margin, &PhysicBalls);

			PhysicBalls.HSplitTop(HeaderHeight, &Button, &PhysicBalls);
			Ui()->DoLabel(&Button, "Physic Balls", HeaderSize, HeaderAlignment);

			PhysicBalls.HSplitTop(LineSize, &Button, &PhysicBalls);

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Ball amount: %" PRIzu, GameClient()->m_PhysicBalls.GetBallCount());

			CUIRect BallAmountLabel, ClearButton;
			Button.VSplitRight(90.0f, &BallAmountLabel, &ClearButton);
			BallAmountLabel.VSplitRight(MarginSmall, &BallAmountLabel, nullptr);

			Ui()->DoLabel(&BallAmountLabel, aBuf, FontSize, TEXTALIGN_ML);

			static CButtonContainer s_ClearBallsButton;
			const ColorRGBA ButtonColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
			if(DoButtonForceFontSize_Menu(&s_ClearBallsButton, Localize("Clear"), 0, &ClearButton, 12.0f, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			{
				GameClient()->m_PhysicBalls.OnReset();
			}
			PhysicBalls.HSplitTop(MarginSmall, &Button, &PhysicBalls);

			{
				static CLineInput s_NotifyMsg;
				s_NotifyMsg.SetBuffer(g_Config.m_ClPhysicBallsSkin, sizeof(g_Config.m_ClPhysicBallsSkin));
				s_NotifyMsg.SetEmptyText("Volleyball");

				const char *pLabel = "Ball Skin:";
				float Length = TextRender()->TextBoundingBox(FontSize, pLabel).m_W + 3.5f; // Give it some breathing room

				PhysicBalls.HSplitTop(20.0f, &Button, nullptr);

				Button.VSplitLeft(Length, &Label, &Button);
				Button.VSplitLeft(100.0f, &Button, nullptr);

				Ui()->DoEditBox(&s_NotifyMsg, &Button, EditBoxFontSize);

				PhysicBalls.HSplitTop(3.0f, &Button, &PhysicBalls);
				Ui()->DoLabel(&PhysicBalls, pLabel, FontSize, TEXTALIGN_LEFT);
			}
			PhysicBalls.HSplitTop(LineSize, &Button, &PhysicBalls);
			PhysicBalls.HSplitTop(25.0f, &Button, &PhysicBalls);

			CUIRect SpawnButton, SpawnButtonCursor;
			Button.VSplitLeft(110.0f, &SpawnButton, &Button);
			Button.VSplitLeft(MarginSmall, nullptr, &Button);
			Button.VSplitLeft(110.0f, &SpawnButtonCursor, nullptr);

			static CButtonContainer s_SpawnBall, s_OtherBallButton;

			if(DoButtonForceFontSize_Menu(&s_SpawnBall, Localize("New Ball"), 0, &SpawnButton, 12.0f, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			{
				GameClient()->m_PhysicBalls.NewBallPlayer(60.0f);
			}

			if(DoButtonForceFontSize_Menu(&s_OtherBallButton, Localize("New Ball Cursor"), 0, &SpawnButtonCursor, 12.0f, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			{
				GameClient()->m_PhysicBalls.NewBallCursor(60.0f);
			}
		}
	}
	/* Server-Side Rainbow */
	{
		ServerRainbow.HSplitTop(Margin, nullptr, &ServerRainbow);
		ServerRainbow.HSplitTop(260.0f, &ServerRainbow, &PlayerIndicator);
		if(s_ScrollRegion.AddRect(ServerRainbow))
		{
			ServerRainbow.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ServerRainbow.VMargin(Margin, &ServerRainbow);

			ServerRainbow.HSplitTop(HeaderHeight, &Button, &ServerRainbow);
			Ui()->DoLabel(&Button, Localize("Server-Side Rainbow"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClServerRainbow, Localize("Enable Serverside Rainbow"), &g_Config.m_ClServerRainbow, &ServerRainbow, LineSize);

			ColorHSLA Color(GameClient()->m_EClient.m_PreviewRainbowColor[g_Config.m_ClDummy]);
			const char *apLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};
			const float SizePerEntry = 20.0f;
			const float MarginPerEntry = 5.0f;
			const float PreviewMargin = 2.5f;
			const float PreviewHeight = 40.0f + 2 * PreviewMargin;
			const float OffY = (SizePerEntry + MarginPerEntry) * 3 - PreviewHeight;

			CUIRect Preview;
			ServerRainbow.VSplitLeft(PreviewHeight, &Preview, &ServerRainbow);
			Preview.HSplitTop(OffY / 2.0f, nullptr, &Preview);
			Preview.HSplitTop(PreviewHeight, &Preview, nullptr);

			auto &&RenderHueRect = [&](CUIRect *pColorRect) {
				float CurXOff = pColorRect->x;
				const float SizeColor = pColorRect->w / 6;

				// red to yellow
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 1, 0, 0, 1),
						IGraphics::CColorVertex(1, 1, 1, 0, 1),
						IGraphics::CColorVertex(2, 1, 0, 0, 1),
						IGraphics::CColorVertex(3, 1, 1, 0, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}

				// yellow to green
				CurXOff += SizeColor;
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 1, 1, 0, 1),
						IGraphics::CColorVertex(1, 0, 1, 0, 1),
						IGraphics::CColorVertex(2, 1, 1, 0, 1),
						IGraphics::CColorVertex(3, 0, 1, 0, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}

				CurXOff += SizeColor;
				// green to turquoise
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 0, 1, 0, 1),
						IGraphics::CColorVertex(1, 0, 1, 1, 1),
						IGraphics::CColorVertex(2, 0, 1, 0, 1),
						IGraphics::CColorVertex(3, 0, 1, 1, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}

				CurXOff += SizeColor;
				// turquoise to blue
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 0, 1, 1, 1),
						IGraphics::CColorVertex(1, 0, 0, 1, 1),
						IGraphics::CColorVertex(2, 0, 1, 1, 1),
						IGraphics::CColorVertex(3, 0, 0, 1, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}

				CurXOff += SizeColor;
				// blue to purple
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 0, 0, 1, 1),
						IGraphics::CColorVertex(1, 1, 0, 1, 1),
						IGraphics::CColorVertex(2, 0, 0, 1, 1),
						IGraphics::CColorVertex(3, 1, 0, 1, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}

				CurXOff += SizeColor;
				// purple to red
				{
					IGraphics::CColorVertex aColorVertices[] = {
						IGraphics::CColorVertex(0, 1, 0, 1, 1),
						IGraphics::CColorVertex(1, 1, 0, 0, 1),
						IGraphics::CColorVertex(2, 1, 0, 1, 1),
						IGraphics::CColorVertex(3, 1, 0, 0, 1)};
					Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

					IGraphics::CFreeformItem Freeform(
						CurXOff, pColorRect->y,
						CurXOff + SizeColor, pColorRect->y,
						CurXOff, pColorRect->y + pColorRect->h,
						CurXOff + SizeColor, pColorRect->y + pColorRect->h);
					Graphics()->QuadsDrawFreeform(&Freeform, 1);
				}
			};

			auto &&RenderSaturationRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
				ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
				ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

				LeftColor.s = 0.0f;
				RightColor.s = 1.0f;

				const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
				const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

				Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

				IGraphics::CFreeformItem Freeform(
					pColorRect->x, pColorRect->y,
					pColorRect->x + pColorRect->w, pColorRect->y,
					pColorRect->x, pColorRect->y + pColorRect->h,
					pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
				Graphics()->QuadsDrawFreeform(&Freeform, 1);
			};

			auto &&RenderLightingRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
				ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
				ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

				LeftColor.l = ColorHSLA::DARKEST_LGT;
				RightColor.l = 1.0f;

				const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
				const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

				Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

				IGraphics::CFreeformItem Freeform(
					pColorRect->x, pColorRect->y,
					pColorRect->x + pColorRect->w, pColorRect->y,
					pColorRect->x, pColorRect->y + pColorRect->h,
					pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
				Graphics()->QuadsDrawFreeform(&Freeform, 1);
			};

			for(int i = 0; i < 3; i++)
			{
				CUIRect Button2, Label2;
				ServerRainbow.HSplitTop(SizePerEntry, &Button2, &ServerRainbow);
				ServerRainbow.HSplitTop(MarginPerEntry, nullptr, &ServerRainbow);
				Button2.VSplitLeft(10.0f, nullptr, &Button2);
				Button2.VSplitLeft(100.0f, &Label2, &Button2);

				Button2.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

				CUIRect Rail;
				Button2.Margin(2.0f, &Rail);

				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "%s: %03d", apLabels[i], round_to_int(Color[i] * 255.0f));
				Ui()->DoLabel(&Label2, aBuf, 14.0f, TEXTALIGN_ML);

				ColorRGBA HandleColor;
				Graphics()->TextureClear();
				Graphics()->TrianglesBegin();
				if(i == 0)
				{
					RenderHueRect(&Rail);
					HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f));
				}
				else if(i == 1)
				{
					RenderSaturationRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f)));
					HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f));
				}
				else if(i == 2)
				{
					RenderLightingRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f)));
					HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(ColorHSLA::DARKEST_LGT));
				}
				Graphics()->TrianglesEnd();

				Color[i] = Ui()->DoServerSideRainbowScrollbar(&Color[i], &Button2, Color[i], &HandleColor, i != 0);
				GameClient()->m_EClient.m_RainbowSat[g_Config.m_ClDummy] = Color.s * 255.0f - 1;
				GameClient()->m_EClient.m_RainbowLht[g_Config.m_ClDummy] = Color.l * 255.0f - 1;
			}

			ServerRainbow.VSplitLeft(-42, &Button, &ServerRainbow);
			ServerRainbow.HSplitTop(2 * LineSize, &Button, &ServerRainbow);
			Ui()->DoScrollbarOption(&GameClient()->m_EClient.m_RainbowSpeed, &GameClient()->m_EClient.m_RainbowSpeed, &Button, Localize("Rainbow Speed"), 1, 5000, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

			ServerRainbow.VSplitLeft(-93, &Button, &ServerRainbow);
			{
				CTeeRenderInfo TeeRenderInfo;

				TeeRenderInfo.m_Size = 75.0f;

				bool PUseCustomColor = g_Config.m_ClPlayerUseCustomColor;
				int PBodyColor = g_Config.m_ClPlayerColorBody;
				int PFeetColor = g_Config.m_ClPlayerColorFeet;

				bool DUseCustomColor = g_Config.m_ClDummyUseCustomColor;
				int DBodyColor = g_Config.m_ClDummyColorBody;
				int DFeetColor = g_Config.m_ClDummyColorFeet;

				if(GameClient()->m_EClient.m_ServersideDelay[g_Config.m_ClDummy] < time_get() && GameClient()->m_EClient.m_ShowServerSide)
				{
					int Delay = g_Config.m_SvInfoChangeDelay;
					if(Client()->State() != IClient::STATE_ONLINE)
						Delay = 5.0f;

					m_MenusRainbowColor = GameClient()->m_EClient.m_PreviewRainbowColor[g_Config.m_ClDummy];
					GameClient()->m_EClient.m_ServersideDelay[g_Config.m_ClDummy] = time_get() + time_freq() * Delay;
				}
				else if(!GameClient()->m_EClient.m_ShowServerSide)
					m_MenusRainbowColor = GameClient()->m_EClient.m_PreviewRainbowColor[g_Config.m_ClDummy];

				if(g_Config.m_ClServerRainbow)
				{
					if(GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy])
						PBodyColor = DBodyColor = m_MenusRainbowColor;
					if(GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy])
						PFeetColor = DFeetColor = m_MenusRainbowColor;
					PUseCustomColor = DUseCustomColor = true;
				}

				TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
				TeeRenderInfo.ApplyColors(PUseCustomColor, PBodyColor, PFeetColor);

				if(g_Config.m_ClDummy)
				{
					TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClDummySkin));
					TeeRenderInfo.ApplyColors(DUseCustomColor, DBodyColor, DFeetColor);
				}

				RenderTee(Preview.Center() + vec2(0, 4), TeeEyeDirection(Preview.Center()), CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL);
			}
			ServerRainbow.VSplitLeft(88, &Button, &ServerRainbow);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], "Rainbow Body", &GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], &ServerRainbow, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], "Rainbow Feet", &GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], &ServerRainbow, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_BothPlayers, "Do Dummy and Main Player at the same time", &GameClient()->m_EClient.m_BothPlayers, &ServerRainbow, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_ShowServerSide, "Show what it'll look like Server-side", &GameClient()->m_EClient.m_ShowServerSide, &ServerRainbow, LineSize);
		}
	}

	/* Player Indicator */
	{
		static float s_Offset = 0.0f;
		PlayerIndicator.HSplitTop(Margin, nullptr, &PlayerIndicator);

		PlayerIndicator.HSplitTop(g_Config.m_ClPlayerIndicator ? 270.0f + s_Offset : 80.0f, &PlayerIndicator, nullptr);
		if(s_ScrollRegion.AddRect(PlayerIndicator))
		{
			s_Offset = 0.0f;
			PlayerIndicator.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			PlayerIndicator.VMargin(Margin, &PlayerIndicator);

			PlayerIndicator.HSplitTop(HeaderHeight, &Button, &PlayerIndicator);
			Ui()->DoLabel(&Button, Localize("Player Indicator"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, Localize("Show any enabled Indicators"), &g_Config.m_ClPlayerIndicator, &PlayerIndicator, LineSize);

				if(g_Config.m_ClPlayerIndicator)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorHideOnScreen, Localize("Hide indicator for tees on your screen"), &g_Config.m_ClIndicatorHideOnScreen, &PlayerIndicator, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicatorFreeze, Localize("Show only freeze Players"), &g_Config.m_ClPlayerIndicatorFreeze, &PlayerIndicator, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTeamOnly, Localize("Only show after joining a team"), &g_Config.m_ClIndicatorTeamOnly, &PlayerIndicator, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTees, Localize("Render tiny tees instead of circles"), &g_Config.m_ClIndicatorTees, &PlayerIndicator, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicator, Localize("Use warlist groups for indicator"), &g_Config.m_ClWarListIndicator, &PlayerIndicator, LineSize);
					PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorRadius, &g_Config.m_ClIndicatorRadius, &Button, Localize("Indicator size"), 1, 16);
					PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOpacity, &g_Config.m_ClIndicatorOpacity, &Button, Localize("Indicator opacity"), 0, 100);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorVariableDistance, Localize("Change indicator offset based on distance to other tees"), &g_Config.m_ClIndicatorVariableDistance, &PlayerIndicator, LineSize);
					if(g_Config.m_ClIndicatorVariableDistance)
					{
						PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator min offset"), 16, 200);
						PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffsetMax, &g_Config.m_ClIndicatorOffsetMax, &Button, Localize("Indicator max offset"), 16, 200);
						PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorMaxDistance, &g_Config.m_ClIndicatorMaxDistance, &Button, Localize("Indicator max distance"), 500, 7000);
						s_Offset += 40.0f;
					}
					else
					{
						PlayerIndicator.HSplitTop(LineSize, &Button, &PlayerIndicator);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator offset"), 16, 200);
					}
					if(g_Config.m_ClWarListIndicator)
					{
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorColors, Localize("Use warlist colors instead of regular colors"), &g_Config.m_ClWarListIndicatorColors, &PlayerIndicator, LineSize);
						char aBuf[128];
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorAll, Localize("Show all warlist groups"), &g_Config.m_ClWarListIndicatorAll, &PlayerIndicator, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(1)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorEnemy, aBuf, &g_Config.m_ClWarListIndicatorEnemy, &PlayerIndicator, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(2)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorTeam, aBuf, &g_Config.m_ClWarListIndicatorTeam, &PlayerIndicator, LineSize);
						s_Offset += 80.0f;
					}
					if(!g_Config.m_ClWarListIndicatorColors || !g_Config.m_ClWarListIndicator)
					{
						static CButtonContainer s_IndicatorAliveColorId, s_IndicatorDeadColorId, s_IndicatorSavedColorId;
						DoLine_ColorPicker(&s_IndicatorAliveColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator alive color"), &g_Config.m_ClIndicatorAlive, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorAlive)), false);
						DoLine_ColorPicker(&s_IndicatorDeadColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator in freeze color"), &g_Config.m_ClIndicatorFreeze, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorFreeze)), false);
						DoLine_ColorPicker(&s_IndicatorSavedColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator safe color"), &g_Config.m_ClIndicatorSaved, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorSaved)), false);
						s_Offset += 60.0f;
					}
				}
			}
		}
	}

	// right side in settings menu

	/* Miscellaneous */
	{
		int Size = 260;
		if(g_Config.m_ClWhiteFeet)
			Size += LineSize;

		Miscellaneous.VMargin(5.0f, &Miscellaneous);
		Miscellaneous.HSplitTop(Size, &Miscellaneous, &MapOverview);
		if(s_ScrollRegion.AddRect(Miscellaneous))
		{
			Miscellaneous.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			Miscellaneous.VMargin(Margin, &Miscellaneous);

			Miscellaneous.HSplitTop(HeaderHeight, &Button, &Miscellaneous);
			Ui()->DoLabel(&Button, Localize("Miscellaneous"), HeaderSize, HeaderAlignment);
			{
				// T-Client
				{
					static std::vector<const char *> s_FontDropDownNames = {};
					static CUi::SDropDownState s_FontDropDownState;
					static CScrollRegion s_FontDropDownScrollRegion;
					s_FontDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_FontDropDownScrollRegion;
					s_FontDropDownState.m_SelectionPopupContext.m_SpecialFontRenderMode = true;
					int FontSelectedOld = -1;
					for(size_t i = 0; i < TextRender()->GetCustomFaces()->size(); ++i)
					{
						if(s_FontDropDownNames.size() != TextRender()->GetCustomFaces()->size())
							s_FontDropDownNames.push_back(TextRender()->GetCustomFaces()->at(i).c_str());

						if(str_find_nocase(g_Config.m_ClCustomFont, TextRender()->GetCustomFaces()->at(i).c_str()))
							FontSelectedOld = i;
					}
					CUIRect FontDropDownRect, FontDirectory;
					Miscellaneous.HSplitTop(LineSize, &FontDropDownRect, &Miscellaneous);

					float Length = TextRender()->TextBoundingBox(FontSize, "Custom Font:").m_W + 3.5f;

					FontDropDownRect.VSplitLeft(Length, &Label, &FontDropDownRect);
					FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
					FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

					Ui()->DoLabel(&Label, Localize("Custom Font:"), FontSize, TEXTALIGN_ML);
					const int FontSelectedNew = Ui()->DoDropDown(&FontDropDownRect, FontSelectedOld, s_FontDropDownNames.data(), s_FontDropDownNames.size(), s_FontDropDownState);
					Ui()->UpdatePopupMenuOffset(&s_FontDropDownState.m_SelectionPopupContext, FontDropDownRect.x, FontDropDownRect.y);

					if(s_ScrollRegion.ClipRect())
					{
						const float PosY = FontDropDownRect.y + 20.0f;
						if(PosY < s_ScrollRegion.ClipRect()->y || PosY > (s_ScrollRegion.ClipRect()->y + s_ScrollRegion.ClipRect()->h))
						{
							Ui()->ClosePopupMenu(&s_FontDropDownState.m_SelectionPopupContext);
						}
					}

					if(FontSelectedOld != FontSelectedNew)
					{
						str_copy(g_Config.m_ClCustomFont, s_FontDropDownNames[FontSelectedNew]);
						TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

						// Reload *hopefully* all Fonts
						TextRender()->OnPreWindowResize();
						GameClient()->OnWindowResize();
						GameClient()->Editor()->OnWindowResize();
						GameClient()->m_MapImages.SetTextureScale(101);
						GameClient()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
					}
					static CButtonContainer s_FontDirectoryId;
					if(Ui()->DoButton_FontIcon(&s_FontDirectoryId, FontIcon::FOLDER, 0, &FontDirectory, BUTTONFLAG_LEFT))
					{
						Storage()->CreateFolder("data/entity", IStorage::TYPE_ABSOLUTE);
						Storage()->CreateFolder("data/entity/fonts", IStorage::TYPE_ABSOLUTE);
						Client()->ViewFile("data/entity/fonts");
					}
				}

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, ("Show Ping Circles Next To Names"), &g_Config.m_ClPingNameCircle, &Miscellaneous, LineSize);

				Miscellaneous.HSplitTop(5.0f, &Button, &Miscellaneous);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeStars, Localize("Freeze stars"), &g_Config.m_ClFreezeStars, &Miscellaneous, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFrozenKatana, Localize("Show katana on frozen players"), &g_Config.m_TcFrozenKatana, &Miscellaneous, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClColorFrozenTeeBody, Localize("Colored frozen tee skins"), &g_Config.m_ClColorFrozenTeeBody, &Miscellaneous, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWhiteFeet, Localize("Render feet as white feet"), &g_Config.m_ClWhiteFeet, &Miscellaneous, LineSize);
				CUIRect FeetBox;
				if(g_Config.m_ClWhiteFeet)
				{
					Miscellaneous.HSplitTop(LineSize + MarginExtraSmall, &FeetBox, &Miscellaneous);
					FeetBox.HSplitTop(MarginExtraSmall, nullptr, &FeetBox);
					FeetBox.VSplitMid(&FeetBox, nullptr);
					static CLineInput s_WhiteFeet(g_Config.m_ClWhiteFeetSkin, sizeof(g_Config.m_ClWhiteFeetSkin));
					s_WhiteFeet.SetEmptyText("x_ninja");
					Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize);
				}

				Miscellaneous.HSplitTop(5.0f, &Button, &Miscellaneous);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClScoreboardOutlineTeams, Localize("Outline Teams in Scoreboard"), &g_Config.m_ClScoreboardOutlineTeams, &Miscellaneous, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRevertTeamColors, Localize("Use Old Team Colors"), &g_Config.m_ClRevertTeamColors, &Miscellaneous, LineSize);

				Miscellaneous.HSplitTop(5.0f, &Button, &Miscellaneous);

				{
					static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}, {}};
					static const std::vector<const char *> s_vTooltips = {
						Localize("Don't show moving tiles in entities"),
						Localize("Use map design for moving tilesin entities"),
						Localize("Use selected asset colors for moving tiles in entities"),
						Localize("Use asset colors and map design for moving tiles in entities"),
					};
					int Value = g_Config.m_ClShowMovingTilesEntities;
					if(DoLine_RadioMenu_Compact(Miscellaneous, Localize("Moving Tiles:"),
						   s_vButtonContainers,
						   {"Off", "Design", "Entity", "Both"},
						   {0, 1, 2, 3},
						   Value,
						   5.0f,
						   &s_vTooltips))
					{
						g_Config.m_ClShowMovingTilesEntities = Value;
					}
				}

				Miscellaneous.HSplitTop(5.0f, &Button, &Miscellaneous);
				Miscellaneous.HSplitTop(LineSize, &Button, &Miscellaneous);
				Ui()->DoScrollbarOption(&g_Config.m_ClCursorOpacitySpec, &g_Config.m_ClCursorOpacitySpec, &Button, Localize("Cursor Opacity in Spec"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
			}
		}
	}

	/* Map Overview */
	{
		MapOverview.HSplitTop(Margin, nullptr, &MapOverview);
		MapOverview.HSplitTop(100.0f, &MapOverview, &DiscordRpc);
		if(s_ScrollRegion.AddRect(MapOverview))
		{
			MapOverview.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			MapOverview.VMargin(Margin, &MapOverview);

			MapOverview.HSplitTop(HeaderHeight, &Button, &MapOverview);
			Ui()->DoLabel(&Button, Localize("Map Overview"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMapOverview, "Enable map overview", &g_Config.m_ClMapOverview, &MapOverview, LineSize);
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMapOverview, &Button, "Renders areas darker that you have already explored", FontSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMapOverviewSpectatingOnly, "Only show map overview while spectating", &g_Config.m_ClMapOverviewSpectatingOnly, &MapOverview, LineSize);
				MapOverview.HSplitTop(LineSize, &Button, &MapOverview);
				Ui()->DoScrollbarOption(&g_Config.m_ClMapOverviewOpacity, &g_Config.m_ClMapOverviewOpacity, &Button, "Explored area opacity", 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
			}
		}
	}

#if defined(CONF_DISCORD)
	/* Discord RPC */
	{
		DiscordRpc.HSplitTop(Margin, nullptr, &DiscordRpc);
		DiscordRpc.HSplitTop(125.0f, &DiscordRpc, &ChatBubbles);
		if(s_ScrollRegion.AddRect(DiscordRpc))
		{
			DiscordRpc.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			DiscordRpc.VMargin(Margin, &DiscordRpc);

			DiscordRpc.HSplitTop(HeaderHeight, &Button, &DiscordRpc);
			Ui()->DoLabel(&Button, Localize("Discord RPC"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordRPC, "Use Discord Rich Presence", &g_Config.m_ClDiscordRPC, &DiscordRpc, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordMapStatus, "Show What Map you're on", &g_Config.m_ClDiscordMapStatus, &DiscordRpc, LineSize);

				std::array<float, 2> Sizes = {
					TextRender()->TextBoundingBox(FontSize, "Online Message:").m_W,
					TextRender()->TextBoundingBox(FontSize, "Offline Message:").m_W};
				float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 3.5f;

				{
					DiscordRpc.HSplitTop(19.9f, &Button, &MainView);

					DiscordRpc.HSplitTop(2.5f, &Label, &Label);
					Ui()->DoLabel(&Label, "Online Message:", FontSize, TEXTALIGN_TL);

					Button.VSplitLeft(0.0f, 0, &DiscordRpc);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClDiscordOnlineStatus, sizeof(g_Config.m_ClDiscordOnlineStatus));
					s_PrefixMsg.SetEmptyText("Online");
					if(Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize))
						Client()->DiscordRPCchange();
				}

				DiscordRpc.HSplitTop(21.0f, &Button, &DiscordRpc);
				{
					DiscordRpc.HSplitTop(19.9f, &Button, &MainView);

					DiscordRpc.HSplitTop(2.5f, &Label, &Label);
					Ui()->DoLabel(&Label, "Offline Message:", FontSize, TEXTALIGN_TL);

					Button.VSplitLeft(0.0f, 0, &DiscordRpc);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					static CLineInput s_PrefixMsg;
					s_PrefixMsg.SetBuffer(g_Config.m_ClDiscordOfflineStatus, sizeof(g_Config.m_ClDiscordOfflineStatus));
					s_PrefixMsg.SetEmptyText("Offline");
					if(Ui()->DoEditBox(&s_PrefixMsg, &Button, EditBoxFontSize))
						Client()->DiscordRPCchange();
				}
			}
		}
	}

#else
	/* Discord RPC */
	{
		DiscordRpc.HSplitTop(Margin, nullptr, &DiscordRpc);
		DiscordRpc.HSplitTop(85.0f, &DiscordRpc, &ChatBubbles);
		if(s_ScrollRegion.AddRect(DiscordRpc))
		{
			DiscordRpc.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			DiscordRpc.VMargin(Margin, &DiscordRpc);

			DiscordRpc.HSplitTop(HeaderHeight, &Button, &DiscordRpc);
			Ui()->DoLabel(&Button, Localize("Discord RPC"), HeaderSize, HeaderAlignment);
			{
				Ui()->DoLabel(&DiscordRpc, "You need to compile with -DDISCORD=ON to use discord rpc", FontSize, TEXTALIGN_ML);
			}
		}
	}

#endif

	/* Chat Bubbles */
	{
		ChatBubbles.HSplitTop(Margin, nullptr, &ChatBubbles);
		ChatBubbles.HSplitTop(145.0f, &ChatBubbles, &TileOutlines);
		if(s_ScrollRegion.AddRect(ChatBubbles))
		{
			ChatBubbles.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ChatBubbles.VMargin(Margin, &ChatBubbles);

			ChatBubbles.HSplitTop(HeaderHeight, &Button, &ChatBubbles);
			Ui()->DoLabel(&Button, Localize("Chat Bubbles"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatBubbles, Localize("Show Chatbubbles above players"), &g_Config.m_ClChatBubbles, &ChatBubbles, LineSize);
				ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
				Ui()->DoScrollbarOption(&g_Config.m_ClChatBubbleSize, &g_Config.m_ClChatBubbleSize, &Button, Localize("Chat Bubble Size"), 20, 30);
				ChatBubbles.HSplitTop(MarginSmall, &Button, &ChatBubbles);
				ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleShowTime, &g_Config.m_ClChatBubbleShowTime, &Button, Localize("Show the Bubbles for"), 200, 1000, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
				ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeIn, &g_Config.m_ClChatBubbleFadeIn, &Button, Localize("fade in for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
				ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeOut, &g_Config.m_ClChatBubbleFadeOut, &Button, Localize("fade out for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
			}
		}
	}

	/* Tile Outlines */
	{
		static float s_Offset = 0.0f;
		TileOutlines.HSplitTop(Margin, nullptr, &TileOutlines);
		TileOutlines.HSplitTop(g_Config.m_ClOutline ? 230.0f + s_Offset : 80.0f, &TileOutlines, &BgDraw);
		if(s_ScrollRegion.AddRect(TileOutlines))
		{
			s_Offset = 0.0f;
			TileOutlines.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			TileOutlines.VMargin(Margin, &TileOutlines);

			TileOutlines.HSplitTop(HeaderHeight, &Button, &TileOutlines);
			Ui()->DoLabel(&Button, Localize("Tile Outlines"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutline, Localize("Show any enabled outlines"), &g_Config.m_ClOutline, &TileOutlines, LineSize);

				if(g_Config.m_ClOutline)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineEntities, Localize("Only show outlines in entities"), &g_Config.m_ClOutlineEntities, &TileOutlines, LineSize);

					static CButtonContainer s_OutlineColorFreezeId, s_OutlineColorSolidId, s_OutlineColorTeleId, s_OutlineColorUnfreezeId, s_OutlineColorKillId;

					TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);

					const float ScrollBarOffset = 8.5f;

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineFreeze, Localize("Outline freezes & deeps"), &g_Config.m_ClOutlineFreeze, &TileOutlines, LineSize);
					TileOutlines.HSplitTop(-20.0f, &Button, &TileOutlines);
					DoLine_ColorPicker(&s_OutlineColorFreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutlines, Localize(""), &g_Config.m_ClOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false, nullptr, true);
					if(g_Config.m_ClOutlineFreeze)
					{
						s_Offset += 25.0f - ScrollBarOffset;
						TileOutlines.HSplitTop(-ScrollBarOffset, &Button, &TileOutlines);
						TileOutlines.HSplitTop(LineSize, &Button, &TileOutlines);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthFreeze, &g_Config.m_ClOutlineWidthFreeze, &Button, Localize("Freeze width"), 1, 16);
						TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineUnfreeze, Localize("Outline unfreezes & undeeps"), &g_Config.m_ClOutlineUnfreeze, &TileOutlines, LineSize);
					TileOutlines.HSplitTop(-20.0f, &Button, &TileOutlines);
					DoLine_ColorPicker(&s_OutlineColorUnfreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutlines, Localize(""), &g_Config.m_ClOutlineColorUnfreeze, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorUnfreeze)), false, nullptr, true);
					if(g_Config.m_ClOutlineUnfreeze)
					{
						s_Offset += 25.0f - ScrollBarOffset;
						TileOutlines.HSplitTop(-ScrollBarOffset, &Button, &TileOutlines);
						TileOutlines.HSplitTop(LineSize, &Button, &TileOutlines);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthUnfreeze, &g_Config.m_ClOutlineWidthUnfreeze, &Button, Localize("Unfreeze width"), 1, 16);
						TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineSolid, Localize("Outline solids"), &g_Config.m_ClOutlineSolid, &TileOutlines, LineSize);
					TileOutlines.HSplitTop(-20.0f, &Button, &TileOutlines);
					DoLine_ColorPicker(&s_OutlineColorSolidId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutlines, Localize(""), &g_Config.m_ClOutlineColorSolid, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorSolid)), false, nullptr, true);
					if(g_Config.m_ClOutlineSolid)
					{
						s_Offset += 25.0f - ScrollBarOffset;
						TileOutlines.HSplitTop(-ScrollBarOffset, &Button, &TileOutlines);
						TileOutlines.HSplitTop(LineSize, &Button, &TileOutlines);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthSolid, &g_Config.m_ClOutlineWidthSolid, &Button, Localize("Solid width"), 1, 16);
						TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineTele, Localize("Outline teleporters"), &g_Config.m_ClOutlineTele, &TileOutlines, LineSize);
					TileOutlines.HSplitTop(-20.0f, &Button, &TileOutlines);
					DoLine_ColorPicker(&s_OutlineColorTeleId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutlines, Localize(""), &g_Config.m_ClOutlineColorTele, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorTele)), false, nullptr, true);
					if(g_Config.m_ClOutlineTele)
					{
						s_Offset += 25.0f - ScrollBarOffset;
						TileOutlines.HSplitTop(-ScrollBarOffset, &Button, &TileOutlines);
						TileOutlines.HSplitTop(LineSize, &Button, &TileOutlines);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthTele, &g_Config.m_ClOutlineWidthTele, &Button, Localize("Tele width"), 1, 16);
						TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineKill, Localize("Outline kills"), &g_Config.m_ClOutlineKill, &TileOutlines, LineSize);
					TileOutlines.HSplitTop(-20.0f, &Button, &TileOutlines);
					DoLine_ColorPicker(&s_OutlineColorKillId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutlines, Localize(""), &g_Config.m_ClOutlineColorKill, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorKill)), false, nullptr, true);
					if(g_Config.m_ClOutlineKill)
					{
						s_Offset += 25.0f - ScrollBarOffset;
						TileOutlines.HSplitTop(-ScrollBarOffset, &Button, &TileOutlines);
						TileOutlines.HSplitTop(LineSize, &Button, &TileOutlines);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthKill, &g_Config.m_ClOutlineWidthKill, &Button, Localize("Kill width"), 1, 16);
						TileOutlines.HSplitTop(5.0f, &Button, &TileOutlines);
					}
				}
			}
		}
	}

	/* Background Draw */
	{
		BgDraw.HSplitTop(Margin, nullptr, &BgDraw);
		BgDraw.HSplitTop(180.0f, &BgDraw, &SweatMode);
		if(s_ScrollRegion.AddRect(BgDraw))
		{
			BgDraw.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			BgDraw.VMargin(Margin, &BgDraw);

			BgDraw.HSplitTop(HeaderHeight, &Button, &BgDraw);

			Ui()->DoLabel(&Button, Localize("Background Draw"), HeaderSize, HeaderAlignment);
			BgDraw.HSplitTop(MarginSmall, nullptr, &BgDraw);

			static CButtonContainer s_BgDrawColor;
			DoLine_ColorPicker(&s_BgDrawColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &BgDraw, Localize("Color"), &g_Config.m_TcBgDrawColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::TcBgDrawColor)), false);

			BgDraw.HSplitTop(LineSize * 2.0f, &Button, &BgDraw);
			if(g_Config.m_TcBgDrawFadeTime == 0)
				Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, Localize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, Localize(" seconds (never)"));
			else
				Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, Localize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, Localize(" seconds"));

			BgDraw.HSplitTop(LineSize * 2.0f, &Button, &BgDraw);
			Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawWidth, &g_Config.m_TcBgDrawWidth, &Button, Localize("Width"), 1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

			static CButtonContainer s_ReaderButtonDraw, s_ClearButtonDraw;
			DoLine_KeyReader(BgDraw, s_ReaderButtonDraw, s_ClearButtonDraw, Localize("Draw where mouse is"), "+bg_draw");
		}
	}

	/* Sweat Mode */
	if(g_Config.m_ClWarList)
	{
		SweatMode.HSplitTop(Margin, nullptr, &SweatMode);
		SweatMode.HSplitTop(130.0f, &SweatMode, nullptr);
		if(s_ScrollRegion.AddRect(SweatMode))
		{
			SweatMode.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			SweatMode.VMargin(Margin, &SweatMode);

			SweatMode.HSplitTop(HeaderHeight, &Button, &SweatMode);
			Ui()->DoLabel(&Button, Localize("Warlist Sweat Mode"), HeaderSize, HeaderAlignment);

			SweatMode.HSplitTop(5, &Button, &SweatMode);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatMode, ("Sweat Mode"), &g_Config.m_ClSweatMode, &SweatMode, LineMargin);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeOnlyOthers, ("Don't Change Own Skin"), &g_Config.m_ClSweatModeOnlyOthers, &SweatMode, LineMargin);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeSelfColor, ("Don't Change Own Color"), &g_Config.m_ClSweatModeSelfColor, &SweatMode, LineMargin);

			static CLineInput s_Name;
			s_Name.SetBuffer(g_Config.m_ClSweatModeSkinName, sizeof(g_Config.m_ClSweatModeSkinName));
			s_Name.SetEmptyText("x_ninja");

			SweatMode.HSplitTop(2.4f, &Label, &SweatMode);
			SweatMode.VSplitLeft(25.0f, &SweatMode, &SweatMode);
			Ui()->DoLabel(&SweatMode, "Skin Name:", 13.0f, TEXTALIGN_LEFT);

			SweatMode.HSplitTop(-1, &Button, &SweatMode);
			SweatMode.HSplitTop(18.9f, &Button, &SweatMode);

			float Length = TextRender()->TextBoundingBox(FontSize, "Skin Name").m_W + 3.5f;

			Button.VSplitLeft(0.0f, 0, &SweatMode);
			Button.VSplitLeft(Length, &Label, &Button);
			Button.VSplitLeft(150.0f, &Button, 0);

			Ui()->DoEditBox(&s_Name, &Button, EditBoxFontSize);
		}
	}
	else
		SweatMode.HSplitTop(0, &SweatMode, nullptr);
	s_ScrollRegion.End();
}

void CMenus::PopupConfirmRemoveWarType()
{
	GameClient()->m_WarList.RemoveWarType(m_pRemoveWarType->m_aWarName);
	m_pRemoveWarType = nullptr;
}

void CMenus::RenderFontIcon(CUIRect Rect, const char *pText, float Size, int Align)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&Rect, pText, Size, Align);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

int CMenus::DoButtonNoRect_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextSelectionColor());
	if(Ui()->HotItem() == pButtonContainer)
	{
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	CUIRect Temp;
	pRect->HMargin(0.0f, &Temp);
	Ui()->DoLabel(&Temp, pText, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_ALL);
}

vec2 CMenus::TeeEyeDirection(vec2 Pos)
{
	vec2 DeltaPosition = Ui()->MousePos() - Pos;
	vec2 TeeDirection = normalize(DeltaPosition);
	return TeeDirection;
}

void CMenus::RenderDraggableTee(CUIRect MainView, vec2 SpawnPos, vec2 TeeDirection, const CAnimState *pAnim, CTeeRenderInfo *pInfo, int EyeEmote, bool HappyHover)
{
	static bool s_OverrideTeePos = false;
	static bool s_CanDrag = false;
	static vec2 s_Pos = SpawnPos;

	if(m_ResetTeePos || !s_OverrideTeePos)
		s_Pos = SpawnPos;
	if(m_ResetTeePos)
		m_ResetTeePos = false;

	if(length(Ui()->MousePos() - s_Pos) < pInfo->m_Size / 2.4f)
	{
		s_CanDrag = true;
		Ui()->SetHotItem(nullptr);
	}

	if(GameClient()->Input()->KeyIsPressed(KEY_MOUSE_1) && s_CanDrag)
	{
		vec2 Offset = vec2(0.0f, 2.5f);
		s_Pos = Ui()->MousePos() - Offset;

		float MenuTop = MainView.y + 25.0f;
		float MenuBottom = MainView.Size().y + 35.0f;

		float MenuLeft = MainView.x + 15.0f;
		float MenuRight = MainView.Size().x + 10.0f;

		if(Ui()->MousePos().y < MenuTop)
			s_Pos.y = MenuTop - Offset.y;
		if(Ui()->MousePos().y > MenuBottom)
			s_Pos.y = MenuBottom - Offset.y;

		if(Ui()->MousePos().x < MenuLeft)
			s_Pos.x = MenuLeft;
		if(Ui()->MousePos().x > MenuRight)
			s_Pos.x = MenuRight;

		s_CanDrag = true;
		s_OverrideTeePos = true;
	}
	else if(GameClient()->Input()->KeyIsPressed(KEY_MOUSE_2) && s_OverrideTeePos && s_CanDrag)
		s_OverrideTeePos = false;
	else
		s_CanDrag = false;

	RenderTee(s_Pos, TeeEyeDirection(s_Pos), pAnim, pInfo, EyeEmote, HappyHover);
}

void CMenus::RenderTee(vec2 Pos, vec2 TeeDirection, const CAnimState *pAnim, CTeeRenderInfo *pInfo, int EyeEmote, bool HappyHover)
{
	if(g_Config.m_ClFatSkins)
		pInfo->m_Size -= 20.0f;

	int TeeEmote = EyeEmote;

	if(HappyHover)
	{
		vec2 DeltaPosition = Ui()->MousePos() - Pos;
		float Distance = length(DeltaPosition);
		float InteractionDistance = pInfo->m_Size / 2.4f;
		if(Distance < InteractionDistance)
			TeeEmote = EMOTE_HAPPY;
	}
	RenderTools()->RenderTee(pAnim, pInfo, TeeEmote, TeeDirection, Pos);
}

bool CMenus::DoLine_KeyReader(CUIRect &View, CButtonContainer &ReaderButton, CButtonContainer &ClearButton, const char *pName, const char *pCommand)
{
	CBindSlot Bind(0, 0);
	for(int Mod = 0; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, pCommand) == 0)
			{
				Bind.m_Key = KeyId;
				Bind.m_ModifierMask = Mod;
				break;
			}
		}
	}

	CUIRect KeyButton, KeyLabel;
	View.HSplitTop(LineSize, &KeyButton, &View);
	KeyButton.VSplitMid(&KeyLabel, &KeyButton);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s", pName);
	Ui()->DoLabel(&KeyLabel, aBuf, FontSize, TEXTALIGN_ML);

	View.HSplitTop(MarginExtraSmall, nullptr, &View);

	const auto Result = GameClient()->m_KeyBinder.DoKeyReader(&ReaderButton, &ClearButton, &KeyButton, Bind, false);
	if(Result.m_Bind != Bind)
	{
		if(Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Bind.m_Key, "", false, Bind.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
		return true;
	}
	return false;
}

enum
{
	PG1921_TAB_SETTINGS = 0,
	NUMBER_OF_PG1921_TABS,
};

void CMenus::RenderSettingsProGaming(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	static int s_CurTab = 0;

	CUIRect TabBar, Button, Label;

	int TabCount = NUMBER_OF_PG1921_TABS;

	s_CurTab = PG1921_TAB_SETTINGS;

	MainView.HSplitTop(LineSize * 1.1f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / TabCount;
	static CButtonContainer s_aPageTabs[NUMBER_OF_PG1921_TABS] = {};
	const char *apTabNames[NUMBER_OF_PG1921_TABS] = 
	{
		Localize("PRO_Gaming1921"),
	};

	for(int Tab = 0; Tab < NUMBER_OF_PG1921_TABS; ++Tab)
	{

		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);

		int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == 1 ? IGraphics::CORNER_R :
										       IGraphics::CORNER_NONE;
		if(NUMBER_OF_PG1921_TABS == 1)
			Corners = IGraphics::CORNER_ALL;

		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	// Client Page 1

	if(s_CurTab == PG1921_TAB_SETTINGS)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollParams);
		MainView.y += ScrollOffset.y;

		// left side in settings menu

		CUIRect AllPlayerSettings, DemoSettings;
		MainView.VSplitMid(&AllPlayerSettings, &DemoSettings);

		{
			CUIRect TeeRect;
			AllPlayerSettings.VMargin(5.0f, &AllPlayerSettings);
			AllPlayerSettings.HSplitTop(Margin, nullptr, &TeeRect);
			AllPlayerSettings.HSplitTop(235.0f, &AllPlayerSettings, 0);
			if(s_ScrollRegion.AddRect(AllPlayerSettings))
			{
				AllPlayerSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				AllPlayerSettings.VMargin(Margin, &AllPlayerSettings);

				AllPlayerSettings.HSplitTop(HeaderHeight, &Button, &AllPlayerSettings);
				Ui()->DoLabel(&Button, Localize("Change All Skins"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChangeAllSkin, Localize("Change All Skins"), &g_Config.m_ClChangeAllSkin, &AllPlayerSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChangeAllCustomCol, Localize("Use Custom Color"), &g_Config.m_ClChangeAllCustomCol, &AllPlayerSettings, LineSize);

				{
					static CLineInput s_Name;
					s_Name.SetBuffer(g_Config.m_ClChangeAllSkinName, sizeof(g_Config.m_ClChangeAllSkinName));
					s_Name.SetEmptyText("default");

					AllPlayerSettings.HSplitTop(3.4f, &Label, &AllPlayerSettings);
					AllPlayerSettings.VSplitLeft(25.0f, &AllPlayerSettings, &AllPlayerSettings);
					Ui()->DoLabel(&AllPlayerSettings, "Skin Name:", 13.0f, TEXTALIGN_LEFT);

					AllPlayerSettings.HSplitTop(-1, &Button, &AllPlayerSettings);
					AllPlayerSettings.HSplitTop(18.9f, &Button, &AllPlayerSettings);

					float Length = TextRender()->TextBoundingBox(FontSize, "Skin Name").m_W + 3.5f;

					Button.VSplitLeft(0.0f, 0, &AllPlayerSettings);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitLeft(150.0f, &Button, 0);

					Ui()->DoEditBox(&s_Name, &Button, 14.0f);
					AllPlayerSettings.VSplitLeft(-25.0f, &AllPlayerSettings, &AllPlayerSettings);
					AllPlayerSettings.HSplitTop(23.0f, &Button, &AllPlayerSettings);
				}

				static int s_CustomColorSwitch = 0;

				DoButton_CheckBoxAutoVMarginAndSet(&s_CustomColorSwitch, Localize("Change Feet Color"), &s_CustomColorSwitch, &AllPlayerSettings, LineSize);


				AllPlayerSettings.VSplitLeft(52, &Button, &AllPlayerSettings);
				if(s_CustomColorSwitch)
					RenderHslaScrollbars(&AllPlayerSettings, &g_Config.m_ClChangeAllColorFeet, false, ColorHSLA::DARKEST_LGT, false);
				else 
					RenderHslaScrollbars(&AllPlayerSettings, &g_Config.m_ClChangeAllColorBody, false, ColorHSLA::DARKEST_LGT, false);
				AllPlayerSettings.VSplitLeft(-140, &Button, &AllPlayerSettings);

				{
					TeeRect.HSplitTop(105.0f, nullptr, &TeeRect);
					TeeRect.HSplitTop(80.0f, &TeeRect, nullptr);
					TeeRect.VSplitLeft(75.0f, &TeeRect, nullptr);

					CTeeRenderInfo TeeRenderInfo;

					bool UseCustomColor = g_Config.m_ClChangeAllCustomCol;
					int BodyColor = g_Config.m_ClChangeAllColorBody;
					int FeetColor = g_Config.m_ClChangeAllColorFeet;

					TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClChangeAllSkinName));
					TeeRenderInfo.ApplyColors(UseCustomColor, BodyColor, FeetColor);
					TeeRenderInfo.m_Size = 75.0f;

					RenderTee(TeeRect.Center(), TeeEyeDirection(TeeRect.Center()), CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL);
				}
				CUIRect CurSkin;
				const char *pText = "Use Current Tee Settings";
				float Length = TextRender()->TextBoundingBox(FontSize, pText).m_W + 8.0f;
				float Height = TextRender()->TextBoundingBox(FontSize, pText).m_H + 15.0f;

				AllPlayerSettings.HSplitTop(Margin, &CurSkin, 0);
				CurSkin.HSplitTop(Height, &CurSkin, 0);
				CurSkin.VSplitLeft(90.0f, &AllPlayerSettings, &CurSkin);
				CurSkin.VSplitLeft(Length, &CurSkin, &Button);
				static CButtonContainer s_NewestRelGithub;
				if(DoButtonLineSize_Menu(&s_NewestRelGithub, Localize("Use Current Tee Settings"), 0, &CurSkin, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f,-2))
				{
					str_copy(g_Config.m_ClChangeAllSkinName, g_Config.m_ClPlayerSkin);
					g_Config.m_ClChangeAllCustomCol = g_Config.m_ClPlayerUseCustomColor;
					g_Config.m_ClChangeAllColorBody = g_Config.m_ClPlayerColorBody;
					g_Config.m_ClChangeAllColorFeet = g_Config.m_ClPlayerColorFeet;
				}
			}
		}

		{
			DemoSettings.VMargin(5.0f, &DemoSettings);
			DemoSettings.HSplitTop(80.0f, &DemoSettings, 0);
			if(s_ScrollRegion.AddRect(DemoSettings))
			{
				DemoSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				DemoSettings.VMargin(Margin, &DemoSettings);

				DemoSettings.HSplitTop(HeaderHeight, &Button, &DemoSettings);
				Ui()->DoLabel(&Button, Localize("Demo Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDemoHideIfSolo, Localize("Hide Solo Players in Demos"), &g_Config.m_ClDemoHideIfSolo, &DemoSettings, LineSize);
			}
		}
		s_ScrollRegion.End();
	}
}