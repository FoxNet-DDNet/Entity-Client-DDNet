#include <engine/client.h>
#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include <game/collision.h>
#include <game/generated/client_data.h>
#include <game/mapitems.h>

#include "aiodob.h"
#include <game/client/prediction/entities/character.h>
#include <libavutil/mathematics.h>

void CAiodob::OnInit()
{
	AttempedJoinTeam = false;
	JoinedTeam = false;
	m_SentKill = false;
	m_KogModeRebound = false;
	Mult = 0;
	m_JoinTeam = 0;
	m_LastFreeze = 0;
	m_LastMovement = 0;
	dbg_msg("Aiodob", "Aiodob Client Features Loaded Successfully!");

	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot("mouse1");
	*g_Config.m_ClKogModeSaved = *GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "KoG Mode Saved Bind Currently is: %s", g_Config.m_ClKogModeSaved);
	dbg_msg("Aiodob", aBuf);
}

void CAiodob::FreezeKill()
{
	float Time = g_Config.m_ClFreezeKillMs / 1000.0f;

	float TimeReset = time_get() + time_freq() * Time;

	if(!g_Config.m_ClFreezeKill)
	{
		m_LastFreeze = TimeReset;
		return;
	}

	if(!GameClient()->CurrentRaceTime())
	{
		m_SentKill = false;
		m_LastFreeze = TimeReset + 3;
		return;
	}
	
	if(g_Config.m_ClFreezeKillMultOnly)
		if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
			return;


	if(g_Config.m_ClFreezeKillDebug)
	{
		float a = (m_LastFreeze - time_get()) / 1000000000.0f;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "until kill: %f", a );
		GameClient()->TextRender()->Text(50, 100, 10, aBuf);
	}
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int Local = m_pClient->m_Snap.m_LocalClientId;

		CCharacterCore *pCharacterOther = &m_pClient->m_aClients[i].m_Predicted;

		CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

		vec2 Position = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_RenderPos;
		CGameClient::CClientData OtherTee = m_pClient->m_aClients[i];
		int Distance = g_Config.m_ClFreezeKillTeamDistance * 100;

		if(m_SentKill == true)
			return;

		if(m_pClient->m_aClients[Local].m_Paused || m_pClient->m_aClients[Local].m_Spec)
			m_LastFreeze = TimeReset;

		if((pCharacter->m_IsInFreeze || m_pClient->m_aClients[Local].m_FreezeEnd > 0) && i == Local && g_Config.m_ClFreezeDontKillMoving)
		{
			if(!m_pClient->m_Menus.IsActive() || !m_pClient->m_Chat.IsActive())
				if(GameClient()->m_Controls.m_aInputData[2].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[0] || GameClient()->m_Controls.m_aInputDirectionRight[0]))
					m_LastFreeze = TimeReset;
		}
		if(!pCharacterOther->m_IsInFreeze)
		{
			if(g_Config.m_ClFreezeKillTeamClose && OtherTee.m_IsTeam && !OtherTee.m_Solo && OtherTee.m_Team == m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_Team && i != Local)
				if(!((OtherTee.m_RenderPos.x < Position.x - Distance) || (OtherTee.m_RenderPos.x > Position.x + Distance) || (OtherTee.m_RenderPos.y > Position.y + Distance) || (OtherTee.m_RenderPos.y < Position.y - Distance)))
					m_LastFreeze = TimeReset;
		}

		if(g_Config.m_ClFreezeKillWaitMs)
		{
			if(!pCharacter->m_IsInFreeze)
			{
				if(m_pClient->m_aClients[Local].m_FreezeEnd < 3 && !g_Config.m_ClFreezeKillOnlyFullFrozen)
					m_LastFreeze = TimeReset;
				if(g_Config.m_ClFreezeKillOnlyFullFrozen)
					m_LastFreeze = TimeReset;
			}

			if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
			{
				m_pClient->m_Chat.SendChat(0, "/kill");
				m_SentKill = true;
				m_LastFreeze = time_get() + time_freq() * 5;
				return;
			}
			else if(m_LastFreeze <= time_get() && (pCharacter->m_IsInFreeze || m_pClient->m_aClients[Local].m_FreezeEnd > 0))
			{
				GameClient()->SendKill(Local);
				m_SentKill = true;
				return;
			}
		}
		else if(pCharacter->m_IsInFreeze)
		{
			if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
			{
				m_pClient->m_Chat.SendChat(0, "/kill");
				m_SentKill = true;
				m_LastFreeze = time_get() + time_freq() * 5;
			}
			else
			{
				GameClient()->SendKill(Local);
				m_SentKill = true;
			}
			return;
		}
	}
}

