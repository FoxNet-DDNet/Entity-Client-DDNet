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
	int Local = GameClient()->m_Snap.m_LocalClientId;

	// if Anti Spawn Block isnt turned on, stop
	if(!g_Config.m_ClAntiSpawnBlock)
	{
		if(GameClient()->m_Teams.Team(Local) != 0 && m_SentTeamRequest)
		{
			GameClient()->m_Chat.SendChat(0, "/team 0");
			m_SentTeamRequest = false;
			m_SentKill = false;
		}
		return;
	}

	// if Can't find Player or player STARTED the race, stop
	if(!GameClient()->m_Snap.m_pLocalCharacter || GameClient()->CurrentRaceTime())
	{
		m_SentTeamRequest = false;
		m_SentKill = false;
		return;
	}

	// if map name isnt "Multeasymap", stop
	if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
		return;

	if(m_SentKill) // So it resets the state
	{
		if(GameClient()->m_Teams.Team(Local) != 0 && m_SentTeamRequest)
			GameClient()->m_Chat.SendChat(0, "/team 0");
		m_SentKill = false;
	}

	vec2 Pos = GameClient()->m_PredictedChar.m_Pos;

	if(GameClient()->RaceHelper()->IsNearStart(Pos, 2) && m_SentTeamRequest)
	{
		if(GameClient()->m_Teams.Team(Local) != 0)
		{
			GameClient()->m_Chat.SendChat(0, "/team 0");
			m_SentTeamRequest = true;
		}
	}
	else if(!m_SentTeamRequest)
	{
		if(GameClient()->m_Teams.Team(Local) != 0)
			m_SentTeamRequest = true;
		else if(m_Delay < time_get())
		{
			GameClient()->m_Chat.SendChat(0, "/mc;team -1;lock"); // multi-command
			m_Delay = time_get() + time_freq() * 2.5f;
		}
	}
}