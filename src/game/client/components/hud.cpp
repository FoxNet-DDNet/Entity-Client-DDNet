/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "hud.h"

#include "camera.h"
#include "controls.h"
#include "tclient/warlist.h"
#include "voting.h"

#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>
#include <base/time.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/video.h>
#include <engine/textrender.h>

#include <generated/client_data.h>
#include <generated/data_types.h>
#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/animstate.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/gamecore.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

CHud::CHud()
{
	m_FPSTextContainerIndex.Reset();
	m_DDRaceEffectsTextContainerIndex.Reset();
	m_PlayerAngleTextContainerIndex.Reset();
	m_PlayerCheckpointTextContainerIndex.Reset(); // EClient
	m_PlayerPrevAngle = -INFINITY;
	m_PlayerPrevCheckpoint = -1; // EClient

	for(int i = 0; i < 2; i++)
	{
		m_aPlayerSpeedTextContainers[i].Reset();
		m_aPlayerPrevSpeed[i] = -INFINITY;
		m_aPlayerPositionContainers[i].Reset();
		m_aPlayerPrevPosition[i] = -INFINITY;
	}

	// EClient
	m_Island.Reset();

	for(size_t i = 0; i < std::size(m_aPlayerInfoTextContainers); i++)
	{
		m_aPlayerInfoTextContainers[i].Reset();
	}
	m_PlayerInfoPrevPosition = vec2(-INFINITY, -INFINITY);
	m_PlayerInfoPrevSpeed = vec2(-INFINITY, -INFINITY);
}

void CHud::ResetHudContainers()
{
	for(auto &ScoreInfo : m_aScoreInfo)
	{
		TextRender()->DeleteTextContainer(ScoreInfo.m_OptionalNameTextContainerIndex);
		TextRender()->DeleteTextContainer(ScoreInfo.m_TextRankContainerIndex);
		TextRender()->DeleteTextContainer(ScoreInfo.m_TextScoreContainerIndex);
		Graphics()->DeleteQuadContainer(ScoreInfo.m_RoundRectQuadContainerIndex);

		ScoreInfo.Reset();
	}

	TextRender()->DeleteTextContainer(m_FPSTextContainerIndex);
	TextRender()->DeleteTextContainer(m_DDRaceEffectsTextContainerIndex);
	TextRender()->DeleteTextContainer(m_PlayerAngleTextContainerIndex);
	TextRender()->DeleteTextContainer(m_PlayerCheckpointTextContainerIndex); // EClient
	m_PlayerPrevAngle = -INFINITY;
	for(int i = 0; i < 2; i++)
	{
		TextRender()->DeleteTextContainer(m_aPlayerSpeedTextContainers[i]);
		m_aPlayerPrevSpeed[i] = -INFINITY;
		TextRender()->DeleteTextContainer(m_aPlayerPositionContainers[i]);
		m_aPlayerPrevPosition[i] = -INFINITY;
	}

	// EClient
	m_TextWidthScore10 = TextRender()->TextWidth(14.0f, "10", -1, -1.0f);
	m_TextWidthScore100 = TextRender()->TextWidth(14.0f, "100", -1, -1.0f);

	m_TextWidthFPS0 = TextRender()->TextWidth(12.0f, "0", -1, -1.0f);
	m_TextWidthFPS00 = TextRender()->TextWidth(12.0f, "00", -1, -1.0f);
	m_TextWidthFPS000 = TextRender()->TextWidth(12.0f, "000", -1, -1.0f);
	m_TextWidthFPS0000 = TextRender()->TextWidth(12.0f, "0000", -1, -1.0f);
	m_TextWidthFPS00000 = TextRender()->TextWidth(12.0f, "00000", -1, -1.0f);

	m_Island.Reset();

	for(size_t i = 0; i < std::size(m_aPlayerInfoTextContainers); i++)
	{
		TextRender()->DeleteTextContainer(m_aPlayerInfoTextContainers[i]);
	}
	m_PlayerInfoPrevPosition = vec2(-INFINITY, -INFINITY);
	m_PlayerInfoPrevSpeed = vec2(-INFINITY, -INFINITY);
}

void CHud::OnWindowResize()
{
	ResetHudContainers();

	GameTimerWidth(0.1f, 0);
}

void CHud::OnReset()
{
	m_TimeCpDiff = 0.0f;
	m_DDRaceTime = 0;
	m_FinishTimeLastReceivedTick = 0;
	m_TimeCpLastReceivedTick = 0;
	m_ShowFinishTime = false;
	m_aPlayerRecord[0] = -1.0f;
	m_aPlayerRecord[1] = -1.0f;
	m_aPlayerSpeed[0] = 0;
	m_aPlayerSpeed[1] = 0;
	m_aLastPlayerSpeedChange[0] = ESpeedChange::NONE;
	m_aLastPlayerSpeedChange[1] = ESpeedChange::NONE;
	m_LastSpectatorCountTick = 0;

	ResetHudContainers();
}

void CHud::OnInit()
{
	OnReset();

	Graphics()->SetColor(1.0, 1.0, 1.0, 1.0);

	m_HudQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	PrepareAmmoHealthAndArmorQuads();

	// all cursors for the different weapons
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteCursor, ScaleX, ScaleY);
		m_aCursorOffset[i] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	}

	// the flags
	m_FlagOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 8.f, 16.f);

	PreparePlayerStateQuads();

	Graphics()->QuadContainerUpload(m_HudQuadContainerIndex);
}

// EClient
bool CHud::PreviewActive() const
{
	// The editor being open is enough for stand in content. Switching elements on is what ctrl is
	// for, and that is handled where the config values are overridden.
	return GameClient()->m_HudEditor.IsActive();
}

// EClient: asked both by the box itself and by the spectator count that it pushes up. Those were
// two separate copies of the same condition, which is how the preview came to draw the box without
// anything moving out of its way.
bool CHud::HasDummyActionsBox() const
{
	return g_Config.m_ClShowhudDummyActions &&
	       !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) &&
	       (Client()->DummyConnected() || PreviewActive());
}

void CHud::OnConsoleInit()
{
	// The layout is not a registered component, so it needs its interfaces wired up by hand. All
	// components have had OnInterfacesInit called on them by the time this runs.
	m_HudLayout.OnInterfacesInit(GameClient());
	m_HudLayout.OnConsoleInit();
}

float CHud::GameTimerWidth(float Size, int Time)
{
	static float s_TextSize = Size;
	static float s_TextWidthM = TextRender()->TextWidth(s_TextSize, "00:00", -1, -1.0f);
	static float s_TextWidthH = TextRender()->TextWidth(s_TextSize, "00:00:00", -1, -1.0f);
	static float s_TextWidth0D = TextRender()->TextWidth(s_TextSize, "0d 00:00:00", -1, -1.0f);
	static float s_TextWidth00D = TextRender()->TextWidth(s_TextSize, "00d 00:00:00", -1, -1.0f);
	static float s_TextWidth000D = TextRender()->TextWidth(s_TextSize, "000d 00:00:00", -1, -1.0f);
	if(s_TextSize != Size)
	{
		s_TextSize = Size;
		s_TextWidthM = TextRender()->TextWidth(s_TextSize, "00:00", -1, -1.0f);
		s_TextWidthH = TextRender()->TextWidth(s_TextSize, "00:00:00", -1, -1.0f);
		s_TextWidth0D = TextRender()->TextWidth(s_TextSize, "0d 00:00:00", -1, -1.0f);
		s_TextWidth00D = TextRender()->TextWidth(s_TextSize, "00d 00:00:00", -1, -1.0f);
		s_TextWidth000D = TextRender()->TextWidth(s_TextSize, "000d 00:00:00", -1, -1.0f);
	}
	float w = Time >= 3600 * 24 * 100 ? s_TextWidth000D : (Time >= 3600 * 24 * 10 ? s_TextWidth00D : (Time >= 3600 * 24 ? s_TextWidth0D : (Time >= 3600 ? s_TextWidthH : s_TextWidthM)));
	return w;
}

int CHud::GameTimerTime()
{
	int Time = 0;
	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
	{
		if(GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit && (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			Time = GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit * 60 - ((Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed());

			if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
		{
			// The Warmup timer is negative in this case to make sure that incompatible clients will not see a warmup timer
			Time = (Client()->GameTick(g_Config.m_ClDummy) + GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer) / Client()->GameTickSpeed();
		}
		else
		{
			Time = (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed();
		}
	}
	return Time;
}

void CHud::RenderGameTimer(vec2 Pos, float Size, float ClipRight)
{
	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
	{
		char aBuf[32];
		int Time = GameTimerTime();
		str_time((int64_t)Time * 100, ETimeFormat::DAYS, aBuf, sizeof(aBuf));
		float w = GameTimerWidth(Size, Time);
		// last 60 sec red, last 10 sec blink
		if(GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			float Alpha = Time <= 10 && (2 * time() / time_freq()) % 2 ? 0.5f : 1.0f;
			TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
		}

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(Pos.x - w / 2, Pos.y));
		Cursor.m_FontSize = Size;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END;
		Cursor.m_LineWidth = ClipRight > 0.0f ? std::max(ClipRight - Cursor.m_X, 0.001f) : -1.0f;

		TextRender()->TextEx(&Cursor, aBuf);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		if(g_Config.m_ClShowHudTimerStartedFlag && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH) && (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME))
		{
			Cursor.m_FontSize = Size * 0.55f;
			Cursor.SetPosition(vec2(Pos.x + w * 0.5f, Pos.y + Size * 0.25f));
			Cursor.m_LineWidth = ClipRight > 0.0f ? std::max(ClipRight - Cursor.m_X, 0.001f) : -1.0f;
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			TextRender()->TextEx(&Cursor, FontIcon::FLAG_CHECKERED);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
	}
}

void CHud::RenderPauseNotification()
{
	if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED &&
		!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		const char *pText = Localize("Game paused");
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
		TextRender()->Text(150.0f * Graphics()->ScreenAspect() + -w / 2.0f, 50.0f, FontSize, pText, -1.0f);
	}
}

void CHud::RenderSuddenDeath()
{
	if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = m_Width / 2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
		TextRender()->Text(Half - w / 2, 2, FontSize, pText, -1.0f);
	}
}

void CHud::RenderScoreHud()
{
	// render small score hud
	if(!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
	{
		float StartY = 229.0f; // the height of this display is 56, so EndY is 285

		const float ScoreSingleBoxHeight = 18.0f;

		bool ForceScoreInfoInit = !m_aScoreInfo[0].m_Initialized || !m_aScoreInfo[1].m_Initialized;
		m_aScoreInfo[0].m_Initialized = m_aScoreInfo[1].m_Initialized = true;

		// EClient: both sides follow the layout. Upstream squares the right and rounds the left
		// because that is where the box sits by default, but either side can be dragged against an
		// edge. Asked about the side rather than the corners, so the two boxes round together.
		const int ScoreCorners =
			(m_HudLayout.SideBlocked(EHudElement::SCORE, EHudPushDirection::LEFT) ? 0 : IGraphics::CORNER_L) |
			(m_HudLayout.SideBlocked(EHudElement::SCORE, EHudPushDirection::RIGHT) ? 0 : IGraphics::CORNER_R);

		if(GameClient()->IsTeamPlay() && GameClient()->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][16];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam[TEAM_RED]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam[TEAM_BLUE]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreBlue);

			bool aRecreateTeamScore[2] = {str_comp(aScoreTeam[0], m_aScoreInfo[0].m_aScoreText) != 0, str_comp(aScoreTeam[1], m_aScoreInfo[1].m_aScoreText) != 0};

			const int aFlagCarrier[2] = {
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierRed,
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue};

			bool RecreateRect = ForceScoreInfoInit || ScoreCorners != m_ScoreCorners; // EClient
			for(int t = 0; t < 2; t++)
			{
				if(aRecreateTeamScore[t])
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE], -1, -1.0f);
					str_copy(m_aScoreInfo[t].m_aScoreText, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE]);
					RecreateRect = true;
				}
			}

			m_ScoreCorners = ScoreCorners; // EClient
			float ScoreWidthMax = std::max({m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth, m_TextWidthScore100});
			float Split = 3.0f;
			float ImageSize = (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) ? 16.0f : Split;
			// EClient: StartY is advanced by the loop below, so the rect is reported up front
			const float BoxWidth = ScoreWidthMax + ImageSize + 2 * Split;
			m_HudLayout.ReportNaturalRect(EHudElement::SCORE, vec2(m_Width - BoxWidth, StartY), vec2(BoxWidth, 56.0f));
			for(int t = 0; t < 2; t++)
			{
				// draw box
				if(RecreateRect)
				{
					Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == 0)
						Graphics()->SetColor(0.975f, 0.17f, 0.17f, 0.3f);
					else
						Graphics()->SetColor(0.17f, 0.46f, 0.975f, 0.3f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split, ScoreSingleBoxHeight, 5.0f, ScoreCorners); // EClient
				}
				Graphics()->TextureClear();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				// draw score
				if(aRecreateTeamScore[t])
				{
					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) / 2 - Split, StartY + t * 20 + (18.f - 14.f) / 2.f));
					Cursor.m_FontSize = 14.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, aScoreTeam[t]);
				}
				if(m_aScoreInfo[t].m_TextScoreContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, TColor, TOutlineColor);
				}

				if(GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
				{
					int BlinkTimer = (GameClient()->m_aFlagDropTick[t] != 0 &&
								 (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_aFlagDropTick[t]) / Client()->GameTickSpeed() >= 25) ?
								 10 :
								 20;
					if(aFlagCarrier[t] == FLAG_ATSTAND || (aFlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick(g_Config.m_ClDummy) / BlinkTimer) & 1)))
					{
						// draw flag
						Graphics()->TextureSet(t == 0 ? GameClient()->m_GameSkin.m_SpriteFlagRed : GameClient()->m_GameSkin.m_SpriteFlagBlue);
						Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
						Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_FlagOffset, m_Width - ScoreWidthMax - ImageSize, StartY + 1.0f + t * 20);
					}
					else if(aFlagCarrier[t] >= 0)
					{
						// draw name of the flag holder
						int Id = aFlagCarrier[t] % MAX_CLIENTS;
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0 || RecreateRect)
						{
							str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);

							float w = TextRender()->TextWidth(8.0f, pName, -1, -1.0f);

							CTextCursor Cursor;
							Cursor.SetPosition(vec2(std::min(m_Width - w - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split), StartY + (t + 1) * 20.0f - 2.0f));
							Cursor.m_FontSize = 8.0f;
							TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex.Valid())
						{
							ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
							ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, TColor, TOutlineColor);
						}

						// draw tee of the flag holder
						CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
						TeeInfo.m_Size = ScoreSingleBoxHeight;

						const CAnimState *pIdleState = CAnimState::GetIdle();
						vec2 OffsetToMid;
						CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
						vec2 TeeRenderPos(m_Width - ScoreWidthMax - TeeInfo.m_Size / 2 - Split, StartY + (t * 20) + ScoreSingleBoxHeight / 2.0f + OffsetToMid.y);

						RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
					}
				}
				StartY += 8.0f;
			}
		}
		else
		{
			int Local = -1;
			int aPos[2] = {1, 2};
			const CNetObj_PlayerInfo *apPlayerInfo[2] = {nullptr, nullptr};
			int i = 0;
			for(int t = 0; t < 2 && i < MAX_CLIENTS && GameClient()->m_Snap.m_apInfoByScore[i]; ++i)
			{
				if(GameClient()->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS)
				{
					apPlayerInfo[t] = GameClient()->m_Snap.m_apInfoByScore[i];
					if(apPlayerInfo[t]->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
						Local = t;
					++t;
				}
			}
			// search local player info if not a spectator, nor within top2 scores
			if(Local == -1 && GameClient()->m_Snap.m_pLocalInfo && GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
			{
				for(; i < MAX_CLIENTS && GameClient()->m_Snap.m_apInfoByScore[i]; ++i)
				{
					if(GameClient()->m_Snap.m_apInfoByScore[i]->m_Team != TEAM_SPECTATORS)
						++aPos[1];
					if(GameClient()->m_Snap.m_apInfoByScore[i]->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
					{
						apPlayerInfo[1] = GameClient()->m_Snap.m_apInfoByScore[i];
						Local = 1;
						break;
					}
				}
			}
			char aScore[2][16];
			for(int t = 0; t < 2; ++t)
			{
				if(apPlayerInfo[t])
				{
					if(Client()->IsSixup() && GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & protocol7::GAMEFLAG_RACE)
					{
						str_time(absolute(static_cast<int64_t>(apPlayerInfo[t]->m_Score)) / 10, ETimeFormat::MINS_CENTISECS, aScore[t], sizeof(aScore[t]));
					}
					else if(GameClient()->m_GameInfo.m_TimeScore)
					{
						const CGameClient::CClientData &ClientData = GameClient()->m_aClients[apPlayerInfo[t]->m_ClientId];
						if(GameClient()->m_ReceivedDDNetPlayerFinishTimes && ClientData.m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
						{
							const int64_t TimeMillis = static_cast<int64_t>(ClientData.m_FinishTimeSeconds) * 1000 + ClientData.m_FinishTimeMillis % 1000;
							str_time(TimeMillis / 10, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else if(apPlayerInfo[t]->m_Score != FinishTime::NOT_FINISHED_TIMESCORE)
						{
							str_time(absolute(static_cast<int64_t>(apPlayerInfo[t]->m_Score)) * 100, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else
						{
							aScore[t][0] = 0;
						}
					}
					else
					{
						str_format(aScore[t], sizeof(aScore[t]), "%d", apPlayerInfo[t]->m_Score);
					}
				}
				else
				{
					aScore[t][0] = 0;
				}
			}

			bool RecreateScores = str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0 || str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0 || m_LastLocalClientId != GameClient()->m_Snap.m_LocalClientId;
			m_LastLocalClientId = GameClient()->m_Snap.m_LocalClientId;

			bool RecreateRect = ForceScoreInfoInit || ScoreCorners != m_ScoreCorners; // EClient
			for(int t = 0; t < 2; t++)
			{
				if(RecreateScores)
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScore[t], -1, -1.0f);
					str_copy(m_aScoreInfo[t].m_aScoreText, aScore[t]);
					RecreateRect = true;
				}

				if(apPlayerInfo[t])
				{
					int Id = apPlayerInfo[t]->m_ClientId;
					if(Id >= 0 && Id < MAX_CLIENTS)
					{
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0)
							RecreateRect = true;
					}
				}
				else
				{
					if(m_aScoreInfo[t].m_aPlayerNameText[0] != 0)
						RecreateRect = true;
				}

				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(str_comp(aBuf, m_aScoreInfo[t].m_aRankText) != 0)
					RecreateRect = true;
			}

			m_ScoreCorners = ScoreCorners; // EClient
			float ScoreWidthMax = std::max({m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth, m_TextWidthScore10});
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;
			// EClient: StartY is advanced by the loop below, so the rect is reported up front
			const float BoxWidth = ScoreWidthMax + ImageSize + 2 * Split + PosSize;
			m_HudLayout.ReportNaturalRect(EHudElement::SCORE, vec2(m_Width - BoxWidth, StartY), vec2(BoxWidth, 56.0f));

			for(int t = 0; t < 2; t++)
			{
				// draw box
				if(RecreateRect)
				{
					Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == Local)
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
					else
						Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.25f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split + PosSize, ScoreSingleBoxHeight, 5.0f, ScoreCorners); // EClient
				}
				Graphics()->TextureClear();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				if(RecreateScores)
				{
					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) - Split, StartY + t * 20 + (18.f - 14.f) / 2.f));
					Cursor.m_FontSize = 14.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &Cursor, aScore[t]);
				}
				// draw score
				if(m_aScoreInfo[t].m_TextScoreContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, TColor, TOutlineColor);
				}

				if(apPlayerInfo[t])
				{
					// draw name
					int Id = apPlayerInfo[t]->m_ClientId;
					if(Id >= 0 && Id < MAX_CLIENTS)
					{
						const char *pName = GameClient()->m_aClients[Id].m_aName;
						if(RecreateRect)
						{
							str_copy(m_aScoreInfo[t].m_aPlayerNameText, pName);

							CTextCursor Cursor;
							Cursor.SetPosition(vec2(std::min(m_Width - TextRender()->TextWidth(8.0f, pName) - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize), StartY + (t + 1) * 20.0f - 2.0f));
							Cursor.m_FontSize = 8.0f;
							TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex.Valid())
						{
							ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
							ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, TColor, TOutlineColor);
						}

						// draw tee
						CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
						TeeInfo.m_Size = ScoreSingleBoxHeight;

						const CAnimState *pIdleState = CAnimState::GetIdle();
						vec2 OffsetToMid;
						CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
						vec2 TeeRenderPos(m_Width - ScoreWidthMax - TeeInfo.m_Size / 2 - Split, StartY + (t * 20) + ScoreSingleBoxHeight / 2.0f + OffsetToMid.y);

						RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
					}
				}
				else
				{
					m_aScoreInfo[t].m_aPlayerNameText[0] = 0;
				}

				// draw position
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(RecreateRect)
				{
					str_copy(m_aScoreInfo[t].m_aRankText, aBuf);

					CTextCursor Cursor;
					Cursor.SetPosition(vec2(m_Width - ScoreWidthMax - ImageSize - Split - PosSize, StartY + t * 20 + (18.f - 10.f) / 2.f));
					Cursor.m_FontSize = 10.0f;
					TextRender()->RecreateTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, &Cursor, aBuf);
				}
				if(m_aScoreInfo[t].m_TextRankContainerIndex.Valid())
				{
					ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
					ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, TColor, TOutlineColor);
				}

				StartY += 8.0f;
			}
		}
	}
}

