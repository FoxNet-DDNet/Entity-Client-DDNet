#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/night_shift.h>
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
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;

enum SettingTab
{
	SETTINGS = 0,
	WARLIST,
	STATUSBAR,
	BINDWHEEL,
	PLAYERACTION,
	INFO,
	NUM_TABS,
};

constexpr float ScrollSpeed = 110.0f;

constexpr float FontSize = 14.0f;
constexpr float EditBoxFontSize = 12.0f;
constexpr float LineSize = 20.0f;
constexpr float HeadlineFontSize = 20.0f;
constexpr float StandardFontSize = 14.0f;

constexpr float LineMargin = 20.0f;
constexpr float Margin = 10.0f;
constexpr float MarginSmall = 5.0f;
constexpr float MarginExtraSmall = 2.5f;
constexpr float MarginBetweenSections = 30.0f;
constexpr float MarginBetweenViews = 30.0f;

constexpr float HeadlineHeight = HeadlineFontSize + 0.0f;

constexpr float HeaderHeight = FontSize + 5.0f + Margin;
constexpr float ColorPickerLineSize = 25.0f;
constexpr float ColorPickerLabelSize = 13.0f;
constexpr float ColorPickerLineSpacing = 5.0f;

constexpr float CornerRoundness = 12.0f;

constexpr float HeaderSize = 20.0f;
constexpr int HeaderAlignment = TEXTALIGN_MC;
constexpr ColorRGBA BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

// EClient: the quick filter chips, in the order they are drawn. Nothing here is saved: a filter
// left on across a restart just looks like settings that went missing.
struct SSettingsFilterChip
{
	unsigned m_Flag;
	const char *m_pName;
};

static const SSettingsFilterChip gs_aSettingsFilterChips[] = {
	{FILTER_VISUAL, "Visual"},
	{FILTER_HUD, "HUD"},
	{FILTER_GAMEPLAY, "Gameplay"},
	{FILTER_NETWORK, "Network"},
	{FILTER_CHAT, "Chat"},
	{FILTER_WARLIST, "Warlist"},
	{FILTER_MISC, "Misc"},
};

static unsigned gs_SettingsModuleFilter = FILTER_NONE;

// A module shows while no chip is on, or while it carries any of the chips that are. The chips OR
// together, so picking a second topic widens the page rather than narrowing it to nothing.
static bool SettingsModuleMatchesFilter(const CSettingsModule &Module)
{
	return gs_SettingsModuleFilter == FILTER_NONE || (Module.m_Filters & gs_SettingsModuleFilter) != 0;
}

static bool SettingsModuleMatchesSearch(const CSettingsModule &Module, const char *pSearch)
{
	if(pSearch == nullptr || pSearch[0] == '\0')
		return true;

	std::string Search = pSearch;
	size_t Start = 0;

	while(Start < Search.size())
	{
		// Skip spaces
		while(Start < Search.size() && Search[Start] == ' ')
			++Start;

		if(Start >= Search.size())
			break;

		// Find token end
		size_t End = Search.find(' ', Start);
		if(End == std::string::npos)
			End = Search.size();

		// IMPORTANT: actual null-terminated string
		std::string Token = Search.substr(Start, End - Start);

		bool Found = false;

		for(const std::string_view Term : Module.m_vSearchTerms)
		{
			if(!Term.empty() &&
				str_find_nocase(Term.data(), Token.c_str()) != nullptr)
			{
				Found = true;
				break;
			}
		}

		// every token must match
		if(!Found)
			return false;

		Start = End + 1;
	}

	return true;
}

// The filter and the search narrow the page together: a chip picks the topic, the search box picks
// the setting inside it.
static bool SettingsModuleVisible(const CSettingsModule &Module, const char *pSearch)
{
	return SettingsModuleMatchesFilter(Module) && SettingsModuleMatchesSearch(Module, pSearch);
}

static bool HasMatchingSettingsModules(const std::vector<CSettingsModule> &vModules, const char *pSearch)
{
	for(const CSettingsModule &Module : vModules)
	{
		if(SettingsModuleVisible(Module, pSearch))
			return true;
	}

	return false;
}

//static int CountMatchingSettingsModules(const std::vector<CSettingsModule> &vModules, const char *pSearch)
//{
//	int Matches = 0;
//	for(const CSettingsModule &Module : vModules)
//	{
//		if(SettingsModuleVisible(Module, pSearch))
//			++Matches;
//	}
//
//	return Matches;
//}
//
//static int CountSettingsSearchTermMatches(const std::vector<CSettingsModule> &vModules, const char *pSearch)
//{
//	if(pSearch == nullptr || pSearch[0] == '\0')
//		return 0;
//
//	int Matches = 0;
//	for(const CSettingsModule &Module : vModules)
//	{
//		if(!SettingsModuleMatchesFilter(Module))
//			continue;
//
//		for(const std::string_view Term : Module.m_vSearchTerms)
//		{
//			if(!Term.empty() && str_find_nocase(Term.data(), pSearch) != nullptr)
//				++Matches;
//		}
//	}
//
//	return Matches;
//}