void CAiodob::AutoKill()
{
	const int Local = m_pClient->m_Snap.m_LocalClientId;
	CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

	if(g_Config.m_ClAutoKill)
	{
		if(m_SentKill == true)
			return;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharacterOther = &m_pClient->m_aClients[i].m_Predicted;
			if(g_Config.m_ClAutoKillMultOnly)
			{
				if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
				{
					if(g_Config.m_ClAutoKillDebug)
						TextRender()->Text(100, 50, 15, "Not on Mult");
					return;
				}
				else if(g_Config.m_ClAutoKillDebug)
					TextRender()->Text(100, 50, 15, "On Mult");
			}

			const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Prev;
			const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Cur;

			const CNetObj_Character *pPrevCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Prev;
			const CNetObj_Character *pCurCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Cur;

			auto IsWar = 0;
			bool EnemyFrozen = 0;

			if(i != Local)
			{
				pPrevCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				pCurCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;
				IsWar = m_pClient->m_aClients[i].m_IsWar || m_pClient->m_aClients[i].m_IsTempWar || m_pClient->m_aClients[i].m_IsWarClanmate;
				EnemyFrozen = pCharacterOther->m_IsInFreeze;
			}

			const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
			const vec2 SelfPos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

			const vec2 EnemyPos = vec2(pPrevCharO->m_X, pPrevCharO->m_Y) / 32.0f;
			const vec2 EnemyVel = mix(vec2(pPrevCharO->m_VelX, pPrevCharO->m_VelY), vec2(pCurCharO->m_VelX, pCurCharO->m_VelY), IntraTick) / 1000 / 1.5;

			const float RangeX = g_Config.m_ClAutoKillRangeX / 100.0f;

			float RangeY = g_Config.m_ClAutoKillRangeY / 100.0f;

			CCharacter *pChar = m_pClient->m_PredictedWorld.GetCharacterById(~Local);

			

			if(g_Config.m_ClAutoKillDebug)
			{
				char aBuf[100];
				str_format(aBuf, sizeof(aBuf), "%d", round_to_int(SelfPos.x));
				TextRender()->Text(100, 100, 15, aBuf);

				char bBuf[100];
				str_format(bBuf, sizeof(bBuf), "%d", round_to_int(EnemyPos.x));
				TextRender()->Text(200, 100, 15, bBuf);

				if(pChar && pChar->IsGrounded())
					TextRender()->Text(300, 100, 15, "grounded");
			}



			if((pCharacter->m_IsInFreeze && !g_Config.m_ClAutoKillWarOnly) || (g_Config.m_ClAutoKillWarOnly && IsWar && pCharacter->m_IsInFreeze))
			{
				if((EnemyPos.y < SelfPos.y && EnemyPos.y > SelfPos.y - 1 - RangeY - EnemyVel.y) && ((EnemyPos.x <= SelfPos.x + RangeX) && (EnemyPos.x + RangeX >= SelfPos.x)))
				{
					if(EnemyFrozen)
						return;
					if(m_pClient->m_aClients[i].m_FreezeEnd > 0)
					{
						if(EnemyPos.x <= SelfPos.x + 0.04f && EnemyPos.x + +0.04f >= SelfPos.x)
						{
							if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
							{
								m_pClient->m_Chat.SendChat(0, "/kill");
								m_SentKill = true;
								m_LastFreeze = time_get() + time_freq() * 5;
								return;
							}
							else
							{
								GameClient()->SendKill(Local);
								m_SentKill = true;
								return;
							}
						}
						return;
					}
					if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
					{
						m_pClient->m_Chat.SendChat(0, "/kill");
						m_SentKill = true;
						m_LastFreeze = time_get() + time_freq() * 5;
						return;
					}
					else
					{
						GameClient()->SendKill(Local);
						m_SentKill = true;
						return;
					}
				}
			}
		}
	}
}