void CHud::RenderWarmupTimer()
{
	if(GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0 ||
		(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME) != 0)
	{
		return;
	}

	const float FontSize = 20.0f;
	const char *pTitle = Localize("Warmup");
	const float TitleWidth = TextRender()->TextWidth(FontSize, pTitle);
	// EClient: from the top of the title to the bottom of the time below it
	m_HudLayout.ReportNaturalRect(EHudElement::WARMUP_TIMER,
		vec2(150.0f * Graphics()->ScreenAspect() - TitleWidth / 2.0f, 50.0f), vec2(TitleWidth, 45.0f));
	TextRender()->Text(150.0f * Graphics()->ScreenAspect() - TitleWidth / 2.0f, 50.0f, FontSize, pTitle);

	const int Seconds = GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer / Client()->GameTickSpeed();
	char aWarmupTime[16];
	float TextWidth;
	if(Seconds < 5)
	{
		str_format(aWarmupTime, sizeof(aWarmupTime), "%d.%d", Seconds, (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer * 10 / Client()->GameTickSpeed()) % 10);
		TextWidth = TextRender()->TextWidth(FontSize, "0.0"); // Calculate width with fixed string to avoid slight changes when using aWarmupTime
	}
	else
	{
		str_format(aWarmupTime, sizeof(aWarmupTime), "%d", Seconds);
		TextWidth = TextRender()->TextWidth(FontSize, aWarmupTime);
	}
	TextRender()->Text(150.0f * Graphics()->ScreenAspect() - TextWidth / 2.0f, 75.0f, FontSize, aWarmupTime);
}

// EClient: shared by the fps counter and the prediction time below it, which are separate
// elements but both hidden while a video is being recorded
int CHud::ShowFps() const
{
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		return 0;
#endif
	return g_Config.m_ClShowfps;
}

void CHud::RenderFps()
{
	char aBuf[16];

	if(ShowFps())
	{
		const int FramesPerSecond = round_to_int(1.0f / Client()->FrameTimeAverage());
		str_format(aBuf, sizeof(aBuf), "%d", FramesPerSecond);

		const float s_aTextWidth[5] = {m_TextWidthFPS0, m_TextWidthFPS00, m_TextWidthFPS000, m_TextWidthFPS0000, m_TextWidthFPS00000};

		int DigitIndex = GetDigitsIndex(FramesPerSecond, 4);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_Width - 10 - s_aTextWidth[DigitIndex], 5));
		Cursor.m_FontSize = 12.0f;
		m_FPSPos = vec2(m_Width - 10 - m_TextWidthFPS00000, 5);
		m_HudLayout.ReportNaturalRect(EHudElement::FPS, m_FPSPos, vec2(m_TextWidthFPS00000, 12.0f)); // EClient
		auto OldFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(OldFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
		if(m_FPSTextContainerIndex.Valid())
			TextRender()->RecreateTextContainerSoft(m_FPSTextContainerIndex, &Cursor, aBuf);
		else
			TextRender()->CreateTextContainer(m_FPSTextContainerIndex, &Cursor, "0");
		TextRender()->SetRenderFlags(OldFlags);
		if(m_FPSTextContainerIndex.Valid())
		{
			TextRender()->RenderTextContainer(m_FPSTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor());
		}
	}
	else
		m_FPSPos = vec2(0, 0);
}

void CHud::RenderPrediction()
{
	if(!g_Config.m_ClShowpred || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Client()->GetPredictionTime());

	// EClient: sitting under the fps counter is the push solver's job now, so this only says where
	// it goes when there is nothing above it
	const float y = 5.0f;
	const float TextWidth = TextRender()->TextWidth(12.0f, aBuf);

	// EClient: measured off a fixed placeholder so the box does not twitch with the digit count
	static float s_PlaceholderWidth = TextRender()->TextWidth(12.0f, "000");
	m_HudLayout.ReportNaturalRect(EHudElement::PREDICTION,
		vec2(m_Width - 10.0f - s_PlaceholderWidth, y), vec2(s_PlaceholderWidth, 12.0f));

	TextRender()->Text(m_Width - 10.0f - TextWidth, y, 12.0f, aBuf, -1.0f);
}
void CHud::RenderConnectionWarning()
{
	if(Client()->ConnectionProblems())
	{
		const char *pText = Localize("Connection Problems…");
		float w = TextRender()->TextWidth(24, pText, -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() - w / 2, 50, 24, pText, -1.0f);
	}
}

void CHud::RenderTeambalanceWarning()
{
	// render prompt about team-balance
	bool Flash = time() / (time_freq() / 2) % 2 == 0;
	if(GameClient()->IsTeamPlay())
	{
		int TeamDiff = GameClient()->m_Snap.m_aTeamSize[TEAM_RED] - GameClient()->m_Snap.m_aTeamSize[TEAM_BLUE];
		if(g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			if(Flash)
				TextRender()->TextColor(1, 1, 0.5f, 1);
			else
				TextRender()->TextColor(0.7f, 0.7f, 0.2f, 1.0f);
			TextRender()->Text(5, 50, 6, pText, -1.0f);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
}

void CHud::RenderCursor()
{
	int CurWeapon = 0;
	vec2 TargetPos;
	float Alpha = 1.0f;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	CScreenRect ScreenRect = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), 1.0f);
	Graphics()->MapScreen(ScreenRect);

	// EClient: a practice tee is aimed with the same cursor, whether or not the server still has a
	// character for the tee it stands in for
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && (GameClient()->m_Snap.m_pLocalCharacter || GameClient()->m_LocalPractice.IsControlling()))
	{
		// Render local cursor
		CurWeapon = std::max(0, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Predicted.m_ActiveWeapon);
		TargetPos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	}
	else if(g_Config.m_ClCursorOpacitySpec > 0 && GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
	{
		CurWeapon = 1;
		Alpha = g_Config.m_ClCursorOpacitySpec / 100.0f;
		TargetPos = Center;
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponCursors[CurWeapon]);
	}
	else
	{
		// Render spec cursor
		if(!g_Config.m_ClSpecCursor || !GameClient()->m_CursorInfo.IsAvailable())
			return;

		bool RenderSpecCursor = (GameClient()->m_Snap.m_SpecInfo.m_Active && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW) || Client()->State() == IClient::STATE_DEMOPLAYBACK;

		if(!RenderSpecCursor)
			return;

		// Calculate factor to keep cursor on screen
		const vec2 HalfSize = Center - ScreenRect.m_TopLeft;
		const vec2 ScreenPos = (GameClient()->m_CursorInfo.WorldTarget() - Center) / GameClient()->m_Camera.m_Zoom;
		const float ClampFactor = std::max({
			1.0f,
			absolute(ScreenPos.x / HalfSize.x),
			absolute(ScreenPos.y / HalfSize.y),
		});

		CurWeapon = std::max(0, GameClient()->m_CursorInfo.Weapon() % NUM_WEAPONS);
		TargetPos = ScreenPos / ClampFactor + Center;
		if(ClampFactor != 1.0f)
			Alpha /= 2.0f;
	}

	// check if cursor is on island, if so, fade it out while island is expanding
	constexpr float Padding = 38.0f;
	const vec2 IslandPosHud = this->IslandPos();
	const vec2 IslandSizeHud = this->IslandSize();
	const float WorldWidth = ScreenRect.Width();
	const float WorldHeight = ScreenRect.Height();
	const vec2 IslandPos(
		ScreenRect.m_TopLeft.x + (IslandPosHud.x / m_Width) * WorldWidth,
		ScreenRect.m_TopLeft.y + (IslandPosHud.y / m_Height) * WorldHeight);
	const vec2 IslandSize(
		(IslandSizeHud.x / m_Width) * WorldWidth,
		(IslandSizeHud.y / m_Height) * WorldHeight);

	if(TargetPos.x >= IslandPos.x - Padding &&
		TargetPos.x <= IslandPos.x + IslandSize.x + Padding &&
		TargetPos.y >= IslandPos.y - Padding &&
		TargetPos.y <= IslandPos.y + IslandSize.y + Padding)
	{
		const float ExpandProgress = m_Island.m_AnimProgress;
		const float HideProgress = std::clamp((ExpandProgress - 0.3f) / 0.4f, 0.0f, 1.0f);

		Alpha *= 1.0f - HideProgress;
	}

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponCursors[CurWeapon]);
	const float SizeMult = g_Config.m_ClCursorSize * 0.01f;

	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aCursorOffset[CurWeapon], TargetPos.x, TargetPos.y, SizeMult, SizeMult);
}

void CHud::PrepareAmmoHealthAndArmorQuads()
{
	float x = 5;
	float y = 5;
	IGraphics::CQuadItem Array[10];

	// ammo of the different weapons
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		// 0.6
		for(int n = 0; n < 10; n++)
			Array[n] = IGraphics::CQuadItem(x + n * 12, y, 10, 10);

		m_aAmmoOffset[i] = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

		// 0.7
		if(i == WEAPON_GRENADE)
		{
			// special case for 0.7 grenade
			for(int n = 0; n < 10; n++)
				Array[n] = IGraphics::CQuadItem(1 + x + n * 12, y, 10, 10);
		}
		else
		{
			for(int n = 0; n < 10; n++)
				Array[n] = IGraphics::CQuadItem(x + n * 12, y, 12, 12);
		}

		Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	}

	// health
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	m_HealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// empty health
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	m_EmptyHealthOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// armor meter
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	m_ArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// empty armor meter
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	m_EmptyArmorOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// 0.7
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 12, 12);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
}

void CHud::RenderAmmoHealthAndArmor(const CNetObj_Character *pCharacter)
{
	if(!pCharacter)
		return;

	bool IsSixupGameSkin = GameClient()->m_GameSkin.IsSixup();
	int QuadOffsetSixup = (IsSixupGameSkin ? 10 : 0);

	// EClient: a 12 unit row per meter that is actually shown, ten 12 unit wide slots across
	{
		const float RowHeight = 12.0f;
		float Height = 0.0f;
		if(GameClient()->m_GameInfo.m_HudHealthArmor)
			Height += 2.0f * RowHeight;
		if(GameClient()->m_GameInfo.m_HudAmmo)
			Height += RowHeight;
		if(Height > 0.0f)
			m_HudLayout.ReportNaturalRect(EHudElement::HEALTH_AMMO, vec2(5.0f, 5.0f), vec2(120.0f, Height));
	}

	if(GameClient()->m_GameInfo.m_HudAmmo)
	{
		// ammo display
		float AmmoOffsetY = GameClient()->m_GameInfo.m_HudHealthArmor ? 24 : 0;
		int CurWeapon = pCharacter->m_Weapon % NUM_WEAPONS;
		// 0.7 only
		if(CurWeapon == WEAPON_NINJA)
		{
			if(!GameClient()->m_GameInfo.m_HudDDRace && Client()->IsSixup())
			{
				const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
				float NinjaProgress = std::clamp(pCharacter->m_AmmoCount - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
				RenderNinjaBarPos(5 + 10 * 12, 5, 6.f, 24.f, NinjaProgress);
			}
		}
		else if(CurWeapon >= 0 && GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon].IsValid())
		{
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon]);
			if(AmmoOffsetY > 0)
			{
				Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_aAmmoOffset[CurWeapon] + QuadOffsetSixup, std::clamp(pCharacter->m_AmmoCount, 0, 10), 0, AmmoOffsetY);
			}
			else
			{
				Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_aAmmoOffset[CurWeapon] + QuadOffsetSixup, std::clamp(pCharacter->m_AmmoCount, 0, 10));
			}
		}
	}

	if(GameClient()->m_GameInfo.m_HudHealthArmor)
	{
		// health display
		const int DisplayHealth = std::min(pCharacter->m_Health, 10);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_HealthOffset + QuadOffsetSixup, DisplayHealth);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_EmptyHealthOffset + QuadOffsetSixup + DisplayHealth, 10 - DisplayHealth);

		// armor display
		const int DisplayArmor = std::min(pCharacter->m_Armor, 10);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup, DisplayArmor);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup + DisplayArmor, 10 - DisplayArmor);
	}
}

void CHud::PreparePlayerStateQuads()
{
	float x = 5;
	float y = 5 + 24;
	IGraphics::CQuadItem Array[10];

	// Quads for displaying the available and used jumps
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	m_AirjumpOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 12, 12);
	m_AirjumpEmptyOffset = Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// Quads for displaying weapons
	for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
	{
		const CDataWeaponspec &WeaponSpec = g_pData->m_Weapons.m_aId[Weapon];
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(WeaponSpec.m_pSpriteBody, ScaleX, ScaleY);
		constexpr float HudWeaponScale = 0.25f;
		float Width = WeaponSpec.m_VisualSize * ScaleX * HudWeaponScale;
		float Height = WeaponSpec.m_VisualSize * ScaleY * HudWeaponScale;
		// EClient: RenderPlayerState draws these rotated by 45 degrees, so they reach further from
		// their centre than their own width and height suggest
		m_MaxWeaponHudExtent = std::max(m_MaxWeaponHudExtent, (Width + Height) / (2.0f * std::sqrt(2.0f)));
		m_aWeaponOffset[Weapon] = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, Width, Height);
	}

	// Quads for displaying capabilities
	m_EndlessJumpOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_EndlessHookOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_JetpackOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportGrenadeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportGunOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_TeleportLaserOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying prohibited capabilities
	m_SoloOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_CollisionDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_HookHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_HammerHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_GunHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_ShotgunHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_GrenadeHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LaserHitDisabledOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying freeze status
	m_DeepFrozenOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LiveFrozenOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying dummy actions
	m_DummyHammerOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_DummyCopyOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);

	// Quads for displaying team modes
	m_PracticeModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_LockModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
	m_Team0ModeOffset = Graphics()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 12.f, 12.f);
}