void CMenus::RenderSettingsModuleSearchBar(CScrollRegion &ScrollRegion, CUIRect &MainView, const std::vector<CSettingsModule> &vModules, CLineInputBuffered<32> &SearchInput)
{
	CUIRect SearchBar, Content, EditBox, ChipRow;
	constexpr float ChipHeight = 18.0f;
	constexpr float EditBoxHeight = EditBoxFontSize * 1.5f;
	MainView.HSplitTop(Margin * 2.0f + EditBoxHeight + MarginSmall + ChipHeight, &SearchBar, &MainView);
	ScrollRegion.AddRect(SearchBar);
	{
		const float SearchWidth = TextRender()->TextWidth(EditBoxFontSize, FontIcon::MAGNIFYING_GLASS) + 5.0f;

		SearchBar.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
		SearchBar.Margin(Margin, &Content);
		Content.HSplitTop(EditBoxHeight, &EditBox, &Content);
		Content.HSplitTop(MarginSmall, nullptr, &Content);
		Content.HSplitTop(ChipHeight, &ChipRow, &Content);

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&EditBox, FontIcon::MAGNIFYING_GLASS, EditBoxFontSize, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		EditBox.VSplitLeft(SearchWidth, nullptr, &EditBox);

		CUIRect HudEditorButton;
		EditBox.VSplitRight(EditBox.h, &EditBox, &HudEditorButton);
		EditBox.VSplitRight(Margin, &EditBox, nullptr);
		static CButtonContainer s_HudEditorButton;
		DoHudEditorButton(&s_HudEditorButton, &HudEditorButton);

		if(!Ui()->IsPopupOpen() && Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
		{
			Ui()->SetActiveItem(&SearchInput);
			SearchInput.SelectAll();
			ScrollRegion.ScrollHere(CScrollRegion::EScrollOption::SCROLLHERE_TOP, true);
		}
		SearchInput.SetEmptyText(Localize("Search"));
		Ui()->DoClearableEditBox(&SearchInput, &EditBox, EditBoxFontSize);

		// EClient: the quick filters. Each one is its own small rounded rect with a gap to the next,
		// not a joined segmented control, and a selected chip fills with a color loud enough to
		// register at a glance instead of the faint grey tint checked buttons usually get.
		{
			constexpr int NumChips = (int)std::size(gs_aSettingsFilterChips);
			constexpr float ChipGap = MarginExtraSmall * 2.0f;
			constexpr float ChipCorner = 2.0f;
			constexpr float ChipFontSize = EditBoxFontSize;
			constexpr ColorRGBA ChipActiveColor(0.38f, 0.46f, 0.9f, 0.65f);
			constexpr ColorRGBA ChipInactiveColor(1.0f, 1.0f, 1.0f, 0.08f);
			constexpr ColorRGBA ChipActiveTextColor(1.0f, 1.0f, 1.0f, 1.0f);
			constexpr ColorRGBA ChipInactiveTextColor(0.75f, 0.75f, 0.75f, 1.0f);

			static CButtonContainer s_aChipButtons[NumChips + 1];

			auto DoFilterChip = [&](CButtonContainer *pId, const char *pLabel, bool Active, CUIRect *pRect) {
				ColorRGBA Color = Active ? ChipActiveColor : ChipInactiveColor;
				Color.a *= Ui()->ButtonColorMul(pId);
				pRect->Draw(Color, IGraphics::CORNER_ALL, ChipCorner);

				TextRender()->TextColor(Active ? ChipActiveTextColor : ChipInactiveTextColor);
				Ui()->DoLabel(pRect, pLabel, ChipFontSize, TEXTALIGN_MC);
				TextRender()->TextColor(TextRender()->DefaultTextColor());

				return Ui()->DoButtonLogic(pId, Active, pRect, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
			};

			CUIRect Chips = ChipRow;
			float ChipWidth = 0;
			for(int i = 0; i < NumChips; ++i)
			{
				const SSettingsFilterChip &ChipInfo = gs_aSettingsFilterChips[i];
				const float TextWidth = TextRender()->TextWidth(ChipFontSize, ChipInfo.m_pName);
				if(TextWidth > ChipWidth - ChipGap * 2.0f)
					ChipWidth = TextWidth + ChipGap * 2.0f;
			}

			CUIRect Chip;
			Chips.VSplitLeft(ChipWidth, &Chip, &Chips);
			if(DoFilterChip(&s_aChipButtons[0], EcLocalize("All"), gs_SettingsModuleFilter == FILTER_NONE, &Chip))
				gs_SettingsModuleFilter = FILTER_NONE;

			for(int i = 0; i < NumChips; ++i)
			{
				Chips.VSplitLeft(ChipGap, nullptr, &Chips);

				const SSettingsFilterChip &ChipInfo = gs_aSettingsFilterChips[i];
				const bool Active = (gs_SettingsModuleFilter & ChipInfo.m_Flag) != 0;

				Chips.VSplitLeft(ChipWidth, &Chip, &Chips);
				const int Result = DoFilterChip(&s_aChipButtons[i + 1], EcLocalize(ChipInfo.m_pName), Active, &Chip);
				if(Result == BUTTONFLAG_LEFT)
					gs_SettingsModuleFilter ^= ChipInfo.m_Flag;
				else if(Result == BUTTONFLAG_RIGHT)
					gs_SettingsModuleFilter = ChipInfo.m_Flag;
			}
		}

		//char aBuf[64];
		//const int ShownModules = CountMatchingSettingsModules(vModules, SearchInput.GetString());
		//const int SearchMatches = CountSettingsSearchTermMatches(vModules, SearchInput.GetString());

		//if(SearchInput.IsEmpty())
		//	str_format(aBuf, sizeof(aBuf), "Modules: %d", ShownModules, SearchMatches);
		//else
		//	str_format(aBuf, sizeof(aBuf), "Modules: %d · search matches: %d", ShownModules, SearchMatches);
		//TextRender()->TextColor(ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f));
		//Ui()->DoLabel(&Status, aBuf, StatusFontSize, TEXTALIGN_ML);
		//TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}

// EClient: draws the modules named by vIndices down one column, in the order they were declared in.
static void RenderSettingsModules(CScrollRegion &ScrollRegion, CUIRect &ColumnRect, const std::vector<CSettingsModule> &vModules, const std::vector<size_t> &vIndices, bool HasSearch)
{
	bool HasRenderedModule = false;

	for(const size_t Index : vIndices)
	{
		const CSettingsModule &Module = vModules[Index];
		const float ModuleHeight = Module.m_GetHeight(HasSearch);

		float TopMargin = Module.m_TopMargin;
		if(HasRenderedModule && TopMargin <= 0.0f)
			TopMargin = Margin;
		if(TopMargin > 0.0f)
			ColumnRect.HSplitTop(TopMargin, nullptr, &ColumnRect);

		CUIRect ModuleRect;
		ColumnRect.HSplitTop(ModuleHeight, &ModuleRect, &ColumnRect);
		if(ScrollRegion.AddRect(ModuleRect))
			Module.m_Render(ModuleRect, HasSearch);
		HasRenderedModule = true;
	}
}

// EClient: on the full page, m_Column is the hand-picked reading order - the settings people touch
// often at the top of a column, the ones they set once and forget lower down or on the other side.
// That order only balances the two columns for the complete set, though, so once a filter or a
// search cuts the page down to part of it, the pins are dropped and modules are dealt out in
// declaration order into whichever column is currently shorter instead.
static void PackSettingsModuleColumns(const std::vector<CSettingsModule> &vModules, const char *pSearch, bool HasSearch, std::vector<size_t> &vLeft, std::vector<size_t> &vRight)
{
	const bool Narrowed = gs_SettingsModuleFilter != FILTER_NONE || HasSearch;

	float LeftHeight = 0.0f;
	float RightHeight = 0.0f;

	for(size_t Index = 0; Index < vModules.size(); ++Index)
	{
		const CSettingsModule &Module = vModules[Index];
		if(!SettingsModuleVisible(Module, pSearch))
			continue;

		const float ModuleHeight = Module.m_GetHeight(HasSearch);
		if(ModuleHeight <= 0.0f)
			continue;

		const bool Left = Narrowed ? LeftHeight <= RightHeight : Module.m_Column == ESettingsModuleColumn::LEFT;

		if(Left)
		{
			vLeft.push_back(Index);
			LeftHeight += ModuleHeight + Margin;
		}
		else
		{
			vRight.push_back(Index);
			RightHeight += ModuleHeight + Margin;
		}
	}
}

// EClient: two columns are only worth having while each one is wide enough to read its own
// labels. The menu is laid out in a space that is always 600 tall and as wide as the aspect ratio
// makes it, so a narrow screen leaves the columns crowded rather than merely small.
static void RenderSettingsModuleColumns(CScrollRegion &ScrollRegion, CUIRect MainView, const std::vector<CSettingsModule> &vModules, const char *pSearch)
{
	constexpr float MinColumnWidth = 360.0f;
	const bool HasSearch = pSearch != nullptr && pSearch[0] != '\0';

	std::vector<size_t> vLeft, vRight;
	PackSettingsModuleColumns(vModules, pSearch, HasSearch, vLeft, vRight);

	if((MainView.w - Margin) * 0.5f < MinColumnWidth)
	{
		// Everything in one column, in the order the modules are declared in, which keeps the two
		// halves interleaved the way they read down the page rather than one half after the other
		std::vector<size_t> vBoth;
		vBoth.reserve(vLeft.size() + vRight.size());
		vBoth.insert(vBoth.end(), vLeft.begin(), vLeft.end());
		vBoth.insert(vBoth.end(), vRight.begin(), vRight.end());
		std::sort(vBoth.begin(), vBoth.end());
		RenderSettingsModules(ScrollRegion, MainView, vModules, vBoth, HasSearch);
		return;
	}

	CUIRect ViewLeft, ViewRight;
	MainView.VSplitMid(&ViewLeft, &ViewRight, Margin);
	RenderSettingsModules(ScrollRegion, ViewLeft, vModules, vLeft, HasSearch);
	RenderSettingsModules(ScrollRegion, ViewRight, vModules, vRight, HasSearch);
}

void CMenus::RenderSettingsEntity(CUIRect MainView)
{
	static int s_CurTab = 0;

	CUIRect TabBar, Button;

	const int NumTabs = SettingTab::NUM_TABS;
	int ActiveTabs = NumTabs;
	for(int Tab = 0; Tab < NumTabs; ++Tab)
	{
		if(IsFlagSet(g_Config.m_ClHideSettingsTabs, Tab))
		{
			ActiveTabs--;
			if(s_CurTab == Tab)
				s_CurTab++;
		}
	}

	if(ActiveTabs <= 0)
	{
		RenderEClientInfoPage(MainView);
		return;
	}

	MainView.HSplitTop(LineSize * 1.1f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / ActiveTabs;
	static CButtonContainer s_aPageTabs[NumTabs] = {};
	const char *apTabNames[NumTabs] = {
		EcLocalize("Settings"),
		EcLocalize("Warlist"),
		EcLocalize("Status bar"),
		EcLocalize("Bindwheel"),
		EcLocalize("Player actions"),
		EcLocalize("Info"),
	};

	for(int Tab = 0; Tab < NumTabs; ++Tab)
	{
		int LeftTab = 0;
		int RightTab = NumTabs - 1;

		if(IsFlagSet(g_Config.m_ClHideSettingsTabs, Tab))
			continue;

		for(int i = 0; i < Tab; ++i)
		{
			if(IsFlagSet(g_Config.m_ClHideSettingsTabs, i) && IsFlagSet(g_Config.m_ClHideSettingsTabs, LeftTab))
				LeftTab++;
		}
		for(int i = NumTabs - 1; i > 0; --i)
		{
			if(IsFlagSet(g_Config.m_ClHideSettingsTabs, i) && IsFlagSet(g_Config.m_ClHideSettingsTabs, RightTab))
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

	if(s_CurTab == SettingTab::SETTINGS)
	{
		RenderSettingsEClient(MainView);
	}
	if(s_CurTab == SettingTab::WARLIST)
	{
		RenderSettingsWarList(MainView);
	}
	if(s_CurTab == SettingTab::STATUSBAR)
	{
		RenderSettingsStatusbar(MainView);
	}
	if(s_CurTab == SettingTab::BINDWHEEL)
	{
		RenderSettingsBindwheel(MainView);
	}
	if(s_CurTab == SettingTab::PLAYERACTION)
	{
		RenderSettingsPlayerActions(MainView);
	}
	if(s_CurTab == SettingTab::INFO)
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
		float m_WrapStartOffset = 0.0f;
		float m_TextHeight = 0.0f;
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
	const bool HoveredNews = Ui()->MouseHovered(&InteractionRect) && !Ui()->IsPopupOpen() && Ui()->HotItem() == nullptr;

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
	ScrollParams.m_ScrollUnit = Ui()->IsPopupOpen() || s_Selecting ? 0.0f : ScrollSpeed;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	CUIRect ContentView = MainView;
	CUIRect Label;

	std::vector<CNewsLine> vVisibleLines;

	auto PrepareSelectionCursor = [&](const CNewsLine &Line, bool Render) {
		CTextCursor Cursor;
		const float TextYOffset = std::max(0.0f, (Line.m_Rect.h - Line.m_TextHeight) / 2.0f);
		Cursor.SetPosition(vec2(Line.m_Rect.x, Line.m_Rect.y + TextYOffset));
		Cursor.m_FontSize = Line.m_FontSize;
		Cursor.m_Flags = TEXTFLAG_RENDER;
		Cursor.m_LineWidth = Line.m_Rect.w;
		if(Line.m_WrapStartOffset > 0.0f)
			Cursor.m_StartX = Line.m_Rect.x + Line.m_WrapStartOffset;
		Cursor.m_SelectionHeightFactor = 1.0f;
		return Cursor;
	};

	auto RenderSelectionForLine = [&](const CNewsLine &Line, int SelectionStart, int SelectionEnd) {
		if(SelectionStart < 0 || SelectionEnd <= SelectionStart || Line.m_Text.empty())
			return;

		CTextCursor Cursor = PrepareSelectionCursor(Line, true);
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
		float WrapStartOffset = 0.0f;

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
				LineText = std::string("• ") + LineText.substr(2);
				WrapStartOffset = TextRender()->TextWidth(LineFontSize, "• ");
			}
		}

		CTextCursor MeasureCursor;
		MeasureCursor.SetPosition(vec2(0.0f, 0.0f));
		MeasureCursor.m_FontSize = LineFontSize;
		MeasureCursor.m_Flags = 0;
		MeasureCursor.m_LineWidth = ContentView.w;
		if(WrapStartOffset > 0.0f)
			MeasureCursor.m_StartX = WrapStartOffset;
		TextRender()->TextEx(&MeasureCursor, LineText.c_str(), -1);
		const float MeasuredHeight = std::max(LineFontSize, MeasureCursor.Height());
		LineHeight = std::max(LineHeight, MeasuredHeight);

		ContentView.HSplitTop(LineHeight, &Label, &ContentView);

		if(s_ScrollRegion.AddRect(Label))
		{
			TextRender()->TextColor(TextColor);
			CTextCursor RenderCursor;
			const float TextYOffset = std::max(0.0f, (Label.h - MeasuredHeight) / 2.0f);
			RenderCursor.SetPosition(vec2(Label.x, Label.y + TextYOffset));
			RenderCursor.m_FontSize = LineFontSize;
			RenderCursor.m_Flags = TEXTFLAG_RENDER;
			RenderCursor.m_LineWidth = Label.w;
			if(WrapStartOffset > 0.0f)
				RenderCursor.m_StartX = Label.x + WrapStartOffset;
			TextRender()->TextEx(&RenderCursor, LineText.c_str(), -1);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			CNewsLine &Line = vVisibleLines.emplace_back();
			Line.m_Index = LineIndex;
			Line.m_Rect = Label;
			Line.m_Text = LineText;
			Line.m_FontSize = LineFontSize;
			Line.m_WrapStartOffset = WrapStartOffset;
			Line.m_TextHeight = MeasuredHeight;
		}

		++LineIndex;
	}

	if(s_Selecting)
	{
		const float SelectionMinY = std::min(s_SelectionMousePress.y, s_SelectionMouseRelease.y);
		const float SelectionMaxY = std::max(s_SelectionMousePress.y, s_SelectionMouseRelease.y);

		std::vector<CSelectedNewsLine> vSelectedLines;
		std::string SelectionText;
		bool AnySelection = false;

		for(const CNewsLine &Line : vVisibleLines)
		{
			const float LineTop = Line.m_Rect.y;
			const float LineBottom = Line.m_Rect.y + Line.m_Rect.h;
			if(LineBottom < SelectionMinY || LineTop > SelectionMaxY || Line.m_Text.empty())
				continue;

			CTextCursor Cursor = PrepareSelectionCursor(Line, false);
			Cursor.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_CALCULATE;
			Cursor.m_PressMouse = s_SelectionMousePress;
			Cursor.m_ReleaseMouse = s_SelectionMouseRelease;

			TextRender()->TextColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
			TextRender()->TextEx(&Cursor, Line.m_Text.c_str(), -1);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			const int SelectionStart = std::min(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);
			const int SelectionEnd = std::max(Cursor.m_SelectionStart, Cursor.m_SelectionEnd);

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
	Ui()->DoLabel(&Label, EcLocalize("Developers:"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	vec2 TeePos1, TeePos2;
	static CButtonContainer s_LinkButton1, s_LinkButton2;
	{
		LeftView.HSplitTop(CardSize, &DevCardRect, &LeftView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		const char *pName = "qxdFox";
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, pName), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, pName, LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton1, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, BUTTONFLAG_LEFT))
			Client()->ViewLink("https://github.com/qxdFox");
		TeePos1 = TeeRect.Center();
	}

	{
		LeftView.HSplitTop(CardSize, &DevCardRect, &LeftView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
		const char *pName = "Tide";
		Label.VSplitLeft(TextRender()->TextWidth(LineSize, pName), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
		Ui()->DoLabel(&Label, pName, LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton2, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, BUTTONFLAG_LEFT))
			Client()->ViewLink("https://github.com/Miro8D");
		TeePos2 = TeeRect.Center();
	}

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Hide Settings Tabs", LineSize, TEXTALIGN_ML);

	static int s_ShowSettings = IsFlagSet(g_Config.m_ClHideSettingsTabs, SettingTab::SETTINGS);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowSettings, EcLocalize("Settings"), &s_ShowSettings, &LeftView, LineSize);
	SetFlag(g_Config.m_ClHideSettingsTabs, SettingTab::SETTINGS, s_ShowSettings);

	static int s_ShowWarlist = IsFlagSet(g_Config.m_ClHideSettingsTabs, SettingTab::WARLIST);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowWarlist, EcLocalize("Warlist"), &s_ShowWarlist, &LeftView, LineSize);
	SetFlag(g_Config.m_ClHideSettingsTabs, SettingTab::WARLIST, s_ShowWarlist);

	static int s_ShowStatusBar = IsFlagSet(g_Config.m_ClHideSettingsTabs, SettingTab::STATUSBAR);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowStatusBar, EcLocalize("Status Bar"), &s_ShowStatusBar, &LeftView, LineSize);
	SetFlag(g_Config.m_ClHideSettingsTabs, SettingTab::STATUSBAR, s_ShowStatusBar);

	static int s_ShowBindwheel = IsFlagSet(g_Config.m_ClHideSettingsTabs, SettingTab::BINDWHEEL);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowBindwheel, EcLocalize("Bindwheel"), &s_ShowBindwheel, &LeftView, LineSize);
	SetFlag(g_Config.m_ClHideSettingsTabs, SettingTab::BINDWHEEL, s_ShowBindwheel);

	static int s_ShowPlayerActions = IsFlagSet(g_Config.m_ClHideSettingsTabs, SettingTab::PLAYERACTION);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowPlayerActions, EcLocalize("Player Actions"), &s_ShowPlayerActions, &LeftView, LineSize);
	SetFlag(g_Config.m_ClHideSettingsTabs, SettingTab::PLAYERACTION, s_ShowPlayerActions);

	char aDeathBuf[32];
	str_format(aDeathBuf, sizeof(aDeathBuf), "Deaths: %" PRId64, GameClient()->m_EClient.DeathCount());

	Ui()->DoLabel(&LeftView, aDeathBuf, FontSize, TEXTALIGN_ML);

	auto FormatPlaytime = [](int64_t Time) -> const char * {
		// playtime is saved in minutes
		static char aBuf[64];

		if(Time < 0)
			Time = 0;

		int64_t Hours = Time / 60;
		int64_t Minutes = Time % 60;

		if(Hours > 0)
			str_format(aBuf, sizeof(aBuf), "%" PRId64 " hour%s %" PRId64 " min%s", Hours, Hours == 1 ? "" : "s", Minutes, Minutes == 1 ? "" : "s");
		else
			str_format(aBuf, sizeof(aBuf), "%" PRId64 " min%s", Minutes, Minutes == 1 ? "" : "s");

		return aBuf;
	};
	LeftView.HSplitTop(LineSize + FontSize * 0.5f, &LeftView, &LeftView);
	char aPlaytimeBuf[48];
	str_format(aPlaytimeBuf, sizeof(aPlaytimeBuf), "Playtime: %s", FormatPlaytime(GameClient()->m_EClient.Playtime()));
	Ui()->DoLabel(&LeftView, aPlaytimeBuf, FontSize, TEXTALIGN_ML);

	// Right Side
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("Config Files"), HeadlineFontSize, TEXTALIGN_MR);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	char aBuf[128 + IO_MAX_PATH_LENGTH];

	CUIRect FilesLeft, FilesRight;

	RightView.HSplitTop(LineSize * 2.0f, &Button, &RightView);
	Button.VSplitMid(&FilesLeft, &FilesRight, MarginSmall);

	static CButtonContainer s_EClientConfig, s_Config, s_Warlist, s_Profiles, s_Chatbinds, s_FontFolder;

	auto DoButtonForConfig = [&](CButtonContainer *pButtonContainer, const char *pText, const char *pConfigPath) {
		if(DoButtonLineSize_Menu(pButtonContainer, pText, 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, pConfigPath, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
		FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
		FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
		FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	};

	DoButtonForConfig(&s_EClientConfig, EcLocalize("E-Client Setting"), s_aConfigDomains[ConfigDomain::ENTITY].m_aConfigPath);
	DoButtonForConfig(&s_Config, EcLocalize("DDNet Settings"), s_aConfigDomains[ConfigDomain::DDNET].m_aConfigPath);
	DoButtonForConfig(&s_Profiles, EcLocalize("Profiles"), s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath);
	DoButtonForConfig(&s_Warlist, EcLocalize("Warlist"), s_aConfigDomains[ConfigDomain::TCLIENTWARLIST].m_aConfigPath);
	DoButtonForConfig(&s_Chatbinds, EcLocalize("Chatbinds"), s_aConfigDomains[ConfigDomain::TCLIENTCHATBINDS].m_aConfigPath);

	if(DoButtonLineSize_Menu(&s_FontFolder, EcLocalize("Fonts Folder"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->CreateFolder("data/entity", IStorage::TYPE_ABSOLUTE);
		Storage()->CreateFolder("data/entity/fonts", IStorage::TYPE_ABSOLUTE);
		Client()->ViewFile("data/entity/fonts");
	}

	static vec2 s_Pos1 = TeePos1, s_Pos2 = TeePos2;
	static bool s_Dragging1 = false, s_Dragging2 = false;

	// Render Tees Above everything else
	{
		CTeeRenderInfo TeeRenderInfo;
		TeeRenderInfo.Apply(GameClient()->m_Skins.Find("Catnoa"));
		TeeRenderInfo.ApplyColors(true, 10784768, 15269690);
		TeeRenderInfo.m_Size = TeeSize;

		RenderDraggableTee(MainView, TeePos1, s_Pos1, s_Dragging1, TeeEyeDirection(TeePos1), &TeeRenderInfo);
	}

	if(s_Dragging1)
		s_Dragging2 = false;
	else if(s_Dragging2)
		s_Dragging1 = false;

	{
		CTeeRenderInfo TeeRenderInfo;
		TeeRenderInfo.Apply(GameClient()->m_Skins.Find("froghat"));
		TeeRenderInfo.ApplyColors(true, 9690163, 16318719);
		TeeRenderInfo.m_Size = TeeSize;

		RenderDraggableTee(MainView, TeePos2, s_Pos2, s_Dragging2, TeeEyeDirection(TeePos2), &TeeRenderInfo);
	}
}

void CMenus::RenderChatPreview(CUIRect MainView)
{
	CChat &Chat = GameClient()->m_Chat;

	const ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	const ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	const ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	const ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClFriendColor));
	const ColorRGBA SpecColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClSpecColor));
	const ColorRGBA EnemyColor = GameClient()->m_WarList.m_WarTypes[1]->m_Color;
	const ColorRGBA HelperColor = GameClient()->m_WarList.m_WarTypes[3]->m_Color;
	const ColorRGBA TeammateColor = GameClient()->m_WarList.m_WarTypes[2]->m_Color;
	const ColorRGBA NormalColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageColor));
	const ColorRGBA ClientColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	const ColorRGBA DefaultNameColor(0.8f, 0.8f, 0.8f, 1.0f);

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
	auto &&SetPreviewLine = [&](int Index, int ClientId, const char *pName, const char *pText, int Flag, int Repeats, const CSkin *pSkin = nullptr) {
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

		if(pSkin)
		{
			s_vLines[Index].m_RenderInfo.m_Size = RealTeeSize;
			s_vLines[Index].m_RenderInfo.Apply(pSkin);
		}
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
		SetPreviewLine(PREVIEW_HIGHLIGHT, 7, "Random Tee", aLineBuilder, FLAG_HIGHLIGHT, 0, GameClient()->m_Skins.Find("pinky"));

		SetPreviewLine(PREVIEW_TEAM, 11, "Your Teammate", "Let's speedrun this!", FLAG_TEAM, 0, GameClient()->m_Skins.Find("default_flower"));
		SetPreviewLine(PREVIEW_FRIEND, 8, "Friend", "Hello there", FLAG_FRIEND, 0, GameClient()->m_Skins.Find("turtle"));
		SetPreviewLine(PREVIEW_SPAMMER, 9, "Spammer", "Hey fools, I'm spamming here!", 0, 5, GameClient()->m_Skins.Find("beast"));
		if(g_Config.m_ClChatClientPrefix)
			SetPreviewLine(PREVIEW_CLIENT, -1, g_Config.m_ClClientPrefix, "Echo command executed", FLAG_CLIENT, 0);
		else
			SetPreviewLine(PREVIEW_CLIENT, -1, "— ", "Echo command executed", FLAG_CLIENT, 0);
		SetPreviewLine(PREVIEW_ENEMY, 6, "Enemy", "Nobo", FLAG_ENEMY, 0, GameClient()->m_Skins.Find("default"));
		SetPreviewLine(PREVIEW_HELPER, 3, "Helper", "Ima help this random :>", FLAG_HELPER, 0, GameClient()->m_Skins.Find("Robot"));
		SetPreviewLine(PREVIEW_TEAMMATE, 10, "Teammate", "Help me!", FLAG_TEAMMATE, 0, GameClient()->m_Skins.Find("Catnoa"));
		SetPreviewLine(PREVIEW_SPEC, 11, "Random Spectator", "Crazy gameplay dude", FLAG_SPEC, 0, GameClient()->m_Skins.Find("cammostripes"));
	}

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
	Ui()->DoLabel(&Label, EcLocalize("Status Bar"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBar, EcLocalize("Show status bar"), &g_Config.m_ClStatusBar, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBarLabels, EcLocalize("Show labels on status bar items"), &g_Config.m_ClStatusBarLabels, &LeftView, LineSize);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarHeight, &g_Config.m_ClStatusBarHeight, &Button, EcLocalize("Status bar height"), 1, 16);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, EcLocalize("Local Time"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBar12HourClock, EcLocalize("Use 12 hour clock"), &g_Config.m_ClStatusBar12HourClock, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatusBarLocalTimeSeconds, EcLocalize("Show seconds on clock"), &g_Config.m_ClStatusBarLocalTimeSeconds, &LeftView, LineSize);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, EcLocalize("Colors"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	static CButtonContainer s_StatusbarColor, s_StatusbarTextColor;

	DoLine_ColorPicker(&s_StatusbarColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, EcLocalize("Status bar color"), &g_Config.m_ClStatusBarColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClStatusBarColor)), false);
	DoLine_ColorPicker(&s_StatusbarTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, EcLocalize("Text color"), &g_Config.m_ClStatusBarTextColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClStatusBarTextColor)), false);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarAlpha, &g_Config.m_ClStatusBarAlpha, &Button, EcLocalize("Status bar alpha"), 0, 100);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_ClStatusBarTextAlpha, &g_Config.m_ClStatusBarTextAlpha, &Button, EcLocalize("Text alpha"), 0, 100);

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("Status Bar Codes:"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("a = Angle"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("p = Ping"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("d = Prediction"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("c = Position"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("l = Local Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("r = Race Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("f = FPS"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("v = Velocity"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("z = Zoom"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, EcLocalize("_ or \' \' = Space"), FontSize, TEXTALIGN_ML);
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
	if(DoButton_Menu(&s_ApplyButton, EcLocalize("Apply"), 0, &Button))
	{
		GameClient()->m_StatusBar.ApplyStatusBarScheme(g_Config.m_ClStatusBarScheme);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		s_SelectedItem = -1;
	}
	Ui()->DoLabel(&Label, EcLocalize("Status Scheme:"), FontSize, TEXTALIGN_MR);
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
	if(DoButton_Menu(&s_AddButton, EcLocalize("Add Item"), 0, &ButtonL) && s_TypeSelectedOld >= 0)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.push_back(&GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld]);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_ClStatusBarScheme);
		s_SelectedItem = (int)GameClient()->m_StatusBar.m_StatusBarItems.size() - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, EcLocalize("Remove Item"), 0, &ButtonR) && s_SelectedItem >= 0)
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
		Ui()->DoLabel(&StatusBar, EcLocalize("No status bar items configured"), FontSize, TEXTALIGN_MC);
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
void CMenus::RenderSettingsPlayerActions(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	const float Radius = std::min(RightView.w, RightView.h) / 2.0f;
	vec2 Pos{RightView.x + RightView.w / 2.0f, RightView.y + RightView.h / 2.0f};
	// Draw Circle
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Pos.x, Pos.y, Radius, 64);
	Graphics()->QuadsEnd();

	static char s_aBindName[PLAYERACTIONS_MAX_NAME];
	static char s_aBindCommand[PLAYERACTIONS_MAX_CMD];

	static int s_SelectedBindIndex = -1;
	int HoveringIndex = -1;

	float MouseDist = distance(Pos, Ui()->MousePos());
	if(GameClient()->m_PlayerActions.m_vBinds.empty())
	{
		float Size = 20.0f;
		TextRender()->Text(Pos.x - TextRender()->TextWidth(Size, "Empty") / 2.0f, Pos.y - Size / 2, Size, "Empty");
	}
	else if(MouseDist < Radius && MouseDist > Radius * 0.25f)
	{
		int SegmentCount = GameClient()->m_PlayerActions.m_vBinds.size();
		float SegmentAngle = 2 * pi / SegmentCount;

		float HoveringAngle = angle(Ui()->MousePos() - Pos) + SegmentAngle / 2;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;

		HoveringIndex = (int)(HoveringAngle / (2 * pi) * SegmentCount);
		if(Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = HoveringIndex;
			str_copy(s_aBindName, GameClient()->m_PlayerActions.m_vBinds[HoveringIndex].m_aName);
			str_copy(s_aBindCommand, GameClient()->m_PlayerActions.m_vBinds[HoveringIndex].m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
		{
			CPlayerActions::CBind BindA = GameClient()->m_PlayerActions.m_vBinds[s_SelectedBindIndex];
			CPlayerActions::CBind BindB = GameClient()->m_PlayerActions.m_vBinds[HoveringIndex];
			str_copy(GameClient()->m_PlayerActions.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
			str_copy(GameClient()->m_PlayerActions.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
			str_copy(GameClient()->m_PlayerActions.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
			str_copy(GameClient()->m_PlayerActions.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
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

	if(!GameClient()->m_PlayerActions.m_vBinds.empty())
	{
		const float Theta = pi * 2.0f / GameClient()->m_PlayerActions.m_vBinds.size();
		for(int i = 0; i < static_cast<int>(GameClient()->m_PlayerActions.m_vBinds.size()); i++)
		{
			float FontSizes = 12.0f;
			if(i == s_SelectedBindIndex)
			{
				FontSizes = 20.0f;
				TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
			}
			else if(i == HoveringIndex)
				FontSizes = 14.0f;

			const CPlayerActions::CBind Bind = GameClient()->m_PlayerActions.m_vBinds[i];
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
	Ui()->DoLabel(&Label, EcLocalize("Name:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText("Name");
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, EcLocalize("Command:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(EcLocalize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, EcLocalize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0)
	{
		CPlayerActions::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		str_copy(GameClient()->m_PlayerActions.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
		str_copy(GameClient()->m_PlayerActions.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	CUIRect ButtonAdd, ButtonRemove;
	Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
	if(DoButton_Menu(&s_AddButton, EcLocalize("Add Bind"), 0, &ButtonAdd))
	{
		CPlayerActions::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_PlayerActions.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_PlayerActions.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, EcLocalize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_PlayerActions.RemoveBind(s_SelectedBindIndex);
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
	DoLine_KeyReader(KeyLabel, s_ReaderButtonWheel, s_ClearButtonWheel, EcLocalize("Player Actions Key:"), "+playeractions");

	LeftView.HSplitBottom(LineSize, &LeftView, &Button);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetPlayerActionMouse, EcLocalize("Reset position of mouse when opening the player actions menu"), &g_Config.m_ClResetPlayerActionMouse, &Button, LineSize);
}

void CMenus::RenderSettingsBindwheel(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	MainView.HSplitTop(MarginBetweenSections, nullptr, &MainView);
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	const float Radius = std::min(RightView.w, RightView.h) / 2.0f;
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
	Ui()->DoLabel(&Label, EcLocalize("Name:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText("Name");
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, EcLocalize("Command:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(EcLocalize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, EcLocalize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0)
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
	if(DoButton_Menu(&s_AddButton, EcLocalize("Add Bind"), 0, &ButtonAdd))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_Bindwheel.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_Bindwheel.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, EcLocalize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_Bindwheel.RemoveBind(s_SelectedBindIndex);
		s_SelectedBindIndex = -1;
	}

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, EcLocalize("Use left mouse to select"), 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, EcLocalize("Use right mouse to swap with selected"), 14.0f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, EcLocalize("Use middle mouse select without copy"), 14.0f, TEXTALIGN_ML);

	CUIRect KeyLabel;
	LeftView.HSplitBottom(LineSize, &LeftView, &Button);
	Button.VSplitLeft(250.0f, &KeyLabel, &Button);

	static CButtonContainer s_ReaderButtonWheel, s_ClearButtonWheel;
	DoLine_KeyReader(KeyLabel, s_ReaderButtonWheel, s_ClearButtonWheel, EcLocalize("Bind Wheel Key:"), "+bindwheel");

	LeftView.HSplitBottom(LineSize, &LeftView, &Button);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetBindWheelMouse, EcLocalize("Reset position of mouse when opening bindwheel"), &g_Config.m_ClResetBindWheelMouse, &Button, LineSize);

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
	Ui()->DoLabel(&Label, EcLocalize("War Entries"), HeadlineFontSize, TEXTALIGN_ML);
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
	Ui()->DoLabel(&Label, EcLocalize("Edit Entry"), HeadlineFontSize, TEXTALIGN_ML);
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

	if(DoButtonLineSize_Menu(&s_OverrideButton, EcLocalize("Override Entry"), 0, &ButtonL, LineSize) && s_pSelectedEntry)
	{
		if(s_pSelectedEntry && s_pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(s_pSelectedEntry->m_aName, s_aEntryName);
			str_copy(s_pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(s_pSelectedEntry->m_aReason, s_aEntryReason);
			s_pSelectedEntry->m_pWarType = s_pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, EcLocalize("Add Entry"), 0, &ButtonR, LineSize))
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
	Ui()->DoLabel(&Label, EcLocalize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, EcLocalize("Enable warlist"), &g_Config.m_ClWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixes, EcLocalize("Warlist Chat Prefixes"), &g_Config.m_ClWarlistPrefixes, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixesServerInfo, EcLocalize("Warlist Prefix Server Info"), &g_Config.m_ClWarlistPrefixesServerInfo, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListChat, EcLocalize("Colors in chat"), &g_Config.m_ClWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListScoreboard, EcLocalize("Colors in scoreboard"), &g_Config.m_ClWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSpecMenu, EcLocalize("Colors in specmenu"), &g_Config.m_ClWarListSpecMenu, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListShowClan, EcLocalize("Show clan if war"), &g_Config.m_ClWarListShowClan, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSwapNameReason, EcLocalize("Swap Reason with Name"), &g_Config.m_ClWarListSwapNameReason, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListAllowDuplicates, EcLocalize("Allow Duplicate Entries"), &g_Config.m_ClWarListAllowDuplicates, &Column2, LineSize);

	// ======WAR TYPE EDITING======

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, EcLocalize("War Groups"), HeadlineFontSize, TEXTALIGN_ML);
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
				BrowserFlagSet ? EcLocalize("Hidden in server browser") : EcLocalize("Shown in server browser"));
		}

		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d %s", pType->m_NumEntries, EcLocalize("entries"));
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
			EcLocalize("Are you sure that you want to remove '%s' from your war groups?"),
			m_pRemoveWarType->m_aWarName);
		PopupConfirm(EcLocalize("Remove War Group"), aMessage, EcLocalize("Yes"), EcLocalize("No"), &CMenus::PopupConfirmRemoveWarType);
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
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, EcLocalize("Color"), &s_ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, EcLocalize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && s_pSelectedType)
	{
		if(s_pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(s_pSelectedType->m_aWarName, s_aTypeName);
			s_pSelectedType->m_Color = s_GroupColor;
		}
	}
	bool AddDisabled = str_comp(GameClient()->m_WarList.FindWarType(s_aTypeName)->m_aWarName, "none") != 0 || str_comp(s_aTypeName, "none") == 0;
	if(DoButtonLineSize_Menu(&s_AddGroupButton, EcLocalize("Add Group"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_WarList.AddWarType(s_aTypeName, s_GroupColor);
	}

	// ======ONLINE PLAYER LIST======

	Column4.HSplitTop(HeadlineHeight, &Label, &Column4);
	Ui()->DoLabel(&Label, EcLocalize("Online Players"), HeadlineFontSize, TEXTALIGN_ML);
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
				str_format(aBuf, sizeof(aBuf), EcLocalize("Name: %s"), Profile.m_Name);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), EcLocalize("Clan: %s"), Profile.m_Clan);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), EcLocalize("Skin: %s"), Profile.m_SkinName);
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
			Ui()->DoLabel(&Label, EcLocalize("Your profile"), FontSize, TEXTALIGN_ML);
			Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
			Profiles.HSplitTop(50.0f, &Skin, &Profiles);
			FRenderProfile(Skin, CurrentProfile, true);

			// After load
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(LineSize, &Label, &Profiles);
				Ui()->DoLabel(&Label, EcLocalize("After Load"), FontSize, TEXTALIGN_ML);
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
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileSkin, EcLocalize("Save/Load Skin"), &g_Config.m_ClProfileSkin, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileColors, EcLocalize("Save/Load Colors"), &g_Config.m_ClProfileColors, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileEmote, EcLocalize("Save/Load Emote"), &g_Config.m_ClProfileEmote, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileName, EcLocalize("Save/Load Name"), &g_Config.m_ClProfileName, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileClan, EcLocalize("Save/Load Clan"), &g_Config.m_ClProfileClan, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClProfileFlag, EcLocalize("Save/Load Flag"), &g_Config.m_ClProfileFlag, &Settings, LineSize);
		}
		{
			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_LoadButton;
			if(DoButton_Menu(&s_LoadButton, EcLocalize("Load"), 0, &Button))
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
			if(DoButton_Menu(&s_SaveButton, EcLocalize("Save"), 0, &Button))
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
			DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, EcLocalize("Enable Deleting"), &s_AllowDelete, &Actions, LineSize);
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			if(s_AllowDelete)
			{
				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_DeleteButton;
				if(DoButton_Menu(&s_DeleteButton, EcLocalize("Delete"), 0, &Button))
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
						GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + s_SelectedProfile);
				Actions.HSplitTop(5.0f, nullptr, &Actions);

				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_OverrideButton;
				if(DoButton_Menu(&s_OverrideButton, EcLocalize("Override"), 0, &Button))
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
		if(DoButton_CheckBox(&m_Dummy, EcLocalize("Dummy"), m_Dummy, &Button))
			m_Dummy = 1 - m_Dummy;

		Options.VSplitLeft(150.0f, &Button, &Options);
		static int s_CustomColorId = 0;
		if(DoButton_CheckBox(&s_CustomColorId, EcLocalize("Custom colors"), *pCurrentUseCustomColor, &Button))
		{
			*pCurrentUseCustomColor = *pCurrentUseCustomColor ? 0 : 1;
			SetNeedSendInfo();
		}

		Button = Options;
		if(DoButton_CheckBox(&g_Config.m_ClProfileOverwriteClanWithEmpty, EcLocalize("Overwrite clan even if empty"), g_Config.m_ClProfileOverwriteClanWithEmpty, &Button))
			g_Config.m_ClProfileOverwriteClanWithEmpty = 1 - g_Config.m_ClProfileOverwriteClanWithEmpty;
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect SelectorRect;
		MainView.HSplitBottom(LineSize + MarginSmall, &MainView, &SelectorRect);
		SelectorRect.HSplitTop(MarginSmall, nullptr, &SelectorRect);

		static CButtonContainer s_ProfilesFile;
		SelectorRect.VSplitLeft(130.0f, &Button, &SelectorRect);
		if(DoButton_Menu(&s_ProfilesFile, EcLocalize("Profiles file"), 0, &Button))
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
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_ForceShowScrollbar = true;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	std::vector<CSettingsModule> vModules;

	CUIRect Label, Button;

	/* Notifications */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_CHAT,
		{"notify", "notifications", "join", "message", "last", "player", "name", "moved", "tabbed", "out", "flash", "window"},
		[](bool HasSearch) {
			int OffSet = 0;
			if(g_Config.m_ClNotifyOnJoin || HasSearch)
				OffSet += 20.0f;
			if(g_Config.m_ClNotifyWhenLast || HasSearch)
				OffSet += 20.0f;

			return 105.0f + OffSet;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Notifications"), HeaderSize, HeaderAlignment);
			{
				auto RenderToggleEditBox = [&](const char *pLabel, int *pConfigValue, CLineInput *pLineInput, char *pBuffer, int BufferSize, const char *pEmptyText, float Length) {
					ModuleRect.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &ModuleRect);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					pLineInput->SetBuffer(pBuffer, BufferSize);
					pLineInput->SetEmptyText(pEmptyText);

					if(DoButton_CheckBox(pConfigValue, pLabel, *pConfigValue, &ModuleRect))
						*pConfigValue ^= 1;

					if(*pConfigValue || HasSearch)
						Ui()->DoEditBox(pLineInput, &Button, EditBoxFontSize);
				};

				auto RenderLabeledEditBox = [&](const char *pLabel, CLineInput *pLineInput, char *pBuffer, int BufferSize, const char *pEmptyText, float Length, float LabelFontSize = 12.5f) {
					ModuleRect.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &ModuleRect);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					pLineInput->SetBuffer(pBuffer, BufferSize);
					pLineInput->SetEmptyText(pEmptyText);
					Ui()->DoEditBox(pLineInput, &Button, EditBoxFontSize);

					ModuleRect.HSplitTop(3.0f, &Button, &ModuleRect);
					Ui()->DoLabel(&ModuleRect, pLabel, LabelFontSize, TEXTALIGN_LEFT);
					ModuleRect.HSplitTop(-3.0f, &Button, &ModuleRect);
				};

				{
					const char *Name = g_Config.m_ClNotifyOnJoin ? "Notify on Join Name" : "Notify on Join";
					float Length = TextRender()->TextBoundingBox(FontSize, "Notify on Join Name").m_W + 16.5f; // Give it some breathing room

					static CLineInput s_NotifyName;
					RenderToggleEditBox(Name, &g_Config.m_ClNotifyOnJoin, &s_NotifyName, g_Config.m_ClAutoNotifyName, sizeof(g_Config.m_ClAutoNotifyName), "qxdFox", Length);
				}

				if(g_Config.m_ClNotifyOnJoin || HasSearch)
				{
					float Length = TextRender()->TextBoundingBox(12.5f, "Notify Message").m_W + 3.5f; // Give it some breathing room

					ModuleRect.HSplitTop(21.0f, &Button, &ModuleRect);

					static CLineInput s_NotifyMsg;
					RenderLabeledEditBox("Notify Message", &s_NotifyMsg, g_Config.m_ClAutoNotifyMsg, sizeof(g_Config.m_ClAutoNotifyMsg), "Your Fav Person Has Joined!", Length);
				}
				ModuleRect.HSplitTop(25.0f, &Button, &ModuleRect);
				{
					ModuleRect.HSplitTop(20.0f, &Button, &ModuleRect);

					Button.VSplitLeft(0.0f, 0, &ModuleRect);

					if(DoButton_CheckBox(&g_Config.m_ClNotifyWhenLast, "Show when you're the last player", g_Config.m_ClNotifyWhenLast, &ModuleRect))
						g_Config.m_ClNotifyWhenLast ^= 1;

					if(g_Config.m_ClNotifyWhenLast || HasSearch)
					{
						static CLineInput s_LastMessage;
						s_LastMessage.SetBuffer(g_Config.m_ClNotifyWhenLastText, sizeof(g_Config.m_ClNotifyWhenLastText));
						s_LastMessage.SetEmptyText("Last!");

						float Length = TextRender()->TextBoundingBox(12.5f, "Text to Show").m_W + 3.5f; // Give it some breathing room

						ModuleRect.HSplitTop(21.0f, &Button, &ModuleRect);
						ModuleRect.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(100.0f, &Button, &MainView);

						Ui()->DoEditBox(&s_LastMessage, &Button, EditBoxFontSize);

						ModuleRect.HSplitTop(20.0f, &Button, &ModuleRect);
						Ui()->DoLabel(&ModuleRect, "Text to Show", 12.5f, TEXTALIGN_ML);
						ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);

						static CButtonContainer s_LastColor;
						ModuleRect.HSplitTop(-3.0f, &Button, &ModuleRect);
						DoLine_ColorPicker(&s_LastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClNotifyWhenLastColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClNotifyWhenLastColor)), true);

						ModuleRect.HSplitTop(-25.0f, &Button, &ModuleRect);
					}
				}
				ModuleRect.HSplitTop(20.0f, &Button, &ModuleRect);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNotifyOnMove, "Notify when in-game character is being moved", &g_Config.m_ClNotifyOnMove, &ModuleRect, LineSize);
			}
		},
	});

	/* Auto Reply */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_CHAT,
		{"auto", "reply", "tabbed", "muted", "reply", "message", "tab"},
		[](bool HasSearch) {
			return 80.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Auto Reply"), HeaderSize, HeaderAlignment);
			{
				auto RenderToggleEditBox = [&](const char *pLabel, int *pConfigValue, CLineInput *pLineInput, char *pBuffer, int BufferSize, const char *pEmptyText, float Length) {
					ModuleRect.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &ModuleRect);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					pLineInput->SetBuffer(pBuffer, BufferSize);
					pLineInput->SetEmptyText(pEmptyText);

					if(DoButton_CheckBox(pConfigValue, pLabel, *pConfigValue, &ModuleRect))
						*pConfigValue ^= 1;

					if(*pConfigValue || HasSearch)
						Ui()->DoEditBox(pLineInput, &Button, EditBoxFontSize);
				};

				std::array<float, 2> Sizes = {
					TextRender()->TextBoundingBox(FontSize, "Tabbed reply").m_W,
					TextRender()->TextBoundingBox(FontSize, "Muted Reply").m_W};
				float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 23.5f;

				static CLineInput s_TabbedReplyMsg;
				RenderToggleEditBox("Tabbed reply", &g_Config.m_ClTabbedOutMsg, &s_TabbedReplyMsg, g_Config.m_ClAutoReplyMsg, sizeof(g_Config.m_ClAutoReplyMsg), "I'm Currently Tabbed Out", Length);
				ModuleRect.HSplitTop(21.0f, &Button, &ModuleRect);

				static CLineInput s_MutedReplyMsg;
				RenderToggleEditBox("Muted Reply", &g_Config.m_ClReplyMuted, &s_MutedReplyMsg, g_Config.m_ClAutoReplyMutedMsg, sizeof(g_Config.m_ClAutoReplyMutedMsg), "You're muted, I can't see your messages", Length);
			}
		},
	});

	/* Player Indicator */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_HUD | FILTER_VISUAL | FILTER_WARLIST,
		{"player", "indicator", "size", "offset", "distance", "warlist", "groups", "colors", "freeze", "circle", "opacity"},
		[](bool HasSearch) {
			int Size = 80.0f;
			if(g_Config.m_ClPlayerIndicator || HasSearch)
			{
				Size = 285.0f;
				if(g_Config.m_ClIndicatorVariableDistance || HasSearch)
					Size += 40.0f;
				if(g_Config.m_ClWarListIndicator || HasSearch)
					Size += 80.0f;
				if(!g_Config.m_ClWarListIndicatorColors || !g_Config.m_ClWarListIndicator || HasSearch)
					Size += 40.0f;
			}

			return Size;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Player Indicator"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, EcLocalize("Show any enabled Indicators"), &g_Config.m_ClPlayerIndicator, &ModuleRect, LineSize);

				if(g_Config.m_ClPlayerIndicator || HasSearch)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorHideOnScreen, EcLocalize("Hide indicator for tees on your screen"), &g_Config.m_ClIndicatorHideOnScreen, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicatorFreeze, EcLocalize("Show only freeze Players"), &g_Config.m_ClPlayerIndicatorFreeze, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTeamOnly, EcLocalize("Only show after joining a team"), &g_Config.m_ClIndicatorTeamOnly, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTees, EcLocalize("Render tiny tees instead of circles"), &g_Config.m_ClIndicatorTees, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicator, EcLocalize("Use warlist groups for indicator"), &g_Config.m_ClWarListIndicator, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorRadius, &g_Config.m_ClIndicatorRadius, &Button, EcLocalize("Indicator size"), 1, 16);
					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOpacity, &g_Config.m_ClIndicatorOpacity, &Button, EcLocalize("Indicator opacity"), 0, 100);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorVariableDistance, EcLocalize("Change indicator offset based on distance to other tees"), &g_Config.m_ClIndicatorVariableDistance, &ModuleRect, LineSize);
					if(g_Config.m_ClIndicatorVariableDistance || HasSearch)
					{
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, EcLocalize("Indicator min offset"), 16, 200);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffsetMax, &g_Config.m_ClIndicatorOffsetMax, &Button, EcLocalize("Indicator max offset"), 16, 200);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorMaxDistance, &g_Config.m_ClIndicatorMaxDistance, &Button, EcLocalize("Indicator max distance"), 500, 7000);
					}
					else
					{
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, EcLocalize("Indicator offset"), 16, 200);
					}
					if(g_Config.m_ClWarListIndicator || HasSearch)
					{
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorColors, EcLocalize("Use warlist colors instead of regular colors"), &g_Config.m_ClWarListIndicatorColors, &ModuleRect, LineSize);
						char aBuf[128];
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorAll, EcLocalize("Show all warlist groups"), &g_Config.m_ClWarListIndicatorAll, &ModuleRect, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(1)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorEnemy, aBuf, &g_Config.m_ClWarListIndicatorEnemy, &ModuleRect, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(2)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorTeam, aBuf, &g_Config.m_ClWarListIndicatorTeam, &ModuleRect, LineSize);
					}
					if(!g_Config.m_ClWarListIndicatorColors || !g_Config.m_ClWarListIndicator || HasSearch)
					{
						static CButtonContainer s_IndicatorAliveColorId, s_IndicatorDeadColorId, s_IndicatorSavedColorId;
						DoLine_ColorPicker(&s_IndicatorAliveColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Indicator alive color"), &g_Config.m_ClIndicatorAlive, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorAlive)), false);
						DoLine_ColorPicker(&s_IndicatorDeadColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Indicator in freeze color"), &g_Config.m_ClIndicatorFreeze, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorFreeze)), false);
						DoLine_ColorPicker(&s_IndicatorSavedColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Indicator safe color"), &g_Config.m_ClIndicatorSaved, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClIndicatorSaved)), false);
					}
				}
			}
		},
	});

	/* Chat Settings */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_CHAT,
		{"chat", "typing", "indicator", "show", "mute", "console", "hide", "enemy", "friend", "spec", "server", "client", "warlist", "client", "id", "preview", "math", "expression", "calculator"},
		[](bool HasSearch) {
			return 415.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Chat Settings"), HeaderSize, HeaderAlignment);
			{
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				if(DoButton_CheckBox(&g_Config.m_ClChatBubble, ("Show Typing Indicator"), g_Config.m_ClChatBubble, &Button))
					g_Config.m_ClChatBubble ^= 1;
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClChatBubble, &Button, "Lets others see the vanilla '...' icon above your tee while you're typing - not the floating chat text, that's the separate Chat Bubbles module");
				ModuleRect.HSplitTop(2.5f, &Button, &ModuleRect);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowMutedInConsole, ("Show Messages of Muted People in The Console"), &g_Config.m_ClShowMutedInConsole, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideEnemyChat, ("Hide Enemy Chat (Shows in Console)"), &g_Config.m_ClHideEnemyChat, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowIdsChat, ("Show Client Ids in Chat"), &g_Config.m_ClShowIdsChat, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				if(DoButton_CheckBox(&g_Config.m_ClChatMath, ("Suggest Math Results in Chat"), g_Config.m_ClChatMath, &Button))
					g_Config.m_ClChatMath ^= 1;
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClChatMath, &Button, "End a message with '=' to get the result suggested, press Tab to insert it");

				ModuleRect.HSplitTop(-10.0f, &Button, &ModuleRect);

				const std::array<float, 5> Sizes = {
					TextRender()->TextWidth(FontSize, "Friend Prefix"),
					TextRender()->TextWidth(FontSize, "Spec Prefix"),
					TextRender()->TextWidth(FontSize, "Server Prefix"),
					TextRender()->TextWidth(FontSize, "Client Prefix"),
					TextRender()->TextWidth(FontSize, "Warlist Prefix"),
				};
				const float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 20.0f;

				auto RenderPrefixOption = [&](CLineInput *pLineInput, const char *pLabel, int *pConfigValue, char *pBuffer, const char *pEmptyText) {
					ModuleRect.HSplitTop(21.0f, &Button, &ModuleRect);
					{
						ModuleRect.HSplitTop(19.9f, &Button, &MainView);
						Button.VSplitLeft(0.0f, &Button, &ModuleRect);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitLeft(85.0f, &Button, 0);
						pLineInput->SetBuffer(pBuffer, 32);
						pLineInput->SetEmptyText(pEmptyText);
						if(DoButton_CheckBox(pConfigValue, pLabel, *pConfigValue, &ModuleRect))
						{
							*pConfigValue ^= 1;
						}
						if(*pConfigValue)
							Ui()->DoEditBox(pLineInput, &Button, EditBoxFontSize);
					}
				};

				static CLineInput s_FriendInput;
				RenderPrefixOption(&s_FriendInput, "Friend Prefix", &g_Config.m_ClMessageFriend, g_Config.m_ClFriendPrefix, "alt3");

				static CLineInput s_SpecInput;
				RenderPrefixOption(&s_SpecInput, "Spec Prefix", &g_Config.m_ClSpectatePrefix, g_Config.m_ClSpecPrefix, "alt7");

				static CLineInput s_ServerInput;
				RenderPrefixOption(&s_ServerInput, "Server Prefix", &g_Config.m_ClChatServerPrefix, g_Config.m_ClServerPrefix, "*** ");

				static CLineInput s_ClientInput;
				RenderPrefixOption(&s_ClientInput, "Client Prefix", &g_Config.m_ClChatClientPrefix, g_Config.m_ClClientPrefix, "alt0151");

				static CLineInput s_WarlistInput;
				RenderPrefixOption(&s_WarlistInput, "Warlist Prefix", &g_Config.m_ClWarlistPrefixes, g_Config.m_ClWarlistPrefix, "alt4");

				ModuleRect.HSplitTop(55.0f, &Button, &ModuleRect);
				Ui()->DoLabel(&Button, EcLocalize("Chat Preview"), FontSize + 3, TEXTALIGN_ML);
				ModuleRect.HSplitTop(-15.0f, &Button, &ModuleRect);

				RenderChatPreview(ModuleRect);
			}
		},
	});

	/* Night Shift */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_VISUAL,
		{"night", "shift", "warm", "temperature", "kelvin", "blue", "light", "eye", "strain", "sunset", "sunrise", "schedule", "latitude", "longitude"},
		[&](bool HasSearch) {
			const bool Scheduled = g_Config.m_ClNightShiftSchedule != NIGHT_SHIFT_SCHEDULE_ALWAYS;
			float Height = HeaderHeight + LineSize * 4.0f + MarginSmall + MarginExtraSmall + MarginSmall;
			if(Scheduled || HasSearch)
				Height += LineSize;
			if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_CUSTOM || HasSearch)
				Height += LineSize * 2.0f;
			if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_SUN || HasSearch)
				Height += LineSize * 3.0f;
			return Height;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Night Shift"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNightShift, EcLocalize("Use Night Shift"), &g_Config.m_ClNightShift, &ModuleRect, LineSize);
			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClNightShift, &ModuleRect, "Shifts the colors of everything on screen towards red, which is easier on the eyes at night");

			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);

			static std::vector<const char *> s_ScheduleDropDownNames;
			s_ScheduleDropDownNames = {EcLocalize("Always on"), EcLocalize("Custom hours"), EcLocalize("Sunset to sunrise")};
			static CUi::SDropDownState s_ScheduleDropDownState;
			static CScrollRegion s_ScheduleDropDownScrollRegion;
			s_ScheduleDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ScheduleDropDownScrollRegion;
			CUIRect ScheduleDropDownRect;
			ModuleRect.HSplitTop(LineSize, &ScheduleDropDownRect, &ModuleRect);
			const int ScheduleSelected = Ui()->DoDropDown(&ScheduleDropDownRect, g_Config.m_ClNightShiftSchedule, s_ScheduleDropDownNames.data(), s_ScheduleDropDownNames.size(), s_ScheduleDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_ScheduleDropDownState.m_SelectionPopupContext, ScheduleDropDownRect.x, ScheduleDropDownRect.y);
			if(s_ScrollRegion.RectClipped(ScheduleDropDownRect))
				Ui()->ClosePopupMenu(&s_ScheduleDropDownState.m_SelectionPopupContext);
			g_Config.m_ClNightShiftSchedule = ScheduleSelected;

			ModuleRect.HSplitTop(MarginExtraSmall, nullptr, &ModuleRect);

			// The bar runs from least warm on the left to most warm on the right, which is the
			// opposite direction to the kelvin value behind it, so it drives a warmth percentage.
			constexpr int NeutralTemperature = 6500;
			constexpr int WarmestTemperature = 1900;
			int Warmth = std::clamp(round_to_int((NeutralTemperature - g_Config.m_ClNightShiftTemperature) * 100.0f / (NeutralTemperature - WarmestTemperature)), 0, 100);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			if(Ui()->DoScrollbarOption(&g_Config.m_ClNightShiftTemperature, &Warmth, &Button, EcLocalize("Warmth"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%"))
				g_Config.m_ClNightShiftTemperature = NeutralTemperature - Warmth * (NeutralTemperature - WarmestTemperature) / 100;
			static char s_aWarmthTooltip[128];
			str_format(s_aWarmthTooltip, sizeof(s_aWarmthTooltip), EcLocalize("Drag right to remove more blue light, currently %dK"), g_Config.m_ClNightShiftTemperature);
			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClNightShiftTemperature, &ModuleRect, s_aWarmthTooltip);

			if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_CUSTOM || HasSearch)
			{
				DoLine_TimeOfDay(ModuleRect, EcLocalize("Turns on at"), &g_Config.m_ClNightShiftFrom);
				DoLine_TimeOfDay(ModuleRect, EcLocalize("Turns off at"), &g_Config.m_ClNightShiftTo);
			}

			if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_SUN || HasSearch)
			{
				// The sunrise and sunset times depend on where you are, and the client has no
				// way of knowing that, so the coordinates have to be typed in once.
				static CLineInputNumber s_LatitudeInput;
				static CLineInputNumber s_LongitudeInput;
				if(!s_LatitudeInput.IsActive())
					s_LatitudeInput.SetFloat(g_Config.m_ClNightShiftLatitude / 100.0f);
				if(!s_LongitudeInput.IsActive())
					s_LongitudeInput.SetFloat(g_Config.m_ClNightShiftLongitude / 100.0f);

				CUIRect EditBoxRect;
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Button.VSplitMid(&Label, &EditBoxRect, std::min(10.0f, Button.w * 0.05f));
				Ui()->DoLabel(&Label, EcLocalize("Latitude"), FontSize, TEXTALIGN_ML);
				if(Ui()->DoEditBox(&s_LatitudeInput, &EditBoxRect, EditBoxFontSize))
					g_Config.m_ClNightShiftLatitude = std::clamp((int)(s_LatitudeInput.GetFloat() * 100.0f), -9000, 9000);

				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Button.VSplitMid(&Label, &EditBoxRect, std::min(10.0f, Button.w * 0.05f));
				Ui()->DoLabel(&Label, EcLocalize("Longitude"), FontSize, TEXTALIGN_ML);
				if(Ui()->DoEditBox(&s_LongitudeInput, &EditBoxRect, EditBoxFontSize))
					g_Config.m_ClNightShiftLongitude = std::clamp((int)(s_LongitudeInput.GetFloat() * 100.0f), -18000, 18000);

				char aBuf[128];
				int Sunrise, Sunset;
				switch(CNightShift::SunTimes(&Sunrise, &Sunset))
				{
				case CNightShift::SUN_NORMAL:
					str_format(aBuf, sizeof(aBuf), EcLocalize("Sunrise %02d:%02d, sunset %02d:%02d"),
						Sunrise / 60, Sunrise % 60, Sunset / 60, Sunset % 60);
					break;
				case CNightShift::SUN_ALWAYS_UP:
					str_copy(aBuf, EcLocalize("The sun does not set here today"));
					break;
				case CNightShift::SUN_ALWAYS_DOWN:
					str_copy(aBuf, EcLocalize("The sun does not rise here today"));
					break;
				default:
					str_copy(aBuf, EcLocalize("Local time is unavailable"));
					break;
				}
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoLabel(&Button, aBuf, FontSize, TEXTALIGN_ML);
			}

			if(g_Config.m_ClNightShiftSchedule != NIGHT_SHIFT_SCHEDULE_ALWAYS || HasSearch)
			{
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_ClNightShiftTransition, &g_Config.m_ClNightShiftTransition, &Button, EcLocalize("Fade"), 0, 180, &CUi::ms_LinearScrollbarScale, 0, EcLocalize(" minutes"));
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClNightShiftTransition, &ModuleRect, "How long the shift takes to fade in at the start and out at the end of its window");
			}

			char aStatus[128];
			const int Now = CNightShift::LocalMinuteOfDay();
			if(Now < 0)
				str_copy(aStatus, EcLocalize("Local time is unavailable"));
			else
				str_format(aStatus, sizeof(aStatus), EcLocalize("Local time %02d:%02d, shift at %d%%"), Now / 60, Now % 60, (int)(CNightShift::Strength() * 100.0f + 0.5f));
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, aStatus, FontSize, TEXTALIGN_ML);

			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);
		},
	});

	/* Ghost Tools */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_GAMEPLAY | FILTER_VISUAL,
		{"ghost", "prediction", "players", "swap", "hide"},
		[](bool HasSearch) {
			return 180.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Ghost Tools"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcShowOthersGhosts, EcLocalize("Show unpredicted ghosts for other players"), &g_Config.m_EcShowOthersGhosts, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcSwapGhosts, EcLocalize("Swap ghosts and normal players"), &g_Config.m_EcSwapGhosts, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_EcPredGhostsAlpha, &g_Config.m_EcPredGhostsAlpha, &Button, EcLocalize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_EcUnpredGhostsAlpha, &g_Config.m_EcUnpredGhostsAlpha, &Button, EcLocalize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcHideFrozenGhosts, EcLocalize("Hide ghosts of frozen players"), &g_Config.m_EcHideFrozenGhosts, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcRenderGhostAsCircle, EcLocalize("Render ghosts as circles"), &g_Config.m_EcRenderGhostAsCircle, &ModuleRect, LineSize);

				static CButtonContainer s_ReaderButtonGhost, s_ClearButtonGhost;
				DoLine_KeyReader(ModuleRect, s_ReaderButtonGhost, s_ClearButtonGhost, EcLocalize("Toggle ghosts key"), "toggle tc_show_others_ghosts 0 1");
			}
		},
	});

	/* Performance */
