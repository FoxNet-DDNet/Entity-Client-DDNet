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
	m_PlayerPrevAngle = -INFINITY;

	for(int i = 0; i < 2; i++)
	{
		m_aPlayerSpeedTextContainers[i].Reset();
		m_aPlayerPrevSpeed[i] = -INFINITY;
		m_aPlayerPositionContainers[i].Reset();
		m_aPlayerPrevPosition[i] = -INFINITY;
	}

	// EClient
	m_Island.Reset();
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
	m_PlayerPrevAngle = -INFINITY;
	for(int i = 0; i < 2; i++)
	{
		TextRender()->DeleteTextContainer(m_aPlayerSpeedTextContainers[i]);
		m_aPlayerPrevSpeed[i] = -INFINITY;
		TextRender()->DeleteTextContainer(m_aPlayerPositionContainers[i]);
		m_aPlayerPrevPosition[i] = -INFINITY;
	}

	// EClient
	m_Island.Reset();
}

void CHud::OnWindowResize()
{
	ResetHudContainers();
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
			Time = (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_Snap.m_pGameInfoObj->m_RoundStartTick) / Client()->GameTickSpeed();
	}
	return Time;
}

void CHud::RenderGameTimer(vec2 Pos, float Size)
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
		TextRender()->Text(Pos.x - w / 2, Pos.y, Size, aBuf, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
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

		if(GameClient()->IsTeamPlay() && GameClient()->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][16];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam[TEAM_RED]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam[TEAM_BLUE]), "%d", GameClient()->m_Snap.m_pGameDataObj->m_TeamscoreBlue);

			bool aRecreateTeamScore[2] = {str_comp(aScoreTeam[0], m_aScoreInfo[0].m_aScoreText) != 0, str_comp(aScoreTeam[1], m_aScoreInfo[1].m_aScoreText) != 0};

			const int aFlagCarrier[2] = {
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierRed,
				GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue};

			bool RecreateRect = ForceScoreInfoInit;
			for(int t = 0; t < 2; t++)
			{
				if(aRecreateTeamScore[t])
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(14.0f, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE], -1, -1.0f);
					str_copy(m_aScoreInfo[t].m_aScoreText, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE]);
					RecreateRect = true;
				}
			}

			static float s_TextWidth100 = TextRender()->TextWidth(14.0f, "100", -1, -1.0f);
			float ScoreWidthMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth100);
			float Split = 3.0f;
			float ImageSize = (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) ? 16.0f : Split;
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
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split, ScoreSingleBoxHeight, 5.0f, IGraphics::CORNER_L);
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
							Cursor.SetPosition(vec2(minimum(m_Width - w - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split), StartY + (t + 1) * 20.0f - 2.0f));
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
						str_time((int64_t)absolute(apPlayerInfo[t]->m_Score) / 10, ETimeFormat::MINS_CENTISECS, aScore[t], sizeof(aScore[t]));
					else if(GameClient()->m_GameInfo.m_TimeScore)
					{
						CGameClient::CClientData &ClientData = GameClient()->m_aClients[apPlayerInfo[t]->m_ClientId];
						if(GameClient()->m_ReceivedDDNetPlayerFinishTimes && ClientData.m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
						{
							int64_t TimeSeconds = static_cast<int64_t>(absolute(ClientData.m_FinishTimeSeconds));
							int64_t TimeMillis = TimeSeconds * 1000 + (absolute(ClientData.m_FinishTimeMillis) % 1000);

							str_time(TimeMillis / 10, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else if(apPlayerInfo[t]->m_Score != FinishTime::NOT_FINISHED_TIMESCORE)
						{
							str_time((int64_t)absolute(apPlayerInfo[t]->m_Score) * 100, ETimeFormat::HOURS, aScore[t], sizeof(aScore[t]));
						}
						else
							aScore[t][0] = 0;
					}
					else
						str_format(aScore[t], sizeof(aScore[t]), "%d", apPlayerInfo[t]->m_Score);
				}
				else
					aScore[t][0] = 0;
			}

			bool RecreateScores = str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0 || str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0 || m_LastLocalClientId != GameClient()->m_Snap.m_LocalClientId;
			m_LastLocalClientId = GameClient()->m_Snap.m_LocalClientId;

			bool RecreateRect = ForceScoreInfoInit;
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

			static float s_TextWidth10 = TextRender()->TextWidth(14.0f, "10", -1, -1.0f);
			float ScoreWidthMax = maximum(maximum(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth10);
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;

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
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = Graphics()->CreateRectQuadContainer(m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split + PosSize, ScoreSingleBoxHeight, 5.0f, IGraphics::CORNER_L);
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
							Cursor.SetPosition(vec2(minimum(m_Width - TextRender()->TextWidth(8.0f, pName) - 1.0f, m_Width - ScoreWidthMax - ImageSize - 2 * Split - PosSize), StartY + (t + 1) * 20.0f - 2.0f));
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
	// render warmup timer
	if(GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer > 0 && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME))
	{
		char aBuf[256];
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(FontSize, Localize("Warmup"), -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() + -w / 2, 50, FontSize, Localize("Warmup"), -1.0f);

		int Seconds = GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer / Client()->GameTickSpeed();
		if(Seconds < 5)
			str_format(aBuf, sizeof(aBuf), "%d.%d", Seconds, (GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer * 10 / Client()->GameTickSpeed()) % 10);
		else
			str_format(aBuf, sizeof(aBuf), "%d", Seconds);
		w = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
		TextRender()->Text(150 * Graphics()->ScreenAspect() + -w / 2, 75, FontSize, aBuf, -1.0f);
	}
}

