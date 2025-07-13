
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <game/client/components/binds.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/menus.h>
#include <game/client/components/skins.h>

#include "e_enums.h"
#include <base/color.h>
#include <string>
#include <vector>

using namespace FontIcons;
using namespace std::chrono_literals;

enum
{
	ENTITY_TAB_SETTINGS = 0,
	ENTITY_TAB_VISUAL,
	ENTITY_TAB_WARLIST,
	ENTITY_TAB_BINDWHEEL,
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

const float FontSize = 14.0f;
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
const float HeaderAlignment = TEXTALIGN_MC;
const ColorRGBA BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);

void SetFlag(int32_t &Flags, int n, bool Value)
{
	if(Value)
		Flags |= (1 << n);
	else
		Flags &= ~(1 << n);
}
bool IsFlagSet(int32_t Flags, int n)
{
	return (Flags & (1 << n)) != 0;
}

void CMenus::RenderSettingsEntity(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	static int s_CurTab = 0;

	CUIRect TabBar, Button, Label;

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

	MainView.HSplitTop(LineSize * 1.1f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / TabCount;
	static CButtonContainer s_aPageTabs[NUMBER_OF_ENTITY_TABS] = {};
	const char *apTabNames[NUMBER_OF_ENTITY_TABS] = {
		Localize("E-Client Settings"),
		Localize("Visual Settings"),
		Localize("Warlist"),
		Localize("Bindwheel"),
	};

	for(int Tab = 0; Tab < NUMBER_OF_ENTITY_TABS; ++Tab)
	{
		int LeftTab = 0;
		int RightTab = NUMBER_OF_ENTITY_TABS - 1;

		if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, Tab))
			continue;

		if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_SETTINGS))
		{
			LeftTab = ENTITY_TAB_VISUAL;
			if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL))
			{
				LeftTab = ENTITY_TAB_WARLIST;
				if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST))
				{
					LeftTab = ENTITY_TAB_BINDWHEEL;
				}
			}
		}

		if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_BINDWHEEL))
		{
			RightTab = ENTITY_TAB_WARLIST;
			if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST))
			{
				RightTab = ENTITY_TAB_VISUAL;
				if(IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL))
				{
					RightTab = ENTITY_TAB_SETTINGS;
				}
			}
		}

		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);

		int Corners = Tab == LeftTab ? IGraphics::CORNER_L : Tab == RightTab ? IGraphics::CORNER_R :
										       IGraphics::CORNER_NONE;
		if(LeftTab == RightTab)
			Corners = IGraphics::CORNER_ALL;

		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
			ResetTeePos = true;
		}
	}

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	// Client Page 1

	if(s_CurTab == ENTITY_TAB_SETTINGS)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		// left side in settings menu

		CUIRect Automation, FreezeKill, ChatSettings, ChatBubbles, PlayerIndicator, GoresMode,
			MenuSettings, TileOutline, AntiLatency, FrozenTeeHud, EntitySettings,
			FastInput, AntiPingSmoothing, GhostTools;
		MainView.VSplitMid(&Automation, &GoresMode);

		{
			static float Offset = 0.0f;

			Automation.VMargin(5.0f, &Automation);
			Automation.HSplitTop(225.0f + Offset, &Automation, &ChatSettings);
			if(s_ScrollRegion.AddRect(Automation))
			{
				Offset = 0.0f;

				Automation.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				Automation.VMargin(Margin, &Automation);

				Automation.HSplitTop(HeaderHeight, &Button, &Automation);
				Ui()->DoLabel(&Button, Localize("Automation"), HeaderSize, HeaderAlignment);
				{
					// group em up
					{
						std::array<float, 2> Sizes = {
							TextRender()->TextBoundingBox(FontSize, "Tabbed reply").m_W,
							TextRender()->TextBoundingBox(FontSize, "Muted Reply").m_W,
						};
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
							{
								g_Config.m_ClTabbedOutMsg ^= 1;
							}

							if(g_Config.m_ClTabbedOutMsg)
								Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
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
							{
								g_Config.m_ClReplyMuted ^= 1;
							}
							if(g_Config.m_ClReplyMuted)
								Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
						}
					}
					Automation.HSplitTop(25.0f, &Button, &Automation);
					{
						{
							const char *Name = g_Config.m_ClNotifyOnJoin ? "Notify on Join Name" : "Notify on Join";
							float Length = TextRender()->TextBoundingBox(FontSize, "Notify on Join Name").m_W + 27.5f; // Give it some breathing room

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
								Ui()->DoEditBox(&s_NotifyName, &Button, 14.0f);
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

							Ui()->DoEditBox(&s_NotifyMsg, &Button, 14.0f);

							Automation.HSplitTop(3.0f, &Button, &Automation);
							Ui()->DoLabel(&Automation, "Notify Message", 12.5f, TEXTALIGN_LEFT);
							Automation.HSplitTop(-3.0f, &Button, &Automation);
							Offset += 20.0f;
						}
					}
					Automation.HSplitTop(25.0f, &Button, &Automation);
					{
						float Length = TextRender()->TextBoundingBox(FontSize, "Run on Join Console").m_W + 20.0f; // Give it some breathing room

						Automation.HSplitTop(20.0f, &Button, &MainView);

						Button.VSplitLeft(0.0f, 0, &Automation);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClRunOnJoinMsg, sizeof(g_Config.m_ClRunOnJoinMsg));
						s_ReplyMsg.SetEmptyText("Any Console Command");

						if(DoButton_CheckBox(&g_Config.m_ClRunOnJoinConsole, "Run on Join Console", g_Config.m_ClRunOnJoinConsole, &Automation))
							g_Config.m_ClRunOnJoinConsole ^= 1;
						Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
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

							Ui()->DoEditBox(&s_LastMessage, &Button, FontSize);

							Automation.HSplitTop(20.0f, &Button, &Automation);
							Ui()->DoLabel(&Automation, "Text to Show", 12.5f, TEXTALIGN_ML);
							Automation.HSplitTop(-20.0f, &Button, &Automation);

							static CButtonContainer s_LastColor;
							Automation.HSplitTop(-3.0f, &Button, &Automation);
							DoLine_ColorPicker(&s_LastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Automation, Localize(""), &g_Config.m_ClNotifyWhenLastColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(29057)), true);

							Automation.HSplitTop(-25.0f, &Button, &Automation);
							Offset += 20.0f;
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
						Offset += 20.0f;
					}
					Automation.HSplitTop(2.5f, &Button, &Automation);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChangeTileNotification, "Notify When Player is Being Moved", &g_Config.m_ClChangeTileNotification, &Automation, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiSpawnBlock, "Anti Mult Spawn Block", &g_Config.m_ClAntiSpawnBlock, &Automation, LineSize);
					GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClAntiSpawnBlock, &Button, "Puts you into a random Team when you Kill and get frozen");
				}
			}
		}
		// chat settings
		{
			ChatSettings.HSplitTop(Margin, nullptr, &ChatSettings);
			ChatSettings.HSplitTop(395.0f, &ChatSettings, &ChatBubbles);
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
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
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
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
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
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
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
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
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
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}
					ChatSettings.HSplitTop(55.0f, &Button, &ChatSettings);
					Ui()->DoLabel(&Button, Localize("Chat Preview"), FontSize + 3, TEXTALIGN_ML);
					ChatSettings.HSplitTop(-15.0f, &Button, &ChatSettings);

					RenderChatPreview(ChatSettings);
				}
			}
		}
		{
			ChatBubbles.HSplitTop(Margin, nullptr, &ChatBubbles);
			ChatBubbles.HSplitTop(145.0f, &ChatBubbles, &PlayerIndicator);
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
					DoFloatScrollBar(&g_Config.m_ClChatBubbleShowTime, &g_Config.m_ClChatBubbleShowTime, &Button, Localize("Show the Bubbles for"), 200, 1000, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
					ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
					DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeIn, &g_Config.m_ClChatBubbleFadeIn, &Button, Localize("fade in for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
					ChatBubbles.HSplitTop(LineSize, &Button, &ChatBubbles);
					DoFloatScrollBar(&g_Config.m_ClChatBubbleFadeOut, &g_Config.m_ClChatBubbleFadeOut, &Button, Localize("fade out for"), 15, 100, 100, &CUi::ms_LinearScrollbarScale, 0, "s");
				}
			}
		}
		{
			static float Offset = 0.0f;
			PlayerIndicator.HSplitTop(Margin, nullptr, &PlayerIndicator);
			PlayerIndicator.HSplitTop(270.0f + Offset, &PlayerIndicator, &GhostTools);
			if(s_ScrollRegion.AddRect(PlayerIndicator))
			{
				Offset = 0.0f;
				PlayerIndicator.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				PlayerIndicator.VMargin(Margin, &PlayerIndicator);

				PlayerIndicator.HSplitTop(HeaderHeight, &Button, &PlayerIndicator);
				Ui()->DoLabel(&Button, Localize("Player Indicator"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, Localize("Show any enabled Indicators"), &g_Config.m_ClPlayerIndicator, &PlayerIndicator, LineSize);
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
						Offset += 40.0f;
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
						Offset += 80.0f;
					}
					if(!g_Config.m_ClWarListIndicatorColors || !g_Config.m_ClWarListIndicator)
					{
						static CButtonContainer s_IndicatorAliveColorId, s_IndicatorDeadColorId, s_IndicatorSavedColorId;
						DoLine_ColorPicker(&s_IndicatorAliveColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator alive color"), &g_Config.m_ClIndicatorAlive, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_IndicatorDeadColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator in freeze color"), &g_Config.m_ClIndicatorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_IndicatorSavedColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicator, Localize("Indicator safe color"), &g_Config.m_ClIndicatorSaved, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						Offset += 60.0f;
					}
				}
			}
		}

		{
			GhostTools.HSplitTop(Margin, nullptr, &GhostTools);
			GhostTools.HSplitTop(180.0f, &GhostTools, 0);
			if(s_ScrollRegion.AddRect(GhostTools))
			{
				GhostTools.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				GhostTools.VMargin(Margin, &GhostTools);

				GhostTools.HSplitTop(HeaderHeight, &Button, &GhostTools);
				Ui()->DoLabel(&Button, Localize("Ghost Tools"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersGhosts, Localize("Show unpredicted ghosts for other players"), &g_Config.m_ClShowOthersGhosts, &GhostTools, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSwapGhosts, Localize("Swap ghosts and normal players"), &g_Config.m_ClSwapGhosts, &GhostTools, LineSize);
					GhostTools.HSplitTop(LineSize, &Button, &GhostTools);
					Ui()->DoScrollbarOption(&g_Config.m_ClPredGhostsAlpha, &g_Config.m_ClPredGhostsAlpha, &Button, Localize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
					GhostTools.HSplitTop(LineSize, &Button, &GhostTools);
					Ui()->DoScrollbarOption(&g_Config.m_ClUnpredGhostsAlpha, &g_Config.m_ClUnpredGhostsAlpha, &Button, Localize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideFrozenGhosts, Localize("Hide ghosts of frozen players"), &g_Config.m_ClHideFrozenGhosts, &GhostTools, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderGhostAsCircle, Localize("Render ghosts as circles"), &g_Config.m_ClRenderGhostAsCircle, &GhostTools, LineSize);

					{
						static CKeyInfo s_Key = CKeyInfo{Localize("Toggle ghosts key"), "toggle tc_show_others_ghosts 0 1", 0, 0};
						s_Key.m_ModifierCombination = s_Key.m_KeyId = 0;
						for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
						{
							for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
							{
								const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
								if(!pBind[0])
									continue;

								if(str_comp(pBind, s_Key.m_pCommand) == 0)
								{
									s_Key.m_KeyId = KeyId;
									s_Key.m_ModifierCombination = Mod;
									break;
								}
							}
						}

						CUIRect KeyButton, KeyLabel;
						GhostTools.HSplitTop(LineSize, &KeyButton, &GhostTools);
						KeyButton.VSplitMid(&KeyLabel, &KeyButton);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s:", Localize(s_Key.m_pName));
						Ui()->DoLabel(&KeyLabel, aBuf, 12.0f, TEXTALIGN_ML);
						int OldId = s_Key.m_KeyId, OldModifierCombination = s_Key.m_ModifierCombination, NewModifierCombination;
						int NewId = DoKeyReader(&s_Key, &KeyButton, OldId, OldModifierCombination, &NewModifierCombination);
						if(NewId != OldId || NewModifierCombination != OldModifierCombination)
						{
							if(OldId != 0 || NewId == 0)
								GameClient()->m_Binds.Bind(OldId, "", false, OldModifierCombination);
							if(NewId != 0)
								GameClient()->m_Binds.Bind(NewId, s_Key.m_pCommand, false, NewModifierCombination);
						}
						GhostTools.HSplitTop(MarginExtraSmall, nullptr, &GhostTools);
					}
				}
			}
		}
		// right side
		{
			GoresMode.VMargin(5.0f, &GoresMode);
			GoresMode.HSplitTop(120.0f, &GoresMode, &MenuSettings);
			if(s_ScrollRegion.AddRect(GoresMode))
			{
				GoresMode.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				GoresMode.VMargin(Margin, &GoresMode);

				GoresMode.HSplitTop(HeaderHeight, &Button, &GoresMode);
				Ui()->DoLabel(&Button, Localize("Gores Mode"), HeaderSize, HeaderAlignment);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresMode, ("\"advanced\" Gores Mode"), &g_Config.m_ClGoresMode, &GoresMode, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresModeDisableIfWeapons, ("Disable if You Have Any Weapon"), &g_Config.m_ClGoresModeDisableIfWeapons, &GoresMode, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoEnableGoresMode, ("Auto Enable if Gametype is \"Gores\""), &g_Config.m_ClAutoEnableGoresMode, &GoresMode, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDisableGoresOnShutdown, ("Disable on Shutdown"), &g_Config.m_ClDisableGoresOnShutdown, &GoresMode, LineSize);
			}
		}

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
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersInMenu, Localize("Show Settigns Icon When Tee's in a Menu"), &g_Config.m_ClShowOthersInMenu, &MenuSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuFriendColor, Localize("Friend Color in Spectate Menu"), &g_Config.m_ClSpecMenuFriendColor, &MenuSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuPrefixes, Localize("Player Prefixes in Spectate Menu"), &g_Config.m_ClSpecMenuPrefixes, &MenuSettings, LineSize);
				}
			}
		}

		{
			static float Offset = 0.0f;
			FreezeKill.HSplitTop(Margin, nullptr, &FreezeKill);
			FreezeKill.HSplitTop(75.0f + Offset, &FreezeKill, &EntitySettings);
			if(s_ScrollRegion.AddRect(FreezeKill))
			{
				Offset = 0.0f;

				FreezeKill.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				FreezeKill.VMargin(Margin, &FreezeKill);

				FreezeKill.HSplitTop(HeaderHeight, &Button, &FreezeKill);
				Ui()->DoLabel(&Button, Localize("Freeze Kill"), HeaderSize, HeaderAlignment);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKill, Localize("Kill on Freeze"), &g_Config.m_ClFreezeKill, &FreezeKill, LineSize);

				if(g_Config.m_ClFreezeKill)
				{
					Offset += 105.0f;

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillMultOnly, Localize("Only Enable on Multeasymap"), &g_Config.m_ClFreezeKillMultOnly, &FreezeKill, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillIgnoreKillProt, Localize("Ignore Kill Protection"), &g_Config.m_ClFreezeKillIgnoreKillProt, &FreezeKill, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeDontKillMoving, Localize("Don't Kill if Moving"), &g_Config.m_ClFreezeDontKillMoving, &FreezeKill, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillOnlyFullFrozen, Localize("Only Kill if Fully Frozen"), &g_Config.m_ClFreezeKillOnlyFullFrozen, &FreezeKill, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillTeamClose, Localize("Dont Kill if Teammate is Close"), &g_Config.m_ClFreezeKillTeamClose, &FreezeKill, LineSize);

					if(g_Config.m_ClFreezeKillTeamClose)
					{
						FreezeKill.HSplitTop(20.0f, &Button, &FreezeKill);
						Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillTeamDistance, &g_Config.m_ClFreezeKillTeamDistance, &Button, Localize("Team Max Distance"), 1, 25, &CUi::ms_LinearScrollbarScale, 0u, "");
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillWaitMs, Localize("Wait Until Kill"), &g_Config.m_ClFreezeKillWaitMs, &FreezeKill, LineSize);
					if(g_Config.m_ClFreezeKillWaitMs)
					{
						Offset += 35.0f;
						FreezeKill.HSplitTop(2 * LineSize, &Button, &FreezeKill);
						Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillMs, &g_Config.m_ClFreezeKillMs, &Button, Localize("Milliseconds to Wait For"), 1, 5000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "ms");
					}
				}
			}
		}

		{
			EntitySettings.HSplitTop(Margin, nullptr, &EntitySettings);
			EntitySettings.HSplitTop(90.0f, &EntitySettings, &FrozenTeeHud);
			if(s_ScrollRegion.AddRect(EntitySettings))
			{
				EntitySettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				EntitySettings.VMargin(Margin, &EntitySettings);

				EntitySettings.HSplitTop(HeaderHeight, &Button, &EntitySettings);
				Ui()->DoLabel(&Button, Localize("Join Info"), HeaderSize, HeaderAlignment);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEnabledInfo, ("Display What Features Are On/Off on Join"), &g_Config.m_ClEnabledInfo, &EntitySettings, LineMargin);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClListsInfo, ("Display amount of players of default lists on Join"), &g_Config.m_ClListsInfo, &EntitySettings, LineMargin);
			}
		}

		{
			FrozenTeeHud.HSplitTop(Margin, nullptr, &FrozenTeeHud);
			FrozenTeeHud.HSplitTop(g_Config.m_ClShowFrozenText ? 120.0f : 100.0f, &FrozenTeeHud, &TileOutline);
			if(s_ScrollRegion.AddRect(FrozenTeeHud))
			{
				FrozenTeeHud.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				FrozenTeeHud.VMargin(Margin, &FrozenTeeHud);

				FrozenTeeHud.HSplitTop(HeaderHeight, &Button, &FrozenTeeHud);
				Ui()->DoLabel(&Button, Localize("Frozen Tee Display"), HeaderSize, HeaderAlignment);
				{
					FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
					Ui()->DoScrollbarOption(&g_Config.m_ClFrozenMaxRows, &g_Config.m_ClFrozenMaxRows, &Button, Localize("Max Rows"), 1, 6);
					FrozenTeeHud.HSplitTop(LineSize, &Button, &FrozenTeeHud);
					Ui()->DoScrollbarOption(&g_Config.m_ClFrozenHudTeeSize, &g_Config.m_ClFrozenHudTeeSize, &Button, Localize("Tee Size"), 8, 27);

					{
						CUIRect CheckBoxRect, CheckBoxRect2;
						FrozenTeeHud.HSplitTop(LineSize, &CheckBoxRect, &FrozenTeeHud);
						FrozenTeeHud.HSplitTop(LineSize, &CheckBoxRect2, &FrozenTeeHud);
						if(DoButton_CheckBox(&g_Config.m_ClShowFrozenText, Localize("Tees left alive text"), g_Config.m_ClShowFrozenText >= 1, &CheckBoxRect))
							g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText >= 1 ? 0 : 1;

						if(g_Config.m_ClShowFrozenText)
						{
							static int s_CountFrozenText = 0;
							if(DoButton_CheckBox(&s_CountFrozenText, Localize("Count frozen tees"), g_Config.m_ClShowFrozenText == 2, &CheckBoxRect2))
								g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText != 2 ? 2 : 1;
						}
					}
				}
			}
		}

		{
			TileOutline.HSplitTop(Margin, nullptr, &TileOutline);
			TileOutline.HSplitTop(g_Config.m_ClOutline ? 295.0f : 80.0f, &TileOutline, &AntiLatency);
			if(s_ScrollRegion.AddRect(TileOutline))
			{
				TileOutline.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				TileOutline.VMargin(Margin, &TileOutline);

				TileOutline.HSplitTop(HeaderHeight, &Button, &TileOutline);
				Ui()->DoLabel(&Button, Localize("Tile Outlines"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutline, Localize("Show any enabled outlines"), &g_Config.m_ClOutline, &TileOutline, LineSize);

					if(g_Config.m_ClOutline)
					{
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineEntities, Localize("Only show outlines in entities"), &g_Config.m_ClOutlineEntities, &TileOutline, LineSize);

						TileOutline.HSplitTop(LineSize, &Button, &TileOutline);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineWidth, &g_Config.m_ClOutlineWidth, &Button, Localize("Outline width"), 1, 16);
						TileOutline.HSplitTop(LineSize, &Button, &TileOutline);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineAlpha, &g_Config.m_ClOutlineAlpha, &Button, Localize("Outline alpha"), 0, 100);
						TileOutline.HSplitTop(LineSize, &Button, &TileOutline);
						Ui()->DoScrollbarOption(&g_Config.m_ClOutlineAlphaSolid, &g_Config.m_ClOutlineAlphaSolid, &Button, Localize("Outline Alpha (walls)"), 0, 100);

						TileOutline.HSplitTop(10.0f, &Button, &TileOutline);

						static CButtonContainer s_OutlineColorFreezeId, s_OutlineColorSolidId, s_OutlineColorTeleId, s_OutlineColorUnfreezeId, s_OutlineColorKillId;
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineFreeze, Localize("Outline Freeze & Deep"), &g_Config.m_ClOutlineFreeze, &TileOutline, LineMargin);
						TileOutline.HSplitTop(10.0f, &Button, &TileOutline);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineSolid, Localize("Outline walls"), &g_Config.m_ClOutlineSolid, &TileOutline, LineSize);
						TileOutline.HSplitTop(10.0f, &Button, &TileOutline);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineTele, Localize("Outline teleporter"), &g_Config.m_ClOutlineTele, &TileOutline, LineSize);
						TileOutline.HSplitTop(10.0f, &Button, &TileOutline);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineUnFreeze, Localize("Outline unfreeze & undeep"), &g_Config.m_ClOutlineUnFreeze, &TileOutline, LineSize);
						TileOutline.HSplitTop(10.0f, &Button, &TileOutline);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineKill, Localize("Outline kill"), &g_Config.m_ClOutlineKill, &TileOutline, LineSize);

						TileOutline.HSplitTop(-140.0f, &Button, &TileOutline);

						DoLine_ColorPicker(&s_OutlineColorFreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutline, Localize(""), &g_Config.m_ClOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_OutlineColorSolidId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutline, Localize(""), &g_Config.m_ClOutlineColorSolid, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_OutlineColorTeleId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutline, Localize(""), &g_Config.m_ClOutlineColorTele, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_OutlineColorUnfreezeId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutline, Localize(""), &g_Config.m_ClOutlineColorUnfreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&s_OutlineColorKillId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &TileOutline, Localize(""), &g_Config.m_ClOutlineColorKill, ColorRGBA(0.0f, 0.0f, 0.0f), false);
					}
				}
			}
		}

		{
			static float Offset = 0.0f;
			AntiLatency.HSplitTop(Margin, nullptr, &AntiLatency);
			AntiLatency.HSplitTop(120.0f + Offset, &AntiLatency, &AntiPingSmoothing);
			if(s_ScrollRegion.AddRect(AntiLatency))
			{
				Offset = 0.0f;
				AntiLatency.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				AntiLatency.VMargin(Margin, &AntiLatency);

				AntiLatency.HSplitTop(HeaderHeight, &Button, &AntiLatency);
				Ui()->DoLabel(&Button, Localize("Anti Latency Tools"), HeaderSize, HeaderAlignment);
				{
					AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
					Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, Localize("Prediction Margin"), 10, 75, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRemoveAnti, Localize("Remove prediction & antiping in freeze"), &g_Config.m_ClRemoveAnti, &AntiLatency, LineSize);
					if(g_Config.m_ClRemoveAnti)
					{
						if(g_Config.m_ClUnfreezeLagDelayTicks < g_Config.m_ClUnfreezeLagTicks)
							g_Config.m_ClUnfreezeLagDelayTicks = g_Config.m_ClUnfreezeLagTicks;
						AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
						DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagTicks, &g_Config.m_ClUnfreezeLagTicks, &Button, Localize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
						AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
						DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagDelayTicks, &g_Config.m_ClUnfreezeLagDelayTicks, &Button, Localize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
						Offset += 40.0f;
					}
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnpredOthersInFreeze, Localize("Dont predict other players if you are frozen"), &g_Config.m_ClUnpredOthersInFreeze, &AntiLatency, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPredMarginInFreeze, Localize("Adjust your prediction margin while frozen"), &g_Config.m_ClPredMarginInFreeze, &AntiLatency, LineSize);
					AntiLatency.HSplitTop(LineSize, &Button, &AntiLatency);
					if(g_Config.m_ClPredMarginInFreeze)
					{
						Ui()->DoScrollbarOption(&g_Config.m_ClPredMarginInFreezeAmount, &g_Config.m_ClPredMarginInFreezeAmount, &Button, Localize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
						Offset += 20.0f;
					}
				}
			}
		}

		{
			AntiPingSmoothing.HSplitTop(Margin, nullptr, &AntiPingSmoothing);
			AntiPingSmoothing.HSplitTop(120.0f, &AntiPingSmoothing, &FastInput);
			if(s_ScrollRegion.AddRect(AntiPingSmoothing))
			{
				AntiPingSmoothing.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				AntiPingSmoothing.VMargin(Margin, &AntiPingSmoothing);

				AntiPingSmoothing.HSplitTop(HeaderHeight, &Button, &AntiPingSmoothing);
				Ui()->DoLabel(&Button, Localize("Anti Ping Smoothing"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingImproved, Localize("Use new smoothing algorithm"), &g_Config.m_ClAntiPingImproved, &AntiPingSmoothing, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingStableDirection, Localize("Optimistic prediction along stable direction"), &g_Config.m_ClAntiPingStableDirection, &AntiPingSmoothing, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAntiPingNegativeBuffer, Localize("Negative stability buffer (for Gores)"), &g_Config.m_ClAntiPingNegativeBuffer, &AntiPingSmoothing, LineSize);
					AntiPingSmoothing.HSplitTop(LineSize, &Button, &AntiPingSmoothing);
					Ui()->DoScrollbarOption(&g_Config.m_ClAntiPingUncertaintyScale, &g_Config.m_ClAntiPingUncertaintyScale, &Button, Localize("Uncertainty duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
				}
			}
		}

		{
			FastInput.HSplitTop(Margin, nullptr, &FastInput);
			FastInput.HSplitTop(g_Config.m_ClFastInput ? 100.0f : 80.0f, &FastInput, 0);
			if(s_ScrollRegion.AddRect(FastInput))
			{
				FastInput.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				FastInput.VMargin(Margin, &FastInput);

				FastInput.HSplitTop(HeaderHeight, &Button, &FastInput);
				Ui()->DoLabel(&Button, Localize("Input"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInput, Localize("Fast Inputs (-20ms visual delay)"), &g_Config.m_ClFastInput, &FastInput, LineSize);

					FastInput.HSplitTop(MarginSmall, nullptr, &FastInput);
					if(g_Config.m_ClFastInput)
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInputOthers, Localize("Extra tick other tees (increases other tees latency, \nmakes dragging slightly easier when using fast input)"), &g_Config.m_ClFastInputOthers, &FastInput, LineSize);
				}
			}
		}
		s_ScrollRegion.End();
	}

	if(s_CurTab == ENTITY_TAB_VISUAL)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		// left side in settings menu
		CUIRect MiscSettings, PlayerSettings, WarVisual, RainbowSettings, DiscordSettings;
		MainView.VSplitMid(&PlayerSettings, &MiscSettings);

		{
			bool RainbowOn = g_Config.m_ClRainbowHook || g_Config.m_ClRainbowTees || g_Config.m_ClRainbowWeapon || g_Config.m_ClRainbowOthers;
			static float Offset = 0.0f;

			PlayerSettings.VMargin(5.0f, &PlayerSettings);
			PlayerSettings.HSplitTop(245.0f + Offset, &PlayerSettings, &RainbowSettings);
			if(s_ScrollRegion.AddRect(PlayerSettings))
			{
				Offset = 0.0f;
				PlayerSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				PlayerSettings.VMargin(Margin, &PlayerSettings);

				PlayerSettings.HSplitTop(HeaderHeight, &Button, &PlayerSettings);
				Ui()->DoLabel(&Button, Localize("Cosmetic Settings"), HeaderSize, HeaderAlignment);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSmallSkins, ("Small Skins"), &g_Config.m_ClSmallSkins, &PlayerSettings, LineMargin);

				static std::vector<const char *> s_EffectDropDownNames;
				s_EffectDropDownNames = {Localize("Off"), Localize("Sparkle effect"), Localize("Fire Trail"), Localize("Switch Effect")};
				static CUi::SDropDownState s_EffectDropDownState;
				static CScrollRegion s_EffectDropDownScrollRegion;
				s_EffectDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_EffectDropDownScrollRegion;
				int EffectSelectedOld = g_Config.m_ClEffect;
				CUIRect EffectDropDownRect;
				PlayerSettings.HSplitTop(LineSize, &EffectDropDownRect, &PlayerSettings);
				const int EffectSelectedNew = Ui()->DoDropDown(&EffectDropDownRect, EffectSelectedOld, s_EffectDropDownNames.data(), s_EffectDropDownNames.size(), s_EffectDropDownState);
				if(EffectSelectedOld != EffectSelectedNew)
				{
					g_Config.m_ClEffect = EffectSelectedNew;
					EffectSelectedOld = EffectSelectedNew;
					dbg_msg("E-Client", "Effect changed to %d", g_Config.m_ClEffect);

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

				PlayerSettings.HSplitTop(5.0f, &Button, &PlayerSettings);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectColors, ("Effect Color"), &g_Config.m_ClEffectColors, &PlayerSettings, LineMargin);

				GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClEffectColors, &PlayerSettings, "Doesn't work if the sprite already has a set color\nMake the sprite the color you want if it doesn't work");
				if(g_Config.m_ClEffectColors)
				{
					static CButtonContainer s_EffectR;
					PlayerSettings.HSplitTop(-3.0f, &Label, &PlayerSettings);
					PlayerSettings.HSplitTop(-17.0f, &Button, &PlayerSettings);
					DoLine_ColorPicker(&s_EffectR, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerSettings, Localize(""), &g_Config.m_ClEffectColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(29057)), true);
					PlayerSettings.HSplitTop(-10.0f, &Button, &PlayerSettings);
				}

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEffectOthers, ("Effect Others"), &g_Config.m_ClEffectOthers, &PlayerSettings, LineMargin);

				PlayerSettings.HSplitTop(MarginSmall, &Button, &PlayerSettings);

				// ***** Rainbow ***** //

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowTees, Localize("Rainbow Tees"), &g_Config.m_ClRainbowTees, &PlayerSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowWeapon, Localize("Rainbow weapons"), &g_Config.m_ClRainbowWeapon, &PlayerSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowHook, Localize("Rainbow hook"), &g_Config.m_ClRainbowHook, &PlayerSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRainbowOthers, Localize("Rainbow others"), &g_Config.m_ClRainbowOthers, &PlayerSettings, LineSize);

				PlayerSettings.HSplitTop(MarginExtraSmall, nullptr, &PlayerSettings);
				static std::vector<const char *> s_RainbowDropDownNames;
				s_RainbowDropDownNames = {Localize("Rainbow"), Localize("Pulse"), Localize("Black"), Localize("Random")};
				static CUi::SDropDownState s_RainbowDropDownState;
				static CScrollRegion s_RainbowDropDownScrollRegion;
				s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
				int RainbowSelectedOld = g_Config.m_ClRainbowMode - 1;
				CUIRect RainbowDropDownRect;
				PlayerSettings.HSplitTop(LineSize, &RainbowDropDownRect, &PlayerSettings);
				const int RainbowSelectedNew = Ui()->DoDropDown(&RainbowDropDownRect, RainbowSelectedOld, s_RainbowDropDownNames.data(), s_RainbowDropDownNames.size(), s_RainbowDropDownState);
				if(RainbowSelectedOld != RainbowSelectedNew)
				{
					g_Config.m_ClRainbowMode = RainbowSelectedNew + 1;
					RainbowSelectedOld = RainbowSelectedNew;
					dbg_msg("rainbow", "rainbow mode changed to %d", g_Config.m_ClRainbowMode);
				}
				PlayerSettings.HSplitTop(MarginExtraSmall, nullptr, &PlayerSettings);
				PlayerSettings.HSplitTop(LineSize, &Button, &PlayerSettings);
				if(RainbowOn)
				{
					Offset += 20.0f;
					Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Button, Localize("Rainbow speed"), 0, 200, &CUi::ms_LogarithmicScrollbarScale, 0, "%");
				}
				PlayerSettings.HSplitTop(MarginExtraSmall, nullptr, &PlayerSettings);
				PlayerSettings.HSplitTop(MarginSmall, nullptr, &PlayerSettings);
			}
		}

		{
			CUIRect TeeRect;
			RainbowSettings.HSplitTop(Margin, nullptr, &RainbowSettings);
			RainbowSettings.HSplitTop(Margin, nullptr, &TeeRect);
			RainbowSettings.HSplitTop(260.0f, &RainbowSettings, 0);
			if(s_ScrollRegion.AddRect(RainbowSettings))
			{
				RainbowSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				RainbowSettings.VMargin(Margin, &RainbowSettings);

				RainbowSettings.HSplitTop(HeaderHeight, &Button, &RainbowSettings);
				Ui()->DoLabel(&Button, Localize("Server-Side Rainbow"), HeaderSize, HeaderAlignment);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClServerRainbow, Localize("Enable Serverside Rainbow"), &g_Config.m_ClServerRainbow, &RainbowSettings, LineSize);

				RainbowSettings.HSplitTop(2 * LineSize, &Button, &RainbowSettings);
				Ui()->DoScrollbarOption(&GameClient()->m_EClient.m_RainbowSpeed, &GameClient()->m_EClient.m_RainbowSpeed, &Button, Localize("Rainbow Speed"), 1, 5000, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
				RainbowSettings.VSplitLeft(52, &Button, &RainbowSettings);
				RenderHslaScrollbars(&RainbowSettings, &GameClient()->m_EClient.m_PreviewRainbowColor[g_Config.m_ClDummy], false, ColorHSLA::DARKEST_LGT, false);
				RainbowSettings.VSplitLeft(-140, &Button, &RainbowSettings);

				RainbowSettings.HSplitTop(-54, &Button, &RainbowSettings);
				RainbowSettings.HSplitTop(28, &Button, &RainbowSettings);
				Ui()->DoScrollbarOptionRender(&GameClient()->m_EClient.m_RainbowSat[g_Config.m_ClDummy], &GameClient()->m_EClient.m_RainbowSat[g_Config.m_ClDummy], &Button, Localize(""), 0, 254, &CUi::ms_LinearScrollbarScale);
				RainbowSettings.HSplitTop(-3, &Button, &RainbowSettings);
				RainbowSettings.HSplitTop(28, &Button, &RainbowSettings);
				Ui()->DoScrollbarOptionRender(&GameClient()->m_EClient.m_RainbowLht[g_Config.m_ClDummy], &GameClient()->m_EClient.m_RainbowLht[g_Config.m_ClDummy], &Button, Localize(""), 0, 254, &CUi::ms_LinearScrollbarScale);
				{
					TeeRect.HSplitTop(80.0f, nullptr, &TeeRect);
					TeeRect.HSplitTop(80.0f, &TeeRect, nullptr);
					TeeRect.VSplitLeft(80.0f, &TeeRect, nullptr);

					CTeeRenderInfo TeeRenderInfo;

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
					RenderACTee(MainView, TeeRect.Center(), CAnimState::GetIdle(), &TeeRenderInfo);
				}
				RainbowSettings.VSplitLeft(88, &Button, &RainbowSettings);
				DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], "Rainbow Body", &GameClient()->m_EClient.m_RainbowBody[g_Config.m_ClDummy], &RainbowSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], "Rainbow Feet", &GameClient()->m_EClient.m_RainbowFeet[g_Config.m_ClDummy], &RainbowSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_BothPlayers, "Do Dummy and Main Player at the same time", &GameClient()->m_EClient.m_BothPlayers, &RainbowSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&GameClient()->m_EClient.m_ShowServerSide, "Show what it'll look like Server-side", &GameClient()->m_EClient.m_ShowServerSide, &RainbowSettings, LineSize);
			}
		}

		// right side in settings menu
		{
			static float Offset = 0.0f;
			MiscSettings.VMargin(5.0f, &MiscSettings);
			MiscSettings.HSplitTop(105.0f + Offset, &MiscSettings, &DiscordSettings);
			if(s_ScrollRegion.AddRect(MiscSettings))
			{
				Offset = 0.0f;
				MiscSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				MiscSettings.VMargin(Margin, &MiscSettings);

				MiscSettings.HSplitTop(HeaderHeight, &Button, &MiscSettings);
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
						MiscSettings.HSplitTop(LineSize, &FontDropDownRect, &MiscSettings);

						float Length = TextRender()->TextBoundingBox(FontSize, "Custom Font:").m_W + 3.5f;

						FontDropDownRect.VSplitLeft(Length, &Label, &FontDropDownRect);
						FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
						FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

						Ui()->DoLabel(&Label, Localize("Custom Font:"), FontSize, TEXTALIGN_ML);
						const int FontSelectedNew = Ui()->DoDropDown(&FontDropDownRect, FontSelectedOld, s_FontDropDownNames.data(), s_FontDropDownNames.size(), s_FontDropDownState);
						if(FontSelectedOld != FontSelectedNew)
						{
							str_copy(g_Config.m_ClCustomFont, s_FontDropDownNames[FontSelectedNew]);
							FontSelectedOld = FontSelectedNew;
							TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

							// Reload *hopefully* all Fonts
							TextRender()->OnPreWindowResize();
							GameClient()->OnWindowResize();
							GameClient()->Editor()->OnWindowResize();
							GameClient()->m_MapImages.SetTextureScale(101);
							GameClient()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
						}
						static CButtonContainer s_FontDirectoryId;
						if(Ui()->DoButton_FontIcon(&s_FontDirectoryId, FONT_ICON_FOLDER, 0, &FontDirectory, 0))
						{
							Storage()->CreateFolder("data/entity", IStorage::TYPE_ABSOLUTE);
							Storage()->CreateFolder("data/entity/fonts", IStorage::TYPE_ABSOLUTE);
							Client()->ViewFile("data/entity/fonts");
						}
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, ("Show Ping Circles Next To Names"), &g_Config.m_ClPingNameCircle, &MiscSettings, LineSize);

					MiscSettings.HSplitTop(5.0f, &Button, &MiscSettings);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderCursorSpec, ("Show Cursor While Spectating"), &g_Config.m_ClRenderCursorSpec, &MiscSettings, LineSize);
					if(g_Config.m_ClRenderCursorSpec)
					{
						Offset += 20.0f;
						MiscSettings.HSplitTop(20.f, &Button, &MiscSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClRenderCursorSpecOpacity, &g_Config.m_ClRenderCursorSpecOpacity, &Button, Localize("Cursor Opacity"), 1, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
					}
				}
			}
		}

		{
			DiscordSettings.HSplitTop(Margin, nullptr, &DiscordSettings);
			DiscordSettings.HSplitTop(125.0f, &DiscordSettings, &WarVisual);
			if(s_ScrollRegion.AddRect(DiscordSettings))
			{
				DiscordSettings.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				DiscordSettings.VMargin(Margin, &DiscordSettings);

				DiscordSettings.HSplitTop(HeaderHeight, &Button, &DiscordSettings);
				Ui()->DoLabel(&Button, Localize("Discord RPC"), HeaderSize, HeaderAlignment);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordRPC, "Use Discord Rich Presence", &g_Config.m_ClDiscordRPC, &DiscordSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDiscordMapStatus, "Show What Map you're on", &g_Config.m_ClDiscordMapStatus, &DiscordSettings, LineSize);
					static int DiscordRPC = g_Config.m_ClDiscordRPC;
					static int DiscordRPCMap = g_Config.m_ClDiscordMapStatus;
					static char DiscordRPCOnlineMsg[25];
					static char DiscordRPCOfflineMsg[25];
					static bool DiscordRpcSet = false;
					if(!DiscordRpcSet)
					{
						str_copy(DiscordRPCOnlineMsg, g_Config.m_ClDiscordOnlineStatus);
						str_copy(DiscordRPCOfflineMsg, g_Config.m_ClDiscordOfflineStatus);
						DiscordRpcSet = true;
					}
					if(DiscordRPC != g_Config.m_ClDiscordRPC)
					{
						m_RPC_Ratelimit = time_get() + time_freq() * 1.5f;
						DiscordRPC = g_Config.m_ClDiscordRPC;
					}

					if(g_Config.m_ClDiscordRPC)
					{
						if(DiscordRPCMap != g_Config.m_ClDiscordMapStatus)
						{
							// Ratelimit this so it doesn't get changed instantly every time you edit this
							DiscordRPCMap = g_Config.m_ClDiscordMapStatus;
							m_RPC_Ratelimit = time_get() + time_freq() * 1.5f;
						}
						else if(str_comp(DiscordRPCOnlineMsg, g_Config.m_ClDiscordOnlineStatus) != 0)
						{
							str_copy(DiscordRPCOnlineMsg, g_Config.m_ClDiscordOnlineStatus);
							m_RPC_Ratelimit = time_get() + time_freq() * 2.5f;
						}
						else if(str_comp(DiscordRPCOfflineMsg, g_Config.m_ClDiscordOfflineStatus) != 0)
						{
							str_copy(DiscordRPCOfflineMsg, g_Config.m_ClDiscordOfflineStatus);
							m_RPC_Ratelimit = time_get() + time_freq() * 2.5f;
						}
					}

					std::array<float, 2> Sizes = {
						TextRender()->TextBoundingBox(FontSize, "Online Message:").m_W,
						TextRender()->TextBoundingBox(FontSize, "Offline Message:").m_W};
					float Length = *std::max_element(Sizes.begin(), Sizes.end()) + 3.5f;

					{
						DiscordSettings.HSplitTop(19.9f, &Button, &MainView);

						DiscordSettings.HSplitTop(2.5f, &Label, &Label);
						Ui()->DoLabel(&Label, "Online Message:", FontSize, TEXTALIGN_TL);

						Button.VSplitLeft(0.0f, 0, &DiscordSettings);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClDiscordOnlineStatus, sizeof(g_Config.m_ClDiscordOnlineStatus));
						s_PrefixMsg.SetEmptyText("Online");
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}

					DiscordSettings.HSplitTop(21.0f, &Button, &DiscordSettings);
					{
						DiscordSettings.HSplitTop(19.9f, &Button, &MainView);

						DiscordSettings.HSplitTop(2.5f, &Label, &Label);
						Ui()->DoLabel(&Label, "Offline Message:", FontSize, TEXTALIGN_TL);

						Button.VSplitLeft(0.0f, 0, &DiscordSettings);
						Button.VSplitLeft(Length, &Label, &Button);
						Button.VSplitRight(0.0f, &Button, &MainView);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClDiscordOfflineStatus, sizeof(g_Config.m_ClDiscordOfflineStatus));
						s_PrefixMsg.SetEmptyText("Offline");
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}
				}
			}
		}

		{
			WarVisual.HSplitTop(Margin, nullptr, &WarVisual);
			WarVisual.HSplitTop(130.0f, &WarVisual, 0);
			if(s_ScrollRegion.AddRect(WarVisual))
			{
				WarVisual.Draw(BackgroundColor, IGraphics::CORNER_ALL, CornerRoundness);
				WarVisual.VMargin(Margin, &WarVisual);

				WarVisual.HSplitTop(HeaderHeight, &Button, &WarVisual);
				Ui()->DoLabel(&Button, Localize("Warlist Sweat Mode"), HeaderSize, HeaderAlignment);

				WarVisual.HSplitTop(5, &Button, &WarVisual);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatMode, ("Sweat Mode"), &g_Config.m_ClSweatMode, &WarVisual, LineMargin);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeOnlyOthers, ("Don't Change Own Skin"), &g_Config.m_ClSweatModeOnlyOthers, &WarVisual, LineMargin);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeSelfColor, ("Don't Change Own Color"), &g_Config.m_ClSweatModeSelfColor, &WarVisual, LineMargin);

				static CLineInput s_Name;
				s_Name.SetBuffer(g_Config.m_ClSweatModeSkinName, sizeof(g_Config.m_ClSweatModeSkinName));
				s_Name.SetEmptyText("x_ninja");

				WarVisual.HSplitTop(2.4f, &Label, &WarVisual);
				WarVisual.VSplitLeft(25.0f, &WarVisual, &WarVisual);
				Ui()->DoLabel(&WarVisual, "Skin Name:", 13.0f, TEXTALIGN_LEFT);

				WarVisual.HSplitTop(-1, &Button, &WarVisual);
				WarVisual.HSplitTop(18.9f, &Button, &WarVisual);

				float Length = TextRender()->TextBoundingBox(FontSize, "Skin Name").m_W + 3.5f;

				Button.VSplitLeft(0.0f, 0, &WarVisual);
				Button.VSplitLeft(Length, &Label, &Button);
				Button.VSplitLeft(150.0f, &Button, 0);

				Ui()->DoEditBox(&s_Name, &Button, 14.0f);
			}
		}
		s_ScrollRegion.End();
	}

	if(s_CurTab == ENTITY_TAB_BINDWHEEL)
	{
		RenderSettingsBindwheel(MainView);
	}

	if(s_CurTab == ENTITY_TAB_WARLIST)
	{
		RenderSettingsWarList(MainView);
	}
}