#if defined(CONF_FAMILY_WINDOWS)
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_MISC,
		{"performance", "high", "lower", "process", "discord", "priority", "ddnet"},
		[](bool HasSearch) {
			return 80.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Performance"), HeaderSize, HeaderAlignment);

			if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHighProcessPriority, ("High DDNet Process Priority"), &g_Config.m_ClHighProcessPriority, &ModuleRect, LineSize))
				GameClient()->m_EClient.SetDDNetProcessPriority(g_Config.m_ClHighProcessPriority);

			if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordNormalProcessPriority, ("Lower Discords Process Priority"), &g_Config.m_ClDiscordNormalProcessPriority, &ModuleRect, LineSize))
			{
				if(g_Config.m_ClDiscordNormalProcessPriority)
					GameClient()->m_EClient.StartDiscordPriorityThread();
			}
		},
	});
#endif

	/* Fast Input */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_GAMEPLAY | FILTER_NETWORK,
		{"fast", "input", "reduced", "visual", "delay", "tick", "others", "flux", "classic", "mode", "prediction", "offset"},
		[](bool HasSearch) {
			int Size = 122.0f;
			if(g_Config.m_EcFastInput || HasSearch)
				Size += 25.0f;

			return Size;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Input"), HeaderSize, HeaderAlignment);
			{
				if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcFastInput, EcLocalize("Enable Fast Input"), &g_Config.m_EcFastInput, &ModuleRect, LineSize))
					Client()->SendFastInputsInfo(g_Config.m_ClDummy);

				static std::vector<CButtonContainer> s_vFastInputModeButtons(2);
				static const std::vector<const char *> s_vFastInputModeLabels = {EcLocalize("Classic"), EcLocalize("Flux")};
				static const std::vector<int> s_vFastInputModeValues = {CGameClient::FAST_INPUT_MODE_CLASSIC, CGameClient::FAST_INPUT_MODE_FLUX};

				DoLine_RadioMenu_Compact(ModuleRect, "", s_vFastInputModeButtons, s_vFastInputModeLabels, s_vFastInputModeValues, g_Config.m_EcFastInputMode, 0.0f);

				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				if(g_Config.m_EcFastInputMode == CGameClient::FAST_INPUT_MODE_FLUX)
				{
					CUIRect FastInputLabel, FastInputScrollBar;
					Button.VSplitMid(&FastInputLabel, &FastInputScrollBar, std::min(10.0f, Button.w * 0.05f));
					int Value = Ui()->ScrollbarKeyboardAdjust(&Button, g_Config.m_EcFluxInputAmount, 0, 500);

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %.2f %s", EcLocalize("Prediction offset"), Value / 100.0f, EcLocalize("ticks"));
					Ui()->DoLabel(&FastInputLabel, aBuf, FastInputLabel.h * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_ML);

					const float Relative = CUi::ms_LinearScrollbarScale.ToRelative(Value, 0, 500);
					Value = CUi::ms_LinearScrollbarScale.ToAbsolute(Ui()->DoScrollbarH(&g_Config.m_EcFluxInputAmount, &FastInputScrollBar, Relative), 0, 500);
					g_Config.m_EcFluxInputAmount = std::clamp(Value, 0, 500);
				}
				else if(Ui()->DoScrollbarOption(&g_Config.m_EcFastInputAmount, &g_Config.m_EcFastInputAmount, &Button, EcLocalize("Prediction offset"), 1, 40, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE, "ms"))
					Client()->SendFastInputsInfo(g_Config.m_ClDummy);
				ModuleRect.HSplitTop(2.0f, &Button, &ModuleRect);

				static const std::vector<const char *> s_vFastInputOthersLabels = {EcLocalize("Fast input others"), EcLocalize("Flux input others")};
				if(g_Config.m_EcFastInput || HasSearch)
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcFastInputOthers, s_vFastInputOthersLabels[g_Config.m_EcFastInputMode], &g_Config.m_EcFastInputOthers, &ModuleRect, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, EcLocalize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &ModuleRect, LineSize);
			}
		},
	});

	/* Anti Ping Smoothing */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_NETWORK | FILTER_GAMEPLAY,
		{"anti", "ping", "smoothing", "new", "algorithm", "optimistic", "prediction", "stable", "direction", "remember", "instability", "longer", "uncertainty", "duration"},
		[](bool HasSearch) {
			return 120.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Anti Ping Smoothing"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcAntiPingImproved, EcLocalize("Use new smoothing algorithm"), &g_Config.m_EcAntiPingImproved, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcAntiPingStableDirection, EcLocalize("Optimistic prediction in stable direction"), &g_Config.m_EcAntiPingStableDirection, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcAntiPingNegativeBuffer, EcLocalize("Remember instability for longer"), &g_Config.m_EcAntiPingNegativeBuffer, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_EcAntiPingUncertaintyScale, &g_Config.m_EcAntiPingUncertaintyScale, &Button, EcLocalize("Uncertainty duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
			}
		},
	});

	auto RenderHudEditorButton = [this](CButtonContainer *pId, const CUIRect *pRect) {
		CUIRect Temp = *pRect;
		constexpr float a = HeaderSize + MarginExtraSmall;
		Temp.y += MarginSmall;
		Temp.h = a;
		Temp.x = Temp.x + Temp.w - a - MarginExtraSmall;
		Temp.w = a;
		DoHudEditorButton(pId, &Temp);
	};