void CHud::RenderPlayerState(const int ClientId)
{
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	// pCharacter contains the predicted character for local players or the last snap for players who are spectated
	CCharacterCore *pCharacter = &GameClient()->m_aClients[ClientId].m_Predicted;
	CNetObj_Character *pPlayer = &GameClient()->m_aClients[ClientId].m_RenderCur;
	int TotalJumpsToDisplay = 0;

	// EClient: this used to shift itself down by the height of the health, armor and ammo rows by
	// hand, which assumed those were always drawn at full size. The push solver stacks it under
	// whatever health_ammo actually turned out to be now, so it scales with it.
	// The rect this element covers is only known once every row below has laid itself out, so it
	// is accumulated as they go and reported at the end.
	float MaxX = 5.0f;
	float TopY = -1.0f;
	if(g_Config.m_ClShowhudJumpsIndicator)
	{
		int AvailableJumpsToDisplay;
		if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
		{
			const bool Grounded = Collision()->IsOnGround(vec2(pPlayer->m_X, pPlayer->m_Y), CCharacterCore::PhysicalSize());
			int UsedJumps = pCharacter->m_JumpedTotal;
			if(pCharacter->m_Jumps > 1)
			{
				UsedJumps += !Grounded;
			}
			else if(pCharacter->m_Jumps == 1)
			{
				// If the player has only one jump, each jump is the last one
				UsedJumps = pPlayer->m_Jumped & 2;
			}
			else if(pCharacter->m_Jumps == -1)
			{
				// The player has only one ground jump
				UsedJumps = !Grounded;
			}

			if(pCharacter->m_EndlessJump && UsedJumps >= absolute(pCharacter->m_Jumps))
			{
				UsedJumps = absolute(pCharacter->m_Jumps) - 1;
			}

			int UnusedJumps = absolute(pCharacter->m_Jumps) - UsedJumps;
			if(!(pPlayer->m_Jumped & 2) && UnusedJumps <= 0)
			{
				// In some edge cases when the player just got another number of jumps, UnusedJumps is not correct
				UnusedJumps = 1;
			}
			TotalJumpsToDisplay = std::clamp(absolute(pCharacter->m_Jumps), 0, 10);
			AvailableJumpsToDisplay = std::clamp(UnusedJumps, 0, TotalJumpsToDisplay);
		}
		else
		{
			TotalJumpsToDisplay = AvailableJumpsToDisplay = absolute(GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Jumps);
		}

		// render available and used jumps
		// EClient: the jump quads are baked at y = 5 + 24
		if(TotalJumpsToDisplay > 0)
		{
			TopY = 5.0f + 24.0f;
			MaxX = std::max(MaxX, 5.0f + TotalJumpsToDisplay * 12.0f);
		}
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjump);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpOffset, AvailableJumpsToDisplay);
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + AvailableJumpsToDisplay, TotalJumpsToDisplay - AvailableJumpsToDisplay);
	}

	float x = 5 + 12;
	float y = 5.0f + 12.0f;

	// EClient: the weapon row is centred on the cursor and drawn rotated, so it reaches further
	// above the cursor than a plain 12 unit icon would, and it sits above the jump row
	if(TopY < 0.0f || y - m_MaxWeaponHudExtent < TopY)
		TopY = y - m_MaxWeaponHudExtent;

	// render weapons
	{
		constexpr float aWeaponWidth[NUM_WEAPONS] = {16, 12, 12, 12, 12, 12};
		constexpr float aWeaponInitialOffset[NUM_WEAPONS] = {-3, -4, -1, -1, -2, -4};
		bool InitialOffsetAdded = false;
		for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
		{
			if(!pCharacter->m_aWeapons[Weapon].m_Got)
				continue;
			if(!InitialOffsetAdded)
			{
				x += aWeaponInitialOffset[Weapon];
				InitialOffsetAdded = true;
			}
			if(pPlayer->m_Weapon != Weapon)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
			Graphics()->QuadsSetRotation(pi * 7 / 4);
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpritePickupWeapons[Weapon]);
			Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aWeaponOffset[Weapon], x, y);
			Graphics()->QuadsSetRotation(0);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			x += aWeaponWidth[Weapon];
		}
		if(pCharacter->m_aWeapons[WEAPON_NINJA].m_Got)
		{
			const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
			float NinjaProgress = std::clamp(pCharacter->m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000 - Client()->GameTick(g_Config.m_ClDummy), 0, Max) / (float)Max;
			if(NinjaProgress > 0.0f && GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
			{
				RenderNinjaBarPos(x, y - 12, 6.f, 24.f, NinjaProgress);
			}
		}
	}

	// render capabilities
	MaxX = std::max(MaxX, x + m_MaxWeaponHudExtent); // EClient: the rotated weapons overhang too
	x = 5;
	y += 12;
	if(TotalJumpsToDisplay > 0)
	{
		y += 12;
	}
	bool HasCapabilities = false;
	if(pCharacter->m_EndlessJump)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudEndlessJump);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_EndlessJumpOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_EndlessHook)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudEndlessHook);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_EndlessHookOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_Jetpack)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudJetpack);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_JetpackOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunGun && pCharacter->m_aWeapons[WEAPON_GUN].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportGun);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportGunOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunGrenade && pCharacter->m_aWeapons[WEAPON_GRENADE].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportGrenade);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportGrenadeOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HasTelegunLaser && pCharacter->m_aWeapons[WEAPON_LASER].m_Got)
	{
		HasCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeleportLaser);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_TeleportLaserOffset, x, y);
	}

	// render prohibited capabilities
	MaxX = std::max(MaxX, x); // EClient
	x = 5;
	if(HasCapabilities)
	{
		y += 12;
	}
	bool HasProhibitedCapabilities = false;
	if(pCharacter->m_Solo)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudSolo);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_SoloOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_CollisionDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudCollisionDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_CollisionDisabledOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HookHitDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudHookHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_HookHitDisabledOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_HammerHitDisabled)
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudHammerHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_HammerHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_GrenadeHitDisabled && pCharacter->m_HasTelegunGun && pCharacter->m_aWeapons[WEAPON_GUN].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudGunHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LaserHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_ShotgunHitDisabled && pCharacter->m_aWeapons[WEAPON_SHOTGUN].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudShotgunHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_ShotgunHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_GrenadeHitDisabled && pCharacter->m_aWeapons[WEAPON_GRENADE].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudGrenadeHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_GrenadeHitDisabledOffset, x, y);
		x += 12;
	}
	if((pCharacter->m_LaserHitDisabled && pCharacter->m_aWeapons[WEAPON_LASER].m_Got))
	{
		HasProhibitedCapabilities = true;
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLaserHitDisabled);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LaserHitDisabledOffset, x, y);
	}

	// render dummy actions and freeze state
	MaxX = std::max(MaxX, x); // EClient
	x = 5;
	if(HasProhibitedCapabilities)
	{
		y += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_LOCK_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLockMode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LockModeOffset, x, y);
		x += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudPracticeMode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_PracticeModeOffset, x, y);
		x += 12;
	}
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo && GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Flags & CHARACTERFLAG_TEAM0_MODE)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudTeam0Mode);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_Team0ModeOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_DeepFrozen)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDeepFrozen);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DeepFrozenOffset, x, y);
		x += 12;
	}
	if(pCharacter->m_LiveFrozen)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudLiveFrozen);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_LiveFrozenOffset, x, y);
	}

	// EClient: the capability, prohibition and freeze icons are added with the four argument
	// QuadContainerAddSprite, which puts the quad's top left on the origin rather than centring
	// it, so the last row runs from y to y + 12. Only the weapon row above is centred.
	MaxX = std::max(MaxX, x);
	if(TopY >= 0.0f)
		m_HudLayout.ReportNaturalRect(EHudElement::PLAYER_STATE, vec2(5.0f, TopY), vec2(MaxX - 5.0f, y + 12.0f - TopY));
}

void CHud::RenderNinjaBarPos(const float x, float y, const float Width, const float Height, float Progress, const float Alpha)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);

	// what percentage of the end pieces is used for the progress indicator and how much is the rest
	// half of the ends are used for the progress display
	const float RestPct = 0.5f;
	const float ProgPct = 0.5f;

	const float EndHeight = Width; // to keep the correct scale - the width of the sprite is as long as the height
	const float BarWidth = Width;
	const float WholeBarHeight = Height;
	const float MiddleBarHeight = WholeBarHeight - (EndHeight * 2.0f);
	const float EndProgressHeight = EndHeight * ProgPct;
	const float EndRestHeight = EndHeight * RestPct;
	const float ProgressBarHeight = WholeBarHeight - (EndProgressHeight * 2.0f);
	const float EndProgressProportion = EndProgressHeight / ProgressBarHeight;
	const float MiddleProgressProportion = MiddleBarHeight / ProgressBarHeight;

	// beginning piece
	float BeginningPieceProgress = 1;
	if(Progress <= 1)
	{
		if(Progress <= (EndProgressProportion + MiddleProgressProportion))
		{
			BeginningPieceProgress = 0;
		}
		else
		{
			BeginningPieceProgress = (Progress - EndProgressProportion - MiddleProgressProportion) / EndProgressProportion;
		}
	}
	// empty
	Graphics()->WrapClamp();
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: btm_r, top_r, top_m, btm_m | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
	Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, ProgPct - ProgPct * (1.0f - BeginningPieceProgress), 0, ProgPct - ProgPct * (1.0f - BeginningPieceProgress), 1);
	IGraphics::CQuadItem QuadEmptyBeginning(x, y, BarWidth, EndRestHeight + EndProgressHeight * (1.0f - BeginningPieceProgress));
	Graphics()->QuadsDrawTL(&QuadEmptyBeginning, 1);
	Graphics()->QuadsEnd();
	// full
	if(BeginningPieceProgress > 0.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFullLeft);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// Subset: btm_m, top_m, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(RestPct + ProgPct * (1.0f - BeginningPieceProgress), 1, RestPct + ProgPct * (1.0f - BeginningPieceProgress), 0, 1, 0, 1, 1);
		IGraphics::CQuadItem QuadFullBeginning(x, y + (EndRestHeight + EndProgressHeight * (1.0f - BeginningPieceProgress)), BarWidth, EndProgressHeight * BeginningPieceProgress);
		Graphics()->QuadsDrawTL(&QuadFullBeginning, 1);
		Graphics()->QuadsEnd();
	}

	// middle piece
	y += EndHeight;

	float MiddlePieceProgress = 1;
	if(Progress <= EndProgressProportion + MiddleProgressProportion)
	{
		if(Progress <= EndProgressProportion)
		{
			MiddlePieceProgress = 0;
		}
		else
		{
			MiddlePieceProgress = (Progress - EndProgressProportion) / MiddleProgressProportion;
		}
	}

	const float FullMiddleBarHeight = MiddleBarHeight * MiddlePieceProgress;
	const float EmptyMiddleBarHeight = MiddleBarHeight - FullMiddleBarHeight;

	// empty ninja bar
	if(EmptyMiddleBarHeight > 0.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmpty);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// select the middle portion of the sprite so we don't get edge bleeding
		if(EmptyMiddleBarHeight <= EndHeight)
		{
			// prevent pixel puree, select only a small slice
			// Subset: btm_r, top_r, top_m, btm_m | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
			Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, 1.0f - (EmptyMiddleBarHeight / EndHeight), 0, 1.0f - (EmptyMiddleBarHeight / EndHeight), 1);
		}
		else
		{
			// Subset: btm_r, top_r, top_l, btm_l | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
			Graphics()->QuadsSetSubsetFree(1, 1, 1, 0, 0, 0, 0, 1);
		}
		IGraphics::CQuadItem QuadEmpty(x, y, BarWidth, EmptyMiddleBarHeight);
		Graphics()->QuadsDrawTL(&QuadEmpty, 1);
		Graphics()->QuadsEnd();
	}

	// full ninja bar
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFull);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// select the middle portion of the sprite so we don't get edge bleeding
	if(FullMiddleBarHeight <= EndHeight)
	{
		// prevent pixel puree, select only a small slice
		// Subset: btm_m, top_m, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(1.0f - (FullMiddleBarHeight / EndHeight), 1, 1.0f - (FullMiddleBarHeight / EndHeight), 0, 1, 0, 1, 1);
	}
	else
	{
		// Subset: btm_l, top_l, top_r, btm_r | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(0, 1, 0, 0, 1, 0, 1, 1);
	}
	IGraphics::CQuadItem QuadFull(x, y + EmptyMiddleBarHeight, BarWidth, FullMiddleBarHeight);
	Graphics()->QuadsDrawTL(&QuadFull, 1);
	Graphics()->QuadsEnd();

	// ending piece
	y += MiddleBarHeight;
	float EndingPieceProgress = 1;
	if(Progress <= EndProgressProportion)
	{
		EndingPieceProgress = Progress / EndProgressProportion;
	}
	// empty
	if(EndingPieceProgress < 1.0f)
	{
		Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarEmptyRight);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		// Subset: btm_l, top_l, top_m, btm_m | it is rotated 90 degrees clockwise
		Graphics()->QuadsSetSubsetFree(0, 1, 0, 0, ProgPct - ProgPct * EndingPieceProgress, 0, ProgPct - ProgPct * EndingPieceProgress, 1);
		IGraphics::CQuadItem QuadEmptyEnding(x, y, BarWidth, EndProgressHeight * (1.0f - EndingPieceProgress));
		Graphics()->QuadsDrawTL(&QuadEmptyEnding, 1);
		Graphics()->QuadsEnd();
	}
	// full
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudNinjaBarFullLeft);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
	// Subset: btm_m, top_m, top_l, btm_l | it is mirrored on the horizontal axe and rotated 90 degrees counterclockwise
	Graphics()->QuadsSetSubsetFree(RestPct + ProgPct * EndingPieceProgress, 1, RestPct + ProgPct * EndingPieceProgress, 0, 0, 0, 0, 1);
	IGraphics::CQuadItem QuadFullEnding(x, y + (EndProgressHeight * (1.0f - EndingPieceProgress)), BarWidth, EndRestHeight + EndProgressHeight * EndingPieceProgress);
	Graphics()->QuadsDrawTL(&QuadFullEnding, 1);
	Graphics()->QuadsEnd();

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->WrapNormal();
}

void CHud::RenderSpectatorCount()
{
	if(!g_Config.m_ClShowhudSpectatorCount)
	{
		return;
	}

	int Count = 0;
	if(Client()->IsSixup())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == GameClient()->m_aLocalIds[0] || (GameClient()->Client()->DummyConnected() && i == GameClient()->m_aLocalIds[1]))
				continue;

			if(Client()->m_TranslationContext.m_aClients[i].m_PlayerFlags7 & protocol7::PLAYERFLAG_WATCHING)
			{
				Count++;
			}
		}
	}
	else
	{
		const CNetObj_SpectatorCount *pSpectatorCount = GameClient()->m_Snap.m_pSpectatorCount;
		if(!pSpectatorCount)
		{
			m_LastSpectatorCountTick = Client()->GameTick(g_Config.m_ClDummy);
			return;
		}
		Count = pSpectatorCount->m_NumSpectators;
	}

	if(Count == 0)
	{
		m_LastSpectatorCountTick = Client()->GameTick(g_Config.m_ClDummy);
		return;
	}

	// 1 second delay
	if(Client()->GameTick(g_Config.m_ClDummy) < m_LastSpectatorCountTick + Client()->GameTickSpeed())
		return;

	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Count);

	const float Fontsize = 6.0f;
	const float BoxHeight = 14.f;
	const float BoxWidth = 13.f + TextRender()->TextWidth(Fontsize, aBuf);

	// EClient: this used to subtract the height of the movement box, the score box and the dummy
	// actions box by hand. The push solver in CHudLayout stacks it now, so it only has to say
	// where it sits on its own.
	const float StartX = m_Width - BoxWidth;
	const float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;

	m_HudLayout.ReportNaturalRect(EHudElement::SPECTATOR_COUNT, vec2(StartX, StartY), vec2(BoxWidth, BoxHeight)); // EClient

	// EClient: rounded only where nothing is up against it, so panels put side by side merge
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, CHudLayout::BackgroundColor(), m_HudLayout.CornerFlags(EHudElement::SPECTATOR_COUNT), 5.0f);

	float y = StartY + BoxHeight / 3;
	float x = StartX + 2;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->Text(x, y, Fontsize, FontIcon::EYE, -1.0f);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->Text(x + Fontsize + 3.f, y, Fontsize, aBuf, -1.0f);
}

void CHud::RenderDummyActions()
{
	// EClient: DummyConnected cannot be substituted the way the snapshot and config can, so the
	// preview is let through inside HasDummyActionsBox
	if(!HasDummyActionsBox())
	{
		return;
	}
	// render small dummy actions hud
	const float BoxHeight = 29.0f;
	const float BoxWidth = 16.0f;

	// EClient: stacked by the push solver now, see RenderSpectatorCount
	const float StartX = m_Width - BoxWidth;
	const float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;

	m_HudLayout.ReportNaturalRect(EHudElement::DUMMY_ACTIONS, vec2(StartX, StartY), vec2(BoxWidth, BoxHeight)); // EClient

	// EClient: rounded only where nothing is up against it, so panels put side by side merge
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, CHudLayout::BackgroundColor(), m_HudLayout.CornerFlags(EHudElement::DUMMY_ACTIONS), 5.0f);

	float y = StartY + 2;
	float x = StartX + 2;
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	if(g_Config.m_ClDummyHammer)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDummyHammer);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyHammerOffset, x, y);
	y += 13;
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.4f);
	if(g_Config.m_ClDummyCopyMoves)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudDummyCopy);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_DummyCopyOffset, x, y);
}

inline int CHud::GetDigitsIndex(int Value, int Max)
{
	if(Value < 0)
	{
		Value *= -1;
	}
	int DigitsIndex = std::log10((Value ? Value : 1));
	if(DigitsIndex < 0)
	{
		DigitsIndex = 0;
	}
	return DigitsIndex;
}

inline float CHud::GetMovementInformationBoxHeight()
{
	const float AngleLines = g_Config.m_ClShowhudPlayerCompact ? 1.0f : 2.0f;

	if(GameClient()->m_Snap.m_SpecInfo.m_Active && (GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW || GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId].m_SpecCharPresent))
		return g_Config.m_ClShowhudPlayerPosition ? 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT + 2.0f : 0.0f;
	float BoxHeight = 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * (g_Config.m_ClShowhudPlayerPosition + g_Config.m_ClShowhudPlayerSpeed) + AngleLines * MOVEMENT_INFORMATION_LINE_HEIGHT * g_Config.m_ClShowhudPlayerAngle;
	if(HasMovementInformationBox())
	{
		BoxHeight += 2.0f;
	}

	// EClient
	const int ClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	const CCharacter *pCharacter = GameClient()->m_GameWorld.GetCharacterById(ClientId);
	if(CheckpointInfoEnabled() && pCharacter)
	{
		BoxHeight += AngleLines * MOVEMENT_INFORMATION_LINE_HEIGHT;
	}

	return BoxHeight;
}

void CHud::UpdateMovementInformationTextContainer(STextContainerIndex &TextContainer, float FontSize, float Value, float &PrevValue)
{
	Value = std::round(Value * 100.0f) / 100.0f; // Round to 2dp
	if(TextContainer.Valid() && PrevValue == Value)
		return;
	PrevValue = Value;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%.2f", Value);

	CTextCursor Cursor;
	Cursor.m_FontSize = FontSize;
	TextRender()->RecreateTextContainer(TextContainer, &Cursor, aBuf);
}

void CHud::RenderMovementInformationTextContainer(STextContainerIndex &TextContainer, const ColorRGBA &Color, float X, float Y)
{
	if(TextContainer.Valid())
	{
		TextRender()->RenderTextContainer(TextContainer, Color, TextRender()->DefaultTextOutlineColor(), X - TextRender()->GetBoundingBoxTextContainer(TextContainer).m_W, Y);
	}
}