void CMenus::RenderEClientVersionPage(CUIRect MainView)
{
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.VSplitLeft(15.0f, nullptr, &MainView);
	MainView.VSplitRight(15.0f, &MainView, nullptr);
	MainView.HSplitBottom(10.0f, &MainView, nullptr);

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
		if(Ui()->DoButton_FontIcon(&s_LinkButton, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, 0))
			Client()->ViewLink("https://github.com/qxdFox");
	}

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, "Hide Settings Tabs", LineSize, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize, &LeftView, &LeftView);

	static int s_ShowSettings = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_SETTINGS);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowSettings, Localize("Settings"), &s_ShowSettings, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_SETTINGS, s_ShowSettings);

	static int s_ShowBindWheel = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowBindWheel, Localize("Visual"), &s_ShowBindWheel, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_VISUAL, s_ShowBindWheel);

	static int s_ShowBindChat = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowBindChat, Localize("Warlist"), &s_ShowBindChat, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_WARLIST, s_ShowBindChat);

	static int s_ShowStatusBar = IsFlagSet(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_BINDWHEEL);
	DoButton_CheckBoxAutoVMarginAndSet(&s_ShowStatusBar, Localize("BindWheel"), &s_ShowStatusBar, &LeftView, LineSize);
	SetFlag(g_Config.m_ClEClientSettingsTabs, ENTITY_TAB_BINDWHEEL, s_ShowStatusBar);

	// Make this Saveable and somewhere hidden in roaming
	char DeathCounter[32];
	if(GameClient()->m_EClient.m_KillCount > 1 || GameClient()->m_EClient.m_KillCount == 0)
		str_format(DeathCounter, sizeof(DeathCounter), "%d deaths (all time)", GameClient()->m_EClient.m_KillCount);
	else
		str_format(DeathCounter, sizeof(DeathCounter), "%d death (all time)", GameClient()->m_EClient.m_KillCount);
	LeftView.HSplitTop(LineSize, &LeftView, &LeftView);
	Ui()->DoLabel(&LeftView, DeathCounter, FontSize, TEXTALIGN_ML);

	CUIRect LeftBottom;
	MainView.HSplitBottom(Margin, 0, &LeftBottom);
	LeftBottom.HSplitBottom(Margin, &LeftView, &LeftBottom);
	LeftBottom.HSplitBottom(LineSize * 2.0f, 0, &LeftBottom);
	LeftBottom.VSplitLeft(5.0f, &LeftView, &LeftBottom);
	LeftBottom.VSplitLeft(LineSize * 6.0f, &LeftBottom, &Button);
	static CButtonContainer s_NewestRelGithub;
	ColorRGBA Color = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
	if(str_comp(GameClient()->m_AcUpdate.m_aVersionStr, "0") != 0)
		Color = ColorRGBA(0.2f, 0.7f, 0.5, 0.25f);

	if(DoButtonLineSize_Menu(&s_NewestRelGithub, Localize("Newest Release"), 0, &LeftBottom, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, Color))
	{
		open_link("https://github.com/qxdFox/Entity-Client-DDNet/releases");
	}

	// Right Side

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, Localize("Config Files"), HeadlineFontSize, TEXTALIGN_MR);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	char aBuf[128 + IO_MAX_PATH_LENGTH];

	CUIRect FilesLeft, FilesRight;

	RightView.HSplitTop(LineSize * 2.0f, &Button, &RightView);
	Button.VSplitMid(&FilesLeft, &FilesRight, MarginSmall);

	static CButtonContainer s_AClientConfig, s_Config, s_Warlist, s_Profiles, s_Chatbinds, s_FontFolder;
	if(DoButtonLineSize_Menu(&s_AClientConfig, Localize("E-Client Setting File"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, ECONFIG_FILE, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Config, Localize("Default Settings File"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, CONFIG_FILE, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Profiles, Localize("Profiles File"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, PROFILES_FILE, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);

	if(DoButtonLineSize_Menu(&s_Warlist, Localize("Warlist File"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, WARLIST_FILE, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(Margin, &FilesRight, &FilesRight);
	FilesRight.HSplitTop(LineSize * 2.0f, &FilesRight, 0);
	if(DoButtonLineSize_Menu(&s_Chatbinds, Localize("Chatbinds File"), 0, &FilesRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, BINDCHAT_FILE, aBuf, sizeof(aBuf));
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
		TeeRenderInfo.ApplyColors(true, 5374207, 12767844);

		RenderACTee(MainView, TeeRect.Center(), CAnimState::GetIdle(), &TeeRenderInfo, 2);
	}
}

void CMenus::RenderChatPreview(CUIRect MainView)
{
	CChat &Chat = GameClient()->m_Chat;

	ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
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

	struct SPreviewLine
	{
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

	static std::vector<SPreviewLine> s_vLines;

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
		SPreviewLine *pLine;
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
		TextRender()->SetCursor(&LocalCursor, x, y, RealFontSize, Render ? TEXTFLAG_RENDER : 0);
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
		if(Line.m_Friend && g_Config.m_ClDoFriendColors)
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
		SetPreviewLine(PREVIEW_HELPER, 3, "Helper", "Ima Help this random :>", FLAG_HELPER, 0);
		SetPreviewLine(PREVIEW_TEAMMATE, 10, "Teammate", "Help me There's too many!", FLAG_TEAMMATE, 0);
		SetPreviewLine(PREVIEW_SPEC, 11, "Random Spectator", "Crazy Gameplay dude", FLAG_SPEC, 0);
	}

	SetLineSkin(1, GameClient()->m_Skins.Find("pinky"));
	SetLineSkin(2, GameClient()->m_Skins.Find("default_flower"));
	SetLineSkin(3, GameClient()->m_Skins.Find("cammostripes"));
	SetLineSkin(4, GameClient()->m_Skins.Find("beast"));
	SetLineSkin(5, GameClient()->m_Skins.Find("default"));
	SetLineSkin(6, GameClient()->m_Skins.Find("Robot"));
	SetLineSkin(7, GameClient()->m_Skins.Find("Catnoa"));
	SetLineSkin(8, GameClient()->m_Skins.Find("turtle"));

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
	if(GameClient()->m_Bindwheel.m_vBinds.empty()) // E-Client -> Fixes a Crash
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

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Name:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText("Name");
	Ui()->DoEditBox(&s_NameInput, &Button, 12.0f);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, Localize("Command:"), 14.0f, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(Localize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, 12.0f);

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

	// Do Settings Key
	CKeyInfo Key = CKeyInfo{"Bind Wheel Key", "+bindwheel", 0, 0};
	for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, Key.m_pCommand) == 0)
			{
				Key.m_KeyId = KeyId;
				Key.m_ModifierCombination = Mod;
				break;
			}
		}
	}

	CUIRect KeyLabel;
	LeftView.HSplitBottom(LineSize, &LeftView, &Button);
	Button.VSplitLeft(120.0f, &KeyLabel, &Button);
	Button.VSplitLeft(100.0f, &Button, nullptr);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize((const char *)Key.m_pName));

	Ui()->DoLabel(&KeyLabel, aBuf, 14.0f, TEXTALIGN_ML);
	int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
	int NewId = DoKeyReader((void *)&Key.m_pName, &Button, OldId, OldModifierCombination, &NewModifierCombination);
	if(NewId != OldId || NewModifierCombination != OldModifierCombination)
	{
		if(OldId != 0 || NewId == 0)
			GameClient()->m_Binds.Bind(OldId, "", false, OldModifierCombination);
		if(NewId != 0)
			GameClient()->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
	}
	LeftView.HSplitBottom(LineSize, &LeftView, &Button);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetBindWheelMouse, Localize("Reset position of mouse when opening bindwheel"), &g_Config.m_ClResetBindWheelMouse, &Button, LineSize);

	CUIRect CopyRight;
	MainView.HSplitBottom(20.0f, 0, &CopyRight);
	CopyRight.VSplitLeft(225.0f, &CopyRight, &CopyRight);
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
	if(Ui()->DoButton_FontIcon(&s_ReverseEntries, FONT_ICON_CHEVRON_DOWN, 0, &Button, 0))
	{
		s_Reversed = !s_Reversed;
	}

	CUIRect EntriesSearch;
	Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
	EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);

	static CWarEntry *pSelectedEntry = nullptr;
	static CWarType *pSelectedType = GameClient()->m_WarList.m_WarTypes[0];

	// Filter the list
	static CLineInputBuffered<128> s_EntriesFilterInput;
	std::vector<CWarEntry *> vpFilteredEntries;
	for(size_t i = 0; i < GameClient()->m_WarList.m_WarEntries.size(); ++i)
	{
		CWarEntry *pEntry = &GameClient()->m_WarList.m_WarEntries[i];
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

	const int MaxEntries = GameClient()->m_WarList.m_WarEntries.size();
	s_vItemIds.resize(MaxEntries);
	s_vDeleteButtons.resize(MaxEntries);

	for(size_t i = 0; i < vpFilteredEntries.size(); i++)
	{
		CWarEntry *pEntry = vpFilteredEntries[i];

		// idk why it wants this, it was complaining
		if(!pEntry)
			continue;

		if(pSelectedEntry && pEntry == pSelectedEntry)
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

		if(Ui()->DoButton_FontIcon(&s_vDeleteButtons[i], FONT_ICON_TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
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
			RenderFontIcon(EntryTypeRect, FONT_ICON_USERS, 18.0f, TEXTALIGN_MC);
		}
		else
		{
			// TODO: stop misusing this function
			// TODO: render the real skin with skin remembering component (to be added)
			RenderDevSkin(EntryTypeRect.Center(), 35.0f, "default", "default", false, 0, 0, 0, false);
		}

		if(str_comp(pEntry->m_aReason, "") != 0)
		{
			EntryRect.VSplitRight(20.0f, &EntryRect, &ToolTip);
			RenderFontIcon(ToolTip, FONT_ICON_COMMENT, 18.0f, TEXTALIGN_MC);
			GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &ToolTip, pEntry->m_aReason);
			GameClient()->m_Tooltips.SetFadeTime(&s_vItemIds[i], 0.0f);
		}

		EntryRect.HMargin(MarginExtraSmall, &EntryRect);
		EntryRect.HSplitMid(&EntryRect, &WarType, MarginSmall);

		Ui()->DoLabel(&EntryRect, aBuf, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(pEntry->m_pWarType->m_Color);
		Ui()->DoLabel(&WarType, pEntry->m_pWarType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedEntry = s_EntriesListBox.DoEnd();
	if(SelectedOldEntry != NewSelectedEntry || (SelectedOldEntry >= 0 && Ui()->HotItem() == &s_vItemIds[NewSelectedEntry] && Ui()->MouseButtonClicked(0)))
	{
		pSelectedEntry = vpFilteredEntries[NewSelectedEntry];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aEntryName, pSelectedEntry->m_aName);
			str_copy(s_aEntryClan, pSelectedEntry->m_aClan);
			str_copy(s_aEntryReason, pSelectedEntry->m_aReason);
			if(str_comp(pSelectedEntry->m_aClan, "") != 0)
			{
				s_IsName = 0;
				s_IsClan = 1;
			}
			else
			{
				s_IsName = 1;
				s_IsClan = 0;
			}
			pSelectedType = pSelectedEntry->m_pWarType;
		}
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
		Ui()->DoEditBox(&s_NameInput, &ButtonL, 12.0f);
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
		Ui()->DoEditBox(&s_ClanInput, &ButtonR, 12.0f);
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
	Ui()->DoEditBox(&s_ReasonInput, &Button, 12.0f);

	static CButtonContainer s_AddButton, s_OverrideButton;

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize * 2.0f, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);

	if(DoButtonLineSize_Menu(&s_OverrideButton, Localize("Override Entry"), 0, &ButtonL, LineSize) && pSelectedEntry)
	{
		if(pSelectedEntry && pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(pSelectedEntry->m_aName, s_aEntryName);
			str_copy(pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(pSelectedEntry->m_aReason, s_aEntryReason);
			pSelectedEntry->m_pWarType = pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, Localize("Add Entry"), 0, &ButtonR, LineSize))
	{
		if(pSelectedType)
			GameClient()->m_WarList.AddWarEntry(s_aEntryName, s_aEntryClan, s_aEntryReason, pSelectedType->m_aWarName);
	}
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column2);
	if(pSelectedType)
	{
		float Shade = 0.0f;
		Button.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), 15, 3.0f);
		TextRender()->TextColor(pSelectedType->m_Color);
		Ui()->DoLabel(&Button, pSelectedType->m_aWarName, HeadlineFontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Column2.HSplitBottom(180.0f, nullptr, &Column2);

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, Localize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, Localize("Enable warlist"), &g_Config.m_ClWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixes, Localize("Warlist Prefixes"), &g_Config.m_ClWarlistPrefixes, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListChat, Localize("Colors in chat"), &g_Config.m_ClWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListScoreboard, Localize("Colors in scoreboard"), &g_Config.m_ClWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSpecMenu, Localize("Colors in specmenu"), &g_Config.m_ClWarListSpecMenu, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListShowClan, Localize("Show clan if war"), &g_Config.m_ClWarListShowClan, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListSwapNameReason, Localize("Switch Reason with Name"), &g_Config.m_ClWarListSwapNameReason, &Column2, LineSize);
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

	const int MaxTypes = GameClient()->m_WarList.m_WarTypes.size();
	s_vTypeItemIds.resize(MaxTypes);
	s_vTypeDeleteButtons.resize(MaxTypes);

	for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
	{
		CWarType *pType = GameClient()->m_WarList.m_WarTypes[i];

		if(!pType)
			continue;

		if(pSelectedType && pType == pSelectedType)
			SelectedOldType = i;

		const CListboxItem Item = s_WarTypeListBox.DoNextItem(&s_vTypeItemIds[i], SelectedOldType >= 0 && SelectedOldType == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TypeRect, DeleteButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);

		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vTypeDeleteButtons[i], FONT_ICON_TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				m_pRemoveWarType = pType;
		}
		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedType = s_WarTypeListBox.DoEnd();
	if((SelectedOldType != NewSelectedType && NewSelectedType >= 0) || (NewSelectedType >= 0 && Ui()->HotItem() == &s_vTypeItemIds[NewSelectedType] && Ui()->MouseButtonClicked(0)))
	{
		pSelectedType = GameClient()->m_WarList.m_WarTypes[NewSelectedType];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aTypeName, pSelectedType->m_aWarName);
			s_GroupColor = pSelectedType->m_Color;
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
	Ui()->DoEditBox(&s_TypeNameInput, &Button, 12.0f);
	static CButtonContainer s_AddGroupButton, s_OverrideGroupButton, s_GroupColorPicker;

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int ColorValue = 0;
	ColorValue = color_cast<ColorHSLA>(s_GroupColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, Localize("Color"), &ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, Localize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && pSelectedType)
	{
		if(pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(pSelectedType->m_aWarName, s_aTypeName);
			pSelectedType->m_Color = s_GroupColor;
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
		bool Paused = GameClient()->m_aClients[ClientId].m_Paused || GameClient()->m_aClients[ClientId].m_Spec;
		const CAnimState *pAnimState = Paused ? CAnimState::GetSpec() : CAnimState::GetIdle();
		RenderTools()->RenderTee(pAnimState, &TeeInfo, Paused ? EMOTE_BLINK : EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.5f));
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
			const auto TextBoudningBox = TextRender()->TextBoundingBox(MaxExtent * 0.8f, FONT_ICON_XMARK);
			TextRender()->Text(Cross.x + (Cross.w - TextBoudningBox.m_W) / 2.0f, Cross.y + (Cross.h - TextBoudningBox.m_H) / 2.0f, MaxExtent * 0.8f, FONT_ICON_XMARK);
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
			DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, Localizable("Enable Deleting"), &s_AllowDelete, &Actions, LineSize);
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
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, PROFILES_FILE, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
	}

	const std::vector<CProfile> &ProfileList = GameClient()->m_SkinProfiles.m_Profiles;
	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, ProfileList.size(), MainView.w / 200.0f, 3, s_SelectedProfile, &MainView, true, IGraphics::CORNER_ALL, true);

	static bool s_Indexs[1024];

	for(size_t i = 0; i < ProfileList.size(); ++i)
	{
		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexs[i], s_SelectedProfile >= 0 && (size_t)s_SelectedProfile == i);
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