void CHud::RenderTextInfo()
{
	int Showfps = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		Showfps = 0;
#endif
	char aBuf[16];

	if(Showfps)
	{
		const int FramesPerSecond = round_to_int(1.0f / Client()->FrameTimeAverage());
		str_format(aBuf, sizeof(aBuf), "%d", FramesPerSecond);

		static float s_TextWidth0 = TextRender()->TextWidth(12.f, "0", -1, -1.0f);
		static float s_TextWidth00 = TextRender()->TextWidth(12.f, "00", -1, -1.0f);
		static float s_TextWidth000 = TextRender()->TextWidth(12.f, "000", -1, -1.0f);
		static float s_TextWidth0000 = TextRender()->TextWidth(12.f, "0000", -1, -1.0f);
		static float s_TextWidth00000 = TextRender()->TextWidth(12.f, "00000", -1, -1.0f);
		static const float s_aTextWidth[5] = {s_TextWidth0, s_TextWidth00, s_TextWidth000, s_TextWidth0000, s_TextWidth00000};

		int DigitIndex = GetDigitsIndex(FramesPerSecond, 4);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(m_Width - 10 - s_aTextWidth[DigitIndex], 5));
		Cursor.m_FontSize = 12.0f;
		m_FPSPos = vec2(m_Width - 10 - s_TextWidth00000, 5);
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

	if(g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		str_format(aBuf, sizeof(aBuf), "%d", Client()->GetPredictionTime());
		TextRender()->Text(m_Width - 10.0f - TextRender()->TextWidth(12.0f, aBuf), Showfps ? 20.0f : 5.0f, 12.0f, aBuf, -1.0f);
	}
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
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f, 0, 0, Graphics()->ScreenAspect(), 1.0f, aPoints);
	Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);

	if(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_pLocalCharacter)
	{
		// Render local cursor
		CurWeapon = maximum(0, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Predicted.m_ActiveWeapon);
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
		const vec2 HalfSize = vec2(Center.x - aPoints[0], Center.y - aPoints[1]);
		const vec2 ScreenPos = (GameClient()->m_CursorInfo.WorldTarget() - Center) / GameClient()->m_Camera.m_Zoom;
		const float ClampFactor = maximum(
			1.0f,
			absolute(ScreenPos.x / HalfSize.x),
			absolute(ScreenPos.y / HalfSize.y));

		CurWeapon = maximum(0, GameClient()->m_CursorInfo.Weapon() % NUM_WEAPONS);
		TargetPos = ScreenPos / ClampFactor + Center;
		if(ClampFactor != 1.0f)
			Alpha /= 2.0f;
	}

	// check if cursor is on island, if so, fade it out while island is expanding
	constexpr float Padding = 38.0f;
	const vec2 IslandPosHud = this->IslandPos();
	const vec2 IslandSizeHud = this->IslandSize();
	const float WorldWidth = aPoints[2] - aPoints[0];
	const float WorldHeight = aPoints[3] - aPoints[1];
	const vec2 IslandPos(
		aPoints[0] + (IslandPosHud.x / m_Width) * WorldWidth,
		aPoints[1] + (IslandPosHud.y / m_Height) * WorldHeight);
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
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aCursorOffset[CurWeapon], TargetPos.x, TargetPos.y);
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
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_HealthOffset + QuadOffsetSixup, minimum(pCharacter->m_Health, 10));
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHealthEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_EmptyHealthOffset + QuadOffsetSixup + minimum(pCharacter->m_Health, 10), 10 - minimum(pCharacter->m_Health, 10));

		// armor display
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorFull);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup, minimum(pCharacter->m_Armor, 10));
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteArmorEmpty);
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_ArmorOffset + QuadOffsetSixup + minimum(pCharacter->m_Armor, 10), 10 - minimum(pCharacter->m_Armor, 10));
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
			TotalJumpsToDisplay = maximum(minimum(absolute(pCharacter->m_Jumps), 10), 0);
			AvailableJumpsToDisplay = maximum(minimum(UnusedJumps, TotalJumpsToDisplay), 0);
		}
		else
		{
			TotalJumpsToDisplay = AvailableJumpsToDisplay = absolute(GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData.m_Jumps);
		}

		// render available and used jumps
		int JumpsOffsetY = ((GameClient()->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
				    (GameClient()->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0));
		if(JumpsOffsetY > 0)
		{
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpOffset, AvailableJumpsToDisplay, 0, JumpsOffsetY);
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + AvailableJumpsToDisplay, TotalJumpsToDisplay - AvailableJumpsToDisplay, 0, JumpsOffsetY);
		}
		else
		{
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjump);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpOffset, AvailableJumpsToDisplay);
			Graphics()->TextureSet(GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty);
			Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, m_AirjumpEmptyOffset + AvailableJumpsToDisplay, TotalJumpsToDisplay - AvailableJumpsToDisplay);
		}
	}

	float x = 5 + 12;
	float y = (5 + 12 + (GameClient()->m_GameInfo.m_HudHealthArmor && g_Config.m_ClShowhudHealthAmmo ? 24 : 0) +
		   (GameClient()->m_GameInfo.m_HudAmmo && g_Config.m_ClShowhudHealthAmmo ? 12 : 0));

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

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		StartY -= 4;
	}
	StartY -= GetMovementInformationBoxHeight();

	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56;
	}

	if(g_Config.m_ClShowhudDummyActions && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && Client()->DummyConnected())
	{
		StartY = StartY - 29.0f - 4; // dummy actions height and padding
	}

	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_L, 5.0f);

	float y = StartY + BoxHeight / 3;
	float x = StartX + 2;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->Text(x, y, Fontsize, FontIcon::EYE, -1.0f);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->Text(x + Fontsize + 3.f, y, Fontsize, aBuf, -1.0f);
}