CHud::CMovementInformation CHud::GetMovementInformation(int ClientId, int Conn) const
{
	CMovementInformation Out;
	// EClient: a practice tee has everything to say about itself, even while the server is showing
	// a spectator character for the tee it stands in for
	const bool Practice = GameClient()->m_LocalPractice.IsSimulated(ClientId);
	if(ClientId == SPEC_FREEVIEW)
	{
		Out.m_Pos = GameClient()->m_Camera.m_Center / 32.0f;
	}
	else if(GameClient()->m_aClients[ClientId].m_SpecCharPresent && !Practice)
	{
		Out.m_Pos = GameClient()->m_aClients[ClientId].m_SpecChar / 32.0f;
	}
	else
	{
		// The render characters are where the practice tee lives; the snapshot may have nothing
		const CNetObj_Character *pPrevChar = Practice ? &GameClient()->m_aClients[ClientId].m_RenderPrev : &GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
		const CNetObj_Character *pCurChar = Practice ? &GameClient()->m_aClients[ClientId].m_RenderCur : &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
		const float IntraTick = Practice ? Client()->PredIntraGameTick(Conn) : Client()->IntraGameTick(Conn);

		// To make the player position relative to blocks we need to divide by the block size
		Out.m_Pos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

		const vec2 Vel = mix(vec2(pPrevChar->m_VelX, pPrevChar->m_VelY), vec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);

		float VelspeedX = Vel.x / 256.0f * Client()->GameTickSpeed();
		if(Vel.x >= -1.0f && Vel.x <= 1.0f)
		{
			VelspeedX = 0.0f;
		}
		float VelspeedY = Vel.y / 256.0f * Client()->GameTickSpeed();
		if(Vel.y >= -128.0f && Vel.y <= 128.0f)
		{
			VelspeedY = 0.0f;
		}
		// We show the speed in Blocks per Second (Bps) and therefore have to divide by the block size
		Out.m_Speed.x = VelspeedX / 32.0f;
		float VelspeedLength = length(vec2(Vel.x, Vel.y) / 256.0f) * Client()->GameTickSpeed();
		// Todo: Use Velramp tuning of each individual player
		// Since these tuning parameters are almost never changed, the default values are sufficient in most cases
		float Ramp = VelocityRamp(VelspeedLength, GameClient()->m_aTuning[Conn].m_VelrampStart, GameClient()->m_aTuning[Conn].m_VelrampRange, GameClient()->m_aTuning[Conn].m_VelrampCurvature);
		Out.m_Speed.x *= Ramp;
		Out.m_Speed.y = VelspeedY / 32.0f;

		float Angle = GameClient()->m_Players.GetPlayerTargetAngle(pPrevChar, pCurChar, ClientId, IntraTick);
		if(Angle < 0.0f)
		{
			Angle += 2.0f * pi;
		}
		Out.m_Angle = Angle * 180.0f / pi;
	}
	return Out;
}

void CHud::RenderMovementInformation()
{
	const int ClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	// EClient: a practice tee has a speed and an angle to show, so it is not position only
	const bool PosOnly = ClientId == SPEC_FREEVIEW || (GameClient()->m_aClients[ClientId].m_SpecCharPresent && !GameClient()->m_LocalPractice.IsSimulated(ClientId));
	// Draw the information depending on settings: Position, speed and target angle
	// This display is only to present the available information from the last snapshot, not to interpolate or predict
	if(!HasMovementInformationBox())
	{
		return;
	}
	const float LineSpacer = 1.0f; // above and below each entry
	const float Fontsize = 6.0f;

	float BoxHeight = GetMovementInformationBoxHeight();

	if(BoxHeight <= MOVEMENT_INFORMATION_LINE_HEIGHT)
		return;

	const float BoxWidth = 62.0f;

	// EClient: stacked by the push solver now, see RenderSpectatorCount
	const float StartX = m_Width - BoxWidth;
	const float StartY = 285.0f - BoxHeight - 4.0f; // 4 units distance to the next display;

	m_HudLayout.ReportNaturalRect(EHudElement::MOVEMENT_INFO, vec2(StartX, StartY), vec2(BoxWidth, BoxHeight)); // EClient

	// EClient: rounded only where nothing is up against it, so panels put side by side merge
	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, CHudLayout::BackgroundColor(), m_HudLayout.CornerFlags(EHudElement::MOVEMENT_INFO), 5.0f);

	const CMovementInformation Info = GetMovementInformation(ClientId, g_Config.m_ClDummy);

	float y = StartY + LineSpacer * 2.0f;
	const float LeftX = StartX + 2.0f;
	const float RightX = m_Width - 2.0f;

	const char aaCoordinates[][4] = {"X:", "Y:"};

	if(g_Config.m_ClShowhudPlayerPosition)
	{
		TextRender()->Text(LeftX, y, Fontsize, Localize("Position:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		for(int i = 0; i < 2; i++)
		{
			TextRender()->Text(LeftX, y, Fontsize, aaCoordinates[i], -1.0f);
			UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[i], Fontsize, i == 0 ? Info.m_Pos.x : Info.m_Pos.y, m_aPlayerPrevPosition[i]);
			RenderMovementInformationTextContainer(m_aPlayerPositionContainers[i], TextRender()->DefaultTextColor(), RightX, y);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if(PosOnly)
		return;
	if(g_Config.m_ClShowhudPlayerSpeed)
	{
		TextRender()->Text(LeftX, y, Fontsize, Localize("Speed:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		for(int i = 0; i < 2; i++)
		{
			ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::INCREASE)
				Color = ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f);
			if(m_aLastPlayerSpeedChange[i] == ESpeedChange::DECREASE)
				Color = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
			TextRender()->Text(LeftX, y, Fontsize, aaCoordinates[i], -1.0f);
			UpdateMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], Fontsize, i == 0 ? Info.m_Speed.x : Info.m_Speed.y, m_aPlayerPrevSpeed[i]);
			RenderMovementInformationTextContainer(m_aPlayerSpeedTextContainers[i], Color, RightX, y);
			y += MOVEMENT_INFORMATION_LINE_HEIGHT;
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	auto RenderSoloInfo = [this](const char *pLabel, float FontSize, STextContainerIndex &TextContainer, int Value, int &PrevValue, float OffLeftX, float OffRightX, float &OffY) {
		TextRender()->Text(OffLeftX, OffY, FontSize, Localize(pLabel), -1.0f);
		if(!g_Config.m_ClShowhudPlayerCompact)
			OffY += MOVEMENT_INFORMATION_LINE_HEIGHT;

		if(!TextContainer.Valid() || PrevValue != Value)
		{
			PrevValue = Value;

			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "%d", Value);

			CTextCursor Cursor;
			Cursor.m_FontSize = FontSize;
			TextRender()->RecreateTextContainer(TextContainer, &Cursor, aBuf);
		}
		if(TextContainer.Valid())
			TextRender()->RenderTextContainer(TextContainer, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), OffRightX - TextRender()->GetBoundingBoxTextContainer(TextContainer).m_W, OffY);
		OffY += MOVEMENT_INFORMATION_LINE_HEIGHT;
	};

	auto RenderSoloInfoFloat = [this](const char *pLabel, float FontSize, STextContainerIndex &TextContainer, float Value, float &PrevValue, float OffLeftX, float OffRightX, float &OffY) {
		TextRender()->Text(OffLeftX, OffY, FontSize, Localize(pLabel), -1.0f);
		if(!g_Config.m_ClShowhudPlayerCompact)
			OffY += MOVEMENT_INFORMATION_LINE_HEIGHT;

		if(!TextContainer.Valid() || PrevValue != Value)
		{
			PrevValue = Value;

			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "%.2f", Value);

			CTextCursor Cursor;
			Cursor.m_FontSize = FontSize;
			TextRender()->RecreateTextContainer(TextContainer, &Cursor, aBuf);
		}
		if(TextContainer.Valid())
			TextRender()->RenderTextContainer(TextContainer, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), OffRightX - TextRender()->GetBoundingBoxTextContainer(TextContainer).m_W, OffY);
		OffY += MOVEMENT_INFORMATION_LINE_HEIGHT;
	};

	if(g_Config.m_ClShowhudPlayerAngle)
		RenderSoloInfoFloat("Angle:", Fontsize, m_PlayerAngleTextContainerIndex, Info.m_Angle, m_PlayerPrevAngle, LeftX, RightX, y);

	if(CheckpointInfoEnabled())
	{
		const CCharacter *pCharacter = GameClient()->m_GameWorld.GetCharacterById(ClientId);
		if(pCharacter)
			RenderSoloInfo("Checkpoint:", Fontsize, m_PlayerCheckpointTextContainerIndex, pCharacter->m_TeleCheckpoint, m_PlayerPrevCheckpoint, LeftX, RightX, y);
	}
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectator)
		return;

	// draw the box
	m_HudLayout.ReportNaturalRect(EHudElement::SPECTATOR_HUD, vec2(m_Width - 180.0f, m_Height - 15.0f), vec2(180.0f, 15.0f)); // EClient

	Graphics()->DrawRect(m_Width - 180.0f, m_Height - 15.0f, 180.0f, 15.0f, CHudLayout::BackgroundColor(), m_HudLayout.CornerFlags(EHudElement::SPECTATOR_HUD), 5.0f); // EClient

	// draw the text
	char aBuf[128];
	if(GameClient()->m_MultiViewActivated)
	{
		str_copy(aBuf, Localize("Multi-View"));
	}
	else if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
	{
		const auto &Player = GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId];
		if(g_Config.m_ClShowIds)
			str_format(aBuf, sizeof(aBuf), Localize("Following %d: %s", "Spectating"), Player.ClientId(), Player.m_aName);
		else
			str_format(aBuf, sizeof(aBuf), Localize("Following %s", "Spectating"), Player.m_aName);
	}
	else
	{
		str_copy(aBuf, Localize("Free-View"));
	}
	TextRender()->Text(m_Width - 174.0f, m_Height - 15.0f + (15.f - 8.f) / 2.f, 8.0f, aBuf, -1.0f);

	// draw the camera info
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera() && g_Config.m_ClSpecAutoSync)
	{
		bool AutoSpecCameraEnabled = GameClient()->m_Camera.m_AutoSpecCamera;
		const char *pLabelText = Localize("AUTO", "Spectating Camera Mode Icon");
		const float TextWidth = TextRender()->TextWidth(6.0f, pLabelText);

		constexpr float RightMargin = 4.0f;
		constexpr float IconWidth = 6.0f;
		constexpr float Padding = 3.0f;
		const float TagWidth = IconWidth + TextWidth + Padding * 3.0f;
		const float TagX = m_Width - RightMargin - TagWidth;
		Graphics()->DrawRect(TagX, m_Height - 12.0f, TagWidth, 10.0f, ColorRGBA(1.0f, 1.0f, 1.0f, AutoSpecCameraEnabled ? 0.50f : 0.10f), IGraphics::CORNER_ALL, 2.5f);
		TextRender()->TextColor(1, 1, 1, AutoSpecCameraEnabled ? 1.0f : 0.65f);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->Text(TagX + Padding, m_Height - 10.0f, 6.0f, FontIcon::CAMERA, -1.0f);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		TextRender()->Text(TagX + Padding + IconWidth + Padding, m_Height - 10.0f, 6.0f, pLabelText, -1.0f);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CHud::RenderLocalTime(float x)
{
	if(!RenderLocalTime())
		return;

	const bool Seconds = g_Config.m_EcShowLocalTimeSeconds; // TClient

	char aTimeStr[16];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), Seconds ? "%H:%M.%S" : "%H:%M");
	const float Width = std::round(TextRender()->TextBoundingBox(5.0f, aTimeStr).m_W);

	// EClient
	m_HudLayout.ReportNaturalRect(EHudElement::LOCAL_TIME, vec2(x - (Width + 15.0f), 0.0f), vec2(Width + 10.0f, 12.5f));

	Graphics()->DrawRect(x - (Width + 15.0f), 0.0f, Width + 10.0f, 12.5f, CHudLayout::BackgroundColor(), m_HudLayout.CornerFlags(EHudElement::LOCAL_TIME), 3.75f);
	TextRender()->Text(x - (Width + 10.0f), (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1.0f);

	// Graphics()->DrawRect(x - 30.0f, 0.0f, 25.0f, 12.5f, CHudLayout::BackgroundColor(), IGraphics::CORNER_B, 3.75f);
	// TextRender()->Text(x - 25.0f, (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1.0f);
}

void CHud::OnNewSnapshot()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	int ClientId = -1;
	if(GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		ClientId = GameClient()->m_Snap.m_LocalClientId;
	else if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		ClientId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;

	if(ClientId == -1)
		return;

	const CNetObj_Character *pPrevChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
	const CNetObj_Character *pCurChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
	const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	ivec2 Vel = mix(ivec2(pPrevChar->m_VelX, pPrevChar->m_VelY), ivec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);

	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	if(pChar && pChar->IsGrounded())
		Vel.y = 0;

	int aVels[2] = {Vel.x, Vel.y};

	for(int i = 0; i < 2; i++)
	{
		int AbsVel = abs(aVels[i]);
		if(AbsVel > m_aPlayerSpeed[i])
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::INCREASE;
		}
		if(AbsVel < m_aPlayerSpeed[i])
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::DECREASE;
		}
		if(AbsVel < 2)
		{
			m_aLastPlayerSpeedChange[i] = ESpeedChange::NONE;
		}
		m_aPlayerSpeed[i] = AbsVel;
	}
}

