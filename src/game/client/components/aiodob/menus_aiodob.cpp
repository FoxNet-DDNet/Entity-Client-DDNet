#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
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

#include <array>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace FontIcons;
using namespace std::chrono_literals;



enum {
		AIODOB_TAB_PAGE1 = 0,
		AIODOB_TAB_VISUAL = 1,
		AIODOB_TAB_TCLIENT = 2,
		AIODOB_TAB_WARLIST = 3,
		AIODOB_TAB_BINDWHEEL = 4,
		NUMBER_OF_AIODOB_TABS = 5

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

void CMenus::RenderSettingsAiodob(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}


	static int s_CurTab = 0;

	CUIRect TabBar, Button, Label;

	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / NUMBER_OF_AIODOB_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_AIODOB_TABS] = {};
	const char *apTabNames[NUMBER_OF_AIODOB_TABS] = {
		Localize("A-Client Settings"),
		Localize("Other Settings"),
		Localize("T-Client Settings"),
		Localize("Warlist"),
		Localize("Bindwheel"),
	};

	for(int Tab = AIODOB_TAB_PAGE1; Tab < NUMBER_OF_AIODOB_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == AIODOB_TAB_PAGE1 ? IGraphics::CORNER_L : Tab == NUMBER_OF_AIODOB_TABS - 1 ? IGraphics::CORNER_R :
														       IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	// Client Page 1

	if(s_CurTab == AIODOB_TAB_PAGE1)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;


		// left side in settings menu

		CUIRect OtherSettings, MiscSettings, ColorSettings, FreezeKillSettings, ScoreboardSettings, NameplateSettings, MenuSettings, AutoKillOntopSettings;
		MainView.VSplitMid(&OtherSettings, &MiscSettings);

		{
			OtherSettings.VMargin(5.0f, &OtherSettings);
			if(g_Config.m_ClNotifyOnJoin)
				OtherSettings.HSplitTop(185.0f, &OtherSettings, &FreezeKillSettings);
			else
				OtherSettings.HSplitTop(165.0f, &OtherSettings, &FreezeKillSettings);
			if(s_ScrollRegion.AddRect(OtherSettings))
			{
				OtherSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				OtherSettings.VMargin(Margin, &OtherSettings);

				OtherSettings.HSplitTop(HeaderHeight, &Button, &OtherSettings);
				Ui()->DoLabel(&Button, Localize("Other Features"), FontSize, TEXTALIGN_MC);
				{
					{
						OtherSettings.HSplitTop(20.0f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &OtherSettings);
						Button.VSplitLeft(100.0f, &Label, &Button);
						Button.VSplitLeft(300.0f, &Button, 0);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClAutoReplyMsg, sizeof(g_Config.m_ClAutoReplyMsg));
						s_ReplyMsg.SetEmptyText("I'm Currently Tabbed Out");

						if(DoButton_CheckBox(&g_Config.m_ClTabbedOutMsg, "Auto reply", g_Config.m_ClTabbedOutMsg, &OtherSettings))
						{
							g_Config.m_ClTabbedOutMsg ^= 1;
						}

						if(g_Config.m_ClTabbedOutMsg)
							Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);

						
					}

					OtherSettings.HSplitTop(21.0f, &Button, &OtherSettings);

					{
						OtherSettings.HSplitTop(20.0f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &OtherSettings);
						Button.VSplitLeft(110.0f, &Label, &Button);
						Button.VSplitLeft(290.0f, &Button, 0);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClAutoReplyMutedMsg, sizeof(g_Config.m_ClAutoReplyMutedMsg));
						s_ReplyMsg.SetEmptyText("You're muted, I can't see your messages");

						if(DoButton_CheckBox(&g_Config.m_ClReplyMuted, "Muted Reply", g_Config.m_ClReplyMuted, &OtherSettings))
						{
							g_Config.m_ClReplyMuted ^= 1;
						}
						if(g_Config.m_ClReplyMuted)
							Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
					}

					OtherSettings.HSplitTop(21.0f, &Button, &OtherSettings);

					{
						OtherSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &OtherSettings);
						Button.VSplitLeft(150.0f, &Label, &Button);
						Button.VSplitLeft(170.0f, &Button, 0);

						static CLineInput s_NotifyName;
						s_NotifyName.SetBuffer(g_Config.m_ClAutoNotifyName, sizeof(g_Config.m_ClAutoNotifyName));
						s_NotifyName.SetEmptyText("qxdFox");

						if(DoButton_CheckBox(&g_Config.m_ClNotifyOnJoin, "Notify on Join Name", g_Config.m_ClNotifyOnJoin, &OtherSettings))
						{
							g_Config.m_ClNotifyOnJoin ^= 1;
						}
						if(g_Config.m_ClNotifyOnJoin)
						Ui()->DoEditBox(&s_NotifyName, &Button, 14.0f);

						static CLineInput s_NotifyMsg;
						s_NotifyMsg.SetBuffer(g_Config.m_ClAutoNotifyMsg, sizeof(g_Config.m_ClAutoNotifyMsg));
						s_NotifyMsg.SetEmptyText("Your Fav Person Has Joined!");

						if(g_Config.m_ClNotifyOnJoin)
						{
							OtherSettings.HSplitTop(21.0f, &Button, &OtherSettings);
							OtherSettings.HSplitTop(19.9f, &Button, &MainView);

							Button.VSplitLeft(25.0f, &Button, &OtherSettings);
							Button.VSplitLeft(125.0f, &Label, &Button);
							Button.VSplitLeft(275.0f, &Button, 0);

							OtherSettings.HSplitTop(2.8f, &Label, &OtherSettings);
							Ui()->DoLabel(&OtherSettings, "Noitfy Message:", 12.5f, TEXTALIGN_LEFT);

							Ui()->DoEditBox(&s_NotifyMsg, &Button, 14.0f);
						}
					}
					if(g_Config.m_ClNotifyOnJoin)
					{
						OtherSettings.VSplitLeft(-25.0f, &Button, &OtherSettings);
					}
					OtherSettings.HSplitTop(20.0f, &Button, &OtherSettings);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChangeTileNotification, ("Notify When Player is Being Moved"), &g_Config.m_ClChangeTileNotification, &OtherSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatBubble, ("Show Chat Bubble"), &g_Config.m_ClChatBubble, &OtherSettings, LineSize);
				
				}

			}
		}
		
		{
			FreezeKillSettings.HSplitTop(Margin, nullptr, &FreezeKillSettings);
			if(g_Config.m_ClFreezeKill)
			{
			//	if(g_Config.m_ClFreezeKillOnlyFullFrozen)
			//	{
					if(g_Config.m_ClFreezeKillWaitMs)
					{
						FreezeKillSettings.HSplitTop(260.0f, &FreezeKillSettings, &ColorSettings);
					}
					else
					{
						FreezeKillSettings.HSplitTop(220.0f, &FreezeKillSettings, &ColorSettings);
					}
			//	}
				//else
			//	{
					if(g_Config.m_ClFreezeKillWaitMs)
					{
						FreezeKillSettings.HSplitTop(240.0f, &FreezeKillSettings, &ColorSettings);
					}
					else
					{
						FreezeKillSettings.HSplitTop(200.0f, &FreezeKillSettings, &ColorSettings);
					}
				//}
			}
			else
				FreezeKillSettings.HSplitTop(75.0f, &FreezeKillSettings, &ColorSettings);
			if(s_ScrollRegion.AddRect(FreezeKillSettings))
			{
				FreezeKillSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				FreezeKillSettings.VMargin(Margin, &FreezeKillSettings);

				FreezeKillSettings.HSplitTop(HeaderHeight, &Button, &FreezeKillSettings);
				Ui()->DoLabel(&Button, Localize("Freeze Kill"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKill, Localize("Kill on Freeze"), &g_Config.m_ClFreezeKill, &FreezeKillSettings, LineSize);

				if(g_Config.m_ClFreezeKill)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillMultOnly, Localize("Only Enable on Multeasymap"), &g_Config.m_ClFreezeKillMultOnly, &FreezeKillSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillIgnoreKillProt, Localize("Ignore Kill Protection"), &g_Config.m_ClFreezeKillIgnoreKillProt, &FreezeKillSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeDontKillMoving, Localize("Don't Kill if Moving"), &g_Config.m_ClFreezeDontKillMoving, &FreezeKillSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillOnlyFullFrozen, Localize("Only Kill if Fully Frozen"), &g_Config.m_ClFreezeKillOnlyFullFrozen, &FreezeKillSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillTeamClose, Localize("Dont Kill if Teammate is Close"), &g_Config.m_ClFreezeKillTeamClose, &FreezeKillSettings, LineSize);

					if(g_Config.m_ClFreezeKillTeamClose)
					{
						FreezeKillSettings.HSplitTop(20.f, &Button, &FreezeKillSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillTeamDistance, &g_Config.m_ClFreezeKillTeamDistance, &Button, Localize("Team Max Distance"), 1, 25, &CUi::ms_LinearScrollbarScale, 0u, "");
					}

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeKillWaitMs, Localize("Wait Until Kill"), &g_Config.m_ClFreezeKillWaitMs, &FreezeKillSettings, LineSize);
					if(g_Config.m_ClFreezeKillWaitMs)
					{
						FreezeKillSettings.HSplitTop(2 * LineSize, &Button, &FreezeKillSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClFreezeKillMs, &g_Config.m_ClFreezeKillMs, &Button, Localize("Milliseconds to Wait For"), 1, 5000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "ms");
					}
				}
			}
		}
	
		{
			ColorSettings.HSplitTop(Margin, nullptr, &ColorSettings);
				ColorSettings.HSplitTop(140.0f, &ColorSettings, &NameplateSettings);
			if(s_ScrollRegion.AddRect(ColorSettings))
			{
				ColorSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ColorSettings.VMargin(Margin, &ColorSettings);

				ColorSettings.HSplitTop(HeaderHeight, &Button, &ColorSettings);
				Ui()->DoLabel(&Button, Localize("Color Settings"), FontSize, TEXTALIGN_MC);
				{
					static CButtonContainer s_MutedColor;
					DoLine_ColorPicker(&s_MutedColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &ColorSettings, Localize("Color of The Muted Icon"), &g_Config.m_ClMutedColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(5439743)), true);

					static CButtonContainer s_FriendColor;
					DoLine_ColorPicker(&s_FriendColor, ColorPickerLineSize + 0.25f, ColorPickerLabelSize + 0.25f, ColorPickerLineSpacing, &ColorSettings, Localize("Name Color of Friends"), &g_Config.m_ClFriendColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(14745554)), true);

					static CButtonContainer s_SpecColor;
					DoLine_ColorPicker(&s_SpecColor, ColorPickerLineSize + 0.3f, ColorPickerLabelSize + 0.3f, ColorPickerLineSpacing, &ColorSettings, Localize("Spectate Prefix Color"), &g_Config.m_ClSpecColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(8936607)), true);
				}
			}
		}

		{
			NameplateSettings.HSplitTop(Margin, nullptr, &NameplateSettings);
				NameplateSettings.HSplitTop(90.0f, &NameplateSettings, 0);
			if(s_ScrollRegion.AddRect(NameplateSettings))
			{
				NameplateSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				NameplateSettings.VMargin(Margin, &NameplateSettings);

				NameplateSettings.HSplitTop(HeaderHeight, &Button, &NameplateSettings);
				Ui()->DoLabel(&Button, Localize("Nameplate Settings"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClMutedIconNameplate, Localize("Show Muted Icon in Nameplates"), &g_Config.m_ClMutedIconNameplate, &NameplateSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoFriendNameColor, Localize("Do Friend Nameplate Color"), &g_Config.m_ClDoFriendNameColor, &NameplateSettings, LineSize);

				}
		
			}
		}


		// right side

			// UI/Hud Settings
		
		{
			MiscSettings.VMargin(5.0f, &MiscSettings);
				if(g_Config.m_ClRenderCursorSpec)
				{
					MiscSettings.HSplitTop(200.0f, &MiscSettings, &AutoKillOntopSettings);
				}
				else
					MiscSettings.HSplitTop(180.0f, &MiscSettings, &AutoKillOntopSettings);
			if(s_ScrollRegion.AddRect(MiscSettings))
			{
				MiscSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				MiscSettings.VMargin(Margin, &MiscSettings);

				MiscSettings.HSplitTop(HeaderHeight, &Button, &MiscSettings);
				Ui()->DoLabel(&Button, Localize("Miscellaneous"), FontSize, TEXTALIGN_MC);
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
						FontDropDownRect.VSplitLeft(100.0f, &Label, &FontDropDownRect);
						FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
						FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

						Ui()->DoLabel(&Label, Localize("Custom Font: "), FontSize, TEXTALIGN_ML);
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
						if(DoButton_FontIcon(&s_FontDirectoryId, FONT_ICON_FOLDER, 0, &FontDirectory, IGraphics::CORNER_ALL))
						{
							Storage()->CreateFolder("data/aiodob", IStorage::TYPE_ABSOLUTE);
							Storage()->CreateFolder("data/aiodob/fonts", IStorage::TYPE_ABSOLUTE);
							Client()->ViewFile("data/aiodob/fonts");
						}
					}
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoVerify, ("Auto Verify"), &g_Config.m_ClAutoVerify, &MiscSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, ("Show Ping Circles Next To Names"), &g_Config.m_ClPingNameCircle, &MiscSettings, LineSize);

					MiscSettings.HSplitTop(5.0f, &Button, &MiscSettings);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderCursorSpec, ("Show Cursor While Spectating"), &g_Config.m_ClRenderCursorSpec, &MiscSettings, LineSize);
					if(g_Config.m_ClRenderCursorSpec)
					{
						MiscSettings.HSplitTop(20.f, &Button, &MiscSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClRenderCursorSpecOpacity, &g_Config.m_ClRenderCursorSpecOpacity, &Button, Localize("Cursor Opacity"), 1, 100, &CUi::ms_LinearScrollbarScale, 0u, "");
					}

					MiscSettings.HSplitTop(5.0f, &Button, &MiscSettings);
					{
						MiscSettings.HSplitTop(20.0f, &Button, &MainView);
						Button.VSplitLeft(0.0f, &Button, &MiscSettings);
						Button.VSplitLeft(158.0f, &Label, &Button);
						Button.VSplitLeft(248.0f, &Button, 0);

						static CLineInput s_ReplyMsg;
						s_ReplyMsg.SetBuffer(g_Config.m_ClRunOnJoinMsg, sizeof(g_Config.m_ClRunOnJoinMsg));
						s_ReplyMsg.SetEmptyText("Any Console Command");

						if(DoButton_CheckBox(&g_Config.m_ClRunOnJoinConsole, "Run on Join Console", g_Config.m_ClRunOnJoinConsole, &MiscSettings))
						{
							g_Config.m_ClRunOnJoinConsole ^= 1;
						}
						Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
					}

					{
						MiscSettings.HSplitTop(25.0f, &Button, &MiscSettings);
						MiscSettings.HSplitTop(20.0f, &Button, &MiscSettings);

						Button.VSplitLeft(0.0f, &Button, &MiscSettings);
						Button.VSplitLeft(175.0f, &Label, &Button);
						Button.VSplitLeft(125.0f, &Button, 0);

						static CLineInput s_LastInput;
						s_LastInput.SetBuffer(g_Config.m_ClNotifyWhenLastText, sizeof(g_Config.m_ClNotifyWhenLastText));
						s_LastInput.SetEmptyText("Last!");

						if(DoButton_CheckBox(&g_Config.m_ClNotifyWhenLast, "Show when you're last", g_Config.m_ClNotifyWhenLast, &MiscSettings))
						{
							g_Config.m_ClNotifyWhenLast ^= 1;
						}

						if(g_Config.m_ClNotifyWhenLast)
						{
							Ui()->DoEditBox(&s_LastInput, &Button, 14.0f);
							static CLineInput s_NotifyMsg;
							static CButtonContainer s_LastColor;
							s_NotifyMsg.SetBuffer(g_Config.m_ClNotifyWhenLastText, sizeof(g_Config.m_ClNotifyWhenLastText));
							s_NotifyMsg.SetEmptyText("Last!");

							MiscSettings.HSplitTop(-3.0f, &Label, &MiscSettings);

							DoLine_ColorPicker(&s_LastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &MiscSettings, Localize(""), &g_Config.m_ClNotifyWhenLastColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(29057)), true);
						}
					}
				}
			
			}
		}

			{
			AutoKillOntopSettings.HSplitTop(Margin, nullptr, &AutoKillOntopSettings);
			if(g_Config.m_ClAutoKill)
				AutoKillOntopSettings.HSplitTop(205.0f, &AutoKillOntopSettings, &ScoreboardSettings);
			else
				AutoKillOntopSettings.HSplitTop(75.0f, &AutoKillOntopSettings, &ScoreboardSettings);
			if(s_ScrollRegion.AddRect(AutoKillOntopSettings))
			{
				AutoKillOntopSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				AutoKillOntopSettings.VMargin(Margin, &AutoKillOntopSettings);

				AutoKillOntopSettings.HSplitTop(HeaderHeight, &Button, &AutoKillOntopSettings);
				Ui()->DoLabel(&Button, Localize("Auto Kill"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoKill, ("Auto Kill if Frozen And You're Below a Player"), &g_Config.m_ClAutoKill, &AutoKillOntopSettings, LineMargin);
				if(g_Config.m_ClAutoKill)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoKillMultOnly, ("Only Enable on Multeasymap"), &g_Config.m_ClAutoKillMultOnly, &AutoKillOntopSettings, LineMargin);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoKillIgnoreKillProt, ("Ignore Kill Protection"), &g_Config.m_ClAutoKillIgnoreKillProt, &AutoKillOntopSettings, LineMargin);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoKillWarOnly, ("Only Kill if The Player is an Enemy"), &g_Config.m_ClAutoKillWarOnly, &AutoKillOntopSettings, LineMargin);

					AutoKillOntopSettings.HSplitTop(2 * LineSize, &Button, &AutoKillOntopSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClAutoKillRangeX, &g_Config.m_ClAutoKillRangeX, &Button, Localize("x Axis ↔ Auto Kill Range"), 1, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

					AutoKillOntopSettings.HSplitTop(2 * LineSize, &Button, &AutoKillOntopSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClAutoKillRangeY, &g_Config.m_ClAutoKillRangeY, &Button, Localize("y Axis ↕ Auto Kill Range"), 1, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
				}
			}
		}


		{
			ScoreboardSettings.HSplitTop(Margin, nullptr, &ScoreboardSettings);
			ScoreboardSettings.HSplitTop(125.0f, &ScoreboardSettings, &MenuSettings);
			if(s_ScrollRegion.AddRect(ScoreboardSettings))
			{
				ScoreboardSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ScoreboardSettings.VMargin(Margin, &ScoreboardSettings);
				ScoreboardSettings.HSplitTop(HeaderHeight, &Button, &ScoreboardSettings);
				Ui()->DoLabel(&Button, Localize("Scoreboard Settings"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoFriendColorScore, Localize("Do Friend Name Colors"), &g_Config.m_ClDoFriendColorScore, &ScoreboardSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoAfkColors, Localize("Afk Name Colors"), &g_Config.m_ClDoAfkColors, &ScoreboardSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClScoreSpecPrefix, Localize("Show Spec Prefix"), &g_Config.m_ClScoreSpecPrefix, &ScoreboardSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClScoreSpecPlayer, Localize("Make Tees Sit in Scoreboard When Spectating"), &g_Config.m_ClScoreSpecPlayer, &ScoreboardSettings, LineSize);
				}
			}
		}

		{
			MenuSettings.HSplitTop(Margin, nullptr, &MenuSettings);
			MenuSettings.HSplitTop(120.0f, &MenuSettings, 0);
			if(s_ScrollRegion.AddRect(MenuSettings))
			{
				MenuSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				MenuSettings.VMargin(Margin, &MenuSettings);

				MenuSettings.HSplitTop(HeaderHeight, &Button, &MenuSettings);
				Ui()->DoLabel(&Button, Localize("Menu Settings"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersInMenu, Localize("Show Settigns Icon When Tee's in a Menu"), &g_Config.m_ClShowOthersInMenu, &MenuSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuColors, Localize("Player Colors in Spectate Menu"), &g_Config.m_ClSpecMenuColors, &MenuSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuPrefixes, Localize("Player Prefixes in Spectate Menu"), &g_Config.m_ClSpecMenuPrefixes, &MenuSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecMenuFriendPrefix, Localize("Friend Prefix in Spectate Menu"), &g_Config.m_ClSpecMenuFriendPrefix, &MenuSettings, LineSize);
					
				}
			}
		}
		s_ScrollRegion.End();
	}

	if(s_CurTab == AIODOB_TAB_VISUAL)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		const float FontSize = 14.0f;
		const float Margin = 10.0f;
		const float HeaderHeight = FontSize + 5.0f + Margin;

		// left side in settings menu

		CUIRect PlayerSettings, ChatSettings, SoundSettings, WarVisual, UiSettings, AiodobSettings, GoresModeSettings;
		MainView.VSplitMid(&PlayerSettings, &UiSettings);

		{
			PlayerSettings.VMargin(5.0f, &PlayerSettings);
			if(g_Config.m_ClRainbow || g_Config.m_ClRainbowHook || g_Config.m_ClRainbowHookOthers || g_Config.m_ClRainbowOthers)
				PlayerSettings.HSplitTop(195.0f, &PlayerSettings, &ChatSettings);
			else
				PlayerSettings.HSplitTop(175.0f, &PlayerSettings, &ChatSettings);
			if(s_ScrollRegion.AddRect(PlayerSettings))
			{
				PlayerSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				PlayerSettings.VMargin(Margin, &PlayerSettings);

				PlayerSettings.HSplitTop(HeaderHeight, &Button, &PlayerSettings);
				Ui()->DoLabel(&Button, Localize("Cosmetic Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSmallSkins, ("Small Skins"), &g_Config.m_ClSmallSkins, &PlayerSettings, LineMargin);

				PlayerSettings.HSplitTop(5.f, &Button, &PlayerSettings);
		
	
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecialEffect, ("Secret Effect"), &g_Config.m_ClSpecialEffect, &PlayerSettings, LineMargin);
				
				PlayerSettings.HSplitTop(5.f, &Button, &PlayerSettings);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSparkleEffect, ("Sparkle Effect Self"), &g_Config.m_ClSparkleEffect, &PlayerSettings, LineMargin);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSparkleEffectOthers, ("Sparkle Effect Others"), &g_Config.m_ClSparkleEffectOthers, &PlayerSettings, LineMargin);

				static CButtonContainer SparkleR;
				PlayerSettings.HSplitTop(-35.0f, &PlayerSettings, &PlayerSettings);
				DoLine_ColorPicker(&SparkleR, 25.0f, 13.0f, 5.0f, &PlayerSettings, (""), &g_Config.m_ClSparkleColor, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), false);
				PlayerSettings.HSplitTop(5.0f, &PlayerSettings, &PlayerSettings);

				PlayerSettings.HSplitTop(5.f, &Button, &PlayerSettings);
				PlayerSettings.HSplitTop(20.f, &Button, &PlayerSettings);
				if(DoButton_CheckBox(&g_Config.m_ClRainbow, Localize("Rainbow Tee"), g_Config.m_ClRainbow, &Button))
					g_Config.m_ClRainbow ^= 1;

				PlayerSettings.HSplitTop(20.f, &Button, &PlayerSettings);
				if(DoButton_CheckBox(&g_Config.m_ClRainbowHook, Localize("Rainbow Hook"), g_Config.m_ClRainbowHook, &Button))
					g_Config.m_ClRainbowHook ^= 1;
				if(g_Config.m_ClRainbow || g_Config.m_ClRainbowHook || g_Config.m_ClRainbowHookOthers || g_Config.m_ClRainbowOthers)
				{
					PlayerSettings.HSplitTop(20.f, &Button, &PlayerSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Button, Localize("Rainbow Speed"), 1, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
					PlayerSettings.HSplitTop(-20.f, &Button, &PlayerSettings);
				}

				PlayerSettings.VSplitLeft(0.f, &Button, &PlayerSettings);
				PlayerSettings.VSplitLeft(150.f, &Button, &PlayerSettings);

				PlayerSettings.HSplitTop(-40.f, &Button, &PlayerSettings);
				PlayerSettings.HSplitTop(20.f, &Button, &PlayerSettings);
				if(DoButton_CheckBox(&g_Config.m_ClRainbowOthers, Localize("Rainbow Tee Others"), g_Config.m_ClRainbowOthers, &Button))
					g_Config.m_ClRainbowOthers ^= 1;

				PlayerSettings.HSplitTop(20.f, &Button, &PlayerSettings);
				if(DoButton_CheckBox(&g_Config.m_ClRainbowHookOthers, Localize("Rainbow Hook Others"), g_Config.m_ClRainbowHookOthers, &Button))
					g_Config.m_ClRainbowHookOthers ^= 1;
			}
		}

		// chat settings

		{
			ChatSettings.HSplitTop(Margin, nullptr, &ChatSettings);
			ChatSettings.HSplitTop(430.0f, &ChatSettings, 0);
			if(s_ScrollRegion.AddRect(ChatSettings))
			{
				ChatSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ChatSettings.VMargin(Margin, &ChatSettings);

				ChatSettings.HSplitTop(HeaderHeight, &Button, &ChatSettings);
				Ui()->DoLabel(&Button, Localize("Chat Settings"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowMutedInConsole, ("Show Messages of Muted People in The Console"), &g_Config.m_ClShowMutedInConsole, &ChatSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideEnemyChat, ("Hide Enemy Chat (Shows in Console)"), &g_Config.m_ClHideEnemyChat, &ChatSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoFriendColorInchat, ("Chat Friend Name"), &g_Config.m_ClDoFriendColorInchat, &ChatSettings, LineSize);

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowIdsChat, ("Client Ids in Chat"), &g_Config.m_ClShowIdsChat, &ChatSettings, LineSize);

					/*
					// enemy prefix

					ChatSettings.HSplitTop(2.0f, &Button, &ChatSettings);
					{
						ChatSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &ChatSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
						Button.VSplitLeft(85.0f, &Button, 0);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClEnemyPrefix, sizeof(g_Config.m_ClEnemyPrefix));
						s_PrefixMsg.SetEmptyText("alt + num4");
						if(DoButton_CheckBox(&g_Config.m_ClChatEnemyPrefix, "Enemy Prefix", g_Config.m_ClChatEnemyPrefix, &ChatSettings))
						{
							g_Config.m_ClChatEnemyPrefix ^= 1;
						}
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}

					// helper prefix

					ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
					{
						ChatSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &ChatSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
						Button.VSplitLeft(85.0f, &Button, 0);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClHelperPrefix, sizeof(g_Config.m_ClHelperPrefix));
						s_PrefixMsg.SetEmptyText("alt + num4");
						if(DoButton_CheckBox(&g_Config.m_ClChatHelperPrefix, "Helper Prefix", g_Config.m_ClChatHelperPrefix, &ChatSettings))
						{
							g_Config.m_ClChatHelperPrefix ^= 1;
						}
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}

					// teammate prefix

					ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
					{
						ChatSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &ChatSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
						Button.VSplitLeft(85.0f, &Button, 0);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClTeammatePrefix, sizeof(g_Config.m_ClTeammatePrefix));
						s_PrefixMsg.SetEmptyText("alt + num4");
						if(DoButton_CheckBox(&g_Config.m_ClChatTeammatePrefix, "Teammate Prefix", g_Config.m_ClChatTeammatePrefix, &ChatSettings))
						{
							g_Config.m_ClChatTeammatePrefix ^= 1;
						}
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}
					*/
					// friend prefix

					ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);

					{
						ChatSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &ChatSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
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
						Button.VSplitLeft(140.0f, &Label, &Button);
						Button.VSplitLeft(85.0f, &Button, 0);

						static CLineInput s_PrefixMsg;
						s_PrefixMsg.SetBuffer(g_Config.m_ClSpecPrefix, sizeof(g_Config.m_ClSpecPrefix));
						s_PrefixMsg.SetEmptyText("(s) ");
						if(DoButton_CheckBox(&g_Config.m_ClChatSpecPrefix, "Spec Prefix", g_Config.m_ClChatSpecPrefix, &ChatSettings))
						{
							g_Config.m_ClChatSpecPrefix ^= 1;
						}
						Ui()->DoEditBox(&s_PrefixMsg, &Button, 14.0f);
					}

					// server prefix

					ChatSettings.HSplitTop(21.0f, &Button, &ChatSettings);
					{
						ChatSettings.HSplitTop(19.9f, &Button, &MainView);

						Button.VSplitLeft(0.0f, &Button, &ChatSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
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
						Button.VSplitLeft(140.0f, &Label, &Button);
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
						Button.VSplitLeft(140.0f, &Label, &Button);
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

						const float X = RealMsgPaddingX / 2.0f + ChatSettings.x;
						float Y = ChatSettings.y;
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
							int m_ClShowIdsChat;
							bool m_Team;
							char m_aName[64];
							char m_aText[256];
							bool m_Spec;
							bool m_Enemy;
							bool m_Helper;
							bool m_Teammate;
							bool m_Friend;
							bool m_Player;
							bool m_Client;
							bool m_Highlighted;
							int m_TimesRepeated;

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
								if(Line.m_Spec && g_Config.m_ClChatSpecPrefix)
								{
									if(Render)
										TextRender()->TextColor(SpecColor);
									TextRender()->TextEx(&LocalCursor, g_Config.m_ClSpecPrefix, -1);
								}
							}

							ColorRGBA NameColor;
							if(Line.m_Friend && g_Config.m_ClDoFriendColorInchat)
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
						SetLineSkin(2, GameClient()->m_Skins.Find("-Baby_Dou_KZ-"));
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
				}
			}
		}

		// right side in settings menu

		
		{
			UiSettings.VMargin(5.0f, &UiSettings);
			if(g_Config.m_ClFpsSpoofer)
				UiSettings.HSplitTop(160.0f, &UiSettings, &SoundSettings);
			else
				UiSettings.HSplitTop(125.0f, &UiSettings, &SoundSettings);
			if(s_ScrollRegion.AddRect(UiSettings))
			{
				UiSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				UiSettings.VMargin(10.0f, &UiSettings);

				UiSettings.HSplitTop(HeaderHeight, &Button, &UiSettings);
				Ui()->DoLabel(&Button, Localize("Ui/Hud Settings"), FontSize, TEXTALIGN_MC);

				UiSettings.HSplitTop(2 * LineSize, &Button, &UiSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClCornerRoundness, &g_Config.m_ClCornerRoundness, &Button, Localize("How round Corners should be"), 0, 150, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				static CButtonContainer s_UiColor;
				// DoLine_ColorPicker(&s_UiColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &FriendSettings, Localize("Foe (war?) Name Color (old)"), &g_Config.m_AiodobColor, ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f), true);

				DoLine_ColorPicker(&s_UiColor, 25.0f, 13.0f, 2.0f, &UiSettings, Localize("Background Color"), &g_Config.m_AiodobColor, color_cast<ColorRGBA>(ColorHSLA(654311494, true)), false, nullptr, true);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFpsSpoofer, Localize("Fps Spoofer"), &g_Config.m_ClFpsSpoofer, &UiSettings, LineSize);
				if(g_Config.m_ClFpsSpoofer)
				{
					UiSettings.HSplitTop(2 * LineSize, &Button, &UiSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClFpsSpoofPercentage, &g_Config.m_ClFpsSpoofPercentage, &Button, Localize("Fps Spoofer Margin"), 1, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
				}
			}
		}

		{
			SoundSettings.HSplitTop(Margin, nullptr, &SoundSettings);
			SoundSettings.HSplitTop(90.0f, &SoundSettings, &WarVisual);
			if(s_ScrollRegion.AddRect(SoundSettings))
			{
				SoundSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				SoundSettings.VMargin(Margin, &SoundSettings);

				SoundSettings.HSplitTop(HeaderHeight, &Button, &SoundSettings);
				Ui()->DoLabel(&Button, Localize("Sound Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_SndFriendChat, ("Do Chat Sound For Friends Only"), &g_Config.m_SndFriendChat, &SoundSettings, LineMargin);
				if(g_Config.m_SndFriendChat)
				{
					g_Config.m_SndChat = 0;
				}

				if(g_Config.m_ClNotifyOnJoin)
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoNotifySound, ("Do 'Notify on Player Join' Sound Effect"), &g_Config.m_ClAutoNotifySound, &SoundSettings, LineMargin);
			}
		}

		{
			WarVisual.HSplitTop(Margin, nullptr, &WarVisual);
			if(g_Config.m_ClSweatMode)
				if(g_Config.m_ClSweatModeSkin)
					WarVisual.HSplitTop(185.0f, &WarVisual, &GoresModeSettings);
				else
					WarVisual.HSplitTop(145.0f, &WarVisual, &GoresModeSettings);
			else
				WarVisual.HSplitTop(125.0f, &WarVisual, &GoresModeSettings);
			if(s_ScrollRegion.AddRect(WarVisual))
			{
				WarVisual.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				WarVisual.VMargin(Margin, &WarVisual);

				WarVisual.HSplitTop(HeaderHeight, &Button, &WarVisual);
				Ui()->DoLabel(&Button, Localize("Extra Warlist Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistConsoleColors, ("Show Colors in Console"), &g_Config.m_ClWarlistConsoleColors, &WarVisual, LineMargin);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoClanWar, ("Make Clanmates of Enemies Also Have a Red-Ish Color"), &g_Config.m_ClAutoClanWar, &WarVisual, LineMargin);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoAddOnNameChange, Localize("Auto Add to Lists on Name Change"), &g_Config.m_ClAutoAddOnNameChange, &WarVisual, LineSize);

				WarVisual.HSplitTop(5, &Button, &WarVisual);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatMode, ("Sweat Mode."), &g_Config.m_ClSweatMode, &WarVisual, LineMargin);

				if(g_Config.m_ClSweatMode)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeSkin, ("Change Everyones Skin"), &g_Config.m_ClSweatModeSkin, &WarVisual, LineMargin);

					if(g_Config.m_ClSweatModeSkin)
					{
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSweatModeOnlyOthers, ("Don't Change Own Skin"), &g_Config.m_ClSweatModeOnlyOthers, &WarVisual, LineMargin);

						static CLineInput s_Name;
						s_Name.SetBuffer(g_Config.m_ClSweatModeSkinName, sizeof(g_Config.m_ClSweatModeSkinName));
						s_Name.SetEmptyText("x_ninja");

						WarVisual.HSplitTop(2.4f, &Label, &WarVisual);
						WarVisual.VSplitLeft(25.0f, &WarVisual, &WarVisual);
						Ui()->DoLabel(&WarVisual, "Skin Name:", 13.0f, TEXTALIGN_LEFT);

						WarVisual.HSplitTop(-1, &Button, &WarVisual);
						WarVisual.HSplitTop(18.9f, &Button, &WarVisual);

						Button.VSplitLeft(0.0f, &Button, &WarVisual);
						Button.VSplitLeft(80.0f, &Label, &Button);
						Button.VSplitLeft(120.0f, &Button, 0);

						Ui()->DoEditBox(&s_Name, &Button, 14.0f);
					}
					else
						g_Config.m_ClSweatModeOnlyOthers = 0;
				}
			}
		}

		{
			GoresModeSettings.HSplitTop(Margin, nullptr, &GoresModeSettings);
			GoresModeSettings.HSplitTop(140.0f, &GoresModeSettings, &AiodobSettings);
			if(s_ScrollRegion.AddRect(GoresModeSettings))
			{
				GoresModeSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				GoresModeSettings.VMargin(Margin, &GoresModeSettings);

				GoresModeSettings.HSplitTop(HeaderHeight, &Button, &GoresModeSettings);
				Ui()->DoLabel(&Button, Localize("Gores Mode"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresMode, ("\"advanced\" Gores Mode"), &g_Config.m_ClGoresMode, &GoresModeSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClGoresModeDisableIfWeapons, ("Disable if Player Has Any Weapon"), &g_Config.m_ClGoresModeDisableIfWeapons, &GoresModeSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoEnableGoresMode, ("Auto Enable if Gametype is 'Gores'"), &g_Config.m_ClAutoEnableGoresMode, &GoresModeSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDisableGoresOnShutdown, ("Disable on Shutdown"), &g_Config.m_ClDisableGoresOnShutdown, &GoresModeSettings, LineSize);
			}
		}

		{
			AiodobSettings.HSplitTop(Margin, nullptr, &AiodobSettings);
			AiodobSettings.HSplitTop(90.0f, &AiodobSettings, 0);
			if(s_ScrollRegion.AddRect(AiodobSettings))
			{
				AiodobSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				AiodobSettings.VMargin(Margin, &AiodobSettings);

				AiodobSettings.HSplitTop(HeaderHeight, &Button, &AiodobSettings);
				Ui()->DoLabel(&Button, Localize("Aiodob Info"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClEnabledInfo, ("Display What Features Are On/Off on Join"), &g_Config.m_ClEnabledInfo, &AiodobSettings, LineMargin);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClListsInfo, ("Tells You How Many People of Default Lists Are Online"), &g_Config.m_ClListsInfo, &AiodobSettings, LineMargin);
			}
		}
		s_ScrollRegion.End();
	}

	if(s_CurTab == AIODOB_TAB_TCLIENT)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		const float FontSize = 14.0f;
		const float Margin = 10.0f;
		const float HeaderHeight = FontSize + 5.0f + Margin;

		// left side in settings menu

		CUIRect OutlineSettings, PlayerIndicatorSettings, FrozenTeeHudSettings, LatencySettings, GhostSettings, FastInputSettings;
		MainView.VSplitMid(&OutlineSettings, &PlayerIndicatorSettings);

		// Weapon Settings
		{
			OutlineSettings.VMargin(5.0f, &OutlineSettings);
			OutlineSettings.HSplitTop(365.0f, &OutlineSettings, &FrozenTeeHudSettings);
			if(s_ScrollRegion.AddRect(OutlineSettings))
			{
				OutlineSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				OutlineSettings.VMargin(Margin, &OutlineSettings);

				OutlineSettings.HSplitTop(HeaderHeight, &Button, &OutlineSettings);
				Ui()->DoLabel(&Button, Localize("Outlines"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutline, ("Enable Outlines"), &g_Config.m_ClOutline, &OutlineSettings, LineMargin);
					OutlineSettings.HSplitTop(5.0f, &Button, &OutlineSettings);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineEntities, ("Only show outlines in entities"), &g_Config.m_ClOutlineEntities, &OutlineSettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineFreeze, ("Outline freeze & deep"), &g_Config.m_ClOutlineFreeze, &OutlineSettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineSolid, ("Outline walls"), &g_Config.m_ClOutlineSolid, &OutlineSettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineTele, ("Outline teleporter"), &g_Config.m_ClOutlineTele, &OutlineSettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClOutlineUnFreeze, ("Outline unfreeze & undeep"), &g_Config.m_ClOutlineUnFreeze, &OutlineSettings, LineMargin);

					{
						OutlineSettings.HSplitTop(5.0f, &Button, &OutlineSettings);
						OutlineSettings.HSplitTop(20.0f, &Button, &OutlineSettings);
						Button.VSplitLeft(150.0f, &Label, &Button);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s: %i ", "Outline Width", g_Config.m_ClOutlineWidth);
						Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
						g_Config.m_ClOutlineWidth = (int)(Ui()->DoScrollbarH(&g_Config.m_ClOutlineWidth, &Button, (g_Config.m_ClOutlineWidth - 1) / 15.0f) * 15.0f) + 1;
					}
					{
						OutlineSettings.HSplitTop(5.0f, &Button, &OutlineSettings);
						OutlineSettings.HSplitTop(20.0f, &Button, &OutlineSettings);
						Button.VSplitLeft(150.0f, &Label, &Button);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s: %i ", "Outline Alpha", g_Config.m_ClOutlineAlpha);
						Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
						g_Config.m_ClOutlineAlpha = (int)(Ui()->DoScrollbarH(&g_Config.m_ClOutlineAlpha, &Button, (g_Config.m_ClOutlineAlpha) / 100.0f) * 100.0f);
					}
					{
						OutlineSettings.HSplitTop(5.0f, &Button, &OutlineSettings);
						OutlineSettings.HSplitTop(20.0f, &Button, &OutlineSettings);
						Button.VSplitLeft(185.0f, &Label, &Button);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s: %i ", "Outline Alpha (walls)", g_Config.m_ClOutlineAlphaSolid);
						Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
						g_Config.m_ClOutlineAlphaSolid = (int)(Ui()->DoScrollbarH(&g_Config.m_ClOutlineAlphaSolid, &Button, (g_Config.m_ClOutlineAlphaSolid) / 100.0f) * 100.0f);
					}
					static CButtonContainer OutlineColorFreezeID, OutlineColorSolidID, OutlineColorTeleID, OutlineColorUnfreezeID;

					OutlineSettings.HSplitTop(5.0f, 0x0, &OutlineSettings);
					OutlineSettings.VSplitLeft(-5.0f, 0x0, &OutlineSettings);

					OutlineSettings.HSplitTop(2.0f, &OutlineSettings, &OutlineSettings);
					DoLine_ColorPicker(&OutlineColorFreezeID, 25.0f, 13.0f, 5.0f, &OutlineSettings, ("Freeze Outline Color"), &g_Config.m_ClOutlineColorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), false);

					OutlineSettings.HSplitTop(2.0f, &OutlineSettings, &OutlineSettings);
					DoLine_ColorPicker(&OutlineColorSolidID, 25.0f, 13.0f, 5.0f, &OutlineSettings, ("Walls Outline Color"), &g_Config.m_ClOutlineColorSolid, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), false);

					OutlineSettings.HSplitTop(2.0f, &OutlineSettings, &OutlineSettings);
					DoLine_ColorPicker(&OutlineColorTeleID, 25.0f, 13.0f, 5.0f, &OutlineSettings, ("Teleporter Outline Color"), &g_Config.m_ClOutlineColorTele, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), false);

					OutlineSettings.HSplitTop(2.0f, &OutlineSettings, &OutlineSettings);
					DoLine_ColorPicker(&OutlineColorUnfreezeID, 25.0f, 13.0f, 5.0f, &OutlineSettings, ("Unfreeze Outline Color"), &g_Config.m_ClOutlineColorUnfreeze, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), false);
		
				}
			}
		}
		{
			FrozenTeeHudSettings.HSplitTop(Margin, nullptr, &FrozenTeeHudSettings);

			FrozenTeeHudSettings.HSplitTop(165.0f, &FrozenTeeHudSettings, &FastInputSettings);
			if(s_ScrollRegion.AddRect(FrozenTeeHudSettings))
			{
				FrozenTeeHudSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				FrozenTeeHudSettings.VMargin(Margin, &FrozenTeeHudSettings);

				FrozenTeeHudSettings.HSplitTop(HeaderHeight, &Button, &FrozenTeeHudSettings);
				Ui()->DoLabel(&Button, Localize("Frozen Tee Hud"), FontSize, TEXTALIGN_MC);
				{
					// ***** FROZEN TEE HUD ***** //

					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHud, ("Enable Frozen Tee Display"), &g_Config.m_ClShowFrozenHud, &FrozenTeeHudSettings, LineMargin);
					FrozenTeeHudSettings.HSplitTop(5.0f, &Button, &FrozenTeeHudSettings);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFrozenHudSkins, ("Use Skins Instead of Ninja Tees"), &g_Config.m_ClShowFrozenHudSkins, &FrozenTeeHudSettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFrozenHudTeamOnly, ("Only Show After Joining a Team"), &g_Config.m_ClFrozenHudTeamOnly, &FrozenTeeHudSettings, LineMargin);
					{
						FrozenTeeHudSettings.HSplitTop(20.0f, &Button, &FrozenTeeHudSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s: %i", "Max Rows", g_Config.m_ClFrozenMaxRows);
						Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
						g_Config.m_ClFrozenMaxRows = (int)(Ui()->DoScrollbarH(&g_Config.m_ClFrozenMaxRows, &Button, (g_Config.m_ClFrozenMaxRows - 1) / 5.0f) * 5.0f) + 1;
					}
					{
						FrozenTeeHudSettings.HSplitTop(20.0f, &Button, &FrozenTeeHudSettings);
						Button.VSplitLeft(140.0f, &Label, &Button);
						char aBuf[64];
						str_format(aBuf, sizeof(aBuf), "%s: %i", "Tee Size", g_Config.m_ClFrozenHudTeeSize);
						Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
						g_Config.m_ClFrozenHudTeeSize = (int)(Ui()->DoScrollbarH(&g_Config.m_ClFrozenHudTeeSize, &Button, (g_Config.m_ClFrozenHudTeeSize - 8) / 19.0f) * 19.0f) + 8;
					}

					{
						CUIRect CheckBoxRect, CheckBoxRect2;
						FrozenTeeHudSettings.HSplitTop(LineMargin, &CheckBoxRect, &FrozenTeeHudSettings);
						CheckBoxRect.VSplitMid(&CheckBoxRect, &CheckBoxRect2);
						if(DoButton_CheckBox(&g_Config.m_ClShowFrozenText, Localize("Tees Left Alive Text"), g_Config.m_ClShowFrozenText >= 1, &CheckBoxRect))
						{
							g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText >= 1 ? 0 : 1;
						}
						if(g_Config.m_ClShowFrozenText)
						{
							static int s_CountFrozenText = 0;
							if(DoButton_CheckBox(&s_CountFrozenText, Localize("Count Frozen Tees"), g_Config.m_ClShowFrozenText == 2, &CheckBoxRect2))
							{
								g_Config.m_ClShowFrozenText = g_Config.m_ClShowFrozenText != 2 ? 2 : 1;
							}
						}
					}
				}
			}
		}

		{
			FastInputSettings.HSplitTop(Margin, nullptr, &FastInputSettings);
			if(g_Config.m_ClFastInput)
				FastInputSettings.HSplitTop(125.0f, &FastInputSettings, 0);
			else
			FastInputSettings.HSplitTop(95.0f, &FastInputSettings, 0);
			if(s_ScrollRegion.AddRect(FastInputSettings))
			{
				FastInputSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				FastInputSettings.VMargin(Margin, &FastInputSettings);

				FastInputSettings.HSplitTop(HeaderHeight, &Button, &FastInputSettings);
				Ui()->DoLabel(&Button, Localize("Input"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInput, ("Fast Inputs (-20ms visual input delay)"), &g_Config.m_ClFastInput, &FastInputSettings, LineMargin);
					if(g_Config.m_ClFastInput)
					{
						FastInputSettings.HSplitTop(7.0f, 0x0, &FastInputSettings);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFastInputOthers, ("Extra tick other tees (increases other tees visual latency, \nmakes dragging slightly easier when using fast input)"), &g_Config.m_ClFastInputOthers, &FastInputSettings, LineMargin);
						FastInputSettings.HSplitTop(5.0f, 0x0, &FastInputSettings);
					}
					
					FastInputSettings.HSplitTop(10.0f, 0x0, &FastInputSettings);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClImproveMousePrecision, Localize("Improve mouse precision by scaling sent max distance to 1000"), &g_Config.m_ClImproveMousePrecision, &FastInputSettings, LineMargin);
				}
			}
		}
		// right side
		{
			PlayerIndicatorSettings.VMargin(5.0f, &PlayerIndicatorSettings);
			PlayerIndicatorSettings.HSplitTop(365.0f, &PlayerIndicatorSettings, &LatencySettings);
			if(s_ScrollRegion.AddRect(PlayerIndicatorSettings))
			{
				PlayerIndicatorSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				PlayerIndicatorSettings.VMargin(Margin, &PlayerIndicatorSettings);

				PlayerIndicatorSettings.HSplitTop(HeaderHeight, &Button, &PlayerIndicatorSettings);
				Ui()->DoLabel(&Button, Localize("Player Indicator"), FontSize, TEXTALIGN_MC);
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, ("Enable Player Indicators"), &g_Config.m_ClPlayerIndicator, &PlayerIndicatorSettings, LineSize);
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorHideOnScreen, Localize("Hide indicator for tees on your screen"), &g_Config.m_ClIndicatorHideOnScreen, &PlayerIndicatorSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicatorFreeze, Localize("Show only freeze Players"), &g_Config.m_ClPlayerIndicatorFreeze, &PlayerIndicatorSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTeamOnly, Localize("Only show after joining a team"), &g_Config.m_ClIndicatorTeamOnly, &PlayerIndicatorSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTees, Localize("Render tiny tees instead of circles"), &g_Config.m_ClIndicatorTees, &PlayerIndicatorSettings, LineSize);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicator, Localize("Use warlist groups for indicator"), &g_Config.m_ClWarListIndicator, &PlayerIndicatorSettings, LineSize);
		
					PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorRadius, &g_Config.m_ClIndicatorRadius, &Button, Localize("Indicator size"), 1, 16);
					PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOpacity, &g_Config.m_ClIndicatorOpacity, &Button, Localize("Indicator opacity"), 0, 100);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorVariableDistance, Localize("Change indicator offset based on distance to other tees"), &g_Config.m_ClIndicatorVariableDistance, &PlayerIndicatorSettings, LineSize);

					if(g_Config.m_ClIndicatorVariableDistance)
					{
						PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator min offset"), 16, 200);
						PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffsetMax, &g_Config.m_ClIndicatorOffsetMax, &Button, Localize("Indicator max offset"), 16, 200);
						PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorMaxDistance, &g_Config.m_ClIndicatorMaxDistance, &Button, Localize("Indicator max distance"), 500, 7000);
					}
					else
					{
						PlayerIndicatorSettings.HSplitTop(LineSize, &Button, &PlayerIndicatorSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClIndicatorOffset, &g_Config.m_ClIndicatorOffset, &Button, Localize("Indicator offset"), 16, 200);
						PlayerIndicatorSettings.HSplitTop(LineSize * 2, nullptr, &PlayerIndicatorSettings);
					}
					if(g_Config.m_ClWarListIndicator)
					{
						char aBuf[128];
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorAll, Localize("Show all warlist groups"), &g_Config.m_ClWarListIndicatorAll, &PlayerIndicatorSettings, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(1)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorEnemy, aBuf, &g_Config.m_ClWarListIndicatorEnemy, &PlayerIndicatorSettings, LineSize);
						str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(2)->m_aWarName);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListIndicatorTeam, aBuf, &g_Config.m_ClWarListIndicatorTeam, &PlayerIndicatorSettings, LineSize);
					}
					else
					{
						static CButtonContainer IndicatorAliveColorID, IndicatorDeadColorID, IndicatorSavedColorID;
						DoLine_ColorPicker(&IndicatorAliveColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, Localize("Indicator alive color"), &g_Config.m_ClIndicatorAlive, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&IndicatorDeadColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, Localize("Indicator in freeze color"), &g_Config.m_ClIndicatorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
						DoLine_ColorPicker(&IndicatorSavedColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, Localize("Indicator safe color"), &g_Config.m_ClIndicatorSaved, ColorRGBA(0.0f, 0.0f, 0.0f), false);
					}
				}
			}
		}


	
		{
			LatencySettings.HSplitTop(Margin, nullptr, &LatencySettings);
			LatencySettings.HSplitTop(190.0f, &LatencySettings, &GhostSettings);
			if(s_ScrollRegion.AddRect(LatencySettings))
			{
				LatencySettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				LatencySettings.VMargin(Margin, &LatencySettings);

				LatencySettings.HSplitTop(HeaderHeight, &Button, &LatencySettings);
				Ui()->DoLabel(&Button, Localize("Anti Latency Tools"), FontSize, TEXTALIGN_MC);
				{
					LatencySettings.HSplitTop(20.f, &Button, &LatencySettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, Localize("Prediction Margin"), 10, 25, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRemoveAnti, Localize("Remove prediction & antiping in freeze"), &g_Config.m_ClRemoveAnti, &LatencySettings, LineMargin);
					if(g_Config.m_ClRemoveAnti)
					{
						if(g_Config.m_ClUnfreezeLagDelayTicks < g_Config.m_ClUnfreezeLagTicks)
							g_Config.m_ClUnfreezeLagDelayTicks = g_Config.m_ClUnfreezeLagTicks;
						LatencySettings.HSplitTop(LineMargin, &Button, &LatencySettings);
						DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagTicks, &g_Config.m_ClUnfreezeLagTicks, &Button, Localize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
						LatencySettings.HSplitTop(LineMargin, &Button, &LatencySettings);
						DoSliderWithScaledValue(&g_Config.m_ClUnfreezeLagDelayTicks, &g_Config.m_ClUnfreezeLagDelayTicks, &Button, Localize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
					}
					else
						LatencySettings.HSplitTop(LineMargin * 2, nullptr, &LatencySettings);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClUnpredOthersInFreeze, Localize("Dont predict other players if you are frozen"), &g_Config.m_ClUnpredOthersInFreeze, &LatencySettings, LineMargin);
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPredMarginInFreeze, Localize("Adjust your prediction margin while frozen"), &g_Config.m_ClPredMarginInFreeze, &LatencySettings, LineMargin);
					LatencySettings.HSplitTop(LineMargin, &Button, &LatencySettings);
					if(g_Config.m_ClPredMarginInFreeze)
						Ui()->DoScrollbarOption(&g_Config.m_ClPredMarginInFreezeAmount, &g_Config.m_ClPredMarginInFreezeAmount, &Button, Localize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
		
				}
			}
		}

		{
			GhostSettings.HSplitTop(Margin, nullptr, &GhostSettings);
			GhostSettings.HSplitTop(160.0f, &GhostSettings, 0);
			if(s_ScrollRegion.AddRect(GhostSettings))
			{
				GhostSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				GhostSettings.VMargin(Margin, &GhostSettings);

				GhostSettings.HSplitTop(HeaderHeight, &Button, &GhostSettings);
				Ui()->DoLabel(&Button, Localize("Ghost Tools"), FontSize, TEXTALIGN_MC);
				{
					{
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersGhosts, Localize("Show unpredicted ghosts for other players"), &g_Config.m_ClShowOthersGhosts, &GhostSettings, LineSize);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSwapGhosts, Localize("Swap ghosts and normal players"), &g_Config.m_ClSwapGhosts, &GhostSettings, LineSize);
						GhostSettings.HSplitTop(LineSize, &Button, &GhostSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClPredGhostsAlpha, &g_Config.m_ClPredGhostsAlpha, &Button, Localize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
						GhostSettings.HSplitTop(LineSize, &Button, &GhostSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClUnpredGhostsAlpha, &g_Config.m_ClUnpredGhostsAlpha, &Button, Localize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClHideFrozenGhosts, Localize("Hide ghosts of frozen players"), &g_Config.m_ClHideFrozenGhosts, &GhostSettings, LineSize);
						DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderGhostAsCircle, Localize("Render ghosts as circles"), &g_Config.m_ClRenderGhostAsCircle, &GhostSettings, LineSize);

					}
				}
			}
		}

		s_ScrollRegion.End();
	}

	if(s_CurTab == AIODOB_TAB_BINDWHEEL)
	{
		RenderSettingsBindwheel(MainView);
	}

	if(s_CurTab == AIODOB_TAB_WARLIST)
	{
		RenderSettingsWarList(MainView);
	}
}

