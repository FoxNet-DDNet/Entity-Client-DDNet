#include "anti_spawn_block.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>

void CAntiSpawnBlock::Reset(int State)
{
	if(m_State == State)
		return;

	if(GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK)
		GameClient()->m_Chat.SendChat(0, "/team 0");

	m_State = State;
}

void CAntiSpawnBlock::OnRender()
{
	int LocalId = GameClient()->m_Snap.m_LocalClientId;

	if(!g_Config.m_ClAntiSpawnBlock)
	{
		if(m_State != STATE_NONE)
			Reset(STATE_NONE);
		return;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	// if Can't find Player or Player STARTED the race, stop
	if(!GameClient()->m_Snap.m_pLocalCharacter || GameClient()->CurrentRaceTime())
		return;

	vec2 Pos = GameClient()->m_PredictedChar.m_Pos;

	if(GameClient()->RaceHelper()->IsNearStart(Pos, 2))
	{
		if(GameClient()->m_Teams.Team(LocalId) != 0 && m_State == STATE_IN_TEAM)
		{
			GameClient()->m_Chat.SendChat(0, "/team 0");
			m_State = STATE_TEAM_ZERO;
		}
	}
	else if(m_State == STATE_NONE)
	{
		if(GameClient()->m_Teams.Team(LocalId) != 0)
		{
			m_State = STATE_IN_TEAM;
			return;
		}

		if(m_Delay < time_get())
		{
			GameClient()->m_Chat.SendChat(0, "/mc;team -1;lock"); // multi-command
			m_Delay = time_get() + time_freq() * 2.5f;
		}
	}
}

void CAntiSpawnBlock::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
		Reset(STATE_NONE);
}