void CHud::OnRender()
{
	// EClient: everything below describes the tee being played, which while practicing is the
	// practice one. Wrapped rather than taught about it, so the code itself is untouched.
	CLocalPractice::CScope PracticeScope(&GameClient()->m_LocalPractice);

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreenToSize(m_Width, m_Height);

	// EClient: while the editor is up, every element is shown with everything it is capable of
	// showing, so that its real maximum footprint can be placed. Rather than duplicating the
	// drawing code into a preview path, the inputs it reads are swapped out for the duration of
	// this one frame and put straight back afterwards.
	// Stand in content goes in whenever the editor is open, so nothing sits there empty while it
	// is being placed. Holding ctrl is a stronger thing: it switches on the elements that are
	// turned off entirely, which is the part worth having to ask for.
	const bool Preview = GameClient()->m_HudEditor.IsActive();
	const bool ForceOn = GameClient()->m_HudEditor.IsPreviewing();
	const int PreviewClientId = GameClient()->m_Snap.m_LocalClientId;
	const bool PreviewCharacter = Preview && PreviewClientId >= 0 && PreviewClientId < MAX_CLIENTS;

	int *apPreviewConfigs[] = {
		&g_Config.m_ClShowhud,
		&g_Config.m_ClShowhudHealthAmmo,
		&g_Config.m_ClShowhudDDRace,
		&g_Config.m_ClShowhudJumpsIndicator,
		&g_Config.m_ClShowhudScore,
		&g_Config.m_ClShowhudDummyActions,
		&g_Config.m_ClShowhudSpectatorCount,
		&g_Config.m_ClShowhudPlayerPosition,
		&g_Config.m_ClShowhudPlayerSpeed,
		&g_Config.m_ClShowhudPlayerAngle,
		&g_Config.m_ClShowhudPlayerCheckpoint,
		&g_Config.m_ClShowfps,
		&g_Config.m_ClShowpred,
		&g_Config.m_ClShowRecord,
		&g_Config.m_ClShowFrozenHud,
		&g_Config.m_ClShowFrozenText,
		&g_Config.m_ClNotifyWhenLast,
		&g_Config.m_ClLocalPracticeAlert}; // EClient
	int aPreviewSavedConfigs[std::size(apPreviewConfigs)];

	CNetObj_Character PreviewLocalCharacter{};
	const CNetObj_GameInfo *pPreviewSavedGameInfo = GameClient()->m_Snap.m_pGameInfoObj;
	const auto PreviewSavedGameInfo = GameClient()->m_GameInfo;
	const int PreviewSavedMapBestSeconds = GameClient()->m_MapBestTimeSeconds;
	const int PreviewSavedMapBestMillis = GameClient()->m_MapBestTimeMillis;
	const bool PreviewSavedReceivedFinishTimes = GameClient()->m_ReceivedDDNetPlayerFinishTimes;
	const float PreviewSavedPlayerRecord = m_aPlayerRecord[g_Config.m_ClDummy];
	const CNetObj_SpectatorCount *pPreviewSavedSpectatorCount = GameClient()->m_Snap.m_pSpectatorCount;
	const int PreviewSavedSpectatorTick = m_LastSpectatorCountTick;

	CVoting &Voting = GameClient()->m_Voting;
	const int64_t PreviewSavedVoteClosetime = Voting.m_Closetime;
	const int PreviewSavedVoteYes = Voting.m_Yes;
	const int PreviewSavedVoteNo = Voting.m_No;
	const int PreviewSavedVotePass = Voting.m_Pass;
	const int PreviewSavedVoteTotal = Voting.m_Total;
	const int PreviewSavedVoteVoted = Voting.m_Voted;
	char aPreviewSavedVoteDescription[VOTE_DESC_LENGTH];
	char aPreviewSavedVoteReason[VOTE_REASON_LENGTH];
	// Only worth copying when there is something to put back. Preview is off on essentially every
	// frame, and these are not free.
	CCharacterCore PreviewSavedPredicted;
	CGameClient::CSnapState::CCharacterInfo PreviewSavedSnapCharacter;

	if(ForceOn)
	{
		for(size_t i = 0; i < std::size(apPreviewConfigs); i++)
		{
			aPreviewSavedConfigs[i] = *apPreviewConfigs[i];
			*apPreviewConfigs[i] = 1;
		}

		// A DDRace server reports no health, armor or ammo at all, so those meters would stay
		// invisible however the config is set
		GameClient()->m_GameInfo.m_HudHealthArmor = true;
		GameClient()->m_GameInfo.m_HudAmmo = true;
		GameClient()->m_GameInfo.m_HudDDRace = true;
	}

	if(Preview)
	{
		str_copy(aPreviewSavedVoteDescription, Voting.m_aDescription);
		str_copy(aPreviewSavedVoteReason, Voting.m_aReason);

		PreviewLocalCharacter.m_Health = 10;
		PreviewLocalCharacter.m_Armor = 10;
		PreviewLocalCharacter.m_AmmoCount = 10;
		PreviewLocalCharacter.m_Weapon = WEAPON_GUN;

		// Elements that only appear under conditions the editor cannot arrange are handed stand in
		// state instead. The warmup clock counts down on a loop and the spectator count walks up,
		// so that both are visibly alive rather than looking frozen or broken.
		// Held on the component rather than on the stack. The snapshot keeps the pointer for the
		// length of this call, and a local would leave it aimed at a dead frame the moment anything
		// held on to it a little longer than expected.
		m_PreviewGameInfo = *GameClient()->m_Snap.m_pGameInfoObj;
		m_PreviewGameInfo.m_GameStateFlags &= ~(GAMESTATEFLAG_RACETIME | GAMESTATEFLAG_GAMEOVER);
		m_PreviewGameInfo.m_WarmupTimer = (10 - (int)Client()->LocalTime() % 10) * Client()->GameTickSpeed();
		GameClient()->m_Snap.m_pGameInfoObj = &m_PreviewGameInfo;

		// Stand in times for the record lines, which otherwise only appear once the server has
		// actually sent a best time
		GameClient()->m_MapBestTimeSeconds = 90 + (int)Client()->LocalTime() % 30;
		GameClient()->m_MapBestTimeMillis = 120;
		GameClient()->m_ReceivedDDNetPlayerFinishTimes = false;
		m_aPlayerRecord[g_Config.m_ClDummy] = 123.45f;

		// A vote in progress cannot be arranged either, so one is stood in. Setting the same
		// fields the server would means CVoting::Render needs no preview path of its own: its
		// early returns simply pass.
		const int VoteElapsed = (int)Client()->LocalTime() % 25;
		Voting.m_Closetime = time_get() + time_freq() * (25 - VoteElapsed);
		Voting.m_Voted = 0;
		Voting.m_Total = 12;
		Voting.m_Yes = 1 + VoteElapsed % 6;
		Voting.m_No = 1 + (VoteElapsed / 2) % 4;
		Voting.m_Pass = Voting.m_Total - Voting.m_Yes - Voting.m_No;
		str_copy(Voting.m_aDescription, Localize("Kick player"));
		str_copy(Voting.m_aReason, Localize("No reason given"));

		m_PreviewSpectatorCount.m_NumSpectators = 1 + (int)Client()->LocalTime() % 20;
		GameClient()->m_Snap.m_pSpectatorCount = &m_PreviewSpectatorCount;
		// The counter throttles itself to one update a second, which would leave it blank at first
		m_LastSpectatorCountTick = 0;
	}

	if(PreviewCharacter)
	{
		PreviewSavedPredicted = GameClient()->m_aClients[PreviewClientId].m_Predicted;
		PreviewSavedSnapCharacter = GameClient()->m_Snap.m_aCharacters[PreviewClientId];

		CCharacterCore &Core = GameClient()->m_aClients[PreviewClientId].m_Predicted;
		Core.m_Jumps = 10;
		Core.m_JumpedTotal = 0;
		Core.m_EndlessJump = true;
		Core.m_EndlessHook = true;
		Core.m_Jetpack = true;
		Core.m_HasTelegunGun = true;
		Core.m_HasTelegunGrenade = true;
		Core.m_HasTelegunLaser = true;
		Core.m_Solo = true;
		Core.m_CollisionDisabled = true;
		Core.m_HookHitDisabled = true;
		Core.m_HammerHitDisabled = true;
		Core.m_ShotgunHitDisabled = true;
		Core.m_GrenadeHitDisabled = true;
		Core.m_LaserHitDisabled = true;
		Core.m_DeepFrozen = true;
		Core.m_LiveFrozen = true;
		for(int Weapon = 0; Weapon < NUM_WEAPONS; Weapon++)
		{
			// The ninja bar reaches outside the row it is drawn on, so it is left out
			Core.m_aWeapons[Weapon].m_Got = Weapon != WEAPON_NINJA;
		}

		auto &SnapCharacter = GameClient()->m_Snap.m_aCharacters[PreviewClientId];
		SnapCharacter.m_HasExtendedData = true;
		SnapCharacter.m_HasExtendedDisplayInfo = true;
		SnapCharacter.m_ExtendedData.m_Jumps = 10;
		SnapCharacter.m_ExtendedData.m_Flags |= CHARACTERFLAG_LOCK_MODE | CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_TEAM0_MODE;
	}

	m_HudLayout.OnBaseScreenSet(vec2(m_Width, m_Height));
	if(m_HudLayout.TakeContainersDirty())
	{
		// Text containers keep the glyph size they were rasterized at, so anything cached before a
		// scale change has to go or it renders blurry at the new size.
		ResetHudContainers();
	}

#if defined(CONF_VIDEORECORDER)
	if((IVideo::Current() && g_Config.m_ClVideoShowhud) || (!IVideo::Current() && g_Config.m_ClShowhud))
#else
	if(g_Config.m_ClShowhud)
#endif
	{
		// EClient: playing a practice tee is playing, even when the server has no character for the
		// tee it stands in for -- the position readout and the rest belong to it
		if((GameClient()->m_Snap.m_pLocalCharacter || GameClient()->m_LocalPractice.IsControlling()) && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		{
			if(g_Config.m_ClShowhudHealthAmmo)
			{
				if(!m_HudLayout.IsOccluded(EHudElement::HEALTH_AMMO) && (Preview || GameClient()->m_Snap.m_pLocalCharacter))
				{
					CHudLayout::CScope Scope(&m_HudLayout, EHudElement::HEALTH_AMMO); // EClient
					RenderAmmoHealthAndArmor(Preview ? &PreviewLocalCharacter : GameClient()->m_Snap.m_pLocalCharacter);
				}
			}
			if(GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_LocalClientId].m_HasExtendedData && g_Config.m_ClShowhudDDRace && GameClient()->m_GameInfo.m_HudDDRace)
			{
				if(!m_HudLayout.IsOccluded(EHudElement::PLAYER_STATE))
				{
					CHudLayout::CScope Scope(&m_HudLayout, EHudElement::PLAYER_STATE); // EClient
					RenderPlayerState(GameClient()->m_Snap.m_LocalClientId);
				}
			}
			// EClient
			if(!m_HudLayout.IsOccluded(EHudElement::SPECTATOR_COUNT))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::SPECTATOR_COUNT);
				RenderSpectatorCount();
			}
			if(!m_HudLayout.IsOccluded(EHudElement::MOVEMENT_INFO))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::MOVEMENT_INFO);
				RenderMovementInformation();
			}
			RenderDDRaceEffects();
		}
		else if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			int SpectatorId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
			if(SpectatorId != SPEC_FREEVIEW && g_Config.m_ClShowhudHealthAmmo)
			{
				if(!m_HudLayout.IsOccluded(EHudElement::HEALTH_AMMO))
				{
					CHudLayout::CScope Scope(&m_HudLayout, EHudElement::HEALTH_AMMO); // EClient
					RenderAmmoHealthAndArmor(&GameClient()->m_Snap.m_aCharacters[SpectatorId].m_Cur);
				}
			}
			if(SpectatorId != SPEC_FREEVIEW &&
				GameClient()->m_Snap.m_aCharacters[SpectatorId].m_HasExtendedData &&
				g_Config.m_ClShowhudDDRace &&
				(!GameClient()->m_MultiViewActivated || GameClient()->m_MultiViewShowHud) &&
				GameClient()->m_GameInfo.m_HudDDRace)
			{
				if(!m_HudLayout.IsOccluded(EHudElement::PLAYER_STATE))
				{
					CHudLayout::CScope Scope(&m_HudLayout, EHudElement::PLAYER_STATE); // EClient
					RenderPlayerState(SpectatorId);
				}
			}
			// EClient
			if(!m_HudLayout.IsOccluded(EHudElement::MOVEMENT_INFO))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::MOVEMENT_INFO);
				RenderMovementInformation();
			}
			if(!m_HudLayout.IsOccluded(EHudElement::SPECTATOR_HUD))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::SPECTATOR_HUD);
				RenderSpectatorHud();
			}
		}

		// EClient
		//if(g_Config.m_ClShowhudTimer)
		//	RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		// EClient: unreachable from the playing branch above, so the preview draws it on its own
		if(Preview && !GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			if(!m_HudLayout.IsOccluded(EHudElement::SPECTATOR_HUD))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::SPECTATOR_HUD);
				RenderSpectatorHud();
			}
		}

		// EClient
		if(g_Config.m_ClShowhudScore)
		{
			if(!m_HudLayout.IsOccluded(EHudElement::SCORE))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::SCORE);
				RenderScoreHud();
			}
		}
		if(!m_HudLayout.IsOccluded(EHudElement::DUMMY_ACTIONS))
		{
			CHudLayout::CScope Scope(&m_HudLayout, EHudElement::DUMMY_ACTIONS);
			RenderDummyActions();
		}
		if(!m_HudLayout.IsOccluded(EHudElement::WARMUP_TIMER))
		{
			CHudLayout::CScope Scope(&m_HudLayout, EHudElement::WARMUP_TIMER);
			RenderWarmupTimer();
		}
		if(!m_HudLayout.IsOccluded(EHudElement::FPS))
		{
			CHudLayout::CScope Scope(&m_HudLayout, EHudElement::FPS);
			RenderFps();
		}
		if(!m_HudLayout.IsOccluded(EHudElement::PREDICTION))
		{
			CHudLayout::CScope Scope(&m_HudLayout, EHudElement::PREDICTION);
			RenderPrediction();
		}

		// EClient
		// RenderLocalTime((m_Width / 7) * 3);
		// The scope belongs inside RenderIsland, around the island itself. It cannot go here,
		// because the same function draws the standalone clock and game timer when the island is
		// switched off, and those are not part of this element.
		RenderIsland();

		FreezeHelpers();

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		// EClient
		if(!m_HudLayout.IsOccluded(EHudElement::VOTING))
		{
			CHudLayout::CScope Scope(&m_HudLayout, EHudElement::VOTING);
			GameClient()->m_Voting.Render();
		}
		if(g_Config.m_ClShowRecord)
		{
			if(!m_HudLayout.IsOccluded(EHudElement::RECORD))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::RECORD); // EClient
				RenderRecord();
			}
		}
		// EClient: scopes itself, since it only draws on the frames it has something to say
		GameClient()->m_LocalPractice.RenderMovedAlert(PreviewActive());
	}
	RenderCursor();

	// EClient
	if(ForceOn)
	{
		for(size_t i = 0; i < std::size(apPreviewConfigs); i++)
			*apPreviewConfigs[i] = aPreviewSavedConfigs[i];
	}

	if(Preview)
	{
		GameClient()->m_Snap.m_pGameInfoObj = pPreviewSavedGameInfo;
		GameClient()->m_Snap.m_pSpectatorCount = pPreviewSavedSpectatorCount;
		m_LastSpectatorCountTick = PreviewSavedSpectatorTick;

		Voting.m_Closetime = PreviewSavedVoteClosetime;
		Voting.m_Yes = PreviewSavedVoteYes;
		Voting.m_No = PreviewSavedVoteNo;
		Voting.m_Pass = PreviewSavedVotePass;
		Voting.m_Total = PreviewSavedVoteTotal;
		Voting.m_Voted = PreviewSavedVoteVoted;
		str_copy(Voting.m_aDescription, aPreviewSavedVoteDescription);
		str_copy(Voting.m_aReason, aPreviewSavedVoteReason);
		GameClient()->m_GameInfo = PreviewSavedGameInfo;
		GameClient()->m_MapBestTimeSeconds = PreviewSavedMapBestSeconds;
		GameClient()->m_MapBestTimeMillis = PreviewSavedMapBestMillis;
		GameClient()->m_ReceivedDDNetPlayerFinishTimes = PreviewSavedReceivedFinishTimes;
		m_aPlayerRecord[g_Config.m_ClDummy] = PreviewSavedPlayerRecord;
	}
	if(PreviewCharacter)
	{
		GameClient()->m_aClients[PreviewClientId].m_Predicted = PreviewSavedPredicted;
		GameClient()->m_Snap.m_aCharacters[PreviewClientId] = PreviewSavedSnapCharacter;
	}
}

void CHud::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_DDRACETIME || MsgType == NETMSGTYPE_SV_DDRACETIMELEGACY)
	{
		CNetMsg_Sv_DDRaceTime *pMsg = (CNetMsg_Sv_DDRaceTime *)pRawMsg;

		m_DDRaceTime = pMsg->m_Time;

		m_ShowFinishTime = pMsg->m_Finish != 0;

		if(!m_ShowFinishTime)
		{
			m_TimeCpDiff = (float)pMsg->m_Check / 100;
			m_TimeCpLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
		}
		else
		{
			m_FinishTimeDiff = (float)pMsg->m_Check / 100;
			m_FinishTimeLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RECORD || MsgType == NETMSGTYPE_SV_RECORDLEGACY)
	{
		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;

		// NETMSGTYPE_SV_RACETIME on old race servers
		if(MsgType == NETMSGTYPE_SV_RECORDLEGACY && GameClient()->m_GameInfo.m_DDRaceRecordMessage)
		{
			m_DDRaceTime = pMsg->m_ServerTimeBest; // First value: m_Time

			m_FinishTimeLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);

			if(pMsg->m_PlayerTimeBest) // Second value: m_Check
			{
				m_TimeCpDiff = (float)pMsg->m_PlayerTimeBest / 100;
				m_TimeCpLastReceivedTick = Client()->GameTick(g_Config.m_ClDummy);
			}
		}
		else if(MsgType == NETMSGTYPE_SV_RECORD || GameClient()->m_GameInfo.m_RaceRecordMessage)
		{
			// ignore m_ServerTimeBest, it's handled by the game client
			m_aPlayerRecord[g_Config.m_ClDummy] = (float)pMsg->m_PlayerTimeBest / 100;
		}
	}
}