void CMenus::PopupConfirmRemoveWarType()
{
	GameClient()->m_WarList.RemoveWarType(m_pRemoveWarType->m_aWarName);
	m_pRemoveWarType = nullptr;
}

int CMenus::DoButtonLineSize_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, float Line_Size, bool Fake, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color)
{
	CUIRect Text = *pRect;

	if(Checked)
		Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	Color.a *= Ui()->ButtonColorMul(pButtonContainer);

	if(Fake)
		Color.a *= 0.5f;

	pRect->Draw(Color, Corners, Rounding);

	Text.HMargin(Line_Size / 2.0f, &Text);
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	Ui()->DoLabel(&Text, pText, Text.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	if(Fake)
		return 0;

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_ALL);
}

void CMenus::RenderDevSkin(vec2 RenderPos, float Size, const char *pSkinName, const char *pBackupSkin, bool CustomColors, int FeetColor, int BodyColor, int Emote, bool Rainbow, ColorRGBA ColorFeet, ColorRGBA ColorBody)
{
	float DefTick = std::fmod(s_Time, 1.0f);

	CTeeRenderInfo SkinInfo;
	const CSkin *pSkin = GameClient()->m_Skins.Find(pSkinName);
	if(str_comp(pSkin->GetName(), pSkinName) != 0)
		pSkin = GameClient()->m_Skins.Find(pBackupSkin);

	SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	SkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	SkinInfo.m_CustomColoredSkin = CustomColors;
	if(SkinInfo.m_CustomColoredSkin)
	{
		SkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		SkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		if(ColorFeet.a != 0.0f)
		{
			SkinInfo.m_ColorBody = ColorBody;
			SkinInfo.m_ColorFeet = ColorFeet;
		}
	}
	else
	{
		SkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		SkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	if(Rainbow)
	{
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(DefTick, 1.0f, 0.5f));
		SkinInfo.m_ColorBody = Col;
		SkinInfo.m_ColorFeet = Col;
	}
	SkinInfo.m_Size = Size;
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &SkinInfo, OffsetToMid);
	vec2 TeeRenderPos(RenderPos.x, RenderPos.y + OffsetToMid.y);
	RenderTools()->RenderTee(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos);
}

