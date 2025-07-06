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

void CAntiSpawnBlock::Reset(int State)
{
	if(State != -1 && m_State == State && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != 0)
		GameClient()->m_Chat.SendChat(0, "/team 0");

	m_State = STATE_NONE;
	m_SentKill = false;
}

void CAntiSpawnBlock::OnRender()
{
	int Local = GameClient()->m_Snap.m_LocalClientId;

	if(!g_Config.m_ClAntiSpawnBlock)
	{
		Reset(STATE_IN_TEAM);
		return;
	}

	// if Can't find Player or Player STARTED the race, stop
	if(!GameClient()->m_Snap.m_pLocalCharacter || GameClient()->CurrentRaceTime())
	{
		if(m_State != STATE_NONE)
			Reset();
		return;
	}

	if(m_SentKill)
		Reset(STATE_IN_TEAM);

	// if map name isnt "Multeasymap", stop
	if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
		return;

	vec2 Pos = GameClient()->m_PredictedChar.m_Pos;

	if(GameClient()->RaceHelper()->IsNearStart(Pos, 2))
	{
		if(GameClient()->m_Teams.Team(Local) != 0 && m_State == STATE_IN_TEAM)
		{
			GameClient()->m_Chat.SendChat(0, "/team 0");
			m_State = STATE_TEAM_ZERO;
		}
	}
	else if(m_State == STATE_NONE)
	{
		if(m_Delay < time_get())
		{
			GameClient()->m_Chat.SendChat(0, "/mc;team -1;lock"); // multi-command
			m_Delay = time_get() + time_freq() * 2.5f;
		}
		if(GameClient()->m_Teams.Team(Local) != 0)
			m_State = STATE_IN_TEAM;
	}
}