void CHud::RenderDDRaceEffects()
{
	if(m_DDRaceTime)
	{
		char aBuf[64];
		char aTime[32];
		if(m_ShowFinishTime && m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
		{
			str_time(m_DDRaceTime, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "Finish time: %s", aTime);

			// calculate alpha (4 sec 1 than get lower the next 2 sec)
			float Alpha = 1.0f;
			if(m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 4 < Client()->GameTick(g_Config.m_ClDummy) && m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
			{
				// lower the alpha slowly to blend text out
				Alpha = ((float)(m_FinishTimeLastReceivedTick + Client()->GameTickSpeed() * 6) - (float)Client()->GameTick(g_Config.m_ClDummy)) / (float)(Client()->GameTickSpeed() * 2);
			}

			TextRender()->TextColor(1, 1, 1, Alpha);
			CTextCursor Cursor;
			Cursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(12, aBuf) / 2, 20));
			Cursor.m_FontSize = 12.0f;
			TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);
			if(m_FinishTimeDiff != 0.0f && m_DDRaceEffectsTextContainerIndex.Valid())
			{
				if(m_FinishTimeDiff < 0)
				{
					str_time_float(-m_FinishTimeDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
					str_format(aBuf, sizeof(aBuf), "-%s", aTime);
					TextRender()->TextColor(0.5f, 1.0f, 0.5f, Alpha); // green
				}
				else
				{
					str_time_float(m_FinishTimeDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
					str_format(aBuf, sizeof(aBuf), "+%s", aTime);
					TextRender()->TextColor(1.0f, 0.5f, 0.5f, Alpha); // red
				}
				CTextCursor DiffCursor;
				DiffCursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(10, aBuf) / 2, 34));
				DiffCursor.m_FontSize = 10.0f;
				TextRender()->AppendTextContainer(m_DDRaceEffectsTextContainerIndex, &DiffCursor, aBuf);
			}
			if(m_DDRaceEffectsTextContainerIndex.Valid())
			{
				auto OutlineColor = TextRender()->DefaultTextOutlineColor();
				OutlineColor.a *= Alpha;
				TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OutlineColor);
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		else if(g_Config.m_ClShowhudTimeCpDiff && !m_ShowFinishTime && m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
		{
			if(m_TimeCpDiff < 0)
			{
				str_time_float(-m_TimeCpDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "-%s", aTime);
			}
			else
			{
				str_time_float(m_TimeCpDiff, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "+%s", aTime);
			}

			// calculate alpha (4 sec 1 than get lower the next 2 sec)
			float Alpha = 1.0f;
			if(m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 4 < Client()->GameTick(g_Config.m_ClDummy) && m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6 > Client()->GameTick(g_Config.m_ClDummy))
			{
				// lower the alpha slowly to blend text out
				Alpha = ((float)(m_TimeCpLastReceivedTick + Client()->GameTickSpeed() * 6) - (float)Client()->GameTick(g_Config.m_ClDummy)) / (float)(Client()->GameTickSpeed() * 2);
			}

			if(m_TimeCpDiff > 0)
				TextRender()->TextColor(1.0f, 0.5f, 0.5f, Alpha); // red
			else if(m_TimeCpDiff < 0)
				TextRender()->TextColor(0.5f, 1.0f, 0.5f, Alpha); // green
			else if(!m_TimeCpDiff)
				TextRender()->TextColor(1, 1, 1, Alpha); // white

			float TextY = 12.0f;
			const vec2 IslandPos = this->IslandPos();
			const vec2 IslandSize = this->IslandSize();
			if(IslandSize.x > 0.0f && IslandSize.y > 0.0f)
			{
				const bool OverlapY = TextY < IslandPos.y + IslandSize.y && TextY + 10 > IslandPos.y;
				if(OverlapY)
				{
					constexpr float OverlapPadding = 2.0f;
					TextY = IslandPos.y + IslandSize.y + OverlapPadding;
				}
			}
			// EClient: the frozen count is a HUD element now, so its real rect is what to step past
			if(m_HudLayout.IsLive(EHudElement::FROZEN_TEXT))
			{
				const CHudLayout::CRect FrozenText = m_HudLayout.ResolvedRect(EHudElement::FROZEN_TEXT);
				TextY = std::max(TextY, FrozenText.m_Pos.y + FrozenText.m_Size.y + 2.0f);
			}

			CTextCursor Cursor;
			Cursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(10, aBuf) / 2, TextY));
			Cursor.m_FontSize = 10.0f;
			TextRender()->RecreateTextContainer(m_DDRaceEffectsTextContainerIndex, &Cursor, aBuf);

			if(m_DDRaceEffectsTextContainerIndex.Valid())
			{
				auto OutlineColor = TextRender()->DefaultTextOutlineColor();
				OutlineColor.a *= Alpha;
				TextRender()->RenderTextContainer(m_DDRaceEffectsTextContainerIndex, TextRender()->DefaultTextColor(), OutlineColor);
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}
}

void CHud::RenderRecord()
{
	// EClient: the width follows the time text and the translated label, so it is accumulated from
	// what is actually drawn rather than assumed. The second line is only sometimes there.
	float MaxX = 0.0f;
	float MaxY = 0.0f;
	auto TrackLine = [&](const char *pLabel, const char *pTime, float LineY) {
		MaxX = std::max(MaxX, std::max(5.0f + TextRender()->TextWidth(6.0f, pLabel), 53.0f + TextRender()->TextWidth(6.0f, pTime)));
		MaxY = std::max(MaxY, LineY + 6.0f);
	};

	if(GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET && GameClient()->m_MapBestTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
	{
		char aBuf[64];
		TextRender()->Text(0, 110, 6, Localize("Server best:"), -1.0f);
		char aTime[32];
		int64_t TimeCentiseconds = static_cast<int64_t>(GameClient()->m_MapBestTimeSeconds) * 100 + static_cast<int64_t>(GameClient()->m_MapBestTimeMillis) / 10;
		str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "%s%s", GameClient()->m_MapBestTimeSeconds > 3600 ? "" : "   ", aTime);
		TextRender()->Text(47, 110, 6, aBuf, -1.0f);
		TrackLine(Localize("Server best:"), aBuf, 110.0f); // EClient
	}

	if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
	{
		const int PlayerTimeSeconds = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeSeconds;
		if(PlayerTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
		{
			char aBuf[64];
			TextRender()->Text(0, 117, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			const int PlayerTimeMillis = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeMillis;
			int64_t TimeCentiseconds = static_cast<int64_t>(PlayerTimeSeconds) * 100 + static_cast<int64_t>(PlayerTimeMillis) / 10;
			str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerTimeSeconds > 3600 ? "" : "   ", aTime);
			TextRender()->Text(47, 117, 6, aBuf, -1.0f);
			TrackLine(Localize("Personal best:"), aBuf, 82.0f); // EClient
		}
	}
	else
	{
		const float PlayerRecord = m_aPlayerRecord[g_Config.m_ClDummy];
		if(PlayerRecord > 0.0f)
		{
			char aBuf[64];
			TextRender()->Text(0, 117, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			str_time_float(PlayerRecord, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerRecord > 3600 ? "" : "   ", aTime);
			TextRender()->Text(47, 117, 6, aBuf, -1.0f);
			TrackLine(Localize("Personal best:"), aBuf, 82.0f); // EClient
		}
	}
	// EClient
	if(MaxX > 0.0f)
		m_HudLayout.ReportNaturalRect(EHudElement::RECORD, vec2(0.0f, 110.0f), vec2(MaxX - 5.0f, MaxY - 103.0f));
}

void CHud::FreezeHelpers()
{
	// EClient: where the last alive shout starts from
	constexpr float NOTIFY_LAST_X = 170.0f;

	// render team in freeze text and last notify

	// EClient: it rests in the same spot whether or not it is switched on, so it can be placed
	// before there is ever anything to say
	m_HudLayout.ReportNominalRect(EHudElement::NOTIFY_LAST,
		vec2(NOTIFY_LAST_X, 4.0f), vec2(TextRender()->TextWidth(14.0f, g_Config.m_ClNotifyWhenLastText), 14.0f));

	if(g_Config.m_ClShowFrozenText <= 0 && g_Config.m_ClShowFrozenHud <= 0 && !g_Config.m_ClNotifyWhenLast)
	{
		// The tee row has nowhere to be while every frozen feature is off, and saying so is not the
		// same as never having said, which would leave its last rect standing
		m_HudLayout.ClearNominalRect(EHudElement::FROZEN_TEES);
		return;
	}

	if(!GameClient()->m_GameInfo.m_EntitiesDDRace)
		return;

	int NumInTeam = 0;
	int NumFrozen = 0;
	int LocalTeamID = GameClient()->m_Snap.m_SpecInfo.m_Active == 1 && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != -1 ?
				  GameClient()->m_Teams.Team(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId) :
				  GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[i])
			continue;

		if(GameClient()->m_Teams.Team(i) == LocalTeamID)
		{
			NumInTeam++;
			if(GameClient()->m_aClients[i].m_FreezeEnd > 0 || GameClient()->m_aClients[i].m_DeepFrozen)
				NumFrozen++;
		}
	}

	// EClient: this used to know about the media island and the fps counter by name. It now asks
	// the layout what room every element that is actually on screen leaves it, so anything moved
	// into its band is taken into account without this having to be told about it.
	constexpr float OverlapPadding = 2.0f;
	auto GetSpan = [&](float Y, float Height, float PreferredX, float &Left, float &Right) {
		m_HudLayout.FreeSpanX(EHudElement::FROZEN_TEES, Y, Height, PreferredX, OverlapPadding, Left, Right);
	};

	// EClient: where the tee row goes and how far it may run, worked out once. Both the row itself
	// and the rect handed to the layout come from these, so the box the editor draws and the row it
	// draws cannot drift apart. It starts clear of anything to its left and stops short of anything
	// to its right, which is what keeps it off the island on one side and the kill feed on the
	// other. Vertical overlap is left alone: that is the user's business.
	const float TeeSize = g_Config.m_ClFrozenHudTeeSize;
	const float TeeRowY = 0.0f;
	const float TeeDefaultX = m_Width / 2 + 38.0f * (m_Width / m_Height) / 1.78f - TeeSize / 2.0f;
	float TeeSpanLeft, TeeSpanRight;
	GetSpan(TeeRowY, TeeSize, TeeDefaultX, TeeSpanLeft, TeeSpanRight);
	const float TeeHudX = std::max(TeeDefaultX, TeeSpanLeft);
	const int TeeRoom = std::max(round_truncate((TeeSpanRight - TeeHudX) / TeeSize), 0);

	m_HudLayout.ReportNominalRect(EHudElement::FROZEN_TEES,
		vec2(TeeHudX, TeeRowY), vec2(TeeSize, TeeSize + 3.0f));

	// Notify when last
	if(g_Config.m_ClNotifyWhenLast)
	{
		// EClient: being the last one alive is not something the editor can arrange, so the
		// preview says it is
		if((NumInTeam > 1 && NumInTeam - NumFrozen == 1) || PreviewActive())
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s", g_Config.m_ClNotifyWhenLastText);
			const float FontSize = 14.0f;
			const float NotifyY = 4.0f;
			const float NotifyWidth = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);

			// EClient: an element of its own now, so it is placed by the layout rather than by
			// stepping around whatever it found in the way
			const float NotifyX = NOTIFY_LAST_X;
			m_HudLayout.ReportNaturalRect(EHudElement::NOTIFY_LAST, vec2(NotifyX, NotifyY), vec2(NotifyWidth, FontSize));

			if(!m_HudLayout.IsOccluded(EHudElement::NOTIFY_LAST))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::NOTIFY_LAST);
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClNotifyWhenLastColor)));
				TextRender()->Text(NotifyX, NotifyY, FontSize, aBuf, -1);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}
	}
	// Show freeze text
	char aBuf[64];
	if(g_Config.m_ClShowFrozenText == 1)
		str_format(aBuf, sizeof(aBuf), "%d / %d", NumInTeam - NumFrozen, NumInTeam);
	else if(g_Config.m_ClShowFrozenText == 2)
		str_format(aBuf, sizeof(aBuf), "%d / %d", NumFrozen, NumInTeam);
	// EClient: it used to dodge the island by hand and vanish whenever the scoreboard was open.
	// Both are the layout's job now, so it only says where it sits and how big it is.
	if(g_Config.m_ClShowFrozenText > 0 && !m_HudLayout.IsOccluded(EHudElement::FROZEN_TEXT))
	{
		CHudLayout::CScope Scope(&m_HudLayout, EHudElement::FROZEN_TEXT);

		const float FontSize = 8.0f;
		const float TextWidth = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
		const float TextY = 12.0f;
		const float TextX = m_Width / 2 - TextWidth / 2;

		m_HudLayout.ReportNaturalRect(EHudElement::FROZEN_TEXT, vec2(TextX, TextY), vec2(TextWidth, FontSize));
		TextRender()->Text(TextX, TextY, FontSize, aBuf, -1.0f);
	}

	// I told the clanker to rewrite this
	// EClient: hidden only where it would actually end up underneath the scoreboard, rather than
	// whenever the scoreboard happens to be open
	if(g_Config.m_ClShowFrozenHud > 0 && !m_HudLayout.IsOccluded(EHudElement::FROZEN_TEES) && !(LocalTeamID == 0 && g_Config.m_ClFrozenHudTeamOnly))
	{
		// EClient: this block is its own HUD element. The avoidance further down still works in
		// untransformed coordinates, so it dodges where the island and fps counter naturally sit.
		CHudLayout::CScope Scope(&m_HudLayout, EHudElement::FROZEN_TEES);

		CTeeRenderInfo FreezeInfo;
		const CSkin *pSkin = GameClient()->m_Skins.Find("x_ninja");
		FreezeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
		FreezeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
		FreezeInfo.m_BloodColor = pSkin->m_BloodColor;
		FreezeInfo.m_SkinMetrics = pSkin->m_Metrics;
		FreezeInfo.m_ColorBody = ColorRGBA(1, 1, 1);
		FreezeInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
		FreezeInfo.m_CustomColoredSkin = false;

		int MaxTees = (int)(8.3f * (m_Width / m_Height) * 13.0f / TeeSize);
		if(!g_Config.m_ClShowfps && !g_Config.m_ClShowpred)
			MaxTees = (int)(9.5f * (m_Width / m_Height) * 13.0f / TeeSize);
		int MaxRows = g_Config.m_ClFrozenMaxRows;
		// EClient: the same numbers the rect above was reported from
		const float HudY = TeeRowY;
		const float HudX = TeeHudX;
		MaxTees = std::min(MaxTees, TeeRoom);

		if(MaxTees <= 0)
			return;
		float StartPos = HudX + TeeSize / 2.0f;

		std::vector<int> vDisplayClients;
		vDisplayClients.reserve(MAX_CLIENTS);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameClient()->m_Snap.m_apPlayerInfos[i])
				continue;
			if(GameClient()->m_Teams.Team(i) != LocalTeamID)
				continue;

			if(g_Config.m_ClWarList && g_Config.m_ClWarlistFrozenTeeFlags != 0 && i != GameClient()->m_aLocalIds[0] && i != GameClient()->m_aLocalIds[1])
			{
				const CWarDataCache *pWarData = &GameClient()->m_WarList.GetWarData(i);
				const bool ShowNoneType = IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, 0) && pWarData->m_WarTypeIndex == -1;

				if(!IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, pWarData->m_WarTypeIndex) && !ShowNoneType)
					continue;
			}

			vDisplayClients.push_back(i);
		}

		const int TotalCandidates = (int)vDisplayClients.size();
		if(TotalCandidates == 0)
			return;

		bool Overflow = TotalCandidates > MaxTees * MaxRows;

		// We keep the original semantics: if overflowing, first row(s) are frozen, then non-frozen
		std::vector<int> vOrdered;
		vOrdered.reserve(TotalCandidates);

		if(Overflow)
		{
			// first all frozen
			for(int Idx : vDisplayClients)
			{
				bool Frozen = GameClient()->m_aClients[Idx].m_FreezeEnd > 0 || GameClient()->m_aClients[Idx].m_DeepFrozen;
				if(Frozen)
					vOrdered.push_back(Idx);
			}
			// then all non-frozen
			for(int Idx : vDisplayClients)
			{
				bool Frozen = GameClient()->m_aClients[Idx].m_FreezeEnd > 0 || GameClient()->m_aClients[Idx].m_DeepFrozen;
				if(!Frozen)
					vOrdered.push_back(Idx);
			}
		}
		else
		{
			vOrdered = vDisplayClients;
		}

		const int NumDisplayable = std::min((int)vOrdered.size(), MaxTees * MaxRows);

		int TotalRows = (NumDisplayable + MaxTees - 1) / MaxTees;
		TotalRows = std::min(TotalRows, MaxRows);

		int FirstRowCount = NumDisplayable >= MaxTees ? MaxTees : NumDisplayable;

		const float PanelWidth = TeeSize * FirstRowCount;
		const float PanelHeight = TeeSize + 3.0f + (TotalRows - 1) * TeeSize;

		// EClient: the element is the panel that gets drawn, which the tees sit inside
		m_HudLayout.ReportNaturalRect(EHudElement::FROZEN_TEES, vec2(HudX, HudY), vec2(PanelWidth, PanelHeight));

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(CHudLayout::BackgroundColor());
		Graphics()->DrawRectExt(HudX,
			HudY,
			PanelWidth,
			PanelHeight,
			5.0f,
			m_HudLayout.CornerFlags(EHudElement::FROZEN_TEES)); // EClient
		Graphics()->QuadsEnd();

		float ProgressiveOffset = 0.0f;
		int NumInRow = 0;
		int CurrentRow = 0;

		for(int n = 0; n < NumDisplayable; ++n)
		{
			const int Id = vOrdered[n];

			bool Frozen = GameClient()->m_aClients[Id].m_FreezeEnd > 0 || GameClient()->m_aClients[Id].m_DeepFrozen;

			NumInRow++;
			if(NumInRow > MaxTees)
			{
				NumInRow = 1;
				ProgressiveOffset = 0.0f;
				CurrentRow++;
			}

			CTeeRenderInfo TeeInfo = GameClient()->m_aClients[Id].m_RenderInfo;
			if(Frozen && !g_Config.m_ClShowFrozenHudSkins)
			{
				TeeInfo = FreezeInfo;
			}

			TeeInfo.m_Size = TeeSize;
			const CAnimState *pIdleState = CAnimState::GetIdle();
			vec2 OffsetToMid;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
			vec2 TeeRenderPos(StartPos + ProgressiveOffset, HudY + TeeSize * 0.7f + CurrentRow * TeeSize);
			float Alpha = 1.0f;
			CNetObj_Character CurChar = GameClient()->m_aClients[Id].m_RenderCur;

			if(g_Config.m_ClShowFrozenHudSkins && Frozen)
			{
				Alpha = 0.6f;
				TeeInfo.m_ColorBody.r *= 0.4f;
				TeeInfo.m_ColorBody.g *= 0.4f;
				TeeInfo.m_ColorBody.b *= 0.4f;
				TeeInfo.m_ColorFeet.r *= 0.4f;
				TeeInfo.m_ColorFeet.g *= 0.4f;
				TeeInfo.m_ColorFeet.b *= 0.4f;
			}
			if(Frozen)
				RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_PAIN, vec2(1.0f, 0.0f), TeeRenderPos, Alpha);
			else
				RenderTools()->RenderTee(pIdleState, &TeeInfo, CurChar.m_Emote, vec2(1.0f, 0.0f), TeeRenderPos);

			ProgressiveOffset += TeeSize;
		}
	}
}

bool CHud::CheckpointInfoEnabled()
{
	return (g_Config.m_ClShowhudPlayerCheckpoint && Collision()->HasCheckTele()) || GameClient()->m_HudEditor.IsPreviewing();
}

bool CHud::HasMovementInformationBox()
{
	return g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle || CheckpointInfoEnabled();
}

bool CHud::RenderLocalTime() const
{
	return g_Config.m_ClShowLocalTimeAlways || GameClient()->m_Scoreboard.IsActive();
}

static float RoundedArtInset(float LocalX, float W, float Radius)
{
	if(Radius <= 0.0f || W <= 0.0f)
		return 0.0f;

	if(LocalX < Radius)
	{
		const float X = Radius - LocalX;
		return Radius - sqrtf(std::max(0.0f, Radius * Radius - X * X));
	}
	if(LocalX > W - Radius)
	{
		const float X = LocalX - (W - Radius);
		return Radius - sqrtf(std::max(0.0f, Radius * Radius - X * X));
	}
	return 0.0f;
}

inline bool MediaSourceContainsI(std::string_view Text, std::string_view Needle)
{
	if(Text.empty() || Needle.empty() || Needle.size() > Text.size())
		return false;

	return std::search(Text.begin(), Text.end(), Needle.begin(), Needle.end(),
		       [](char Left, char Right) {
			       return std::tolower((unsigned char)Left) == std::tolower((unsigned char)Right);
		       }) != Text.end();
}

static CArtCropProfile MusicArtCropProfile(std::string_view ServiceId)
{
	CArtCropProfile Profile;
	if(MediaSourceContainsI(ServiceId, "spotify"))
	{
		// Spotify overlays a branded strip/logo near the bottom edge on some covers.
		// Bias the crop downward so the artwork fills the frame and the branding stays outside.
		Profile.m_Bottom = 0.20f;
	}
	return Profile;
}

