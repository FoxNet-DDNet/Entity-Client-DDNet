#include <engine/shared/config.h>
#include <engine/client.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/gamecore.h>
#include <game/client/components/chat.h>
#include <game/client/gameclient.h>

#include <base/vmath.h>
#include <base/system.h>

#include "anti_spawn_block.h"

void CAntiSpawnBlock::OnRender()
{
	int Local = m_pClient->m_Snap.m_LocalClientId;

	// if Anti Spawn Block isnt turned on, stop

	if(!g_Config.m_ClAntiSpawnBlock)
		return;


	if(m_SentKill)
	{
		m_SentTeamRequest = false;
		m_SentKill = false;
	}

	// if Can't find Player or player has started the race, stop

	if(!m_pClient || !m_pClient->m_Snap.m_pLocalCharacter || GameClient()->CurrentRaceTime())
		return;

	// if map name isnt "Multeasymap", stop

	if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
		return;

	{

		vec2 Pos = m_pClient->m_PredictedChar.m_Pos;

		if(!m_pClient->m_Teams.Team(Local))
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				int ExcludeTeam = -1;
				if(m_pClient->m_Teams.Team(i))
				{
					ExcludeTeam = m_pClient->m_Teams.Team(i);

				}
				if(i != ExcludeTeam)
				{
					m_Team = i;
				}
			}
		}

		if(m_pClient->m_aClients[Local].m_Predicted.m_FreezeEnd > 0 && !m_SentTeamRequest)
		{
			char TeamChar[256];
			str_format(TeamChar, sizeof(TeamChar), "%d", m_Team);

			char aBuf[2048] = "/team ";
			str_append(aBuf, TeamChar);
			GameClient()->m_Chat.SendChat(0, aBuf);
			if(m_pClient->m_Teams.Team(Local) != 0)
				GameClient()->m_Chat.SendChat(0, "/Lock");
			m_Team = 0;
			m_SentTeamRequest = true;
		}
		else if(m_pClient->RaceHelper()->IsNearStart(Pos, 2) && m_SentTeamRequest)
		{
			if(m_pClient->m_Teams.Team(Local) != 0)
			{
				GameClient()->m_Chat.SendChat(0, "/team 0");
				m_SentTeamRequest = false;
			}
		}
	}
}