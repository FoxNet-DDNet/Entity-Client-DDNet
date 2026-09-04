#include "auto_dummy_connect.h"

#include <base/time.h>

#include <engine/client.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

void CAutoDummyConnect::AdoptDummyLocalId()
{
	const int Conn = IClient::CONN_DUMMY;

	// While the dummy is the active connection OnNewSnapshot keeps its local id up to date.
	if(g_Config.m_ClDummy == Conn || GameClient()->m_aLocalIds[Conn] >= 0)
		return;

	// Only the active connection is unpacked into m_Snap, so a dummy that never became
	// active has no local id. CGameClient::OnSnapInput refuses to produce input without
	// one, which means nothing is ever sent on CONN_DUMMY and the server drops the dummy.
	// Mirror the m_Snap based assignment in OnNewSnapshot against the raw dummy snapshot.
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const CNetObj_PlayerInfo *pInfo = static_cast<const CNetObj_PlayerInfo *>(
			Client()->SnapFindItem(Conn, IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientId));
		if(pInfo && pInfo->m_Local && pInfo->m_ClientId == ClientId)
		{
			GameClient()->m_aLocalIds[Conn] = ClientId;
			return;
		}
	}
}

void CAutoDummyConnect::OnRender()
{
	if(!g_Config.m_ClAutoDummyConnect)
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(Client()->DummyConnected())
	{
		AdoptDummyLocalId();
		return;
	}

	if(Client()->DummyConnecting() || Client()->DummyConnectingDelayed())
		return;

	if(!Client()->DummyAllowed())
	{
		if(!m_WarnedNotAllowed)
		{
			GameClient()->ClientMessage("This server doesn't allow connecting a dummy.");
			m_WarnedNotAllowed = true;
		}
		return;
	}

	const CServerInfo &CurrentServerInfo = Client()->ServerInfo();
	if(GameClient()->m_Snap.m_NumPlayers >= CurrentServerInfo.m_MaxClients)
		return;

	const int64_t Now = time_get();
	if(m_NextAttempt > Now)
		return;

	m_NextAttempt = Now + time_freq() * 5 / 2;

	if(g_Config.m_ClAutoDummyConnect == 2)
		Client()->DummyConnectInBackground();
	else
		Client()->DummyConnect();
}

void CAutoDummyConnect::OnReset()
{
	m_WarnedNotAllowed = false;
	m_NextAttempt = 0;
}