void CHud::RenderDummyActions()
{
	if(!g_Config.m_ClShowhudDummyActions || (GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) || !Client()->DummyConnected())
	{
		return;
	}
	// render small dummy actions hud
	const float BoxHeight = 29.0f;
	const float BoxWidth = 16.0f;

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		StartY -= 4;
	}
	StartY -= GetMovementInformationBoxHeight();

	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56;
	}

	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_L, 5.0f);

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
	if(GameClient()->m_Snap.m_SpecInfo.m_Active && (GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW || GameClient()->m_aClients[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId].m_SpecCharPresent))
		return g_Config.m_ClShowhudPlayerPosition ? 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT + 2.0f : 0.0f;
	float BoxHeight = 3.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * (g_Config.m_ClShowhudPlayerPosition + g_Config.m_ClShowhudPlayerSpeed) + 2.0f * MOVEMENT_INFORMATION_LINE_HEIGHT * g_Config.m_ClShowhudPlayerAngle;
	if(g_Config.m_ClShowhudPlayerPosition || g_Config.m_ClShowhudPlayerSpeed || g_Config.m_ClShowhudPlayerAngle)
	{
		BoxHeight += 2.0f;
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
	if(ClientId == SPEC_FREEVIEW)
	{
		Out.m_Pos = GameClient()->m_Camera.m_Center / 32.0f;
	}
	else if(GameClient()->m_aClients[ClientId].m_SpecCharPresent)
	{
		Out.m_Pos = GameClient()->m_aClients[ClientId].m_SpecChar / 32.0f;
	}
	else
	{
		const CNetObj_Character *pPrevChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
		const CNetObj_Character *pCurChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
		const float IntraTick = Client()->IntraGameTick(Conn);

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
	const bool PosOnly = ClientId == SPEC_FREEVIEW || (GameClient()->m_aClients[ClientId].m_SpecCharPresent);
	// Draw the information depending on settings: Position, speed and target angle
	// This display is only to present the available information from the last snapshot, not to interpolate or predict
	if(!g_Config.m_ClShowhudPlayerPosition && (PosOnly || (!g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle)))
	{
		return;
	}
	const float LineSpacer = 1.0f; // above and below each entry
	const float Fontsize = 6.0f;

	float BoxHeight = GetMovementInformationBoxHeight();
	const float BoxWidth = 62.0f;

	float StartX = m_Width - BoxWidth;
	float StartY = 285.0f - BoxHeight - 4.0f; // 4 units distance to the next display;
	if(g_Config.m_ClShowhudScore)
	{
		StartY -= 56.0f;
	}

	Graphics()->DrawRect(StartX, StartY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_L, 5.0f);

	const CMovementInformation Info = GetMovementInformation(ClientId, g_Config.m_ClDummy);

	float y = StartY + LineSpacer * 2.0f;
	const float LeftX = StartX + 2.0f;
	const float RightX = m_Width - 2.0f;

	if(g_Config.m_ClShowhudPlayerPosition)
	{
		TextRender()->Text(LeftX, y, Fontsize, Localize("Position:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		TextRender()->Text(LeftX, y, Fontsize, "X:", -1.0f);
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[0], Fontsize, Info.m_Pos.x, m_aPlayerPrevPosition[0]);
		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[0], TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		TextRender()->Text(LeftX, y, Fontsize, "Y:", -1.0f);
		UpdateMovementInformationTextContainer(m_aPlayerPositionContainers[1], Fontsize, Info.m_Pos.y, m_aPlayerPrevPosition[1]);
		RenderMovementInformationTextContainer(m_aPlayerPositionContainers[1], TextRender()->DefaultTextColor(), RightX, y);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;
	}

	if(PosOnly)
		return;

	if(g_Config.m_ClShowhudPlayerSpeed)
	{
		TextRender()->Text(LeftX, y, Fontsize, Localize("Speed:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		const char aaCoordinates[][4] = {"X:", "Y:"};
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

	if(g_Config.m_ClShowhudPlayerAngle)
	{
		TextRender()->Text(LeftX, y, Fontsize, Localize("Angle:"), -1.0f);
		y += MOVEMENT_INFORMATION_LINE_HEIGHT;

		UpdateMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, Fontsize, Info.m_Angle, m_PlayerPrevAngle);
		RenderMovementInformationTextContainer(m_PlayerAngleTextContainerIndex, TextRender()->DefaultTextColor(), RightX, y);
	}
}

void CHud::RenderSpectatorHud()
{
	if(!g_Config.m_ClShowhudSpectator)
		return;

	// draw the box
	Graphics()->DrawRect(m_Width - 180.0f, m_Height - 15.0f, 180.0f, 15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_TL, 5.0f);

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

	const bool Seconds = g_Config.m_TcShowLocalTimeSeconds; // TClient

	char aTimeStr[16];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), Seconds ? "%H:%M.%S" : "%H:%M");
	const float Width = std::round(TextRender()->TextBoundingBox(5.0f, aTimeStr).m_W);

	Graphics()->DrawRect(x - (Width + 15.0f), 0.0f, Width + 10.0f, 12.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, 3.75f);
	TextRender()->Text(x - (Width + 10.0f), (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1.0f);

	// Graphics()->DrawRect(x - 30.0f, 0.0f, 25.0f, 12.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, 3.75f);
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
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f * Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

#if defined(CONF_VIDEORECORDER)
	if((IVideo::Current() && g_Config.m_ClVideoShowhud) || (!IVideo::Current() && g_Config.m_ClShowhud))
#else
	if(g_Config.m_ClShowhud)
#endif
	{
		if(GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		{
			if(g_Config.m_ClShowhudHealthAmmo)
			{
				RenderAmmoHealthAndArmor(GameClient()->m_Snap.m_pLocalCharacter);
			}
			if(GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_LocalClientId].m_HasExtendedData && g_Config.m_ClShowhudDDRace && GameClient()->m_GameInfo.m_HudDDRace)
			{
				RenderPlayerState(GameClient()->m_Snap.m_LocalClientId);
			}
			RenderSpectatorCount();
			RenderMovementInformation();
			RenderDDRaceEffects();
		}
		else if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			int SpectatorId = GameClient()->m_Snap.m_SpecInfo.m_SpectatorId;
			if(SpectatorId != SPEC_FREEVIEW && g_Config.m_ClShowhudHealthAmmo)
			{
				RenderAmmoHealthAndArmor(&GameClient()->m_Snap.m_aCharacters[SpectatorId].m_Cur);
			}
			if(SpectatorId != SPEC_FREEVIEW &&
				GameClient()->m_Snap.m_aCharacters[SpectatorId].m_HasExtendedData &&
				g_Config.m_ClShowhudDDRace &&
				(!GameClient()->m_MultiViewActivated || GameClient()->m_MultiViewShowHud) &&
				GameClient()->m_GameInfo.m_HudDDRace)
			{
				RenderPlayerState(SpectatorId);
			}
			RenderMovementInformation();
			RenderSpectatorHud();
		}

		// EClient
		//if(g_Config.m_ClShowhudTimer)
		//	RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		if(g_Config.m_ClShowhudScore)
			RenderScoreHud();
		RenderDummyActions();
		RenderWarmupTimer();
		RenderTextInfo();

		// EClient
		// RenderLocalTime((m_Width / 7) * 3);
		RenderIsland();

		FreezeHelpers();

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		GameClient()->m_Voting.Render();
		if(g_Config.m_ClShowRecord)
			RenderRecord();
	}
	RenderCursor();
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

			CTextCursor Cursor;
			Cursor.SetPosition(vec2(150 * Graphics()->ScreenAspect() - TextRender()->TextWidth(10, aBuf) / 2, 20));
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
	if(GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET && GameClient()->m_MapBestTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
	{
		char aBuf[64];
		TextRender()->Text(5, 75, 6, Localize("Server best:"), -1.0f);
		char aTime[32];
		int64_t TimeCentiseconds = static_cast<int64_t>(GameClient()->m_MapBestTimeSeconds) * 100 + static_cast<int64_t>(GameClient()->m_MapBestTimeMillis) / 10;
		str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), "%s%s", GameClient()->m_MapBestTimeSeconds > 3600 ? "" : "   ", aTime);
		TextRender()->Text(53, 75, 6, aBuf, -1.0f);
	}

	if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
	{
		const int PlayerTimeSeconds = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeSeconds;
		if(PlayerTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
		{
			char aBuf[64];
			TextRender()->Text(5, 82, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			const int PlayerTimeMillis = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_FinishTimeMillis;
			int64_t TimeCentiseconds = static_cast<int64_t>(PlayerTimeSeconds) * 100 + static_cast<int64_t>(PlayerTimeMillis) / 10;
			str_time(TimeCentiseconds, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerTimeSeconds > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf, -1.0f);
		}
	}
	else
	{
		const float PlayerRecord = m_aPlayerRecord[g_Config.m_ClDummy];
		if(PlayerRecord > 0.0f)
		{
			char aBuf[64];
			TextRender()->Text(5, 82, 6, Localize("Personal best:"), -1.0f);
			char aTime[32];
			str_time_float(PlayerRecord, ETimeFormat::HOURS_CENTISECS, aTime, sizeof(aTime));
			str_format(aBuf, sizeof(aBuf), "%s%s", PlayerRecord > 3600 ? "" : "   ", aTime);
			TextRender()->Text(53, 82, 6, aBuf, -1.0f);
		}
	}
}

void CHud::FreezeHelpers()
{
	// render team in freeze text and last notify

	if(g_Config.m_ClShowFrozenText <= 0 && g_Config.m_ClShowFrozenHud <= 0 && !g_Config.m_ClNotifyWhenLast)
		return;

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

	constexpr float OverlapPadding = 2.0f;
	int Showfps = g_Config.m_ClShowfps;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		Showfps = 0;
#endif
	const bool HasFpsRect = Showfps && m_FPSTextContainerIndex.Valid();
	const STextBoundingBox FpsBounds = HasFpsRect ? TextRender()->GetBoundingBoxTextContainer(m_FPSTextContainerIndex) : STextBoundingBox{};
	auto OverlapsY = [](float Y, float Height, float RectY, float RectHeight) {
		return Height > 0.0f && RectHeight > 0.0f && Y < RectY + RectHeight && Y + Height > RectY;
	};
	auto GetReservedRight = [&](float Y, float Height) {
		float ReservedRight = 0.0f;

		vec2 Pos = IslandPos();
		vec2 Size = IslandSize();

		if(Size.x > 0.0f && Size.y > 0.0f && OverlapsY(Y, Height, Pos.y, Size.y))
			ReservedRight = maximum(ReservedRight, Pos.x + Size.x + OverlapPadding);

		return ReservedRight;
	};
	auto GetAvailableRight = [&](float Y, float Height) {
		float AvailableRight = m_Width;
		if(HasFpsRect && OverlapsY(Y, Height, m_FPSPos.y, FpsBounds.m_H))
		{
			AvailableRight = minimum(AvailableRight, m_FPSPos.x - OverlapPadding);
		}
		return AvailableRight;
	};
	auto PlaceRightOfReserved = [&](float DefaultX, float Y, float Width, float Height) {
		const float ReservedRight = GetReservedRight(Y, Height);
		const float AvailableRight = GetAvailableRight(Y, Height);
		float X = DefaultX;
		if(X < ReservedRight && X + Width > ReservedRight)
		{
			X = ReservedRight;
		}
		if(X + Width > AvailableRight)
		{
			X = maximum(ReservedRight, AvailableRight - Width);
		}
		return X;
	};

	// Notify when last
	if(g_Config.m_ClNotifyWhenLast)
	{
		if(NumInTeam > 1 && NumInTeam - NumFrozen == 1)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s", g_Config.m_ClNotifyWhenLastText);
			const float FontSize = 14.0f;
			const float NotifyY = 4.0f;
			const float NotifyWidth = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
			const float NotifyX = PlaceRightOfReserved(170.0f, NotifyY, NotifyWidth, FontSize);
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClNotifyWhenLastColor)));
			TextRender()->Text(NotifyX, NotifyY, FontSize, aBuf, -1);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	// Show freeze text
	char aBuf[64];
	if(g_Config.m_ClShowFrozenText == 1)
		str_format(aBuf, sizeof(aBuf), "%d / %d", NumInTeam - NumFrozen, NumInTeam);
	else if(g_Config.m_ClShowFrozenText == 2)
		str_format(aBuf, sizeof(aBuf), "%d / %d", NumFrozen, NumInTeam);
	if(g_Config.m_ClShowFrozenText > 0)
	{
		const float FontSize = 10.0f;
		const float TextWidth = TextRender()->TextWidth(FontSize, aBuf, -1, -1.0f);
		const float TextY = 12.0f;
		const float TextX = PlaceRightOfReserved(m_Width / 2 - TextWidth / 2, TextY, TextWidth, FontSize);
		TextRender()->Text(TextX, TextY, FontSize, aBuf, -1.0f);
	}

	// I told the clanker to rewrite this
	if(g_Config.m_ClShowFrozenHud > 0 && !GameClient()->m_Scoreboard.IsActive() && !(LocalTeamID == 0 && g_Config.m_ClFrozenHudTeamOnly))
	{
		CTeeRenderInfo FreezeInfo;
		const CSkin *pSkin = GameClient()->m_Skins.Find("x_ninja");
		FreezeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
		FreezeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
		FreezeInfo.m_BloodColor = pSkin->m_BloodColor;
		FreezeInfo.m_SkinMetrics = pSkin->m_Metrics;
		FreezeInfo.m_ColorBody = ColorRGBA(1, 1, 1);
		FreezeInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
		FreezeInfo.m_CustomColoredSkin = false;

		float TeeSize = g_Config.m_ClFrozenHudTeeSize;
		int MaxTees = (int)(8.3f * (m_Width / m_Height) * 13.0f / TeeSize);
		if(!g_Config.m_ClShowfps && !g_Config.m_ClShowpred)
			MaxTees = (int)(9.5f * (m_Width / m_Height) * 13.0f / TeeSize);
		int MaxRows = g_Config.m_ClFrozenMaxRows;
		const float DefaultHudX = m_Width / 2 + 38.0f * (m_Width / m_Height) / 1.78f - TeeSize / 2.0f;
		const float HudY = 0.0f;
		const float ReservedRight = GetReservedRight(HudY, TeeSize);
		const float AvailableRight = GetAvailableRight(HudY, TeeSize);
		const float HudX = maximum(DefaultHudX, ReservedRight);
		MaxTees = minimum(MaxTees, maximum(round_truncate((AvailableRight - HudX) / TeeSize), 0));
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

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
		Graphics()->DrawRectExt(HudX,
			HudY,
			TeeSize * FirstRowCount,
			TeeSize + 3.0f + (TotalRows - 1) * TeeSize,
			5.0f,
			IGraphics::CORNER_B);
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
		return Radius - sqrtf(maximum(0.0f, Radius * Radius - X * X));
	}
	if(LocalX > W - Radius)
	{
		const float X = LocalX - (W - Radius);
		return Radius - sqrtf(maximum(0.0f, Radius * Radius - X * X));
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

	const float Radius = minimum(minimum(Rounding, minimum(Rect.w, Rect.h) * 0.5f), 64.0f);
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
		const float TargetAspect = Rect.w / maximum(Rect.h, 0.001f);
		const float CroppedWidth = AlbumArt.m_Width * (U1 - U0);
		const float CroppedHeight = AlbumArt.m_Height * (V1 - V0);
		const float CroppedAspect = CroppedWidth / maximum(CroppedHeight, 0.001f);

		if(CroppedAspect > TargetAspect)
		{
			const float VisibleWidth = CroppedHeight * TargetAspect;
			const float CropWidth = maximum(CroppedWidth - VisibleWidth, 0.0f);
			const float CropTexels = CropWidth / maximum(AlbumArt.m_Width, 1);
			U0 += CropTexels * 0.5f;
			U1 -= CropTexels * 0.5f;
		}
		else if(CroppedAspect < TargetAspect)
		{
			const float VisibleHeight = CroppedWidth / maximum(TargetAspect, 0.001f);
			const float CropHeight = maximum(CroppedHeight - VisibleHeight, 0.0f);
			const float CropTexels = CropHeight / maximum(AlbumArt.m_Height, 1);
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

		const float RenderV0Top = std::clamp((TopLeft.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Top = std::clamp((TopRight.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV0Bottom = std::clamp((BottomLeft.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Bottom = std::clamp((BottomRight.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
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

static float GetMediaIslandSizeScale(int MediaIslandSize)
{
	return std::clamp(1.0f + (MediaIslandSize - DefaultConfig::ClMediaIslandSize) * 0.1f, 0.5f, 1.5f);
}

static ColorRGBA LerpColor(const ColorRGBA &A, const ColorRGBA &B, float Amount)
{
	Amount = std::clamp(Amount, 0.0f, 1.0f);
	return ColorRGBA(
		std::lerp(A.r, B.r, Amount),
		std::lerp(A.g, B.g, Amount),
		std::lerp(A.b, B.b, Amount),
		A.a);
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
		RenderLocalTime((m_Width / 7) * 3);
		if(HudTimer)
			RenderGameTimer(vec2(CenterX, 2.0f), 10.0f);
		Island.m_AnimProgress = 0.0f;
		Island.ResetPosSize();
		return; // Default rendering
	}

	const int MediaIslandSize = g_Config.m_ClMediaIslandSize;
	const float SizeScale = GetMediaIslandSizeScale(MediaIslandSize);

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

	const bool SizeChanged = !Island.m_Initialized || absolute(Island.m_SizeScale - SizeScale) > 0.001f;
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
			Island.m_TitleScroll.Reset();
			Island.m_ArtistScroll.Reset();
		}

		if(AlbumArtChanged)
		{
			Island.m_Changed = true;
			Island.m_ChangedAnim = 0.0f;
		}

		Island.m_CurState = MediaState;
		Island.m_SizeScale = SizeScale;

		Island.m_PrevCropProfile = Island.m_CropProfile;
		Island.m_CropProfile = MusicArtCropProfile(Island.m_CurState.m_ServiceId);
	}

	const bool ShowSeconds = g_Config.m_TcShowLocalTimeSeconds; // TClient
	const float LocalTimeWidth = LocalTime ? TextRender()->TextBoundingBox(LocalTimeSize, ShowSeconds ? "00:00.00" : "00:00").m_W : 0.0f;
	const float NoGameTimerLocalTimeWidth = LocalTime ? TextRender()->TextBoundingBox(GameTimerSize, ShowSeconds ? "00:00.00" : "00:00").m_W : 0.0f;

	float TimerSidePadding = (HudTimer || LocalTime ? 9.0f : 2.0f) * SizeScale;

	float GameTimerWidth = 0.0f;
	if(HudTimer)
	{
		char aBuf[64];
		int Time = GameTimerTime();
		str_time((int64_t)Time * 100, ETimeFormat::DAYS, aBuf, sizeof(aBuf));
		GameTimerWidth = this->GameTimerWidth(GameTimerSize, Time);
	}
	if(LocalTime)
	{
		GameTimerWidth = HudTimer ? maximum(GameTimerWidth, LocalTimeWidth) : NoGameTimerLocalTimeWidth;
	}

	const bool ShowArt = HasMediaState;
	const bool ShowVisualizer = HasMediaState && g_Config.m_ClMediaIslandVisualizer && MediaState.m_Visualizer.m_Active;
	const float ArtistTitleWidth = Island.m_ArtistTextWidth > Island.m_TitleTextWidth ? Island.m_ArtistTextWidth : Island.m_TitleTextWidth;
	const float CollapsedWidth = GameTimerWidth;
	const float ExpandedWidth = std::clamp(ArtistTitleWidth, 40.0f * SizeScale, 100.0f * SizeScale);
	const float CollapsedIslandHeight = CollapsedMediaSize + Padding * 2.0f;
	const float ExpandedIslandHeight = HoveredMediaSize + ExpandedControlsHeight + Padding;
	const float DeltaTime = Client()->RenderFrameTime() * 1.2f;

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

	auto MakeIslandRect = [&](float CurrentTimerWidth, float CurrentHeight, float CurrentMediaSize, float VisUnavailableWidth) {
		const float LeftWidth = ShowArt ? CurrentMediaSize + TimerSidePadding : 0.0f;
		const float RightWidth = ShowVisualizer ? CurrentMediaSize + TimerSidePadding : VisUnavailableWidth;
		return CUIRect{
			CenterX - CurrentTimerWidth * 0.5f - LeftWidth - Padding,
			IslandY,
			LeftWidth + CurrentTimerWidth + RightWidth + Padding * 2.0f,
			CurrentHeight};
	};

	const CUIRect CollapsedIslandRect = MakeIslandRect(CollapsedWidth, CollapsedIslandHeight, CollapsedMediaSize, 0.0f);
	const CUIRect ExpandedIslandRect = MakeIslandRect(ExpandedWidth, ExpandedIslandHeight, HoveredMediaSize, TimerSidePadding);
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

	const float CurrentTimerWidth = Hovered ? ExpandedWidth : CollapsedWidth;
	const float CurrentMediaSize = Hovered ? HoveredMediaSize : CollapsedMediaSize;
	const float CurrentMainRowHeight = CurrentMediaSize;

	CUIRect TimerRect = {CenterX - CurrentTimerWidth * 0.5f, IslandY, CurrentTimerWidth, CurrentMainRowHeight};
	const CUIRect WantedIslandRect = Hovered ? ExpandedIslandRect : CollapsedIslandRect;

	const float MinWidth = 7.0f * SizeScale;
	if(WantedIslandRect.w < MinWidth)
	{
		Island.ResetPosSize();
		return;
	}

	Island.m_Rect.m_WantedPos = vec2(WantedIslandRect.x, WantedIslandRect.y);
	Island.m_Rect.m_WantedSize = vec2(WantedIslandRect.w, WantedIslandRect.h);
	Island.m_Rect.Update(DeltaTime);

	const float HeightRange = ExpandedIslandRect.h - CollapsedIslandRect.h;
	if(HeightRange > 0.0f)
	{
		Island.m_AnimProgress = std::clamp((Island.m_Rect.m_Size.y - CollapsedIslandRect.h) / HeightRange, 0.0f, 1.0f);
	}
	else
	{
		Island.m_AnimProgress = Hovered ? 1.0f : 0.0f;
	}

	CUIRect IslandRect = {Island.m_Rect.m_Pos.x, Island.m_Rect.m_Pos.y, Island.m_Rect.m_Size.x, Island.m_Rect.m_Size.y};
	IslandRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_ALL, Rounding);

	CUIRect ContentRect;
	IslandRect.Margin(Padding, &ContentRect);
	CUIRect MainRowRect = ContentRect;
	CUIRect ControlsRect;
	const bool ShowExpandedLayout = Hovered || Island.m_AnimProgress > 0.0f;
	if(ShowExpandedLayout)
	{
		CUIRect MainRowWithGap;
		ContentRect.HSplitBottom(ExpandedControlsHeight, &MainRowWithGap, &ControlsRect);
		MainRowWithGap.HSplitTop(CurrentMainRowHeight, &MainRowRect, nullptr);
	}

	CUIRect ArtSlot;
	CUIRect VisualizerSlot;
	TimerRect = {CenterX - CurrentTimerWidth * 0.5f, MainRowRect.y, CurrentTimerWidth, MainRowRect.h};
	if(ShowArt)
	{
		ArtSlot = {
			TimerRect.x - TimerSidePadding - CurrentMediaSize,
			MainRowRect.y,
			CurrentMediaSize,
			MainRowRect.h};
	}
	if(ShowVisualizer)
	{
		VisualizerSlot = {
			TimerRect.x + TimerRect.w + TimerSidePadding,
			MainRowRect.y,
			CurrentMediaSize,
			MainRowRect.h};
	}

	float TimerYOff = IslandRect.y + 1.5f * SizeScale;

	const float TextPadding = 4.0f * SizeScale;
	const float VerticalClipPadding = 2.0f * SizeScale;

	float TitleWantedWidth = (Hovered ? ExpandedWidth : CollapsedWidth);
	TitleWantedWidth *= std::clamp(Island.m_AnimProgress, 0.0f, 1.0f);
	const float CurrentAnimatedTextWidth = maximum(TitleWantedWidth, CollapsedWidth);

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
				const float FrameTime = minimum(Client()->RenderFrameTime(), 0.1f);
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
					ScrollState.m_HoldTime = maximum(ScrollState.m_HoldTime - FrameTime, 0.0f);
				}
				else
				{
					const float TravelTime = maximum(OverflowWidth / ScrollSpeed, 0.001f);
					ScrollState.m_Progress = minimum(ScrollState.m_Progress + FrameTime / TravelTime, 1.0f);
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

			const float ClipScaleX = Graphics()->ScreenWidth() / m_Width;
			const float ClipScaleY = Graphics()->ScreenHeight() / m_Height;
			Graphics()->ClipEnable(
				round_to_int(ClipRect.x * ClipScaleX),
				round_to_int(ClipRect.y * ClipScaleY),
				round_to_int(ClipRect.w * ClipScaleX),
				round_to_int(ClipRect.h * ClipScaleY));
			TextRender()->TextColor(Color);
			TextRender()->Text(TextX, TextRect.y, FontSize, pText, -1.0f);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Graphics()->ClipDisable();
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

			RenderGameTimer(vec2(TimerRect.Center().x, TimerYOff), GameTimerSize);
			TimerYOff += GameTimerSize + 1.0f * SizeScale;
		}
		if(LocalTime)
		{
			char aTimeStr[16];
			str_timestamp_format(aTimeStr, sizeof(aTimeStr), ShowSeconds ? "%H:%M.%S" : "%H:%M");
			if(!HudTimer)
			{
				TextRender()->TextColor(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
				TextRender()->Text(TimerRect.Center().x - NoGameTimerLocalTimeWidth * 0.5f, IslandRect.y + CollapsedIslandRect.h * 0.25f, GameTimerSize, aTimeStr, -1.0f);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
			else
			{
				TextRender()->TextColor(ColorRGBA(0.7f, 0.7f, 0.7f, 1.0f));
				TextRender()->Text(TimerRect.Center().x - LocalTimeWidth * 0.5f, TimerYOff, LocalTimeSize, aTimeStr, -1.0f);
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
			ArtSlot.HMargin(maximum((ArtSlot.h - CurrentMediaSize) * 0.5f, 0.0f), &ArtSlot);
			if(Hovered)
			{
				const float HoveredArtExpansion = 1.0f * SizeScale;
				ArtSlot.w += HoveredArtExpansion;
				ArtSlot.h += HoveredArtExpansion;
			}
			Island.m_AlbumArt.m_WantedPos = vec2(ArtSlot.x, ArtSlot.y);
			Island.m_AlbumArt.m_WantedSize = vec2(ArtSlot.w, ArtSlot.h);

			Island.m_AlbumArt.Update(DeltaTime);
			CUIRect ArtRect = {Island.m_AlbumArt.m_Pos.x, Island.m_AlbumArt.m_Pos.y, Island.m_AlbumArt.m_Size.x, Island.m_AlbumArt.m_Size.y};

			const float ArtRounding = minimum(4.0f * SizeScale, minimum(ArtRect.w, ArtRect.h) * 0.22f);

			const CMediaViewer::CAlbumArt &PrevAlbumArt = Island.m_CurState.m_PrevAlbumArt;
			if(Island.m_Changed && PrevAlbumArt.m_Texture.IsValid())
			{
				DrawRoundedTexture(Graphics(), ArtRect, 1.0f - Island.m_ChangedAnim, ArtRounding, PrevAlbumArt, Island.m_PrevCropProfile);
			}

			if(Island.m_CurState.m_AlbumArt.m_Texture.IsValid())
			{
				const float Alpha = Island.m_Changed && PrevAlbumArt.m_Texture.IsValid() ? Island.m_ChangedAnim : 1.0f;
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
			if(g_Config.m_ClMediaIslandVisualizerAlignment == 2)
				VisualizerSlot.HMargin(maximum((VisualizerSlot.h - CurrentMediaSize) * 0.5f, 0.0f), &VisualizerSlot);
			else
				VisualizerSlot.HSplitBottom(CurrentMediaSize, nullptr, &VisualizerSlot);

			Island.m_Visualizer.m_WantedPos = vec2(VisualizerSlot.x, VisualizerSlot.y);
			Island.m_Visualizer.m_WantedSize = vec2(VisualizerSlot.w, VisualizerSlot.h);

			Island.m_Visualizer.Update(DeltaTime);

			CUIRect VisualizerRect = {Island.m_Visualizer.m_Pos.x, Island.m_Visualizer.m_Pos.y, Island.m_Visualizer.m_Size.x, Island.m_Visualizer.m_Size.y};

			float a = Island.m_Changed ? Island.m_ChangedAnim : 1.0f;

			ColorRGBA PrimaryColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMediaIslandVisualizerColor));
			ColorRGBA SecondaryColor = ColorRGBA(1.0f, 1.0f, 1.0f);

			if(g_Config.m_ClMediaIslandVisualizerColorDynamic)
			{
				if(Island.m_CurState.m_AlbumArt.m_Colors.m_HasPrimary)
					PrimaryColor = LerpColor(Island.m_CurState.m_PrevAlbumArt.m_Colors.GetPrimary(), Island.m_CurState.m_AlbumArt.m_Colors.GetPrimary(), a);
				if(Island.m_CurState.m_AlbumArt.m_Colors.m_HasSecondary)
					SecondaryColor = LerpColor(Island.m_CurState.m_PrevAlbumArt.m_Colors.GetSecondary(), Island.m_CurState.m_AlbumArt.m_Colors.GetSecondary(), a);
			}
			else
				SecondaryColor = PrimaryColor;

			RenderVisualizer(MediaState, PrimaryColor, SecondaryColor, VisualizerRect.TopLeft(), VisualizerRect.Size(), 5);
		}

		if(ShowExpandedLayout)
		{
			CUIRect ButtonRow = ControlsRect;
			ButtonRow.HMargin(maximum((ButtonRow.h - ButtonSize) * 0.5f, 0.0f), &ButtonRow);
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

	const float BarWidth = Size.x / NumBands;
	const float BarSpacing = BarWidth * 0.2f;
	const float ActualBarWidth = BarWidth - BarSpacing;

	float aVisualizerBands[TotalBars];
	std::vector<float> vRenderedBands(NumBands);
	vRenderedBands.reserve(NumBands);

	State.m_Visualizer.GetBands(aVisualizerBands, TotalBars);
	// Average bands into rendered bands
	for(int i = 0; i < NumBands; ++i)
	{
		float Sum = 0.0f;
		int Count = 0;
		for(int j = i * (TotalBars / NumBands); j < (i + 1) * (TotalBars / NumBands) && j < TotalBars; ++j)
		{
			Sum += aVisualizerBands[j];
			Count++;
		}
		vRenderedBands[i] = Count > 0 ? Sum / Count : 0.0f;
	}

	for(int i = 0; i < NumBands; ++i)
	{
		const bool AlignCenter = g_Config.m_ClMediaIslandVisualizerAlignment == 2;
		float Height = 0.2f + vRenderedBands[i] * 0.8f;
		float BarHeight = Height * Size.y;

		float X = Pos.x + i * BarWidth;
		float Y = AlignCenter ? Pos.y + (Size.y - BarHeight) * 0.5f : Pos.y + (Size.y - BarHeight);

		float a = i / (float)NumBands;

		ColorRGBA Color = LerpColor(Primary, Secondary, a);

		float Rounding = minimum(ActualBarWidth * 0.5f, 2.0f);

		Graphics()->DrawRect(X, Y, ActualBarWidth, BarHeight, Color, IGraphics::CORNER_ALL, Rounding);
	}
}

void CHud::CMediaIsland::CPosSize::Update(float DeltaTime)
{
	if(!m_Initialized || g_Config.m_ClMediaIslandAnimation == 0)
	{
		m_Pos = m_WantedPos;
		m_Size = m_WantedSize;
		m_Initialized = true;
	}
	else
	{
		const float LerpAmount = GetLerpAmount(DeltaTime);
		m_Pos = vec2(
			std::lerp(m_Pos.x, m_WantedPos.x, LerpAmount),
			std::lerp(m_Pos.y, m_WantedPos.y, LerpAmount));
		m_Size = vec2(
			std::lerp(m_Size.x, m_WantedSize.x, LerpAmount),
			std::lerp(m_Size.y, m_WantedSize.y, LerpAmount));

		if(distance(m_Pos, m_WantedPos) < 0.01f)
			m_Pos = m_WantedPos;
		if(distance(m_Size, m_WantedSize) < 0.01f)
			m_Size = m_WantedSize;
	}
}
