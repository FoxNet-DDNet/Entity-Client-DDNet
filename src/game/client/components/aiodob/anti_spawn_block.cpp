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

	if(m_SentKill) // So it resets the state
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
		// ToDo: find a way to find an emtpy fkin team im bouta flip

		vec2 Pos = m_pClient->m_PredictedChar.m_Pos;

		if(!m_pClient->m_Teams.Team(Local))
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				int RandomTeam = round_to_int(random_float(1, 63));
				if(m_pClient->m_Teams.Team(i))
				{
					numbers.push_back(m_pClient->m_Teams.Team(i));
				}
				for(int number : numbers)
				{
					for(int t = 1; t < MAX_CLIENTS; t++)
					{
						if(RandomTeam != number)
							m_Team = RandomTeam;

					}
				}
			}
			//char TeamChar[256];
			//str_format(TeamChar, sizeof(TeamChar), "%d", m_Team);
			//GameClient()->aMessage(TeamChar);
		}

		if(m_pClient->m_aClients[Local].m_Predicted.m_FreezeEnd > 0 && !m_SentTeamRequest)
		{
			char TeamChar[256];
			str_format(TeamChar, sizeof(TeamChar), "%d", m_Team);

			char aBuf[2048] = "/team ";
			str_append(aBuf, TeamChar);
			GameClient()->m_Chat.SendChat(0, aBuf);
			m_Team = 0;
			m_SentTeamRequest = true;
			// m_Locked = false;
		}
		else if(m_pClient->RaceHelper()->IsNearStart(Pos, 2) && m_SentTeamRequest)
		{
			if(m_pClient->m_Teams.Team(Local) != 0)
			{
				GameClient()->m_Chat.SendChat(0, "/team 0");
				m_SentTeamRequest = false;
			}
		}

		//if(m_pClient->m_Teams.Team(Local) != 0 && !m_Locked)
		//{
		//	GameClient()->m_Chat.SendChat(0, "/Lock");
		//	m_Locked = true;
		//}
	}
}