#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
	/* Media Island */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_HUD,
		{"media", "music", "island", "visualizer", "size", "alignment", "bottom", "center"},
		[](bool HasSearch) {
			int Offset = 0;

			if(g_Config.m_ClMediaIslandVisualizer || HasSearch)
				Offset += LineSize;

			return 110.0f + Offset;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			static CButtonContainer Id;
			RenderHudEditorButton(&Id, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Media Island"), HeaderSize, HeaderAlignment);

			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMediaIsland, "Enable media island", &g_Config.m_ClMediaIsland, &ModuleRect, LineSize);

				static CButtonContainer s_IslandColor;
				DoLine_ColorPicker(&s_IslandColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Island color"), &g_Config.m_ClMediaIslandColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClMediaIslandColor, true)), false, nullptr, true);

				//ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				//Ui()->DoScrollbarOption(&g_Config.m_ClMediaIslandAnimation, &g_Config.m_ClMediaIslandAnimation, &Button, "Animation Time", 0, 300, &CUi::ms_LinearScrollbarScale, 0u, "");
				//GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMediaIslandAnimation, &Button, "Time it takes for the Islands animation, lower = slower", FontSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMediaIslandVisualizer, "Show Visualizer", &g_Config.m_ClMediaIslandVisualizer, &ModuleRect, LineSize);

				if(g_Config.m_ClMediaIslandVisualizer || HasSearch)
				{
					static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}};
					int Value = g_Config.m_ClMediaIslandVisualizerAlignment;
					if(DoLine_RadioMenu(ModuleRect, EcLocalize("Visualizer Alignment:"), s_vButtonContainers, {"Bottom", "Center"}, {1, 2}, Value))
					{
						g_Config.m_ClMediaIslandVisualizerAlignment = Value;
					}
				}
			}
		},
	});