void CMenus::RenderFontIcon(const CUIRect Rect, const char *pText, float Size, int Align)
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

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;

	int Value = *pOption;
	Min /= Scale;
	Max /= Scale;
	// Allow adjustment of slider options when ctrl is pressed (to avoid scrolling, or accidently adjusting the value)
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
	{
		// clamp the value internally for the scrollbar
		Value = std::clamp(Value, Min, Max);
	}

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, minimum(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
	{
		Value = *pOption;
	}

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}

void CMenus::RenderACTee(CUIRect MainView, vec2 SpawnPos, const CAnimState *pAnim, CTeeRenderInfo *pInfo, int Draggable, float TeeSize, float Alpha)
{
	static bool OverrideTeePos = false;
	static bool CanDrag = false;
	static vec2 Pos = SpawnPos;

	if(ResetTeePos || !OverrideTeePos || !Draggable)
	{
		Pos = SpawnPos;

		if(ResetTeePos)
			ResetTeePos = false;
	}

	pInfo->m_Size = g_Config.m_ClFatSkins ? TeeSize - 20.0f : TeeSize;

	// interactive tee: tee looking towards cursor, and it is happy when you touch it
	vec2 DeltaPosition = Ui()->MousePos() - Pos;
	float Distance = length(DeltaPosition);
	float InteractionDistance = 20.0f;
	vec2 TeeDirection = normalize(DeltaPosition);
	int TeeEmote = Distance < InteractionDistance ? EMOTE_HAPPY : EMOTE_NORMAL;
	if(Draggable > 0)
	{
		if(Distance < InteractionDistance)
			CanDrag = true;

		if(GameClient()->Input()->KeyIsPressed(KEY_MOUSE_1) && CanDrag)
		{
			vec2 Offset = vec2(0.0f, 2.5f);
			Pos = Ui()->MousePos() - Offset;

			TeeDirection = normalize(SpawnPos - Pos);

			if(Draggable == 2)
			{
				float MenuTop = MainView.y + 25.0f;
				float MenuBottom = MainView.Size().y + 35.0f;

				float MenuLeft = MainView.x + 15.0f;
				float MenuRight = MainView.Size().x + 10.0f;

				if(Ui()->MousePos().y < MenuTop)
					Pos.y = MenuTop - Offset.y;
				if(Ui()->MousePos().y > MenuBottom)
					Pos.y = MenuBottom - Offset.y;

				if(Ui()->MousePos().x < MenuLeft)
					Pos.x = MenuLeft;
				if(Ui()->MousePos().x > MenuRight)
					Pos.x = MenuRight;
			}

			CanDrag = true;
			OverrideTeePos = true;
		}
		else if(GameClient()->Input()->KeyIsPressed(KEY_MOUSE_2) && OverrideTeePos && CanDrag)
			OverrideTeePos = false;
		else
			CanDrag = false;
	}

	RenderTools()->RenderTee(pAnim, pInfo, TeeEmote, TeeDirection, Pos);
}