void CAiodob::AutoJoinTeam()
{
	if(m_JoinTeam > time_get())
		return;

	if(!g_Config.m_ClAutoJoinTest)
		return;

	if(m_pClient->m_Chat.IsActive())
		return;

	if(GameClient()->CurrentRaceTime())
		return;

	const int Local = m_pClient->m_Snap.m_LocalClientId;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pClient->m_Teams.Team(i))
		{
			if(str_comp(m_pClient->m_aClients[i].m_aName, g_Config.m_ClAutoJoinTeamName) == 0)
			{
				int LocalTeam;


				if(i == Local)
					return;

				

				int Team = GameClient()->m_Teams.Team(i);
				char TeamChar[256];
				str_format(TeamChar, sizeof(TeamChar), "%d", Team);

				int PrevTeam;

				if(!m_pClient->m_Teams.SameTeam(Local, i) && (Team > 0) && !JoinedTeam)
				{
					char aBuf[2048] = "/team ";
					str_append(aBuf, TeamChar);
					m_pClient->m_Chat.SendChat(0, aBuf);

					char Joined[2048] = "attempting to auto Join ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					JoinedTeam = true;
					AttempedJoinTeam = true;
				}
				if(m_pClient->m_Teams.SameTeam(Local, i) && JoinedTeam)
				{
					char Joined[2048] = "Successfully Joined The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					LocalTeam = GameClient()->m_Teams.Team(Local);

					PrevTeam = Team;

					JoinedTeam = false;
				}
				if(!m_pClient->m_Teams.SameTeam(Local, i) && AttempedJoinTeam)
				{
					char Joined[2048] = "Couldn't Join The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					AttempedJoinTeam = false;
				}
				if(PrevTeam != Team && AttempedJoinTeam)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "team has changed");
					JoinedTeam = false;
				}
				if(LocalTeam > 0)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "self team is bigger than 0");
					JoinedTeam = false;
					LocalTeam = GameClient()->m_Teams.Team(Local);
				}
				if(LocalTeam != Team)
				{
					PrevTeam = Team;
					AttempedJoinTeam = false;
					LocalTeam = GameClient()->m_Teams.Team(Local);
				}

					
				return;

			}
		}
		m_JoinTeam = time_get() + time_freq() * 0.25;
	}
}

void CAiodob::GoresMode()
{
	if(!g_Config.m_ClKogMode)
	{
		if(m_KogModeRebound == true)
		{
			GameClient()->m_Binds.Bind(KEY_MOUSE_1, g_Config.m_ClKogModeSaved);
			m_KogModeRebound = false;
		}
		return;
	}

	if(m_KogModeRebound == false)
	{
		GameClient()->m_Binds.Bind(KEY_MOUSE_1, "+fire;+prevweapon");
		m_KogModeRebound = true;
		return;
	}

	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;

	if(m_pClient->m_Snap.m_pLocalCharacter->m_Weapon == 0)
	{
		GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 2;
	}
}

void CAiodob::OnRender()
{
	GoresMode();
	AutoKill();
	AutoJoinTeam();
	FreezeKill();

	if(GameClient()->m_Controls.m_aInputData[2].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[0] || GameClient()->m_Controls.m_aInputDirectionRight[0]))
		m_LastMovement = time_get() + time_freq() * 30;
}