#endif

	/* Gores Mode */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_GAMEPLAY,
		{"gores", "mode", "advanced", "disable", "weapons", "automation", "enable", "gametype"},
		[](bool HasSearch) {
			return 100.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Gores Mode"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresMode, EcLocalize("Enable Gores Mode"), &g_Config.m_ClGoresMode, &ModuleRect, LineSize);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresModeDisableIfWeapons, EcLocalize("Disable if You Have Any Weapon"), &g_Config.m_ClGoresModeDisableIfWeapons, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoEnableGoresMode, EcLocalize("Auto Enable if Gametype is \"Gores\""), &g_Config.m_ClAutoEnableGoresMode, &ModuleRect, LineSize);
		},
	});

	/* Miscellaneous */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_MISC | FILTER_VISUAL | FILTER_HUD,
		{"miscellaneous", "custom", "font", "options", "ping", "circles", "names", "old", "team", "moving", "tiles", "entities", "entity", "cursor"},
		[](bool HasSearch) {
			return 175.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Miscellaneous"), HeaderSize, HeaderAlignment);
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
					ModuleRect.HSplitTop(LineSize, &FontDropDownRect, &ModuleRect);

					float Length = TextRender()->TextBoundingBox(FontSize, "Custom Font:").m_W + 3.5f;

					FontDropDownRect.VSplitLeft(Length, &Label, &FontDropDownRect);
					FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
					FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

					Ui()->DoLabel(&Label, EcLocalize("Custom Font:"), FontSize, TEXTALIGN_ML);
					const int FontSelectedNew = Ui()->DoDropDown(&FontDropDownRect, FontSelectedOld, s_FontDropDownNames.data(), s_FontDropDownNames.size(), s_FontDropDownState);
					Ui()->UpdatePopupMenuOffset(&s_FontDropDownState.m_SelectionPopupContext, FontDropDownRect.x, FontDropDownRect.y);

					if(s_ScrollRegion.RectClipped(FontDropDownRect))
					{
						Ui()->ClosePopupMenu(&s_FontDropDownState.m_SelectionPopupContext);
					}

					if(FontSelectedOld != FontSelectedNew)
					{
						str_copy(g_Config.m_ClCustomFont, s_FontDropDownNames[FontSelectedNew]);
						TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

						GameClient()->OnFontChange();
					}
					static CButtonContainer s_FontDirectoryId;
					if(Ui()->DoButton_FontIcon(&s_FontDirectoryId, FontIcon::FOLDER, 0, &FontDirectory, BUTTONFLAG_LEFT))
					{
						Storage()->CreateFolder("data/entity", IStorage::TYPE_ABSOLUTE);
						Storage()->CreateFolder("data/entity/fonts", IStorage::TYPE_ABSOLUTE);
						Client()->ViewFile("data/entity/fonts");
					}
				}

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, ("Show Ping Circles Next To Names"), &g_Config.m_ClPingNameCircle, &ModuleRect, LineSize);

				ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClScoreboardOutlineTeams, EcLocalize("Outline Teams in Scoreboard"), &g_Config.m_ClScoreboardOutlineTeams, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRevertTeamColors, EcLocalize("Use Old Team Colors"), &g_Config.m_ClRevertTeamColors, &ModuleRect, LineSize);

				ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);

				{
					static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}, {}};
					static const std::vector<const char *> s_vTooltips = {
						EcLocalize("Don't show moving tiles in entities"),
						EcLocalize("Use map design for moving tiles in entities"),
						EcLocalize("Use selected game-assets' colors for moving tiles in entities"),
						EcLocalize("Use game-assets' colors and map design for moving tiles in entities"),
					};
					int Value = g_Config.m_ClShowMovingTilesEntities;
					if(DoLine_RadioMenu_Compact(ModuleRect, EcLocalize("Moving Tiles:"),
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

				ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_ClCursorOpacitySpec, &g_Config.m_ClCursorOpacitySpec, &Button, EcLocalize("Cursor Opacity in Spec"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
			}
		},
	});

	/* Stats */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_HUD,
		{"stats", "fps", "ping", "snap", "rate", "show"},
		[](bool HasSearch) {
			return 100;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Stats"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatisticsShowFps, EcLocalize("Show FPS"), &g_Config.m_ClStatisticsShowFps, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatisticsShowPing, EcLocalize("Show Ping"), &g_Config.m_ClStatisticsShowPing, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClStatisticsShowSnapRate, EcLocalize("Show Snap Rate"), &g_Config.m_ClStatisticsShowSnapRate, &ModuleRect, LineSize);
			}
		},
	});

	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_GAMEPLAY,
		{"ingame", "in-game", "game", "automation", "auto", "dummy", "connect", "switch", "spawn", "anti", "block"},
		[](bool HasSearch) {
			return 80.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("In-Game Automation"), HeaderSize, HeaderAlignment);

			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			if(DoButton_CheckBox(&g_Config.m_ClAntiSpawnBlock, "Anti Spawn Block", g_Config.m_ClAntiSpawnBlock, &Button))
				g_Config.m_ClAntiSpawnBlock ^= 1;
			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClAntiSpawnBlock, &Button, "Puts you into a random Team when you respawn and back to team 0 when close to start line");

			{
				static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
				static const std::vector<const char *> s_vTooltips = {
					EcLocalize("Off"),
					EcLocalize("Automatically connect dummy on join"),
					EcLocalize("Automatically connect dummy and switch back to main automatically"),
				};
				int Value = g_Config.m_ClAutoDummyConnect;
				if(DoLine_RadioMenu_Compact(ModuleRect, EcLocalize("Auto Dummy:"),
					   s_vButtonContainers,
					   {"Off", "On", "On + No Switch"},
					   {0, 1, 2},
					   Value,
					   5.0f,
					   &s_vTooltips))
				{
					g_Config.m_ClAutoDummyConnect = Value;
				}
			}
		},
	});

	/* Chat Bubbles */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_CHAT | FILTER_VISUAL,
		{"chat", "bubble", "player", "fade", "in", "out", "size"},
		[](bool HasSearch) {
			return 145.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Chat Bubbles"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatBubbles, EcLocalize("Show Chatbubbles above players"), &g_Config.m_ClChatBubbles, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_ClChatBubbleSize, &g_Config.m_ClChatBubbleSize, &Button, EcLocalize("Chat Bubble Size"), 20, 30);

				ModuleRect.HSplitTop(MarginSmall + LineSize, &Button, &ModuleRect);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleShowTime, &g_Config.m_ClChatBubbleShowTime, &Button, EcLocalize("Show the Bubbles for"), 200, 1000, 100, &CUi::ms_LinearScrollbarScale, 0, "s");

				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeIn, &g_Config.m_ClChatBubbleFadeIn, &Button, EcLocalize("fade in for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");

				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeOut, &g_Config.m_ClChatBubbleFadeOut, &Button, EcLocalize("fade out for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
			}
		},
	});

	/* Menu Settings */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_MISC,
		{"menu", "settings", "friend", "prefix", "icon", "show", "others", "in", "menu", "spec"},
		[](bool HasSearch) {
			return 100.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Menu Settings"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersInMenu, EcLocalize("Show Settings Icon When Tee's in a Menu"), &g_Config.m_ClShowOthersInMenu, &ModuleRect, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuFriendColor, EcLocalize("Friend Color in Spectate Menu"), &g_Config.m_ClSpecMenuFriendColor, &ModuleRect, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuPrefixes, EcLocalize("Player Prefixes in Spectate Menu"), &g_Config.m_ClSpecMenuPrefixes, &ModuleRect, LineSize);
			}
		},
	});

	/* Freeze Kill */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_GAMEPLAY,
		{"freeze", "kill", "protection", "not", "moving", "only", "full", "frozen", "team", "in", "view", "wait", "until", "milli"},
		[](bool HasSearch) {
			int Offset = 0;
			if(g_Config.m_ClFreezeKill || HasSearch)
			{
				Offset += 95.0f;
				if(g_Config.m_ClFreezeKillWaitMs || HasSearch)
					Offset += 25.0f;
			}

			return 75.0f + Offset;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Freeze Kill"), HeaderSize, HeaderAlignment);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKill, EcLocalize("Kill on Freeze"), &g_Config.m_ClFreezeKill, &ModuleRect, LineSize);

			if(g_Config.m_ClFreezeKill || HasSearch)
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillIgnoreKillProt, EcLocalize("Ignore Kill Protection"), &g_Config.m_ClFreezeKillIgnoreKillProt, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillNotMoving, EcLocalize("Don't Kill if Moving"), &g_Config.m_ClFreezeKillNotMoving, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillOnlyFullFrozen, EcLocalize("Only Kill if Fully Frozen"), &g_Config.m_ClFreezeKillOnlyFullFrozen, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillTeamInView, EcLocalize("Dont Kill if Teammate is in View"), &g_Config.m_ClFreezeKillTeamInView, &ModuleRect, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillWaitMs, EcLocalize("Wait Until Kill"), &g_Config.m_ClFreezeKillWaitMs, &ModuleRect, LineSize);
				if(g_Config.m_ClFreezeKillWaitMs || HasSearch)
				{
					ModuleRect.HSplitTop(2 * LineSize, &Button, &ModuleRect);
					Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillMs, &g_Config.m_ClFreezeKillMs, &Button, EcLocalize("Milliseconds to Wait For"), 1, 5000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "ms");
				}
			}
		},
	});

	/* Anti Latency Tools */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_NETWORK | FILTER_GAMEPLAY,
		{"latency", "tools", "prediction", "anti", "ping", "margin", "frozen", "freeze"},
		[](bool HasSearch) {
			int Offset = 0;
			if(g_Config.m_EcRemoveAnti || HasSearch)
				Offset += 40.0f;
			if(g_Config.m_EcPredMarginInFreeze || HasSearch)
				Offset += 20.0f;

			return 120.0f + Offset;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Anti Latency Tools"), HeaderSize, HeaderAlignment);
			{
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, EcLocalize("Prediction Margin"), 10, 75, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcRemoveAnti, EcLocalize("Remove prediction & antiping in freeze"), &g_Config.m_EcRemoveAnti, &ModuleRect, LineSize);
				if(g_Config.m_EcRemoveAnti || HasSearch)
				{
					if(g_Config.m_EcUnfreezeLagDelayTicks < g_Config.m_EcUnfreezeLagTicks)
						g_Config.m_EcUnfreezeLagDelayTicks = g_Config.m_EcUnfreezeLagTicks;
					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoSliderWithScaledValue(&g_Config.m_EcUnfreezeLagTicks, &g_Config.m_EcUnfreezeLagTicks, &Button, EcLocalize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoSliderWithScaledValue(&g_Config.m_EcUnfreezeLagDelayTicks, &g_Config.m_EcUnfreezeLagDelayTicks, &Button, EcLocalize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
				}
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcUnpredOthersInFreeze, EcLocalize("Dont predict other players if you are frozen"), &g_Config.m_EcUnpredOthersInFreeze, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcPredMarginInFreeze, EcLocalize("Adjust your prediction margin while frozen"), &g_Config.m_EcPredMarginInFreeze, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				if(g_Config.m_EcPredMarginInFreeze || HasSearch)
				{
					Ui()->DoScrollbarOption(&g_Config.m_EcPredMarginInFreezeAmount, &g_Config.m_EcPredMarginInFreezeAmount, &Button, EcLocalize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
				}
			}
		},
	});

	const bool RainbowOn = g_Config.m_ClRainbowHook || g_Config.m_ClRainbowTees || g_Config.m_ClRainbowWeapon || g_Config.m_ClRainbowOthers;
	/* Cosmetics */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_VISUAL,
		{"cosmetic", "settings", "small", "skin", "effects", "color", "others", "rainbow", "tees", "weapons", "hook", "others", "speed"},
		[&](bool HasSearch) {
			int Offset = 0;
			if(RainbowOn || HasSearch)
				Offset += 20.0f;

			return 235.0f + Offset;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Cosmetic Settings"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSmallSkins, ("Small Skins"), &g_Config.m_ClSmallSkins, &ModuleRect, LineMargin);

			static std::vector<const char *> s_EffectDropDownNames;
			s_EffectDropDownNames = {EcLocalize("No Effect"), EcLocalize("Sparkle effect"), EcLocalize("Fire Trail Effect"), EcLocalize("Switch Effect")};
			static CUi::SDropDownState s_EffectDropDownState;
			static CScrollRegion s_EffectDropDownScrollRegion;
			s_EffectDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_EffectDropDownScrollRegion;
			int EffectSelectedOld = g_Config.m_ClEffect;
			CUIRect EffectDropDownRect;
			ModuleRect.HSplitTop(LineSize, &EffectDropDownRect, &ModuleRect);
			const int EffectSelectedNew = Ui()->DoDropDown(&EffectDropDownRect, EffectSelectedOld, s_EffectDropDownNames.data(), s_EffectDropDownNames.size(), s_EffectDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_EffectDropDownState.m_SelectionPopupContext, EffectDropDownRect.x, EffectDropDownRect.y);

			if(s_ScrollRegion.RectClipped(EffectDropDownRect))
			{
				Ui()->ClosePopupMenu(&s_EffectDropDownState.m_SelectionPopupContext);
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

			ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectColors, ("Effect Color"), &g_Config.m_ClEffectColors, &ModuleRect, LineMargin);

			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClEffectColors, &ModuleRect, "Doesn't work if the sprite already has a set color\nMake the sprite the color you want if it doesn't work");
			if(g_Config.m_ClEffectColors)
			{
				static CButtonContainer s_EffectR;
				ModuleRect.HSplitTop(-3.0f, &Label, &ModuleRect);
				ModuleRect.HSplitTop(-17.0f, &Button, &ModuleRect);
				DoLine_ColorPicker(&s_EffectR, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClEffectColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClEffectColor)), true);
				ModuleRect.HSplitTop(-10.0f, &Button, &ModuleRect);
			}

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectOthers, ("Effect Others"), &g_Config.m_ClEffectOthers, &ModuleRect, LineMargin);

			ModuleRect.HSplitTop(MarginSmall, &Button, &ModuleRect);

			// ***** Rainbow ***** //
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowTees, EcLocalize("Rainbow Tees"), &g_Config.m_ClRainbowTees, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowWeapon, EcLocalize("Rainbow weapons"), &g_Config.m_ClRainbowWeapon, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowHook, EcLocalize("Rainbow hook"), &g_Config.m_ClRainbowHook, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowOthers, EcLocalize("Rainbow others"), &g_Config.m_ClRainbowOthers, &ModuleRect, LineSize);

			ModuleRect.HSplitTop(MarginExtraSmall, nullptr, &ModuleRect);
			static std::vector<const char *> s_RainbowDropDownNames;
			s_RainbowDropDownNames = {EcLocalize("Rainbow"), EcLocalize("Pulse"), EcLocalize("Black"), EcLocalize("Random")};
			static CUi::SDropDownState s_RainbowDropDownState;
			static CScrollRegion s_RainbowDropDownScrollRegion;
			s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
			int RainbowSelectedOld = g_Config.m_ClRainbowMode - 1;
			CUIRect RainbowDropDownRect;
			ModuleRect.HSplitTop(LineSize, &RainbowDropDownRect, &ModuleRect);
			const int RainbowSelectedNew = Ui()->DoDropDown(&RainbowDropDownRect, RainbowSelectedOld, s_RainbowDropDownNames.data(), s_RainbowDropDownNames.size(), s_RainbowDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_RainbowDropDownState.m_SelectionPopupContext, RainbowDropDownRect.x, RainbowDropDownRect.y);

			if(s_ScrollRegion.RectClipped(RainbowDropDownRect))
			{
				Ui()->ClosePopupMenu(&s_RainbowDropDownState.m_SelectionPopupContext);
			}

			if(RainbowSelectedOld != RainbowSelectedNew)
			{
				g_Config.m_ClRainbowMode = RainbowSelectedNew + 1;
			}
			ModuleRect.HSplitTop(MarginExtraSmall, nullptr, &ModuleRect);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			if(RainbowOn || HasSearch)
				Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Button, EcLocalize("Rainbow speed"), 0, 200, &CUi::ms_LogarithmicScrollbarScale, 0, "%");

			ModuleRect.HSplitTop(MarginExtraSmall, nullptr, &ModuleRect);
			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);
		},
	});

	/* Server-Side Rainbow */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_VISUAL,
		{"server", "rainbow", "preview", "color", "hue", "saturation", "lightness", "alpha"},
		[](bool HasSearch) {
			return 255;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Server-Side Rainbow"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClServerRainbow, EcLocalize("Enable Serverside Rainbow"), &g_Config.m_ClServerRainbow, &ModuleRect, LineSize);

			ColorHSLA Color(GameClient()->m_EClient.m_PreviewRainbowColor[g_Config.m_ClDummy]);
			const char *apLabels[] = {EcLocalize("Hue"), EcLocalize("Sat."), EcLocalize("Lht."), EcLocalize("Alpha")};
			const float SizePerEntry = 20.0f;
			const float MarginPerEntry = 5.0f;
			const float PreviewMargin = 2.5f;
			const float PreviewHeight = 40.0f + 2 * PreviewMargin;
			const float OffY = (SizePerEntry + MarginPerEntry) * 3 - PreviewHeight;

			CUIRect Preview;
			ModuleRect.VSplitLeft(PreviewHeight, &Preview, &ModuleRect);
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
				ModuleRect.HSplitTop(SizePerEntry, &Button2, &ModuleRect);
				ModuleRect.HSplitTop(MarginPerEntry, nullptr, &ModuleRect);
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

			ModuleRect.VSplitLeft(-42, &Button, &ModuleRect);
			ModuleRect.HSplitTop(2 * LineSize, &Button, &ModuleRect);
			Ui()->DoScrollbarOption(&GameClient()->m_EClient.m_RainbowSpeed, &GameClient()->m_EClient.m_RainbowSpeed, &Button, EcLocalize("Rainbow Speed"), 1, 5000, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

			ModuleRect.VSplitLeft(-93, &Button, &ModuleRect);
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
			ModuleRect.VSplitLeft(88, &Button, &ModuleRect);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], "Rainbow Body", &GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], "Rainbow Feet", &GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_BothPlayers, "Do Dummy and Main Player at the same time", &GameClient()->m_EClient.m_BothPlayers, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_ShowServerSide, "Show what it'll look like Server-side", &GameClient()->m_EClient.m_ShowServerSide, &ModuleRect, LineSize);
		},
	});

	/* Warlist */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_WARLIST,
		{"warlist", "sweat", "skin", "auto", "add", "name", "change"},
		[&](bool HasSearch) {
			float Height = 180.0f;
			if(g_Config.m_ClAutoAddOnNameChange || HasSearch)
				Height += LineSize;

			return Height;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Warlist"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, ("Use Warlist"), &g_Config.m_ClWarList, &ModuleRect, LineMargin);
			ModuleRect.HSplitTop(5, &Button, &ModuleRect);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatMode, ("Sweat Mode"), &g_Config.m_ClSweatMode, &ModuleRect, LineMargin);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeOnlyOthers, ("Don't Change Own Skin"), &g_Config.m_ClSweatModeOnlyOthers, &ModuleRect, LineMargin);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeSelfColor, ("Don't Change Own Color"), &g_Config.m_ClSweatModeSelfColor, &ModuleRect, LineMargin);

			static CLineInput s_Name;
			s_Name.SetBuffer(g_Config.m_ClSweatModeSkinName, sizeof(g_Config.m_ClSweatModeSkinName));
			s_Name.SetEmptyText("x_ninja");

			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			{
				const float Length = TextRender()->TextBoundingBox(FontSize, "Skin Name:").m_W;
				CUIRect SkinLabel, SkinInput;
				Button.VSplitLeft(0.0f, &SkinLabel, &SkinInput);
				SkinLabel.VSplitLeft(25.0f, &SkinLabel, &SkinLabel);
				SkinLabel.VSplitLeft(Length, &SkinLabel, &SkinInput);
				Ui()->DoLabel(&SkinLabel, "Skin Name:", 13.0f, TEXTALIGN_ML);
				SkinInput.VSplitLeft(150.0f, &SkinInput, nullptr);
				Ui()->DoEditBox(&s_Name, &SkinInput, EditBoxFontSize);
			}

			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			if(DoButton_CheckBox(&g_Config.m_ClAutoAddOnNameChange, EcLocalize("Auto Add to Warlist on Name Change"), g_Config.m_ClAutoAddOnNameChange, &Button))
				g_Config.m_ClAutoAddOnNameChange = g_Config.m_ClAutoAddOnNameChange ? 0 : 1;
			GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClAutoAddOnNameChange, &Button, "Automatically adds players that change their name to the warlist again");

			if(g_Config.m_ClAutoAddOnNameChange || HasSearch)
			{
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				static int s_NamePlatesStrong = 0;
				if(DoButton_CheckBox(&s_NamePlatesStrong, "Notify you everytime someone gets auto added", g_Config.m_ClAutoAddOnNameChange == 2, &Button))
					g_Config.m_ClAutoAddOnNameChange = g_Config.m_ClAutoAddOnNameChange != 2 ? 2 : 1;
			}
		},
	});

	/* Trails */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_VISUAL,
		{"trail", "settings", "color", "mode", "solid", "tee", "rainbow", "speed", "width", "length", "alpha"},
		[](bool HasSearch) {
			int Offset = 0;
			if(g_Config.m_EcTeeTrailColorMode == CTrails::COLORMODE_SOLID || HasSearch)
				Offset += ColorPickerLineSize + ColorPickerLineSpacing;

			return 205.0f + Offset;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Tee Trails"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrail, EcLocalize("Enable tee trails"), &g_Config.m_EcTeeTrail, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailOthers, EcLocalize("Show other tees' trails"), &g_Config.m_EcTeeTrailOthers, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailFade, EcLocalize("Fade trail alpha"), &g_Config.m_EcTeeTrailFade, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcTeeTrailTaper, EcLocalize("Taper trail width"), &g_Config.m_EcTeeTrailTaper, &ModuleRect, LineSize);

			ModuleRect.HSplitTop(MarginExtraSmall, nullptr, &ModuleRect);
			std::vector<const char *> vTrailDropDownNames;
			vTrailDropDownNames = {EcLocalize("Solid"), EcLocalize("Tee"), EcLocalize("Rainbow"), EcLocalize("Speed")};
			static CUi::SDropDownState s_TrailDropDownState;
			static CScrollRegion s_TrailDropDownScrollRegion;
			s_TrailDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailDropDownScrollRegion;
			int TrailSelectedOld = g_Config.m_EcTeeTrailColorMode - 1;
			CUIRect TrailDropDownRect;
			ModuleRect.HSplitTop(LineSize, &TrailDropDownRect, &ModuleRect);
			const int TrailSelectedNew = Ui()->DoDropDown(&TrailDropDownRect, TrailSelectedOld, vTrailDropDownNames.data(), vTrailDropDownNames.size(), s_TrailDropDownState);
			Ui()->UpdatePopupMenuOffset(&s_TrailDropDownState.m_SelectionPopupContext, TrailDropDownRect.x, TrailDropDownRect.y);

			if(s_ScrollRegion.RectClipped(TrailDropDownRect))
			{
				Ui()->ClosePopupMenu(&s_TrailDropDownState.m_SelectionPopupContext);
			}

			if(TrailSelectedOld != TrailSelectedNew)
			{
				g_Config.m_EcTeeTrailColorMode = TrailSelectedNew + 1;
			}
			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);

			static CButtonContainer s_TeeTrailColor;
			if(g_Config.m_EcTeeTrailColorMode == CTrails::COLORMODE_SOLID || HasSearch)
				DoLine_ColorPicker(&s_TeeTrailColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Tee trail color"), &g_Config.m_EcTeeTrailColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailWidth, &g_Config.m_EcTeeTrailWidth, &Button, EcLocalize("Trail width"), 0, 20);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailLength, &g_Config.m_EcTeeTrailLength, &Button, EcLocalize("Trail length"), 0, 200);
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			Ui()->DoScrollbarOption(&g_Config.m_EcTeeTrailAlpha, &g_Config.m_EcTeeTrailAlpha, &Button, EcLocalize("Trail alpha"), 0, 100);
		},
	});

	/* Startup Commands */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_MISC,
		{"execute", "before", "connect", "join", "console", "command", "startup", "run"},
		[](bool HasSearch) {
			return 80.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Startup Commands"), HeaderSize, HeaderAlignment);
			{
				auto RenderLabeledEditBox = [&](const char *pLabel, CLineInput *pLineInput, char *pBuffer, int BufferSize, const char *pEmptyText, float Length, float LabelFontSize = 12.5f) {
					ModuleRect.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, 0, &ModuleRect);
					Button.VSplitLeft(Length, &Label, &Button);
					Button.VSplitRight(0.0f, &Button, &MainView);

					pLineInput->SetBuffer(pBuffer, BufferSize);
					pLineInput->SetEmptyText(pEmptyText);
					Ui()->DoEditBox(pLineInput, &Button, EditBoxFontSize);

					ModuleRect.HSplitTop(3.0f, &Button, &ModuleRect);
					Ui()->DoLabel(&ModuleRect, pLabel, LabelFontSize, TEXTALIGN_LEFT);
					ModuleRect.HSplitTop(-3.0f, &Button, &ModuleRect);
				};

				{
					const char *pN = "Execute before connect";
					float Length = TextRender()->TextBoundingBox(12.5f, pN).m_W + 3.5f; // Give it some breathing room

					static CLineInput s_ReplyMsg;
					RenderLabeledEditBox(pN, &s_ReplyMsg, g_Config.m_ClExecuteOnConnect, sizeof(g_Config.m_ClExecuteOnConnect), "Any Console Command", Length);
				}
				ModuleRect.HSplitTop(25.0f, &Button, &ModuleRect);
				{
					const char *pN = "Execute on join";
					float Length = TextRender()->TextBoundingBox(12.5f, pN).m_W + 3.5f; // Give it some breathing room

					static CLineInput s_ReplyMsg;
					RenderLabeledEditBox(pN, &s_ReplyMsg, g_Config.m_ClRunOnJoinConsole, sizeof(g_Config.m_ClRunOnJoinConsole), "Any Console Command", Length);
				}
			}
		},
	});

	/* Physic Balls */
	vModules.push_back({
		ESettingsModuleColumn::LEFT,
		FILTER_VISUAL | FILTER_GAMEPLAY,
		{"physic", "balls", "new", "ball", "cursor", "amount", "ball", "skin"},
		[](bool HasSearch) {
			return 120;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, "Physic Balls", HeaderSize, HeaderAlignment);

			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Ball amount: %" PRIzu, GameClient()->m_PhysicBalls.GetBallCount());

			CUIRect BallAmountLabel, ClearButton;
			Button.VSplitRight(45.0f, &BallAmountLabel, &ClearButton);
			BallAmountLabel.VSplitRight(MarginSmall, &BallAmountLabel, nullptr);

			Ui()->DoLabel(&BallAmountLabel, aBuf, FontSize, TEXTALIGN_ML);

			static CButtonContainer s_ClearBallsButton;
			const ColorRGBA ButtonColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);

			if(Ui()->DoButton_FontIcon(&s_ClearBallsButton, FontIcon::TRASH, 0, &ClearButton, BUTTONFLAG_LEFT))
				GameClient()->m_PhysicBalls.OnReset();

			ModuleRect.HSplitTop(MarginSmall, &Button, &ModuleRect);

			{
				static CLineInput s_NotifyMsg;
				s_NotifyMsg.SetBuffer(g_Config.m_ClPhysicBallsSkin, sizeof(g_Config.m_ClPhysicBallsSkin));
				s_NotifyMsg.SetEmptyText("Volleyball");

				const char *pLabel = "Ball Skin:";
				float Length = TextRender()->TextBoundingBox(FontSize, pLabel).m_W + 3.5f; // Give it some breathing room

				ModuleRect.HSplitTop(20.0f, &Button, nullptr);

				Button.VSplitLeft(Length, &Label, &Button);
				Button.VSplitLeft(100.0f, &Button, nullptr);

				Ui()->DoEditBox(&s_NotifyMsg, &Button, EditBoxFontSize);

				ModuleRect.HSplitTop(3.0f, &Button, &ModuleRect);
				Ui()->DoLabel(&ModuleRect, pLabel, FontSize, TEXTALIGN_LEFT);
			}
			ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
			ModuleRect.HSplitTop(25.0f, &Button, &ModuleRect);
			CUIRect SpawnButton, SpawnButtonCursor;
			Button.VSplitLeft(110.0f, &SpawnButton, &Button);
			Button.VSplitLeft(MarginSmall, nullptr, &Button);
			Button.VSplitLeft(110.0f, &SpawnButtonCursor, nullptr);

			static CButtonContainer s_SpawnBall, s_OtherBallButton;

			if(DoButtonForceFontSize_Menu(&s_SpawnBall, EcLocalize("New Ball"), 0, &SpawnButton, 12.0f, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			{
				GameClient()->m_PhysicBalls.NewBallPlayer(60.0f);
			}

			if(DoButtonForceFontSize_Menu(&s_OtherBallButton, EcLocalize("New Ball Cursor"), 0, &SpawnButtonCursor, 12.0f, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			{
				GameClient()->m_PhysicBalls.NewBallCursor(60.0f);
			}
		},
	});

	/* Tee Appearance */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_VISUAL,
		{"frozen", "freeze", "stars", "katana", "colored", "tee", "skins", "white", "feet"},
		[](bool HasSearch) {
			int Size = 120;
			if(g_Config.m_ClWhiteFeet || HasSearch)
				Size += LineSize + MarginExtraSmall * 2.0f;

			return Size;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Tee Appearance"), HeaderSize, HeaderAlignment);

			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeStars, EcLocalize("Freeze stars"), &g_Config.m_ClFreezeStars, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_EcFrozenKatana, EcLocalize("Show katana on frozen players"), &g_Config.m_EcFrozenKatana, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClColorFrozenTeeBody, EcLocalize("Colored frozen tee skins"), &g_Config.m_ClColorFrozenTeeBody, &ModuleRect, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWhiteFeet, EcLocalize("Render feet as white feet"), &g_Config.m_ClWhiteFeet, &ModuleRect, LineSize);
			CUIRect FeetBox;
			if(g_Config.m_ClWhiteFeet || HasSearch)
			{
				ModuleRect.HSplitTop(LineSize + MarginExtraSmall, &FeetBox, &ModuleRect);
				FeetBox.HSplitTop(MarginExtraSmall, nullptr, &FeetBox);
				FeetBox.VSplitMid(&FeetBox, nullptr);
				static CLineInput s_WhiteFeet(g_Config.m_ClWhiteFeetSkin, sizeof(g_Config.m_ClWhiteFeetSkin));
				s_WhiteFeet.SetEmptyText("x_ninja");
				Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize);
			}
		},
	});

	/* Map Overview */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_HUD | FILTER_VISUAL,
		{"map", "overview", "options", "opacity", "explore", "dark"},
		[](bool HasSearch) {
			return 100.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Map Overview"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMapOverview, "Enable map overview", &g_Config.m_ClMapOverview, &ModuleRect, LineSize);
				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClMapOverview, &Button, "Renders areas darker that you have already explored", FontSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMapOverviewSpectatingOnly, "Only show map overview while spectating", &g_Config.m_ClMapOverviewSpectatingOnly, &ModuleRect, LineSize);
				ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
				Ui()->DoScrollbarOption(&g_Config.m_ClMapOverviewOpacity, &g_Config.m_ClMapOverviewOpacity, &Button, "Explored area opacity", 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
			}
		},
	});

