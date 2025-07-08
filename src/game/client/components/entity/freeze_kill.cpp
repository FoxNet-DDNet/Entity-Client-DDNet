#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>

#include <base/system.h>
#include <base/vmath.h>

#include "freeze_kill.h"

void CFreezeKill::OnRender()
{
	int Local = GameClient()->m_Snap.m_LocalClientId;
	float Time = g_Config.m_ClFreezeKillMs / 1000.0f;
	float TimeReset = time_get() + time_freq() * Time;

	if(!g_Config.m_ClFreezeKill)
	{
		m_LastFreeze = TimeReset;
		return;
	}

	if(!GameClient()->CurrentRaceTime())
	{
		m_SentFreezeKill = false;
		m_LastFreeze = TimeReset + 3;
		return;
	}

	// if map name isnt "Multeasymap", stop
	if(g_Config.m_ClFreezeKillMultOnly)
		if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
			return;

	// debug

	if(g_Config.m_ClFreezeKillDebug)
	{
		float a = (m_LastFreeze - time_get()) / 1000000000.0f;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "until kill: %f", a);
		GameClient()->TextRender()->Text(50, 100, 10, aBuf);
	}

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		// stuff
		CCharacterCore *pCharacterOther = &GameClient()->m_aClients[ClientId].m_Predicted;

		CCharacterCore *pCharacter = &GameClient()->m_aClients[Local].m_Predicted;

		vec2 Position = GameClient()->m_aClients[Local].m_RenderPos;
		CGameClient::CClientData OtherTee = GameClient()->m_aClients[ClientId];
		int Distance = g_Config.m_ClFreezeKillTeamDistance * 100;

		// if tried to kill, stop
		if(m_SentFreezeKill == true)
			return;

		// stop when spectating
		if(GameClient()->m_aClients[Local].m_Paused || GameClient()->m_aClients[Local].m_Spec)
			m_LastFreeze = TimeReset;

		// dont kill if moving
		if((pCharacter->m_IsInFreeze || GameClient()->m_aClients[Local].m_FreezeEnd > 0) && ClientId == Local && g_Config.m_ClFreezeDontKillMoving)
		{
			if(!GameClient()->m_Menus.IsActive() || !GameClient()->m_Chat.IsActive())
				if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
					m_LastFreeze = TimeReset;
		}

		// dont kill if teamate is in x * 2 blocks range

		if(g_Config.m_ClFreezeKillTeamClose && !GameClient()->m_WarList.m_WarPlayers[ClientId].m_WarGroupMatches[2] && !OtherTee.m_Solo && OtherTee.m_Team == GameClient()->m_aClients[Local].m_Team && ClientId != Local)
		{
			if(!((OtherTee.m_RenderPos.x < Position.x - Distance) || (OtherTee.m_RenderPos.x > Position.x + Distance) || (OtherTee.m_RenderPos.y > Position.y + Distance) || (OtherTee.m_RenderPos.y < Position.y - Distance)))
			{
				if(!pCharacterOther->m_IsInFreeze)
				{
					m_LastFreeze = TimeReset;
				}
			}
		}

		// wait x amount of seconds before killing
		if(g_Config.m_ClFreezeKillWaitMs)
		{
			// kill if frozen (without deep and live freeze)
			if(GameClient()->m_aClients[Local].m_FreezeEnd < 3 && !g_Config.m_ClFreezeKillOnlyFullFrozen && !pCharacter->m_IsInFreeze)
				m_LastFreeze = TimeReset;

			// only kill if player is in a freeze tile
			if(g_Config.m_ClFreezeKillOnlyFullFrozen)
			{
				if(!pCharacter->m_IsInFreeze)
					m_LastFreeze = TimeReset;

				// dont kill if not touching the ground (might crash not sure yet) - does crash

				// if(pCharacter->m_IsInFreeze && !pChar->IsGrounded() && g_Config.m_ClFreezeKillGrounded)
				//{
				//	m_LastFreeze = TimeReset;
				// }
			}

			// default kill protection timer
			if(m_LastFreeze <= time_get())
			{
				if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
				{
					GameClient()->m_Chat.SendChat(0, "/kill");
					m_SentFreezeKill = true;
					m_LastFreeze = time_get() + time_freq() * 5;
					return;
				}
				else if((pCharacter->m_IsInFreeze || GameClient()->m_aClients[Local].m_FreezeEnd > 0))
				{
					GameClient()->SendKill();
					m_SentFreezeKill = true;
					return;
				}
			}
		}

		// if not wating for x amount of seconds
		else if(pCharacter->m_IsInFreeze)
		{
			if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
			{
				GameClient()->m_Chat.SendChat(0, "/kill");
				m_SentFreezeKill = true;
				m_LastFreeze = time_get() + time_freq() * 5;
			}
			else
			{
				GameClient()->SendKill();
				m_SentFreezeKill = true;
			}
			return;
		}
	}
}