static void DrawRoundedTexture(IGraphics *pGraphics, const CUIRect &Rect, float Alpha, float Rounding, const CMediaViewer::CAlbumArt &AlbumArt, const CArtCropProfile &CropProfile)
{
	if(pGraphics == nullptr || !AlbumArt.m_Texture.IsValid() || Rect.w <= 0.0f || Rect.h <= 0.0f)
		return;

	const float Radius = std::min(std::min(Rounding, std::min(Rect.w, Rect.h) * 0.5f), 64.0f);
	constexpr int NUM_SLICES = 32;
	float U0 = 0.0f;
	float U1 = 1.0f;
	float V0 = 0.0f;
	float V1 = 1.0f;
	U0 = std::clamp(CropProfile.m_Left, 0.0f, 0.45f);
	U1 = 1.0f - std::clamp(CropProfile.m_Right, 0.0f, 0.45f);
	V0 = std::clamp(CropProfile.m_Top, 0.0f, 0.45f);
	V1 = 1.0f - std::clamp(CropProfile.m_Bottom, 0.0f, 0.45f);

	if(U1 <= U0 || V1 <= V0)
		return;

	if(AlbumArt.m_Width > 0 && AlbumArt.m_Height > 0)
	{
		const float TargetAspect = Rect.w / std::max(Rect.h, 0.001f);
		const float CroppedWidth = AlbumArt.m_Width * (U1 - U0);
		const float CroppedHeight = AlbumArt.m_Height * (V1 - V0);
		const float CroppedAspect = CroppedWidth / std::max(CroppedHeight, 0.001f);

		if(CroppedAspect > TargetAspect)
		{
			const float VisibleWidth = CroppedHeight * TargetAspect;
			const float CropWidth = std::max(CroppedWidth - VisibleWidth, 0.0f);
			const float CropTexels = CropWidth / std::max(AlbumArt.m_Width, 1);
			U0 += CropTexels * 0.5f;
			U1 -= CropTexels * 0.5f;
		}
		else if(CroppedAspect < TargetAspect)
		{
			const float VisibleHeight = CroppedWidth / std::max(TargetAspect, 0.001f);
			const float CropHeight = std::max(CroppedHeight - VisibleHeight, 0.0f);
			const float CropTexels = CropHeight / std::max(AlbumArt.m_Height, 1);
			V0 += CropTexels * 0.5f;
			V1 -= CropTexels * 0.5f;
		}
	}

	pGraphics->TextureSet(AlbumArt.m_Texture);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int i = 0; i < NUM_SLICES; ++i)
	{
		const float SliceT0 = i / (float)NUM_SLICES;
		const float SliceT1 = (i + 1) / (float)NUM_SLICES;
		const float MixU0 = mix(U0, U1, SliceT0);
		const float MixU1 = mix(U0, U1, SliceT1);
		const float LocalX0 = Rect.w * SliceT0;
		const float LocalX1 = Rect.w * SliceT1;
		const float Inset0 = RoundedArtInset(LocalX0, Rect.w, Radius);
		const float Inset1 = RoundedArtInset(LocalX1, Rect.w, Radius);
		const float RenderX0 = Rect.x + LocalX0;
		const float RenderX1 = Rect.x + LocalX1;

		const vec2 TopLeft(RenderX0, Rect.y + Inset0);
		const vec2 TopRight(RenderX1, Rect.y + Inset1);
		const vec2 BottomLeft(RenderX0, Rect.y + Rect.h - Inset0);
		const vec2 BottomRight(RenderX1, Rect.y + Rect.h - Inset1);

		const float RenderV0Top = std::clamp((TopLeft.y - Rect.y) / std::max(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Top = std::clamp((TopRight.y - Rect.y) / std::max(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV0Bottom = std::clamp((BottomLeft.y - Rect.y) / std::max(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Bottom = std::clamp((BottomRight.y - Rect.y) / std::max(Rect.h, 0.001f), 0.0f, 1.0f);
		const float V0Top = mix(V0, V1, RenderV0Top);
		const float V1Top = mix(V0, V1, RenderV1Top);
		const float V0Bottom = mix(V0, V1, RenderV0Bottom);
		const float V1Bottom = mix(V0, V1, RenderV1Bottom);

		pGraphics->QuadsSetSubsetFree(MixU0, V0Top, MixU1, V1Top, MixU0, V0Bottom, MixU1, V1Bottom);
		const IGraphics::CFreeformItem Item(TopLeft, TopRight, BottomLeft, BottomRight);
		pGraphics->QuadsDrawFreeform(&Item, 1);
	}
	pGraphics->QuadsEnd();
	pGraphics->TextureClear();
}

static float MediaIslandEaseInOut(float Progress)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	return Progress * Progress * (3.0f - 2.0f * Progress);
}

static float GetLerpAmount(float DeltaTime)
{
	const float LerpSpeed = 5.0f + g_Config.m_ClMediaIslandAnimation * 0.1f;
	return std::clamp(DeltaTime * LerpSpeed, 0.0f, 1.0f);
}

void CHud::RenderIsland()
{
	const bool MediaIsland = g_Config.m_ClMediaIsland;

	CMediaIsland &Island = m_Island;

	const bool LocalTime = RenderLocalTime();
	const bool HudTimer = g_Config.m_ClShowhudTimer;

	const float CenterX = m_Width * 0.5f; // Center of the island in virtual coordinates
	if(!MediaIsland)
	{
		// EClient: with the island off the clock stands on its own, so it is its own element here.
		// With the island on it is drawn inside it and belongs to race_timer instead, which is why
		// nothing is reported for it down that path.
		{
			const float ClockX = (m_Width / 7) * 3;

			// Only claims a resting position while it is actually set to be shown. Left to itself
			// the clock appears only for as long as the scoreboard is open, and a box standing
			// where it might one day be is just clutter to place things around.
			if(g_Config.m_ClShowLocalTimeAlways)
			{
				const bool Seconds = g_Config.m_EcShowLocalTimeSeconds;
				const float NominalWidth = std::round(TextRender()->TextBoundingBox(5.0f, Seconds ? "00:00.00" : "00:00").m_W);
				m_HudLayout.ReportNominalRect(EHudElement::LOCAL_TIME,
					vec2(ClockX - (NominalWidth + 15.0f), 0.0f), vec2(NominalWidth + 10.0f, 12.5f));
			}
			else
			{
				m_HudLayout.ClearNominalRect(EHudElement::LOCAL_TIME);
			}

			if(!m_HudLayout.IsOccluded(EHudElement::LOCAL_TIME))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::LOCAL_TIME);
				RenderLocalTime(ClockX);
			}
		}
		// EClient: with the island off the race timer stands on its own, and it is the same
		// element either way. It is what race_timer refers to, the island being only how it gets
		// presented, so it stays movable in both.
		if(HudTimer)
		{
			const float TimerWidth = GameTimerWidth(10.0f, GameTimerTime());
			m_HudLayout.ReportNaturalRect(EHudElement::MEDIA_ISLAND,
				vec2(CenterX - TimerWidth * 0.5f, 2.0f), vec2(TimerWidth, 10.0f));

			if(!m_HudLayout.IsOccluded(EHudElement::MEDIA_ISLAND))
			{
				CHudLayout::CScope Scope(&m_HudLayout, EHudElement::MEDIA_ISLAND);
				RenderGameTimer(vec2(CenterX, 2.0f), 10.0f);
			}
		}
		Island.m_AnimProgress = 0.0f;
		Island.ResetPosSize();
		return; // Default rendering
	}

	// EClient: the clock is drawn inside the island from here on, so the standalone element gives
	// up its resting position rather than leaving a box behind where it used to be
	m_HudLayout.ClearNominalRect(EHudElement::LOCAL_TIME);

	// EClient: everything from here down is the island, so this is where its placement applies.
	// The fallback path above returned already and is deliberately left outside it.
	if(m_HudLayout.IsOccluded(EHudElement::MEDIA_ISLAND))
		return;
	const CHudLayout::CScope IslandScope(&m_HudLayout, EHudElement::MEDIA_ISLAND);

	const float DeltaTime = Client()->RenderFrameTime() * 1.2f;

	// Every dimension below is derived from this, so animating the scale itself is what carries the
	// box, the album art, the visualizer and the text sizes along together. Applying the new scale
	// at once instead left the text snapping to its final size while the box was still lerping.
	// EClient: the island is sized through the HUD layout now, like every other element
	const float WantedSizeScale = 1.0f;
	if(!Island.m_Initialized || g_Config.m_ClMediaIslandAnimation == 0)
	{
		Island.m_SizeScale = WantedSizeScale;
	}
	else
	{
		Island.m_SizeScale = std::lerp(Island.m_SizeScale, WantedSizeScale, GetLerpAmount(DeltaTime));
		if(absolute(Island.m_SizeScale - WantedSizeScale) < 0.001f)
			Island.m_SizeScale = WantedSizeScale;
	}
	const float SizeScale = Island.m_SizeScale;

	const float Rounding = 3.5f * SizeScale;
	const float Padding = 3.0f * SizeScale;
	const float IslandY = 1.0f * SizeScale;
	const float ExpandedControlsHeight = 4.0f * SizeScale;
	const float ButtonSize = 8.0f * SizeScale;
	const float ButtonSpacing = 4.0f * SizeScale;
	const float CollapsedMediaSize = 13.0f * SizeScale;
	const float HoveredMediaSize = 17.0f * SizeScale;

	const float GameTimerSize = 8.0f * SizeScale;
	const float LocalTimeSize = 5.0f * SizeScale;

	const float TitleTextSize = 8.0f * SizeScale;
	const float ArtistTextSize = 5.0f * SizeScale;

	CMediaViewer::CState MediaState;
	const bool HasMediaState = MediaIsland && GameClient()->m_MediaViewer.GetStateSnapshot(MediaState);

	// Text measurements are in pixels, so they only need redoing when the scale they were taken
	// at has actually moved.
	const bool SizeChanged = !Island.m_Initialized || absolute(Island.m_TextSizeScale - SizeScale) > 0.001f;
	const bool StateChanged = Island.m_CurState != MediaState || !Island.m_Initialized;
	const bool AlbumArtChanged = !Island.m_Initialized ||
				     Island.m_CurState.m_AlbumArt.m_Texture.Id() != MediaState.m_AlbumArt.m_Texture.Id() ||
				     Island.m_CurState.m_PrevAlbumArt.m_Texture.Id() != MediaState.m_PrevAlbumArt.m_Texture.Id();

	if(StateChanged || AlbumArtChanged || SizeChanged)
	{
		if(!Island.m_Initialized)
		{
			Island.m_CurState = MediaState;
			Island.Reset();
			Island.m_Initialized = true;
		}

		if(StateChanged || SizeChanged)
		{
			Island.m_TitleTextWidth = TextRender()->TextWidth(TitleTextSize, MediaState.m_Title.c_str());
			Island.m_ArtistTextWidth = TextRender()->TextWidth(ArtistTextSize, MediaState.m_Artist.c_str());
			Island.m_TextSizeScale = SizeScale;
		}

		if(StateChanged)
		{
			// Only a different track restarts the scroll. Resizing must not, or the title would
			// sit still for as long as the size animation runs.
			Island.m_TitleScroll.Reset();
			Island.m_ArtistScroll.Reset();
		}

		if(AlbumArtChanged)
		{
			Island.m_Changed = true;
			Island.m_ChangedAnim = 0.0f;
		}

		Island.m_CurState = MediaState;

		Island.m_PrevCropProfile = Island.m_CropProfile;
		Island.m_CropProfile = MusicArtCropProfile(Island.m_CurState.m_ServiceId);
	}

	const bool ShowSeconds = g_Config.m_EcShowLocalTimeSeconds; // TClient
	if(absolute(Island.m_TimeTextSizeScale - SizeScale) > 0.001f || Island.m_TimeTextShowSeconds != ShowSeconds)
	{
		const char *pTimePlaceholder = ShowSeconds ? "00:00.00" : "00:00";
		Island.m_LocalTimeWidth = TextRender()->TextBoundingBox(LocalTimeSize, pTimePlaceholder).m_W;
		Island.m_NoGameTimerLocalTimeWidth = TextRender()->TextBoundingBox(GameTimerSize, pTimePlaceholder).m_W;
		Island.m_TimeTextSizeScale = SizeScale;
		Island.m_TimeTextShowSeconds = ShowSeconds;
	}
	const float LocalTimeWidth = LocalTime ? Island.m_LocalTimeWidth : 0.0f;
	const float NoGameTimerLocalTimeWidth = LocalTime ? Island.m_NoGameTimerLocalTimeWidth : 0.0f;

	const float CollapsedTimerSidePadding = (HudTimer || LocalTime ? 9.0f : 2.0f) * SizeScale;
	const float ExpandedTimerSidePadding = 9.0f * SizeScale;

	float GameTimerWidth = 0.0f;
	if(HudTimer)
	{
		GameTimerWidth = this->GameTimerWidth(GameTimerSize, GameTimerTime());
	}
	if(LocalTime)
	{
		GameTimerWidth = HudTimer ? std::max(GameTimerWidth, LocalTimeWidth) : NoGameTimerLocalTimeWidth;
	}

	const bool ShowArt = HasMediaState;
	const bool ShowVisualizer = HasMediaState && g_Config.m_ClMediaIslandVisualizer && MediaState.m_Visualizer.m_Active;
	const float ArtistTitleWidth = Island.m_ArtistTextWidth > Island.m_TitleTextWidth ? Island.m_ArtistTextWidth : Island.m_TitleTextWidth;
	const float CollapsedWidth = GameTimerWidth;
	const float ExpandedWidth = std::clamp(ArtistTitleWidth, 40.0f * SizeScale, 100.0f * SizeScale);
	const float CollapsedIslandHeight = CollapsedMediaSize + Padding * 2.0f;
	const float ExpandedIslandHeight = HoveredMediaSize + ExpandedControlsHeight + Padding;

	bool HasMousePos = false;
	vec2 MousePos(0.0f, 0.0f);
	const auto ToHudSpaceFromUi = [&](vec2 UiPos) {
		const CUIRect *pUiScreen = Ui()->Screen();
		if(pUiScreen->w <= 0.0f || pUiScreen->h <= 0.0f)
			return vec2(0.0f, 0.0f);
		return vec2(UiPos.x / pUiScreen->w * m_Width, UiPos.y / pUiScreen->h * m_Height);
	};
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(GameClient()->m_Menus.IsMenuActive() && !GameClient()->m_Menus.m_DemoControlsRect.Inside(Ui()->MousePos()) && GameClient()->m_Menus.GetPopup() == GameClient()->m_Menus.POPUP_NONE)
		{
			MousePos = ToHudSpaceFromUi(Ui()->MousePos());
			HasMousePos = true;
		}
	}
	else if(GameClient()->m_Chat.IsActive())
	{
		const vec2 WindowSize((float)Graphics()->WindowWidth(), (float)Graphics()->WindowHeight());
		if(WindowSize.x > 0.0f && WindowSize.y > 0.0f)
		{
			MousePos = GameClient()->m_Chat.m_SelectorMouse / WindowSize * vec2(m_Width, m_Height);
			HasMousePos = true;
		}
	}
	else if(GameClient()->m_Scoreboard.IsActive() && GameClient()->m_Scoreboard.m_MouseUnlocked)
	{
		MousePos = ToHudSpaceFromUi(Ui()->MousePos());
		HasMousePos = true;
	}

	// EClient: every one of those arrives in the base HUD space, while the hover boxes below are
	// built in the island's own coordinates, which the layout may have moved and scaled away from
	// it. Converting here rather than in each branch is what keeps them from disagreeing.
	if(HasMousePos)
		MousePos = m_HudLayout.ToElementSpace(EHudElement::MEDIA_ISLAND, MousePos);

	auto MakeIslandRect = [&](float CurrentTimerWidth, float CurrentHeight, float CurrentMediaSize, float TimerSidePadding, float VisUnavailableWidth) {
		const float LeftWidth = ShowArt ? CurrentMediaSize + TimerSidePadding : 0.0f;
		const float RightWidth = ShowVisualizer ? CurrentMediaSize + TimerSidePadding : VisUnavailableWidth;
		return CUIRect{
			CenterX - CurrentTimerWidth * 0.5f - LeftWidth - Padding,
			IslandY,
			LeftWidth + CurrentTimerWidth + RightWidth + Padding * 2.0f,
			CurrentHeight};
	};

	const CUIRect CollapsedIslandRect = MakeIslandRect(CollapsedWidth, CollapsedIslandHeight, CollapsedMediaSize, CollapsedTimerSidePadding, 0.0f);
	const CUIRect ExpandedIslandRect = MakeIslandRect(ExpandedWidth, ExpandedIslandHeight, HoveredMediaSize, ExpandedTimerSidePadding, ExpandedTimerSidePadding);
	const float HoverPaddingX = 8.0f * SizeScale;
	const float HoverPaddingY = 8.0f * SizeScale;
	const CUIRect CollapsedHoverRect = {
		CollapsedIslandRect.x - HoverPaddingX,
		CollapsedIslandRect.y - HoverPaddingY,
		CollapsedIslandRect.w + HoverPaddingX * 2.0f,
		ExpandedIslandRect.h + HoverPaddingY * 2.0f};
	const CUIRect ExpandedHoverRect = {
		ExpandedIslandRect.x - HoverPaddingX,
		ExpandedIslandRect.y - HoverPaddingY * 1.5f,
		ExpandedIslandRect.w + HoverPaddingX * 2.0f,
		ExpandedIslandRect.h + HoverPaddingY * 3.0f};
	Island.m_Hovered = HasMediaState && HasMousePos && (CollapsedHoverRect.Inside(MousePos) || (Island.m_PrevHovered && ExpandedHoverRect.Inside(MousePos)));
	const bool Hovered = Island.m_Hovered;

	Island.m_VisualState = Hovered ? CMediaIsland::EVisualState::EXPANDED : CMediaIsland::EVisualState::MINIMIZED;

	// How far along the collapsed to expanded transition the island is. Driven by the hover state
	// rather than measured back out of the animated height: the height is only ever chasing its
	// target, so while the island was still catching up to a smaller size setting it read as
	// taller than a collapsed island and the expanded layout switched itself on without the island
	// being hovered at all.
	const float WantedProgress = Hovered ? 1.0f : 0.0f;
	if(!Island.m_Initialized || g_Config.m_ClMediaIslandAnimation == 0)
	{
		Island.m_AnimProgress = WantedProgress;
	}
	else
	{
		Island.m_AnimProgress = std::lerp(Island.m_AnimProgress, WantedProgress, GetLerpAmount(DeltaTime));
		// Snapped once it is close enough, so a collapsed island really reaches zero instead of
		// approaching it forever and leaving the expanded layout permanently switched on.
		if(absolute(Island.m_AnimProgress - WantedProgress) < 0.001f)
			Island.m_AnimProgress = WantedProgress;
	}
	const float Progress = Island.m_AnimProgress;

	// Every part of the layout is placed from this one value, so the box and everything sitting
	// inside it move as a single piece. Giving the box, the album art and the visualizer an
	// animation each meant the contents were chasing a box that was itself still chasing its own
	// target, and they visibly slid around inside it on the way.
	const float CurrentTimerWidth = std::lerp(CollapsedWidth, ExpandedWidth, Progress);
	const float CurrentMediaSize = std::lerp(CollapsedMediaSize, HoveredMediaSize, Progress);
	const float CurrentMainRowHeight = CurrentMediaSize;
	const float CurrentTimerSidePadding = std::lerp(CollapsedTimerSidePadding, ExpandedTimerSidePadding, Progress);

	CUIRect TimerRect = {CenterX - CurrentTimerWidth * 0.5f, IslandY, CurrentTimerWidth, CurrentMainRowHeight};
	const CUIRect IslandRect = {
		std::lerp(CollapsedIslandRect.x, ExpandedIslandRect.x, Progress),
		std::lerp(CollapsedIslandRect.y, ExpandedIslandRect.y, Progress),
		std::lerp(CollapsedIslandRect.w, ExpandedIslandRect.w, Progress),
		std::lerp(CollapsedIslandRect.h, ExpandedIslandRect.h, Progress)};

	const float MinWidth = 7.0f * SizeScale;
	if(IslandRect.w < MinWidth)
	{
		Island.ResetPosSize();
		return;
	}

	// Kept in step for IslandPos() and IslandSize(), which other components lay out against.
	Island.m_Rect.m_Pos = vec2(IslandRect.x, IslandRect.y);
	Island.m_Rect.m_Size = vec2(IslandRect.w, IslandRect.h);

	// EClient: reported so HUD elements can be stacked against the island. It is measure only, so
	// the layout never transforms it.
	m_HudLayout.ReportNaturalRect(EHudElement::MEDIA_ISLAND, Island.m_Rect.m_Pos, Island.m_Rect.m_Size);

	IslandRect.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMediaIslandColor, true)), m_HudLayout.CornerFlags(EHudElement::MEDIA_ISLAND), Rounding); // EClient

	CUIRect ContentRect;
	IslandRect.Margin(Padding, &ContentRect);
	CUIRect MainRowRect = ContentRect;
	CUIRect ControlsRect{};
	const bool ShowExpandedLayout = Progress > 0.0f;
	if(ShowExpandedLayout)
	{
		// Grown with the rest of the layout. Splitting off the full control row the moment the
		// island started expanding shoved the main row up by its whole height in one frame.
		CUIRect MainRowWithGap;
		ContentRect.HSplitBottom(ExpandedControlsHeight * Progress, &MainRowWithGap, &ControlsRect);
		MainRowWithGap.HSplitTop(CurrentMainRowHeight, &MainRowRect, nullptr);
	}

	CUIRect ArtSlot;
	CUIRect VisualizerSlot;
	TimerRect = {CenterX - CurrentTimerWidth * 0.5f, MainRowRect.y, CurrentTimerWidth, MainRowRect.h};
	if(ShowArt)
	{
		ArtSlot = {
			TimerRect.x - CurrentTimerSidePadding - CurrentMediaSize,
			MainRowRect.y,
			CurrentMediaSize,
			MainRowRect.h};
	}
	if(ShowVisualizer)
	{
		VisualizerSlot = {
			TimerRect.x + TimerRect.w + CurrentTimerSidePadding,
			MainRowRect.y,
			CurrentMediaSize,
			MainRowRect.h};
	}

	CUIRect ArtRect = {IslandRect.x, IslandRect.y, 0.0f, IslandRect.h};
	CUIRect VisualizerRect = {IslandRect.x + IslandRect.w, IslandRect.y, 0.0f, IslandRect.h};
	if(HasMediaState)
	{
		ArtSlot.HMargin(std::max((ArtSlot.h - CurrentMediaSize) * 0.5f, 0.0f), &ArtSlot);
		const float HoveredArtExpansion = 1.0f * SizeScale * Progress;
		ArtSlot.w += HoveredArtExpansion;
		ArtSlot.h += HoveredArtExpansion;
		ArtRect = ArtSlot;

		if(ShowVisualizer)
		{
			if(g_Config.m_ClMediaIslandVisualizerAlignment == 2)
				VisualizerSlot.HMargin(std::max((VisualizerSlot.h - CurrentMediaSize) * 0.5f, 0.0f), &VisualizerSlot);
			else
				VisualizerSlot.HSplitBottom(CurrentMediaSize, nullptr, &VisualizerSlot);

			VisualizerRect = VisualizerSlot;
		}
	}

	float TimerYOff = IslandRect.y + 1.5f * SizeScale;

	const float TextPadding = 4.0f * SizeScale;
	const float VerticalClipPadding = 2.0f * SizeScale;

	// Off the animated width like everything else. Switching between the two widths the instant
	// the hover changed made the text clip snap while the box around it was still moving.
	const float TitleWantedWidth = CurrentTimerWidth * Progress;
	const float CurrentAnimatedTextWidth = std::max(TitleWantedWidth, CollapsedWidth);

	CUIRect TitleRect = {TimerRect.x - TextPadding, TimerYOff, TimerRect.w + TextPadding * 2.0f, GameTimerSize};
	CUIRect ArtistRect = {TimerRect.x - TextPadding, TimerYOff + GameTimerSize + 1.0f * SizeScale, TimerRect.w + TextPadding * 2.0f, LocalTimeSize};

	CUIRect ClipRectTitle = {
		TimerRect.Center().x - CurrentAnimatedTextWidth * 0.5f - TextPadding,
		TitleRect.y - VerticalClipPadding,
		CurrentAnimatedTextWidth + TextPadding * 2.0f,
		TitleRect.h + VerticalClipPadding * 2.0f};

	CUIRect ClipRectArtist = {
		TimerRect.Center().x - CurrentAnimatedTextWidth * 0.5f - TextPadding,
		ArtistRect.y - VerticalClipPadding,
		CurrentAnimatedTextWidth + TextPadding * 2.0f,
		ArtistRect.h + VerticalClipPadding * 2.0f};

	const float TextClipLeft = ShowArt ? ArtRect.x + ArtRect.w : IslandRect.x;
	const float TextClipRight = ShowVisualizer ? VisualizerRect.x : IslandRect.x + IslandRect.w;
	const CUIRect TextClipBounds = {
		TextClipLeft,
		IslandRect.y,
		std::max(TextClipRight - TextClipLeft, 0.0f),
		IslandRect.h};

	auto IntersectClipRect = [](const CUIRect &Rect, const CUIRect &Clip) {
		const float X = std::max(Rect.x, Clip.x);
		const float Y = std::max(Rect.y, Clip.y);
		const float Right = std::min(Rect.x + Rect.w, Clip.x + Clip.w);
		const float Bottom = std::min(Rect.y + Rect.h, Clip.y + Clip.h);
		return CUIRect{X, Y, std::max(Right - X, 0.0f), std::max(Bottom - Y, 0.0f)};
	};
	auto RenderClipped = [&](const CUIRect &ClipRect, auto &&RenderFunc) {
		const CUIRect ActualClipRect = IntersectClipRect(ClipRect, TextClipBounds);
		if(ActualClipRect.w <= 0.0f || ActualClipRect.h <= 0.0f)
			return;

		// EClient: ClipEnable wants screen pixels, so it cannot ride the element's screen mapping
		// the way the drawing does
		const vec2 ClipTopLeft = m_HudLayout.ToBaseSpace(EHudElement::MEDIA_ISLAND, vec2(ActualClipRect.x, ActualClipRect.y));
		const float ElementScale = m_HudLayout.ElementScale(EHudElement::MEDIA_ISLAND);
		const float ClipScaleX = Graphics()->ScreenWidth() / m_Width;
		const float ClipScaleY = Graphics()->ScreenHeight() / m_Height;
		const int ClipX = round_to_int(ClipTopLeft.x * ClipScaleX);
		const int ClipY = round_to_int(ClipTopLeft.y * ClipScaleY);
		const int ClipW = round_to_int(ActualClipRect.w * ElementScale * ClipScaleX);
		const int ClipH = round_to_int(ActualClipRect.h * ElementScale * ClipScaleY);
		if(ClipW <= 0 || ClipH <= 0)
			return;

		Graphics()->ClipEnable(ClipX, ClipY, ClipW, ClipH);
		RenderFunc();
		Graphics()->ClipDisable();
	};

	if(Hovered)
	{
		auto RenderScrollingText = [&](const CUIRect &ClipRect, const CUIRect &TextRect, const char *pText, float TextWidth, float FontSize, ColorRGBA Color, CMediaIsland::CTextScrollState &ScrollState) {
			if(pText == nullptr || pText[0] == '\0' || TextRect.w <= 0.0f || TextRect.h <= 0.0f)
			{
				ScrollState.Reset();
				return;
			}

			float TextX = TextRect.Center().x - TextWidth * 0.5f;
			if(TextWidth > TextRect.w)
			{
				const float OverflowWidth = TextWidth - TextRect.w;
				constexpr float ScrollHoldTime = 1.2f;
				const float ScrollSpeed = 18.0f * SizeScale;
				const float FrameTime = std::min(Client()->RenderFrameTime(), 0.1f);
				if(absolute(ScrollState.m_Overflow - OverflowWidth) > 0.01f)
				{
					ScrollState.Reset();
					ScrollState.m_Overflow = OverflowWidth;
					ScrollState.m_HoldTime = ScrollHoldTime;
				}
				else
				{
					ScrollState.m_Overflow = OverflowWidth;
				}

				if(ScrollState.m_HoldTime > 0.0f)
				{
					ScrollState.m_HoldTime = std::max(ScrollState.m_HoldTime - FrameTime, 0.0f);
				}
				else
				{
					const float TravelTime = std::max(OverflowWidth / ScrollSpeed, 0.001f);
					ScrollState.m_Progress = std::min(ScrollState.m_Progress + FrameTime / TravelTime, 1.0f);
					if(ScrollState.m_Progress >= 1.0f)
					{
						ScrollState.m_Progress = 0.0f;
						ScrollState.m_HoldTime = ScrollHoldTime;
						ScrollState.m_Forward = !ScrollState.m_Forward;
					}
				}

				const float EasedProgress = MediaIslandEaseInOut(ScrollState.m_Progress);
				ScrollState.m_Offset = ScrollState.m_Forward ? OverflowWidth * EasedProgress : OverflowWidth * (1.0f - EasedProgress);
				TextX = TextRect.x - ScrollState.m_Offset;
			}
			else
			{
				ScrollState.Reset();
			}

			RenderClipped(ClipRect, [&]() {
				TextRender()->TextColor(Color);
				TextRender()->Text(TextX, TextRect.y, FontSize, pText, -1.0f);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			});
		};

		const char *pTitle = Island.m_CurState.m_Title.c_str();
		const char *pArtist = Island.m_CurState.m_Artist.c_str();

		RenderScrollingText(ClipRectTitle, TitleRect, pTitle, Island.m_TitleTextWidth, TitleTextSize, TextRender()->DefaultTextColor(), Island.m_TitleScroll);
		RenderScrollingText(ClipRectArtist, ArtistRect, pArtist, Island.m_ArtistTextWidth, ArtistTextSize, ColorRGBA(0.6f, 0.6f, 0.8f, 1.0f), Island.m_ArtistScroll);
	}
	else
	{
		if(HudTimer)
		{
			if(!LocalTime)
				TimerYOff += LocalTimeSize - 2.0f * SizeScale;

			const CUIRect GameTimerClipRect = {
				TextClipBounds.x,
				TimerYOff - VerticalClipPadding,
				TextClipBounds.w,
				GameTimerSize + VerticalClipPadding * 2.0f};
			RenderClipped(GameTimerClipRect, [&]() {
				RenderGameTimer(vec2(TimerRect.Center().x, TimerYOff), GameTimerSize, GameTimerClipRect.x + GameTimerClipRect.w);
			});
			TimerYOff += GameTimerSize + 1.0f * SizeScale;
		}
		if(LocalTime)
		{
			char aTimeStr[16];
			str_timestamp_format(aTimeStr, sizeof(aTimeStr), ShowSeconds ? "%H:%M.%S" : "%H:%M");
			if(!HudTimer)
			{
				TextRender()->TextColor(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
				const CUIRect LocalTimeClipRect = {
					TextClipBounds.x,
					IslandRect.y + CollapsedIslandRect.h * 0.25f - VerticalClipPadding,
					TextClipBounds.w,
					GameTimerSize + VerticalClipPadding * 2.0f};
				RenderClipped(LocalTimeClipRect, [&]() {
					const float TextX = TimerRect.Center().x - NoGameTimerLocalTimeWidth * 0.5f;
					TextRender()->Text(TextX, IslandRect.y + CollapsedIslandRect.h * 0.25f, GameTimerSize, aTimeStr, std::max(LocalTimeClipRect.x + LocalTimeClipRect.w - TextX, 0.001f));
				});
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
			else
			{
				TextRender()->TextColor(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
				const CUIRect LocalTimeClipRect = {
					TextClipBounds.x,
					TimerYOff - VerticalClipPadding,
					TextClipBounds.w,
					LocalTimeSize + VerticalClipPadding * 2.0f};
				RenderClipped(LocalTimeClipRect, [&]() {
					const float TextX = TimerRect.Center().x - LocalTimeWidth * 0.5f;
					TextRender()->Text(TextX, TimerYOff, LocalTimeSize, aTimeStr, std::max(LocalTimeClipRect.x + LocalTimeClipRect.w - TextX, 0.001f));
				});
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
		}
	}

	if(HasMediaState)
	{
		if(Island.m_Changed)
		{
			const float LerpAmount = GetLerpAmount(DeltaTime * 0.5f);
			if(Island.m_ChangedAnim >= 0.99f)
			{
				Island.m_ChangedAnim = 1.0f;
				Island.m_Changed = false;
			}
			else
				Island.m_ChangedAnim = std::lerp(Island.m_ChangedAnim, 1.0f, LerpAmount);
		}

		{
			const float ArtRounding = std::min(4.0f * SizeScale, std::min(ArtRect.w, ArtRect.h) * 0.22f);

			const CMediaViewer::CAlbumArt &PrevAlbumArt = Island.m_CurState.m_PrevAlbumArt;
			if(Island.m_Changed && PrevAlbumArt.m_Texture.IsValid())
			{
				DrawRoundedTexture(Graphics(), ArtRect, 1.0f - Island.m_ChangedAnim, ArtRounding, PrevAlbumArt, Island.m_PrevCropProfile);
			}

			if(Island.m_CurState.m_AlbumArt.m_Texture.IsValid())
			{
				// Fades in even with no previous art to cross fade from, so the first cover of a
				// track appears with the island rather than snapping in ahead of it.
				const float Alpha = Island.m_Changed ? Island.m_ChangedAnim : 1.0f;
				DrawRoundedTexture(Graphics(), ArtRect, Alpha, ArtRounding, Island.m_CurState.m_AlbumArt, Island.m_CropProfile);
			}
			else
			{
				const float Alpha = Island.m_Changed ? Island.m_ChangedAnim : 1.0f;

				ArtRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), IGraphics::CORNER_ALL, ArtRounding);
				constexpr const char *pDefaultArtIcon = "♫";
				const STextBoundingBox TextBoundingBox = TextRender()->TextBoundingBox(TitleTextSize, pDefaultArtIcon, -1, -1.0f);

				TextRender()->TextColor(ColorRGBA(0.0f, 0.0f, 0.0f, Alpha));
				TextRender()->Text(ArtRect.Center().x - TextBoundingBox.m_W * 0.5f, ArtRect.Center().y - TextBoundingBox.m_H * 0.5f, TextBoundingBox.m_H, pDefaultArtIcon, -1.0f);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
		}

		if(ShowVisualizer)
		{
			float a = Island.m_Changed ? Island.m_ChangedAnim : 1.0f;

			ColorRGBA PrimaryColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMediaIslandVisualizerColor));
			ColorRGBA SecondaryColor = ColorRGBA(1.0f, 1.0f, 1.0f);

			if(g_Config.m_ClMediaIslandVisualizerColorDynamic)
			{
				if(Island.m_CurState.m_AlbumArt.m_Colors.m_HasPrimary)
					PrimaryColor = color_lerp(Island.m_CurState.m_PrevAlbumArt.m_Colors.GetPrimary(), Island.m_CurState.m_AlbumArt.m_Colors.GetPrimary(), a);
				if(Island.m_CurState.m_AlbumArt.m_Colors.m_HasSecondary)
					SecondaryColor = color_lerp(Island.m_CurState.m_PrevAlbumArt.m_Colors.GetSecondary(), Island.m_CurState.m_AlbumArt.m_Colors.GetSecondary(), a);
			}
			else
				SecondaryColor = PrimaryColor;

			// Fades in alongside the album art instead of appearing at once beside a box that is
			// still growing.
			PrimaryColor.a = a;
			SecondaryColor.a = a;

			RenderVisualizer(MediaState, PrimaryColor, SecondaryColor, VisualizerRect.TopLeft(), VisualizerRect.Size(), 5);
		}

		if(ShowExpandedLayout)
		{
			CUIRect ButtonRow = ControlsRect;
			ButtonRow.HMargin(std::max((ButtonRow.h - ButtonSize) * 0.5f, 0.0f), &ButtonRow);
			const float ButtonRowWidth = ButtonSize * 3.0f + ButtonSpacing * 2.0f;
			ButtonRow = {
				TimerRect.Center().x - ButtonRowWidth * 0.5f,
				ButtonRow.y - Padding * 0.5f,
				ButtonRowWidth,
				ButtonSize};

			CUIRect PrevButton;
			CUIRect PlayPauseButton;
			CUIRect NextButton;
			CUIRect Remaining = ButtonRow;
			Remaining.VSplitLeft(ButtonSize, &PrevButton, &Remaining);
			Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
			Remaining.VSplitLeft(ButtonSize, &PlayPauseButton, &Remaining);
			Remaining.VSplitLeft(ButtonSpacing, nullptr, &Remaining);
			Remaining.VSplitLeft(ButtonSize, &NextButton, nullptr);

			auto DrawControlButton = [&](CButtonContainer *pButtonContainer, const CUIRect &ButtonRect, const char *pIcon, bool Enabled) {
				const bool ButtonHovered = HasMousePos && ButtonRect.Inside(MousePos);
				const float IconSize = 7.0f * SizeScale;

				const bool Holding = ButtonHovered && Ui()->MouseButton(0);
				const bool WasHolding = Ui()->LastMouseButton(0);

				const float ExpandProgress = Island.m_AnimProgress;

				const float Alpha = std::clamp((ExpandProgress - 0.6f) / 0.4f, 0.0f, 1.0f);

				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				const float IconWidth = TextRender()->TextWidth(IconSize, pIcon, -1, -1.0f);
				ColorRGBA IconColor = Enabled ? ColorRGBA(1.0f, 1.0f, 1.0f, ButtonHovered && !Holding ? 0.9f : 0.65f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f);
				ColorRGBA DefaultOutline = TextRender()->DefaultTextOutlineColor();
				TextRender()->TextColor(IconColor.WithMultipliedAlpha(Alpha));
				TextRender()->TextOutlineColor(DefaultOutline.WithMultipliedAlpha(Alpha));
				TextRender()->Text(ButtonRect.Center().x - IconWidth * 0.5f, ButtonRect.Center().y - IconSize * 0.5f, IconSize, pIcon, -1.0f);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->TextOutlineColor(DefaultOutline);
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

				if(WasHolding && !Holding && ButtonHovered)
					return true;

				return false;
			};

			static CButtonContainer s_aButtons[3];
			if(DrawControlButton(&s_aButtons[0], PrevButton, FontIcon::BACKWARD_STEP, MediaState.m_CanPrev))
				GameClient()->m_MediaViewer.Previous();
			if(DrawControlButton(&s_aButtons[1], PlayPauseButton, MediaState.m_Playing ? FontIcon::PAUSE : FontIcon::PLAY, MediaState.m_CanPause || MediaState.m_CanPlay))
				GameClient()->m_MediaViewer.PlayPause();
			if(DrawControlButton(&s_aButtons[2], NextButton, FontIcon::FORWARD_STEP, MediaState.m_CanNext))
				GameClient()->m_MediaViewer.Next();
		}
	}
	Island.m_PrevHovered = false;
	if(Hovered)
		Island.m_PrevHovered = true;
}