#if defined(CONF_DISCORD)
	/* Discord RPC */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_MISC,
		{"discord", "rpc", "rich", "presence", "offline", "online", "message"},
		[](bool HasSearch) {
			return 125.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Discord RPC"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordRPC, "Use Discord Rich Presence", &g_Config.m_ClDiscordRPC, &ModuleRect, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordMapStatus, "Show what Map you're on", &g_Config.m_ClDiscordMapStatus, &ModuleRect, LineSize);

				std::array<float, 2> Sizes = {
					TextRender()->TextBoundingBox(FontSize, "Online Message:").m_W,
					TextRender()->TextBoundingBox(FontSize, "Offline Message:").m_W};
				float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 3.5f;

				auto &&RenderEditBox = [&](CLineInput *pLineInput, const char *pLabelText, char *pBuffer, size_t BufferSize, const char *pEmptyText) {
					CUIRect Row, LabelRect, EditRect;
					ModuleRect.HSplitTop(19.9f, &Row, &ModuleRect);

					LabelRect = Row;
					LabelRect.HSplitTop(2.5f, nullptr, &LabelRect);
					Ui()->DoLabel(&LabelRect, pLabelText, FontSize, TEXTALIGN_TL);

					EditRect = Row;
					EditRect.VSplitLeft(Length, nullptr, &EditRect);

					pLineInput->SetBuffer(pBuffer, BufferSize);
					pLineInput->SetEmptyText(pEmptyText);
					if(Ui()->DoEditBox(pLineInput, &EditRect, EditBoxFontSize))
						Client()->DiscordRPCUpdate();
				};

				static CLineInput s_OnlineInput;
				RenderEditBox(&s_OnlineInput, "Online Message:", g_Config.m_ClDiscordOnlineStatus, sizeof(g_Config.m_ClDiscordOnlineStatus), "Online");

				ModuleRect.HSplitTop(2.0f, &Button, &ModuleRect);

				static CLineInput s_OfflineInput;
				RenderEditBox(&s_OfflineInput, "Offline Message:", g_Config.m_ClDiscordOfflineStatus, sizeof(g_Config.m_ClDiscordOfflineStatus), "Offline");
			}
		},
	});
