#include "auto_dummy_connect.h"

#include <base/time.h>

#include <engine/client.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

void CAutoDummyConnect::DummyChangeBack()
{
	if(m_AutoSwitchedDummy || g_Config.m_ClAutoDummyConnect != 2)
		return;

	if(g_Config.m_ClDummy != 1)
		return;

	// The local id of a connection is only assigned in CGameClient::OnNewSnapshot,
	// which the engine only runs for the connection that is currently active. As long
	// as the dummy's local id is unknown, CGameClient::OnSnapInput bails out and the
	// client never sends any input on CONN_DUMMY, so the server drops the dummy again.
	// Stay on the dummy until its first snapshot has been processed, then switch back.
	if(GameClient()->m_aLocalIds[IClient::CONN_DUMMY] < 0)
		return;

	g_Config.m_ClDummy = 0;
	m_AutoSwitchedDummy = true;
}

void CAutoDummyConnect::OnRender()
{
	if(!g_Config.m_ClAutoDummyConnect)
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(m_ConnectedDummy)
	{
		DummyChangeBack();
		return;
	}

	if(Client()->DummyConnected())
	{
		m_ConnectedDummy = true;
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
	Client()->DummyConnect();
}

void CAutoDummyConnect::OnReset()
{
	m_ConnectedDummy = false;
	m_AutoSwitchedDummy = false;
	m_WarnedNotAllowed = false;
	m_NextAttempt = 0;
}