bool CMenus::DoFloatScrollBar(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int DivideBy, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool Infinite = Flags & CUi::SCROLLBAR_OPTION_INFINITE;
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;
	const bool MultiLine = Flags & CUi::SCROLLBAR_OPTION_MULTILINE;

	int Value = *pOption;
	if(Infinite)
	{
		Max += 1;
		if(Value == 0)
			Value = Max;
	}

	// Allow adjustment of slider options when ctrl is pressed (to avoid scrolling, or accidently adjusting the value)
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->KeyPress(KEY_A) && Ui()->MouseInside(pRect))
	{
		Value -= Input()->ModifierIsPressed() ? 5 : 1;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->KeyPress(KEY_D) && Ui()->MouseInside(pRect))
	{
		Value += Input()->ModifierIsPressed() ? 5 : 1;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %.1f%s", pStr, (float)Value / DivideBy, pSuffix);

	Value = std::clamp(Value, Min, Max);

	CUIRect Label, ScrollBar;
	if(MultiLine)
		pRect->HSplitMid(&Label, &ScrollBar);
	else
		pRect->VSplitMid(&Label, &ScrollBar, minimum(10.0f, pRect->w * 0.05f));

	const float aFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, aFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
	{
		Value = *pOption; // use previous out of range value instead if the scrollbar is at the edge
	}
	else if(Infinite)
	{
		if(Value == Max)
			Value = 0;
	}

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}