void CMenus::RenderSettingsBindwheel(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	const float MarginBetweenSections = 30.0f;

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
	if(MouseDist < Radius && MouseDist > Radius * 0.25f)
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
			CBindWheel::SBind BindA = GameClient()->m_Bindwheel.m_vBinds[s_SelectedBindIndex];
			CBindWheel::SBind BindB = GameClient()->m_Bindwheel.m_vBinds[HoveringIndex];
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
		float FontSize = 12.0f;
		if(i == s_SelectedBindIndex)
		{
			FontSize = 20.0f;
			TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
		}
		else if(i == HoveringIndex)
			FontSize = 14.0f;

		const CBindWheel::SBind Bind = GameClient()->m_Bindwheel.m_vBinds[i];
		const float Angle = Theta * i;
		vec2 TextPos = direction(Angle);
		TextPos *= Radius * 0.75f;

		float Width = TextRender()->TextWidth(FontSize, Bind.m_aName);
		TextPos += Pos;
		TextPos.x -= Width / 2.0f;
		TextRender()->Text(TextPos.x, TextPos.y, FontSize, Bind.m_aName);
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
		CBindWheel::SBind TempBind;
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
		CBindWheel::SBind TempBind;
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
			const char *pBind = m_pClient->m_Binds.Get(KeyId, Mod);
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
			m_pClient->m_Binds.Bind(OldId, "", false, OldModifierCombination);
		if(NewId != 0)
			m_pClient->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
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
	if(DoButton_FontIcon(&s_ReverseEntries, FONT_ICON_CHEVRON_DOWN, 0, &Button, IGraphics::CORNER_ALL))
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

		if(DoButton_FontIcon(&s_vDeleteButtons[i], FONT_ICON_TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
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
			RenderDevSkin(EntryTypeRect.Center(), 35.0f, "defualt", "default", false, 0, 0, 0, false);
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

	Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && m_pClient->m_GameConsole.IsClosed());

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
	if(DoButton_CheckBox_Common(&s_NameRadio, "Name", s_IsName ? "X" : "", &ButtonL))
	{
		s_IsName = 1;
		s_IsClan = 0;
	}
	if(DoButton_CheckBox_Common(&s_ClanRadio, "Clan", s_IsClan ? "X" : "", &ButtonR))
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

	Column2.HSplitBottom(150.0f, nullptr, &Column2);

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, Localize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListAllowDuplicates, Localize("Allow Duplicate Entries"), &g_Config.m_ClWarListAllowDuplicates, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, Localize("Enable warlist"), &g_Config.m_ClWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListChat, Localize("Colors in chat"), &g_Config.m_ClWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarlistPrefixes, Localize("Warlist Prefixes"), &g_Config.m_ClWarlistPrefixes, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListScoreboard, Localize("Colors in scoreboard"), &g_Config.m_ClWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarListShowClan, Localize("Show clan if war"), &g_Config.m_ClWarListShowClan, &Column2, LineSize);

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

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apPlayerInfos[i])
			continue;

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[i].m_RenderInfo;

		const CListboxItem Item = s_PlayerListBox.DoNextItem(&s_vPlayerItemIds[i], false);
		if(!Item.m_Visible)
			continue;

		CUIRect PlayerRect, TeeRect, NameRect, ClanRect;
		Item.m_Rect.Margin(0.0f, &PlayerRect);
		PlayerRect.VSplitLeft(25.0f, &TeeRect, &PlayerRect);
		// RenderDevSkin(TeeRect.Center(), 35.0f, TeeInfo., "default", false, 0, 0, 0, false);

		PlayerRect.VSplitMid(&NameRect, &ClanRect, 0);
		PlayerRect = NameRect;
		PlayerRect.x = TeeRect.x;
		PlayerRect.w += TeeRect.w;
		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_NameColor);
		ColorRGBA NameButtonColor = Ui()->CheckActiveItem(&s_vNameButtons[i]) ? ColorRGBA(1, 1, 1, 0.75f) :
											(Ui()->HotItem() == &s_vNameButtons[i] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		PlayerRect.Draw(NameButtonColor, IGraphics::CORNER_L, 5.0f);
		Ui()->DoLabel(&NameRect, GameClient()->m_aClients[i].m_aName, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vNameButtons[i], false, &PlayerRect))
		{
			s_IsName = 1;
			s_IsClan = 0;
			str_copy(s_aEntryName, GameClient()->m_aClients[i].m_aName);
		}

		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_ClanColor);
		ColorRGBA ClanButtonColor = Ui()->CheckActiveItem(&s_vClanButtons[i]) ? ColorRGBA(1, 1, 1, 0.75f) :
											(Ui()->HotItem() == &s_vClanButtons[i] ? ColorRGBA(1, 1, 1, 0.33f) : ColorRGBA(1, 1, 1, 0.0f));
		ClanRect.Draw(ClanButtonColor, IGraphics::CORNER_R, 5.0f);
		Ui()->DoLabel(&ClanRect, GameClient()->m_aClients[i].m_aClan, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vClanButtons[i], false, &ClanRect))
		{
			s_IsName = 0;
			s_IsClan = 1;
			str_copy(s_aEntryClan, GameClient()->m_aClients[i].m_aClan);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		TeeInfo.m_Size = 25.0f;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.5f));
	}
	s_PlayerListBox.DoEnd();
}


