#include "anti_spawn_block.h"

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>

void CAntiSpawnBlock::Reset(int Conn, EState State)
{
	if(m_aConns[Conn].m_State == State)
		return;

	const int ClientId = GameClient()->m_aLocalIds[Conn];

	if(Client()->State() == IClient::STATE_ONLINE && in_range(ClientId, 0, MAX_CLIENTS - 1) &&
		GameClient()->m_Teams.Team(ClientId) != TEAM_FLOCK)
		GameClient()->m_Chat.SendChat(0, "/team 0", Conn);

	m_aConns[Conn].m_State = State;
}

void CAntiSpawnBlock::OnRender()
{
	if(!g_Config.m_ClAntiSpawnBlock)
	{
		for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
			Reset(Conn, EState::None);
		return;
	}

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		HandleConn(Conn);
}

bool CAntiSpawnBlock::GetPos(int Conn, int ClientId, vec2 &Pos) const
{
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
	{
		Pos = GameClient()->m_aClients[ClientId].m_Predicted.m_Pos;
		return true;
	}

	// Once our other tee sits in its own team it can be missing from the snapshot of the
	// connection we are playing on, but its own connection always receives it. Without this
	// we would never see it come back to the start and leave it locked in that team forever.
	const auto *pCharacter = (const CNetObj_Character *)Client()->SnapFindItem(Conn, IClient::SNAP_CURRENT, NETOBJTYPE_CHARACTER, ClientId);
	if(!pCharacter)
		return false;

	Pos = vec2(pCharacter->m_X, pCharacter->m_Y);
	return true;
}

void CAntiSpawnBlock::HandleConn(int Conn)
{
	if(Conn == IClient::CONN_DUMMY && !Client()->DummyConnected())
	{
		m_aConns[Conn].m_State = EState::None;
		return;
	}

	const int ClientId = GameClient()->m_aLocalIds[Conn];

	if(!in_range(ClientId, 0, MAX_CLIENTS - 1))
		return;

	// stop when spectating
	if(GameClient()->m_aClients[ClientId].m_Paused || GameClient()->m_aClients[ClientId].m_Spec)
		return;

	// if Can't find Player or Player STARTED the race, stop
	vec2 Pos;
	if(!GetPos(Conn, ClientId, Pos) || GameClient()->CurrentRaceTime(Conn))
		return;

	if(GameClient()->RaceHelper()->IsNearStart(Pos, 2))
	{
		if(GameClient()->m_Teams.Team(ClientId) != TEAM_FLOCK && m_aConns[Conn].m_State == EState::InTeam)
		{
			GameClient()->m_Chat.SendChat(0, "/team 0", Conn);
			m_aConns[Conn].m_State = EState::TeamZero;
		}
	}
	else if(m_aConns[Conn].m_State == EState::None)
	{
		if(GameClient()->m_Teams.Team(ClientId) != TEAM_FLOCK)
		{
			m_aConns[Conn].m_State = EState::InTeam;
			return;
		}

		if(m_aConns[Conn].m_Delay < time_get())
		{
			GameClient()->m_Chat.SendChat(0, "/mc;team -1;lock", Conn); // multi-command
			m_aConns[Conn].m_Delay = time_get() + time_freq() * 2.5f;
		}
	}
}

void CAntiSpawnBlock::OnStateChange(int NewState, int OldState)
{
	if(NewState == OldState)
		return;

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		Reset(Conn, EState::None);
}