#endif

	/* Tile Outlines */
	const float ScrollBarOffset = 8.5f;
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_VISUAL | FILTER_GAMEPLAY,
		{"tile", "outline", "freeze", "deep", "solid", "tele", "kill", "width"},
		[&](bool HasSearch) {
			int Size = 80.0f;
			if(g_Config.m_ClOutline || HasSearch)
			{
				Size = 230.0f;
				if(g_Config.m_ClOutlineFreeze || HasSearch)
					Size += 25.0f - ScrollBarOffset;
				if(g_Config.m_ClOutlineUnfreeze || HasSearch)
					Size += 25.0f - ScrollBarOffset;
				if(g_Config.m_ClOutlineSolid || HasSearch)
					Size += 25.0f - ScrollBarOffset;
				if(g_Config.m_ClOutlineTele || HasSearch)
					Size += 25.0f - ScrollBarOffset;
				if(g_Config.m_ClOutlineKill || HasSearch)
					Size += 25.0f - ScrollBarOffset;
			}

			return Size;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
			Ui()->DoLabel(&Button, EcLocalize("Tile Outlines"), HeaderSize, HeaderAlignment);
			{
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutline, EcLocalize("Show any enabled outlines"), &g_Config.m_ClOutline, &ModuleRect, LineSize);

				if(g_Config.m_ClOutline || HasSearch)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineEntities, EcLocalize("Only show outlines in entities"), &g_Config.m_ClOutlineEntities, &ModuleRect, LineSize);

					static CButtonContainer s_OutlineColorFreezeId, s_OutlineColorSolidId, s_OutlineColorTeleId, s_OutlineColorUnfreezeId, s_OutlineColorKillId;

					ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineFreeze, EcLocalize("Outline freezes & deeps"), &g_Config.m_ClOutlineFreeze, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);
					DoLine_ColorPicker(&s_OutlineColorFreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false, nullptr, true);
					if(g_Config.m_ClOutlineFreeze || HasSearch)
					{
						ModuleRect.HSplitTop(-ScrollBarOffset, &Button, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthFreeze, &g_Config.m_ClOutlineWidthFreeze, &Button, EcLocalize("Freeze width"), 1, 16);
						ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineUnfreeze, EcLocalize("Outline unfreezes & undeeps"), &g_Config.m_ClOutlineUnfreeze, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);
					DoLine_ColorPicker(&s_OutlineColorUnfreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClOutlineColorUnfreeze, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorUnfreeze)), false, nullptr, true);
					if(g_Config.m_ClOutlineUnfreeze || HasSearch)
					{
						ModuleRect.HSplitTop(-ScrollBarOffset, &Button, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthUnfreeze, &g_Config.m_ClOutlineWidthUnfreeze, &Button, EcLocalize("Unfreeze width"), 1, 16);
						ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineSolid, EcLocalize("Outline solids"), &g_Config.m_ClOutlineSolid, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);
					DoLine_ColorPicker(&s_OutlineColorSolidId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClOutlineColorSolid, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorSolid)), false, nullptr, true);
					if(g_Config.m_ClOutlineSolid || HasSearch)
					{
						ModuleRect.HSplitTop(-ScrollBarOffset, &Button, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthSolid, &g_Config.m_ClOutlineWidthSolid, &Button, EcLocalize("Solid width"), 1, 16);
						ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineTele, EcLocalize("Outline teleporters"), &g_Config.m_ClOutlineTele, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);
					DoLine_ColorPicker(&s_OutlineColorTeleId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClOutlineColorTele, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorTele)), false, nullptr, true);
					if(g_Config.m_ClOutlineTele || HasSearch)
					{
						ModuleRect.HSplitTop(-ScrollBarOffset, &Button, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthTele, &g_Config.m_ClOutlineWidthTele, &Button, EcLocalize("Tele width"), 1, 16);
						ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineKill, EcLocalize("Outline kills"), &g_Config.m_ClOutlineKill, &ModuleRect, LineSize);
					ModuleRect.HSplitTop(-20.0f, &Button, &ModuleRect);
					DoLine_ColorPicker(&s_OutlineColorKillId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize(""), &g_Config.m_ClOutlineColorKill, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::ClOutlineColorKill)), false, nullptr, true);
					if(g_Config.m_ClOutlineKill || HasSearch)
					{
						ModuleRect.HSplitTop(-ScrollBarOffset, &Button, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidthKill, &g_Config.m_ClOutlineWidthKill, &Button, EcLocalize("Kill width"), 1, 16);
						ModuleRect.HSplitTop(5.0f, &Button, &ModuleRect);
					}
				}
			}
		},
	});

	/* Background Draw */
	vModules.push_back({
		ESettingsModuleColumn::RIGHT,
		FILTER_VISUAL,
		{"back", "ground", "draw", "time", "until", "stroke", "disappear", "mouse"},
		[&](bool HasSearch) {
			return 180.0f;
		},
		[&](CUIRect ModuleRect, bool HasSearch) {
			ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
			ModuleRect.VMargin(Margin, &ModuleRect);

			ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);

			Ui()->DoLabel(&Button, EcLocalize("Background Draw"), HeaderSize, HeaderAlignment);
			ModuleRect.HSplitTop(MarginSmall, nullptr, &ModuleRect);

			static CButtonContainer s_BgDrawColor;
			DoLine_ColorPicker(&s_BgDrawColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ModuleRect, EcLocalize("Color"), &g_Config.m_EcBgDrawColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(DefaultConfig::EcBgDrawColor)), false);

			ModuleRect.HSplitTop(LineSize * 2.0f, &Button, &ModuleRect);
			if(g_Config.m_EcBgDrawFadeTime == 0)
				Ui()->DoScrollbarOption(&g_Config.m_EcBgDrawFadeTime, &g_Config.m_EcBgDrawFadeTime, &Button, EcLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, EcLocalize(" seconds (never)"));
			else
				Ui()->DoScrollbarOption(&g_Config.m_EcBgDrawFadeTime, &g_Config.m_EcBgDrawFadeTime, &Button, EcLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, EcLocalize(" seconds"));

			ModuleRect.HSplitTop(LineSize * 2.0f, &Button, &ModuleRect);
			Ui()->DoScrollbarOption(&g_Config.m_EcBgDrawWidth, &g_Config.m_EcBgDrawWidth, &Button, EcLocalize("Width"), 1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

			static CButtonContainer s_ReaderButtonDraw, s_ClearButtonDraw;
			DoLine_KeyReader(ModuleRect, s_ReaderButtonDraw, s_ClearButtonDraw, EcLocalize("Draw where mouse is"), "+bg_draw");
		},
	});

	/* Frozen Tee Hud */
	{
		static bool s_Open = false;
		vModules.push_back({
			ESettingsModuleColumn::RIGHT,
			FILTER_HUD | FILTER_WARLIST,
			{"frozen", "freeze", "display", "team", "ninja", "tee", "row", "size", "alive"},
			[&](bool HasSearch) { // Doesnt show all on purpose, its ugly
				int Size = 160.0f;
				if(g_Config.m_ClShowFrozenText || HasSearch)
					Size += 20.0f;
				if(g_Config.m_ClWarList && g_Config.m_ClShowFrozenHud)
					Size += 64.0f;
				if(s_Open)
				{
					const size_t WarTypes = GameClient()->m_WarList.m_WarTypes.size();
					Size += WarTypes * LineSize;
				}

				return Size;
			},
			[&](CUIRect ModuleRect, bool HasSearch) {
				const size_t WarTypes = GameClient()->m_WarList.m_WarTypes.size();
				ModuleRect.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				ModuleRect.VMargin(Margin, &ModuleRect);

				static CButtonContainer Id;
				RenderHudEditorButton(&Id, &ModuleRect);

				ModuleRect.HSplitTop(HeaderHeight, &Button, &ModuleRect);
				Ui()->DoLabel(&Button, EcLocalize("Frozen Tee Display"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHud, EcLocalize("Show frozen tee display"), &g_Config.m_ClShowFrozenHud, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHudSkins, EcLocalize("Use skins instead of ninja tees"), &g_Config.m_ClShowFrozenHudSkins, &ModuleRect, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFrozenHudTeamOnly, EcLocalize("Only show after joining a team"), &g_Config.m_ClFrozenHudTeamOnly, &ModuleRect, LineSize);

					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoScrollbarOption(&g_Config.m_ClFrozenMaxRows, &g_Config.m_ClFrozenMaxRows, &Button, EcLocalize("Max Rows"), 1, 6);
					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					Ui()->DoScrollbarOption(&g_Config.m_ClFrozenHudTeeSize, &g_Config.m_ClFrozenHudTeeSize, &Button, EcLocalize("Tee Size"), 8, 27);

					ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
					if(DoButton_CheckBox(&g_Config.m_ClShowFrozenText, EcLocalize("Tees left alive text"), g_Config.m_ClShowFrozenText >= 1, &Button))
						g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText >= 1 ? 0 : 1;
					if(g_Config.m_ClShowFrozenText || HasSearch)
					{
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);
						static int s_CountFrozenText = 0;
						if(DoButton_CheckBox(&s_CountFrozenText, EcLocalize("Count frozen tees"), g_Config.m_ClShowFrozenText == 2, &Button))
							g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText != 2 ? 2 : 1;
					}

					// This would be fancy asf as a popup ngl, like generally speaking any of these weird extra
					// settings for other features could* (should) be inside popups cause they dont fucking fit
					// Also, fuck UI coding
					if(g_Config.m_ClWarList && g_Config.m_ClShowFrozenHud)
					{
						ModuleRect.HSplitTop(10.0f, nullptr, &ModuleRect);
						ModuleRect.HSplitTop(FontSize, &Button, &ModuleRect);
						Ui()->DoLabel(&Button, EcLocalize("Only show certain Wartypes in frozen tee display"), FontSize, TEXTALIGN_TL);
						ModuleRect.HSplitTop(FontSize, &Button, &ModuleRect);
						Ui()->DoLabel(&Button, EcLocalize("Select none to show all"), FontSize, TEXTALIGN_TL);
						ModuleRect.HSplitTop(5.0f, nullptr, &ModuleRect);
						ModuleRect.HSplitTop(LineSize, &Button, &ModuleRect);

						static CButtonContainer s_OpenedEntries;
						if(Ui()->DoButton_FontIcon(&s_OpenedEntries, s_Open ? FontIcon::CHEVRON_DOWN : FontIcon::CHEVRON_RIGHT, 0, &Button, BUTTONFLAG_LEFT))
							s_Open = !s_Open;

						CUIRect Button2;
						ModuleRect.HSplitTop(0, &Button2, &ModuleRect);

						if(s_Open)
						{
							static std::vector<int> s_vFrozenWarTypeIds;
							if(s_vFrozenWarTypeIds.size() < WarTypes)
								s_vFrozenWarTypeIds.resize(WarTypes);

							ModuleRect.HSplitTop(LineSize, &Button2, &ModuleRect);
							if(DoButton_CheckBox(s_vFrozenWarTypeIds.data(), "Show players without entry", IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, 0), &Button2))
								SetFlag(g_Config.m_ClWarlistFrozenTeeFlags, 0, !IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, 0));

							for(size_t Type = 1; Type < WarTypes; Type++)
							{
								char aBuf[64];
								str_format(aBuf, sizeof(aBuf), "%s '%s'", "Show", GameClient()->m_WarList.m_WarTypes.at(Type)->m_aWarName);
								const bool FlagSet = IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, Type);

								ModuleRect.HSplitTop(LineSize, &Button2, &ModuleRect);
								if(DoButton_CheckBox(&s_vFrozenWarTypeIds[Type], aBuf, FlagSet, &Button2))
									SetFlag(g_Config.m_ClWarlistFrozenTeeFlags, Type, !FlagSet);
							}
						}
					}
				}
			},
		});
	}
	static CLineInputBuffered<32> s_SettingsSearchInput;
	RenderSettingsModuleSearchBar(s_ScrollRegion, MainView, vModules, s_SettingsSearchInput);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	const char *pSearch = s_SettingsSearchInput.GetString();

	if(HasMatchingSettingsModules(vModules, pSearch))
	{
		RenderSettingsModuleColumns(s_ScrollRegion, MainView, vModules, pSearch);
	}
	else
	{
		CUIRect NoResultsRect;
		MainView.HSplitTop(LineSize, &NoResultsRect, &MainView);
		if(s_ScrollRegion.AddRect(NoResultsRect))
			Ui()->DoLabel(&NoResultsRect, "No settings match your search", FontSize, TEXTALIGN_MC);
	}

	s_ScrollRegion.End();
}