void CMenus::ResetSettingsCustomization()
{
	g_Config.m_ClFreezeBarWidth = 0;
	g_Config.m_ClFreezeBarHeight = 0;
	g_Config.m_ClFreezeBarX = 0;
	g_Config.m_ClFreezeBarY = 0;
}

void CMenus::ResetSettingsSprites()
{
	g_Config.m_ClSnowflakeGravity = 0;
	g_Config.m_ClSnowflakeLifeSpan = 0;
	g_Config.m_ClSnowflakeCollision = 0;
	g_Config.m_ClSnowflakeX = 0;
	g_Config.m_ClSnowflakeY = 0;

	g_Config.m_ClDjSize = 0;
	g_Config.m_ClDjGravity = 0;
	g_Config.m_ClDjLifespan = 0;
	g_Config.m_ClDjRotSpeed = 0;
	g_Config.m_ClDjSprite = 0;
	g_Config.m_ClDjPosX = 0;
	g_Config.m_ClDjPosY = 0;

	g_Config.m_ClFreezeBarWidth = 0;
	g_Config.m_ClFreezeBarHeight = 0;
	g_Config.m_ClFreezeBarX = 0;
	g_Config.m_ClFreezeBarY = 0;

	g_Config.m_ClGunSprite = 0;
	g_Config.m_ClSpriteGunFire = 0;
	g_Config.m_ClNoSpriteGunFire = 0;

	g_Config.m_ClHammerSprite = 0;
	g_Config.m_ClShotgunSprite = 0;
	g_Config.m_ClLaserSprite = 0;
	g_Config.m_ClGrenadeSprite = 0;
	g_Config.m_ClNinjaSprite = 0;

	g_Config.m_ClGunCursorSprite = 0;
	g_Config.m_ClHammerCursorSprite = 0;
	g_Config.m_ClShotgunCursorSprite = 0;
	g_Config.m_ClLaserCursorSprite = 0;
	g_Config.m_ClGrenadeCursorSprite = 0;
	g_Config.m_ClNinjaCursorSprite = 0;

	g_Config.m_ClHookChainSprite = 0;
	g_Config.m_ClHookHeadSprite = 0;

	GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
}