void CHud::RenderVisualizer(const CMediaViewer::CState &State, ColorRGBA Primary, ColorRGBA Secondary, vec2 Pos, vec2 Size, int NumBands)
{
	if(!g_Config.m_ClMediaIslandVisualizer || NumBands <= 0)
		return;
	if(!State.m_Visualizer.m_Active)
		return;

	constexpr int TotalBars = CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS;
	// More bars than the analyzer produces would leave the extra ones empty.
	NumBands = std::min(NumBands, TotalBars);

	float aVisualizerBands[TotalBars];
	State.m_Visualizer.GetBands(aVisualizerBands, TotalBars);

	// Fold the analyzer bands down onto the bars actually drawn. The ranges are spread across all
	// of them so nothing at the top of the spectrum falls off when TotalBars is not a multiple of
	// NumBands.
	float aRenderedBands[TotalBars];
	for(int i = 0; i < NumBands; ++i)
	{
		const int Begin = i * TotalBars / NumBands;
		const int End = (i + 1) * TotalBars / NumBands;

		float Sum = 0.0f;
		for(int j = Begin; j < End; ++j)
			Sum += aVisualizerBands[j];
		aRenderedBands[i] = Sum / (End - Begin);
	}

	const float BarWidth = Size.x / NumBands;
	const float BarSpacing = BarWidth * 0.2f;
	const float ActualBarWidth = BarWidth - BarSpacing;
	const float Rounding = std::min(ActualBarWidth * 0.5f, 2.0f);
	const bool AlignCenter = g_Config.m_ClMediaIslandVisualizerAlignment == 2;

	// Bars keep a visible stub at rest so the visualizer still reads as a row of bars when nothing
	// is playing, and stop well short of filling the slot so a peak reads as a peak rather than as
	// the bar running out of room.
	constexpr float MIN_BAR_HEIGHT = 0.20f;
	constexpr float MAX_BAR_HEIGHT = 1.00f;

	// One batch for the whole visualizer instead of a draw call per bar.
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int i = 0; i < NumBands; ++i)
	{
		const float Height = MIN_BAR_HEIGHT + aRenderedBands[i] * (MAX_BAR_HEIGHT - MIN_BAR_HEIGHT);
		const float BarHeight = Height * Size.y;

		const float X = Pos.x + i * BarWidth;
		const float Y = AlignCenter ? Pos.y + (Size.y - BarHeight) * 0.5f : Pos.y + (Size.y - BarHeight);

		// Spread the gradient across the full range, so the last bar really reaches Secondary.
		const float Blend = NumBands > 1 ? i / (float)(NumBands - 1) : 0.0f;

		// DrawRectExt draws its corner arcs without checking they fit, so a radius taller than half
		// the bar folds the top and bottom corners into each other and pinches short bars.
		const float BarRounding = std::min(Rounding, BarHeight * 0.5f);

		Graphics()->SetColor(color_lerp(Primary, Secondary, Blend).WithAlpha(Primary.a));
		Graphics()->DrawRectExt(X, Y, ActualBarWidth, BarHeight, BarRounding, IGraphics::CORNER_ALL);
	}
	Graphics()->QuadsEnd();
}