// EClient: the button that opens the HUD editor, drawn filling whatever rect it is handed. It
// appears in a couple of the modules that are mostly about HUD elements, and once in the search bar
// where it is reachable from any settings page regardless of which modules are on screen.
void CMenus::DoHudEditorButton(CButtonContainer *pId, const CUIRect *pRect)
{
	CUIRect Button = *pRect;

	const IClient::EClientState State = Client()->State();
	const bool Ingame = State == IClient::STATE_ONLINE || State == IClient::STATE_DEMOPLAYBACK;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);

	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextSelectionColor());
	if(Ui()->HotItem() == pId && Ingame)
		TextRender()->TextColor(TextRender()->DefaultTextColor());

	ColorRGBA Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	if(Ingame)
		Color.a *= Ui()->ButtonColorMul(pId);

	Button.Draw(Color, IGraphics::CORNER_ALL, 5.0f);

	// Sized off the rect rather than a constant, so it fits whichever of the two places it is in
	Ui()->DoLabel(&Button, FontIcon::BRUSH, Button.h * CUi::ms_FontmodHeight * 0.7f, TEXTALIGN_MC);

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	GameClient()->m_Tooltips.DoToolTip(pId, &Button, Ingame ? "Open the Hud Editor" : "Join a server to open the Hud Editor");

	if(Ui()->DoButtonLogic(pId, 0, &Button, BUTTONFLAG_LEFT) && Ingame)
		GameClient()->m_HudEditor.ToggleHudEditor();
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

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_ALL);
}

vec2 CMenus::TeeEyeDirection(vec2 Pos)
{
	vec2 DeltaPosition = Ui()->MousePos() - Pos;
	vec2 TeeDirection = normalize(DeltaPosition);
	return TeeDirection;
}

void CMenus::RenderDraggableTee(CUIRect MainView, vec2 StartPos, vec2 &CurPos, bool &Dragging, vec2 TeeDirection, CTeeRenderInfo *pInfo)
{
	bool CanDrag = false;

	bool Hovering = length(Ui()->MousePos() - CurPos) < pInfo->m_Size / 2.4f;
	bool JustStartedDragging = Ui()->MouseButtonClicked(0) && Hovering;

	if(Hovering)
		Ui()->SetHotItem(nullptr);

	if(Dragging && !Ui()->MouseButton(0))
		Dragging = false;

	if(JustStartedDragging || Dragging)
	{
		CanDrag = true;
		Dragging = true;
	}

	if(CanDrag)
	{
		vec2 Offset = vec2(0.0f, 2.5f);
		CurPos = Ui()->MousePos() - Offset;

		float MenuTop = MainView.y + 25.0f;
		float MenuBottom = MainView.Size().y + 35.0f;

		float MenuLeft = MainView.x + 15.0f;
		float MenuRight = MainView.Size().x + 10.0f;

		if(Ui()->MousePos().y < MenuTop)
			CurPos.y = MenuTop - Offset.y;
		if(Ui()->MousePos().y > MenuBottom)
			CurPos.y = MenuBottom - Offset.y;

		if(Ui()->MousePos().x < MenuLeft)
			CurPos.x = MenuLeft;
		if(Ui()->MousePos().x > MenuRight)
			CurPos.x = MenuRight;
	}
	else if(Hovering && Ui()->MouseButtonClicked(1))
		CurPos = StartPos;

	RenderTee(CurPos, TeeEyeDirection(CurPos), CAnimState::GetIdle(), pInfo, EMOTE_NORMAL, true);
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