void CMenus::RenderSettingsProfiles(CUIRect MainView)
{
	CUIRect Label, LabelMid, Section, LabelRight;
	static int SelectedProfile = -1;

	const float LineMargin = 22.0f;
	char *pSkinName = g_Config.m_ClPlayerSkin;
	int *pUseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
	unsigned *pColorBody = &g_Config.m_ClPlayerColorBody;
	unsigned *pColorFeet = &g_Config.m_ClPlayerColorFeet;
	int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;

	if(m_Dummy)
	{
		pSkinName = g_Config.m_ClDummySkin;
		pUseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		pColorBody = &g_Config.m_ClDummyColorBody;
		pColorFeet = &g_Config.m_ClDummyColorFeet;
	}

	// skin info
	CTeeRenderInfo OwnSkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Find(pSkinName);
	OwnSkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	OwnSkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	OwnSkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	OwnSkinInfo.m_CustomColoredSkin = *pUseCustomColor;
	if(*pUseCustomColor)
	{
		OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(*pColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT));
		OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(*pColorFeet).UnclampLighting(ColorHSLA::DARKEST_LGT));
	}
	else
	{
		OwnSkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		OwnSkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	OwnSkinInfo.m_Size = 50.0f;

	//======YOUR PROFILE======
	MainView.HSplitTop(10.0f, &Label, &MainView);
	char aTempBuf[256];
	str_format(aTempBuf, sizeof(aTempBuf), "%s:", Localize("Your profile"));
	Ui()->DoLabel(&Label, aTempBuf, 14.0f, TEXTALIGN_LEFT);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(250.0f, &Label, &LabelMid);
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
	vec2 TeeRenderPos(Label.x + 20.0f, Label.y + Label.h / 2.0f + OffsetToMid.y);
	int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, Emote, vec2(1, 0), TeeRenderPos);

	char aName[64];
	char aClan[64];
	str_format(aName, sizeof(aName), ("%s"), m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
	str_format(aClan, sizeof(aClan), ("%s"), m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);

	CUIRect FlagRect;
	Label.VSplitLeft(90.0, &FlagRect, &Label);
	Label.HMargin(-5.0f, &Label);
	Label.HSplitTop(25.0f, &Section, &Label);

	str_format(aTempBuf, sizeof(aTempBuf), ("Name: %s"), aName);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	Label.HSplitTop(20.0f, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), ("Clan: %s"), aClan);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	Label.HSplitTop(20.0f, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), ("Skin: %s"), pSkinName);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	FlagRect.VSplitRight(50, 0, &FlagRect);
	FlagRect.HSplitBottom(25, 0, &FlagRect);
	FlagRect.y -= 10.0f;
	ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_pClient->m_CountryFlags.Render(m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

	bool doSkin = g_Config.m_ClApplyProfileSkin;
	bool doColors = g_Config.m_ClApplyProfileColors;
	bool doEmote = g_Config.m_ClApplyProfileEmote;
	bool doName = g_Config.m_ClApplyProfileName;
	bool doClan = g_Config.m_ClApplyProfileClan;
	bool doFlag = g_Config.m_ClApplyProfileFlag;

	//======AFTER LOAD======
	if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
	{
		CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile];
		MainView.HSplitTop(20.0f, 0, &MainView);
		MainView.HSplitTop(10.0f, &Label, &MainView);
		str_format(aTempBuf, sizeof(aTempBuf), "%s:", ("After Load"));
		Ui()->DoLabel(&Label, aTempBuf, 14.0f, TEXTALIGN_LEFT);

		MainView.HSplitTop(50.0f, &Label, &MainView);
		Label.VSplitLeft(250.0f, &Label, 0);

		if(doSkin && strlen(LoadProfile.SkinName) != 0)
		{
			const CSkin *pLoadSkin = m_pClient->m_Skins.Find(LoadProfile.SkinName);
			OwnSkinInfo.m_OriginalRenderSkin = pLoadSkin->m_OriginalSkin;
			OwnSkinInfo.m_ColorableRenderSkin = pLoadSkin->m_ColorableSkin;
			OwnSkinInfo.m_SkinMetrics = pLoadSkin->m_Metrics;
		}
		if(*pUseCustomColor && doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
		{
			OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		}

		RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
		TeeRenderPos = vec2(Label.x + 20.0f, Label.y + Label.h / 2.0f + OffsetToMid.y);
		int LoadEmote = Emote;
		if(doEmote && LoadProfile.Emote != -1)
			LoadEmote = LoadProfile.Emote;
		RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, LoadEmote, vec2(1, 0), TeeRenderPos);

		if(doName && strlen(LoadProfile.Name) != 0)
			str_format(aName, sizeof(aName), ("%s"), LoadProfile.Name);
		if(doClan && strlen(LoadProfile.Clan) != 0)
			str_format(aClan, sizeof(aClan), ("%s"), LoadProfile.Clan);

		Label.VSplitLeft(90.0, &FlagRect, &Label);
		Label.HMargin(-5.0f, &Label);
		Label.HSplitTop(25.0f, &Section, &Label);

		str_format(aTempBuf, sizeof(aTempBuf), ("Name: %s"), aName);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		Label.HSplitTop(20.0f, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), ("Clan: %s"), aClan);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		Label.HSplitTop(20.0f, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), ("Skin: %s"), (doSkin && strlen(LoadProfile.SkinName) != 0) ? LoadProfile.SkinName : pSkinName);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		FlagRect.VSplitRight(50, 0, &FlagRect);
		FlagRect.HSplitBottom(25, 0, &FlagRect);
		FlagRect.y -= 10.0f;
		int RenderFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
		if(doFlag && LoadProfile.CountryFlag != -2)
			RenderFlag = LoadProfile.CountryFlag;
		m_pClient->m_CountryFlags.Render(RenderFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		str_format(aName, sizeof(aName), ("%s"), m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
		str_format(aClan, sizeof(aClan), ("%s"), m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);
	}
	else
	{
		MainView.HSplitTop(80.0f, 0, &MainView);
	}

	//===BUTTONS AND CHECK BOX===
	CUIRect DummyCheck, CustomCheck;
	MainView.HSplitTop(30, &DummyCheck, 0);
	DummyCheck.HSplitTop(13, 0, &DummyCheck);

	DummyCheck.VSplitLeft(100, &DummyCheck, &CustomCheck);
	CustomCheck.VSplitLeft(150, &CustomCheck, 0);

	DoButton_CheckBoxAutoVMarginAndSet(&m_Dummy, Localize("Dummy"), (int *)&m_Dummy, &DummyCheck, LineMargin);

	static int s_CustomColorID = 0;
	CustomCheck.HSplitTop(LineMargin, &CustomCheck, 0);

	if(DoButton_CheckBox(&s_CustomColorID, Localize("Custom colors"), *pUseCustomColor, &CustomCheck))
	{
		*pUseCustomColor = *pUseCustomColor ? 0 : 1;
		SetNeedSendInfo();
	}

	LabelMid.VSplitLeft(20.0f, 0, &LabelMid);
	LabelMid.VSplitLeft(160.0f, &LabelMid, &LabelRight);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileSkin, ("Save/Load Skin"), &g_Config.m_ClApplyProfileSkin, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileColors, ("Save/Load Colors"), &g_Config.m_ClApplyProfileColors, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileEmote, ("Save/Load Emote"), &g_Config.m_ClApplyProfileEmote, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileName, ("Save/Load Name"), &g_Config.m_ClApplyProfileName, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileClan, ("Save/Load Clan"), &g_Config.m_ClApplyProfileClan, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileFlag, ("Save/Load Flag"), &g_Config.m_ClApplyProfileFlag, &LabelMid, LineMargin);

	CUIRect Button;
	LabelRight.VSplitLeft(150.0f, &LabelRight, 0);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_LoadButton;

	if(DoButton_Menu(&s_LoadButton, Localize("Load"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
	{
		if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
		{
			CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile];
			if(!m_Dummy)
			{
				if(doSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClPlayerSkin, LoadProfile.SkinName, sizeof(g_Config.m_ClPlayerSkin));
				if(doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClPlayerColorBody = LoadProfile.BodyColor;
					g_Config.m_ClPlayerColorFeet = LoadProfile.FeetColor;
				}
				if(doEmote && LoadProfile.Emote != -1)
					g_Config.m_ClPlayerDefaultEyes = LoadProfile.Emote;
				if(doName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_PlayerName, LoadProfile.Name, sizeof(g_Config.m_PlayerName));
				if(doClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_PlayerClan, LoadProfile.Clan, sizeof(g_Config.m_PlayerClan));
				if(doFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_PlayerCountry = LoadProfile.CountryFlag;
			}
			else
			{
				if(doSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClDummySkin, LoadProfile.SkinName, sizeof(g_Config.m_ClDummySkin));
				if(doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClDummyColorBody = LoadProfile.BodyColor;
					g_Config.m_ClDummyColorFeet = LoadProfile.FeetColor;
				}
				if(doEmote && LoadProfile.Emote != -1)
					g_Config.m_ClDummyDefaultEyes = LoadProfile.Emote;
				if(doName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_ClDummyName, LoadProfile.Name, sizeof(g_Config.m_ClDummyName));
				if(doClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_ClDummyClan, LoadProfile.Clan, sizeof(g_Config.m_ClDummyClan));
				if(doFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_ClDummyCountry = LoadProfile.CountryFlag;
			}
		}
		SetNeedSendInfo();
	}
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_SaveButton;
	if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
	{
		GameClient()->m_SkinProfiles.AddProfile(
			doColors ? *pColorBody : -1,
			doColors ? *pColorFeet : -1,
			doFlag ? CurrentFlag : -2,
			doEmote ? Emote : -1,
			doSkin ? pSkinName : "",
			doName ? aName : "",
			doClan ? aClan : "");
		GameClient()->m_SkinProfiles.SaveProfiles();
	}
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	static int s_AllowDelete;
	DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, ("Enable Deleting"), &s_AllowDelete, &LabelRight, LineMargin);
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	if(s_AllowDelete)
	{
		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
		{
			if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + SelectedProfile);
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
		LabelRight.HSplitTop(5.0f, 0, &LabelRight);

		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_OverrideButton;
		if(DoButton_Menu(&s_OverrideButton, Localize("Override"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
		{
			if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile] = CProfile(
					doColors ? *pColorBody : -1,
					doColors ? *pColorFeet : -1,
					doFlag ? CurrentFlag : -2,
					doEmote ? Emote : -1,
					doSkin ? pSkinName : "",
					doName ? aName : "",
					doClan ? aClan : "");
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
	}

	//---RENDER THE SELECTOR---
	CUIRect SelectorRect;
	MainView.HSplitTop(50, 0, &SelectorRect);
	SelectorRect.HSplitBottom(15.0, &SelectorRect, 0);
	std::vector<CProfile> *pProfileList = &GameClient()->m_SkinProfiles.m_Profiles;

	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, pProfileList->size(), 4, 3, SelectedProfile, &SelectorRect, true);

	static bool s_Indexs[1024];

	for(size_t i = 0; i < pProfileList->size(); ++i)
	{
		CProfile CurrentProfile = GameClient()->m_SkinProfiles.m_Profiles[i];

		char RenderSkin[24];
		if(strlen(CurrentProfile.SkinName) == 0)
			str_copy(RenderSkin, pSkinName, sizeof(RenderSkin));
		else
			str_copy(RenderSkin, CurrentProfile.SkinName, sizeof(RenderSkin));

		const CSkin *pSkinToBeDraw = m_pClient->m_Skins.Find(RenderSkin);

		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexs[i], SelectedProfile >= 0 && (size_t)SelectedProfile == i);

		if(!Item.m_Visible)
			continue;

		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			Info.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_CustomColoredSkin = 1;
			Info.m_OriginalRenderSkin = pSkinToBeDraw->m_OriginalSkin;
			Info.m_ColorableRenderSkin = pSkinToBeDraw->m_ColorableSkin;
			Info.m_SkinMetrics = pSkinToBeDraw->m_Metrics;
			Info.m_Size = 50.0f;
			if(CurrentProfile.BodyColor == -1 && CurrentProfile.FeetColor == -1)
			{
				Info.m_CustomColoredSkin = m_Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor;
				Info.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
				Info.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
			}

			RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &Info, OffsetToMid);

			int RenderEmote = CurrentProfile.Emote == -1 ? Emote : CurrentProfile.Emote;
			TeeRenderPos = vec2(Item.m_Rect.x + 30, Item.m_Rect.y + Item.m_Rect.h / 2 + OffsetToMid.y);

			Item.m_Rect.VSplitLeft(60.0f, 0, &Item.m_Rect);
			CUIRect PlayerRect, ClanRect, FeetColorSquare, BodyColorSquare;

			Item.m_Rect.VSplitLeft(60.0f, 0, &BodyColorSquare); // Delete this maybe

			Item.m_Rect.VSplitRight(60.0, &BodyColorSquare, &FlagRect);
			BodyColorSquare.x -= 11.0;
			BodyColorSquare.VSplitLeft(10, &BodyColorSquare, 0);
			BodyColorSquare.HSplitMid(&BodyColorSquare, &FeetColorSquare);
			BodyColorSquare.HSplitMid(0, &BodyColorSquare);
			FeetColorSquare.HSplitMid(&FeetColorSquare, 0);
			FlagRect.HSplitBottom(10.0, &FlagRect, 0);
			FlagRect.HSplitTop(10.0, 0, &FlagRect);

			Item.m_Rect.HSplitMid(&PlayerRect, &ClanRect);

			SLabelProperties Props;
			Props.m_MaxWidth = Item.m_Rect.w;
			if(CurrentProfile.CountryFlag != -2)
				m_pClient->m_CountryFlags.Render(CurrentProfile.CountryFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

			if(CurrentProfile.BodyColor != -1 && CurrentProfile.FeetColor != -1)
			{
				ColorRGBA BodyColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
				ColorRGBA FeetColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));

				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BodyColor.r, BodyColor.g, BodyColor.b, 1.0f);
				IGraphics::CQuadItem Quads[2];
				Quads[0] = IGraphics::CQuadItem(BodyColorSquare.x, BodyColorSquare.y, BodyColorSquare.w, BodyColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[0], 1);
				Graphics()->SetColor(FeetColor.r, FeetColor.g, FeetColor.b, 1.0f);
				Quads[1] = IGraphics::CQuadItem(FeetColorSquare.x, FeetColorSquare.y, FeetColorSquare.w, FeetColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[1], 1);
				Graphics()->QuadsEnd();
			}
			RenderTools()->RenderTee(pIdleState, &Info, RenderEmote, vec2(1.0f, 0.0f), TeeRenderPos);

			if(strlen(CurrentProfile.Name) == 0 && strlen(CurrentProfile.Clan) == 0)
			{
				PlayerRect = Item.m_Rect;
				Ui()->DoLabel(&PlayerRect, CurrentProfile.SkinName, 12.0f, TEXTALIGN_LEFT, Props);
			}
			else
			{
				Ui()->DoLabel(&PlayerRect, CurrentProfile.Name, 12.0f, TEXTALIGN_LEFT, Props);
				Item.m_Rect.HSplitTop(20.0f, 0, &Item.m_Rect);
				Props.m_MaxWidth = Item.m_Rect.w;
				Ui()->DoLabel(&ClanRect, CurrentProfile.Clan, 12.0f, TEXTALIGN_LEFT, Props);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(SelectedProfile != NewSelected)
	{
		SelectedProfile = NewSelected;
	}
	static CButtonContainer s_ProfilesFile;
	CUIRect FileButton;
	MainView.HSplitBottom(25.0, 0, &FileButton);
	FileButton.y += 15.0;
	FileButton.VSplitLeft(130.0, &FileButton, 0);
	if(DoButton_Menu(&s_ProfilesFile, Localize("Profiles file"), 0, &FileButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, PROFILES_FILE, aTempBuf, sizeof(aTempBuf));
		if(!open_file(aTempBuf))
		{
			dbg_msg("menus", "couldn't open file");
		}
	}
	CUIRect CopyRight;
	MainView.HSplitTop(-641.0f, 0, &CopyRight);
	CopyRight.VSplitLeft(820.0f, &CopyRight, &CopyRight);
	Ui()->DoLabel(&CopyRight, "© Tater", 14.0f, TEXTALIGN_ML);
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

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect);
}


void CMenus::RenderDevSkin(vec2 RenderPos, float Size, const char *pSkinName, const char *pBackupSkin, bool CustomColors, int FeetColor, int BodyColor, int Emote, bool Rainbow, ColorRGBA ColorFeet, ColorRGBA ColorBody)
{

	float DefTick = std::fmod(s_Time, 1.0f);

	CTeeRenderInfo SkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Find(pSkinName);
	if(str_comp(pSkin->GetName(), pSkinName) != 0)
		pSkin = m_pClient->m_Skins.Find(pBackupSkin);

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

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect);
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
		Value = clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
	{
		// clamp the value internally for the scrollbar
		Value = clamp(Value, Min, Max);
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
