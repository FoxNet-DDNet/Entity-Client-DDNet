#include "freeze_kill.h"

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>

void CFreezeKill::ResetTimer()
{
	float Time = g_Config.m_ClFreezeKillWaitMs ? g_Config.m_ClFreezeKillMs / 1000.0f : 0.0f;
	float TimeReset = time_get() + time_freq() * Time;
	m_LastFreeze = TimeReset;
	m_SentFreezeKill = false;
}

void CFreezeKill::OnRender()
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;

	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
	{
		ResetTimer();
		return;
	}

	// Don't render if we can't find our own tee
	if(!GameClient()->m_Snap.m_aCharacters[LocalId].m_Active)
	{
		ResetTimer();
		return;
	}

	// Don't render if not race gamemode or in demo
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		ResetTimer();
		return;
	}

	if(!g_Config.m_ClFreezeKill)
	{
		ResetTimer();
		return;
	}

	if(!g_Config.m_ClFreezeKillBeforeStart && !GameClient()->StartedRace())
	{
		m_SentFreezeKill = false;
		ResetTimer();
		return;
	}

	if(m_SentFreezeKill)
		return;

	// stop when spectating
	if(GameClient()->m_aClients[LocalId].m_Paused || GameClient()->m_aClients[LocalId].m_Spec)
	{
		ResetTimer();
		return;
	}

	if(GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive())
	{
		ResetTimer();
		return;
	}

	const int FreezeEnd = GameClient()->m_aClients[LocalId].m_FreezeEnd;

	// dont kill if moving
	if(g_Config.m_ClFreezeKillNotMoving)
	{
		if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
		{
			ResetTimer();
			return;
		}
	}

	if(g_Config.m_ClFreezeKillTeamInView && FreezeEnd > 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(i == LocalId)
				continue;
			if(!GameClient()->m_Snap.m_apPlayerInfos[i])
				continue;
			if(HandleClients(i))
				return;
		}
	}

	if(g_Config.m_ClFreezeKillOnlyFullFrozen)
	{
		if(!GameClient()->m_aClients[LocalId].m_Predicted.m_IsInFreeze)
		{
			ResetTimer();
			return;
		}
	}
	else if(FreezeEnd <= 0)
	{
		ResetTimer();
		return;
	}

	bool ShouldSendKill = false;

	if(g_Config.m_ClFreezeKillWaitMs)
	{
		if(m_LastFreeze <= time_get() && m_LastFreeze != 0)
			ShouldSendKill = true;
	}
	else if(FreezeEnd > 0)
		ShouldSendKill = true;

	if(ShouldSendKill)
	{
		if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
		{
			GameClient()->m_Chat.SendChat(0, "/kill");
			m_SentFreezeKill = true;
			m_LastFreeze = 0;
		}
		else
		{
			GameClient()->SendKill();
			m_SentFreezeKill = true;
			m_LastFreeze = 0;
		}
	}
}

bool CFreezeKill::HandleClients(int ClientId)
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;

	CGameClient::CClientData OtherTee = GameClient()->m_aClients[ClientId];

	bool Solo = OtherTee.m_Solo;
	bool Teammate = GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[2];
	bool SameTeam = GameClient()->m_Teams.SameTeam(ClientId, LocalId);

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	float BorderBuffer = 128;
	ScreenX0 -= BorderBuffer;
	ScreenX1 += BorderBuffer;
	ScreenY0 -= BorderBuffer;
	ScreenY1 += BorderBuffer;

	const bool InRange = in_range(OtherTee.m_RenderPos.x, ScreenX0, ScreenX1) && in_range(OtherTee.m_RenderPos.y, ScreenY0, ScreenY1);

	if(Teammate && SameTeam && !Solo)
	{
		if(InRange && OtherTee.m_FreezeEnd <= 0)
		{
			ResetTimer();
			return true;
		}
	}

	return false;
}

void CFreezeKill::OnReset()
{
	m_SentFreezeKill = false;
	m_LastFreeze = 0;
}
