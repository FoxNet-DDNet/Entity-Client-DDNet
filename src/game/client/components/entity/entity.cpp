#include <engine/client.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>
#include <game/generated/protocol.h>

#include <base/math.h>
#include <base/system.h>

#include "entity.h"
#include <cmath>

bool CEClient::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_utf8_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}

	return false;
}

void CEClient::OnChatMessage(int ClientId, int Team, const char *pMsg)
{
	if(ClientId < 0 || ClientId > MAX_CLIENTS)
		return;

	bool Highlighted = GameClient()->m_Chat.LineHighlighted(ClientId, pMsg);

	if(Team == 3) // whisper recv
		Highlighted = true;

	if(!Highlighted)
		return;
	char aName[16];
	str_copy(aName, GameClient()->m_aClients[ClientId].m_aName, sizeof(aName));

	if(!str_comp(aName, GameClient()->m_aClients[GameClient()->m_aLocalIds[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, GameClient()->m_aClients[GameClient()->m_aLocalIds[1]].m_aName))
		return;

	bool HiddenMessage = (GameClient()->m_WarList.m_WarPlayers[ClientId].IsMuted || m_TempPlayers[ClientId].IsTempMute) ||
			     (g_Config.m_ClHideEnemyChat && (GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1] || GameClient()->m_EClient.m_TempPlayers[ClientId].IsTempWar));

	if(!HiddenMessage)
	{
		str_copy(m_aLastPing.m_aName, aName);
		str_copy(m_aLastPing.m_aMessage, pMsg);
		m_aLastPing.m_Team = Team;
	}

	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	
	if(ClientId != m_LastReplyId)
	{
		char Reply[MAX_LINE_LENGTH];
		if(g_Config.m_ClReplyMuted && (GameClient()->m_WarList.m_WarPlayers[ClientId].IsMuted || m_TempPlayers[ClientId].IsTempMute))
		{
			str_format(Reply, sizeof(Reply), "%s: %s", aName, g_Config.m_ClAutoReplyMutedMsg);

			if(Team == TEAM_WHISPER_RECV) // whisper recv
				str_format(Reply, sizeof(Reply), "/w %s %s", aName, g_Config.m_ClAutoReplyMutedMsg);

			GameClient()->m_Chat.SendChat(TEAM_FLOCK, Reply);
			m_LastReplyId = ClientId;
		}
		else if(g_Config.m_ClTabbedOutMsg)
		{
			IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
			if(pGraphics && !pGraphics->WindowActive() && Graphics())
			{
				if(Team == TEAM_WHISPER_RECV) // whisper recv
				{
					str_format(Reply, sizeof(Reply), "/w %s ", aName);
					str_append(Reply, g_Config.m_ClAutoReplyMsg);
					GameClient()->m_Chat.SendChat(TEAM_FLOCK, Reply);
				}
				else
				{
					str_format(Reply, sizeof(Reply), "%s: ", aName);
					str_append(Reply, g_Config.m_ClAutoReplyMsg);
					GameClient()->m_Chat.SendChat(TEAM_FLOCK, Reply);
				}
				m_LastReplyId = ClientId;
			}
		}
	}
}

void CEClient::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CEClient::AutoJoinTeam()
{
	if(m_JoinTeam > time_get())
		return;

	if(!g_Config.m_ClAutoJoinTest)
		return;

	if(GameClient()->m_Chat.IsActive())
		return;

	if(GameClient()->CurrentRaceTime())
		return;

	int Local = GameClient()->m_Snap.m_LocalClientId;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(GameClient()->m_Teams.Team(ClientId))
		{
			if(str_comp(GameClient()->m_aClients[ClientId].m_aName, g_Config.m_ClAutoJoinTeamName) == 0)
			{
				int LocalTeam = -1;

				if(ClientId == Local)
					return;

				int Team = GameClient()->m_Teams.Team(ClientId);
				char TeamChar[256];
				str_format(TeamChar, sizeof(TeamChar), "%d", Team);

				int PrevTeam = -1;

				if(!GameClient()->m_Teams.SameTeam(Local, ClientId) && (Team > 0) && !m_JoinedTeam)
				{
					char aBuf[2048] = "/team ";
					str_append(aBuf, TeamChar);
					GameClient()->m_Chat.SendChat(0, aBuf);

					char Joined[2048] = "attempting to auto Join ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					m_JoinedTeam = true;
					m_AttempedJoinTeam = true;
				}
				if(GameClient()->m_Teams.SameTeam(Local, ClientId) && m_JoinedTeam)
				{
					char Joined[2048] = "Successfully Joined The Team of ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					LocalTeam = GameClient()->m_Teams.Team(Local);

					PrevTeam = Team;

					m_JoinedTeam = false;
				}
				if(!GameClient()->m_Teams.SameTeam(Local, ClientId) && m_AttempedJoinTeam)
				{
					char Joined[2048] = "Couldn't Join The Team of ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					m_AttempedJoinTeam = false;
				}
				if(PrevTeam != Team && m_AttempedJoinTeam)
				{
					GameClient()->ClientMessage("team has changed");
					m_JoinedTeam = false;
				}
				if(LocalTeam > 0)
				{
					GameClient()->ClientMessage("self team is bigger than 0");
					m_JoinedTeam = false;
					LocalTeam = GameClient()->m_Teams.Team(Local);
				}
				if(LocalTeam != Team)
				{
					PrevTeam = Team;
					m_AttempedJoinTeam = false;
					LocalTeam = GameClient()->m_Teams.Team(Local);
				}
				return;
			}
		}
		m_JoinTeam = time_get() + time_freq() * 0.25;
	}
}

void CEClient::GoresMode()
{
	// if turning off kog mode and it was on before, rebind to previous bind
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClGoresMode)
		return;

	CCharacterCore Core = GameClient()->m_PredictedPrevChar;

	if(g_Config.m_ClGoresModeDisableIfWeapons)
	{
		if(Core.m_aWeapons[WEAPON_GRENADE].m_Got || Core.m_aWeapons[WEAPON_LASER].m_Got || Core.m_ExplosionGun || Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) 
			m_WeaponsGot = true;
		if((!Core.m_aWeapons[WEAPON_GRENADE].m_Got && !Core.m_aWeapons[WEAPON_LASER].m_Got && !Core.m_ExplosionGun && !Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && m_WeaponsGot)
			m_WeaponsGot = false;

		if(m_WeaponsGot)
			return;
	}

	int Key = g_Config.m_ClGoresModeKey;
	if(Key < KEY_FIRST || Key >= KEY_LAST)
	{
		g_Config.m_ClGoresMode = 0;
		dbg_msg("E-Client", "Invalid key: %d", Key);
		return;
	}
	const char *pKeyName = Input()->KeyName(Key);
	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot(pKeyName);
	const char *pBind = GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];
	if(!pBind)
		return;

	bool GoresBind = false;

	if(!str_comp(pBind, "+fire;+prevweapon"))
		GoresBind = true;

	if(!GoresBind)
		return;

	if(GoresBind)
	{
		if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == 0)
		{
			GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 2;
		}
	}
}

void CEClient::ConchainGoresMode(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CEClient *pSelf = (CEClient *)pUserData;
	if(pResult->NumArguments())
	{
		int GoresMode = pResult->GetInteger(0);

		if(GoresMode)
		{
			pSelf->GoresModeSave(true);
		}
		else
		{
			pSelf->GoresModeRestore();
		}
	}
}


void CEClient::GoresModeSave(bool Enable)
{
	int Key = g_Config.m_ClGoresModeKey;
	if(Key < KEY_FIRST || Key >= KEY_LAST)
	{
		g_Config.m_ClGoresMode = 0;
		dbg_msg("E-Client", "Invalid key: %d", Key);
		return;
	}
	const char *pKeyName = Input()->KeyName(Key);

	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot(pKeyName);
	const char *pBind = GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];
	str_copy(g_Config.m_ClGoresModeSaved, pBind);

	GameClient()->m_Binds.Bind(Key, "+fire;+prevweapon");
}

void CEClient::GoresModeRestore()
{
	int Key = g_Config.m_ClGoresModeKey;
	if(Key < KEY_FIRST || Key >= KEY_LAST)
	{
		g_Config.m_ClGoresMode = 0;
		dbg_msg("E-Client", "Invalid key: %d", Key);
		return;
	}
	GameClient()->m_Binds.Bind(Key, g_Config.m_ClGoresModeSaved);
}

void CEClient::OnConnect()
{
	// if dummy, return, so it doesnt display the info when joining with dummy

	if(g_Config.m_ClDummy)
		return;

	GameClient()->m_EClient.m_LastMovement = time_get();

	// if current server is type "Gores", turn the config on, else turn it off
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);
	static bool SentInfoMessage = false;
	if(m_FirstLaunch && SentInfoMessage)
	{
		GameClient()->ClientMessage("╭──                 E-Client Info");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ Seems like it's your first time running the client!");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ To view a list of Default Chat Commands do \".help\"");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ If you find a bug or have a Feature Request do \".github\"");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ Chat Commands that start with \".\" are silent by default,");
		GameClient()->ClientMessage("│ which means no one will see them.");
		GameClient()->ClientMessage("│ Messages that start with \"!\" will be sent");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("╰───────────────────────");
		SentInfoMessage = true;
	}
	else
	{
		if(g_Config.m_ClAutoEnableGoresMode)
		{
			if(!str_comp(CurrentServerInfo.m_aGameType, "Gores"))
			{
				m_GoresServer = true;
				g_Config.m_ClGoresMode = 1;
			}
			else
			{
				m_GoresServer = false;
				g_Config.m_ClGoresMode = 0;
			}
		}

		// info when joining a server of enabled components

		if(g_Config.m_ClEnabledInfo || g_Config.m_ClListsInfo)
		{
			GameClient()->ClientMessage("╭──               E-Client Info");
			GameClient()->ClientMessage("│");

			if(g_Config.m_ClListsInfo)
			{
				OnlineInfo(true);
				GameClient()->ClientMessage("│");
			}
			if(g_Config.m_ClEnabledInfo)
			{
				// Freeze Kill
				if((g_Config.m_ClFreezeKill && str_comp(Client()->GetCurrentMap(), "Multeasymap") == 0 && g_Config.m_ClFreezeKillMultOnly) || (!g_Config.m_ClFreezeKillMultOnly && g_Config.m_ClFreezeKill))
				{
					GameClient()->ClientMessage("│ Freeze Kill Enabled!");
					GameClient()->ClientMessage("│");
				}
				else if(g_Config.m_ClFreezeKill && (g_Config.m_ClFreezeKillMultOnly && str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0))
				{
					GameClient()->ClientMessage("│ Freeze Kill Disabled, Not on Mult!");
					GameClient()->ClientMessage("│");
				}
				if(!g_Config.m_ClFreezeKill)
				{
					GameClient()->ClientMessage("│ Freeze Kill Disabled!");
					GameClient()->ClientMessage("│");
				}
				if(g_Config.m_ClGoresMode)
				{
					GameClient()->ClientMessage("│ Gores Mode: ON");
					GameClient()->ClientMessage("│");
				}
				else
				{
					GameClient()->ClientMessage("│ Gores Mode: OFF");
					GameClient()->ClientMessage("│");
				}
				if(g_Config.m_ClChatBubble)
				{
					GameClient()->ClientMessage("│ Chat Bubble is Currently: ON");
					GameClient()->ClientMessage("│");
				}
				else
				{
					GameClient()->ClientMessage("│ Chat Bubble is Currently: OFF");
					GameClient()->ClientMessage("│");
				}
			}
			GameClient()->ClientMessage("╰───────────────────────");
		}
	}
}

void CEClient::NotifyOnMove()
{
	if(!g_Config.m_ClNotifyOnMove)
		return;
	IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
	if((pGraphics && pGraphics->WindowActive()) || !Graphics())
		return; // return when window is active

	const CNetObj_Character *pLocalChar = GameClient()->m_Snap.m_pLocalCharacter;
	if(!pLocalChar)
		return;

	int LocalId = GameClient()->m_Snap.m_LocalClientId;

	bool Moved = false;

	int X = GameClient()->m_Snap.m_aCharacters[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_X / 18;
	int Y = GameClient()->m_Snap.m_aCharacters[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_Y / 18;
	vec2 Pos = vec2(X, Y);
	
	vec2 LocalPos = GameClient()->m_aClients[LocalId].m_RenderPos; // Accurate Pos
	float MaxDist = 27.5f; 

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(m_LastPos == Pos)
			continue;
		CGameClient::CClientData pClient = GameClient()->m_aClients[ClientId];
		if(ClientId == LocalId || !pClient.m_Active)
			continue;
		if(pClient.m_Solo)
			continue;
		if(!GameClient()->m_Teams.SameTeam(LocalId, ClientId))
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedData)
			continue;
		const CNetObj_Character *pOtherChar = &GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
		vec2 OtherPos = GameClient()->m_aClients[ClientId].m_RenderPos;

		float dist = distance(LocalPos, OtherPos);
		if(dist < MaxDist + MaxDist)
			Moved = true;

		// check if the player is hooked to the local player
		if(GameClient()->m_aClients[ClientId].m_RenderCur.m_HookedPlayer == LocalId)
			Moved = true;

		// Check for hammer firing
		bool Hammering = (pOtherChar->m_Weapon == WEAPON_HAMMER) && (pOtherChar->m_AttackTick + 2 > Client()->GameTick(g_Config.m_ClDummy));
		dist = distance(vec2(pOtherChar->m_X, pOtherChar->m_Y), vec2(pLocalChar->m_X, pLocalChar->m_Y));
		if(Hammering && dist < 70.0f)
			Moved = true;
	}

	if(Moved)
	{
		Client()->Notify("E-Client", "current tile changed");
		Graphics()->NotifyWindow();
	}
	m_LastPos = Pos;
}

void CEClient::RemoveWarEntryDuplicates(const char *pName)
{
	if(!str_comp(pName, ""))
		return;

	for(auto it = m_TempEntries.begin(); it != m_TempEntries.end();)
	{
		bool IsDuplicate = !str_comp(it->m_aTempWar, pName) || !str_comp(it->m_aTempHelper, pName) || !str_comp(it->m_aTempMute, pName);

		if(IsDuplicate)
		{
			it = m_TempEntries.erase(it);
		}
		else
			++it;
	}
	UpdateTempPlayers();
}

void CEClient::RemoveWarEntry(int Type, const char *pName)
{
	CTempEntry Entry(Type, pName, "");
	auto it = std::find(m_TempEntries.begin(), m_TempEntries.end(), Entry);
	if(it != m_TempEntries.end())
		m_TempEntries.erase(it);

	UpdateTempPlayers();
}

void CEClient::UpdateTempPlayers()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		m_TempPlayers[i].IsTempWar = false;
		m_TempPlayers[i].IsTempHelper = false;
		m_TempPlayers[i].IsTempMute = false;
		memset(m_TempPlayers[i].m_aReason, 0, sizeof(m_TempPlayers[i].m_aReason));

		for(CTempEntry &Entry : m_TempEntries)
		{
			if(!str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempWar) && str_comp(Entry.m_aTempWar, "") != 0)
			{
				str_copy(m_TempPlayers[i].m_aReason, Entry.m_aReason);
				m_TempPlayers[i].IsTempWar = true;
			}
			if(!str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempHelper) && str_comp(Entry.m_aTempHelper, "") != 0)
			{
				str_copy(m_TempPlayers[i].m_aReason, Entry.m_aReason);
				m_TempPlayers[i].IsTempHelper = true;
			}
			if(!str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempMute) && str_comp(Entry.m_aTempMute, "") != 0)
			{
				str_copy(m_TempPlayers[i].m_aReason, Entry.m_aReason);
				m_TempPlayers[i].IsTempMute = true;
			}
		}
	}
}

void CEClient::UpdateRainbow()
{
	static bool m_RainbowWasOn = false;

	if(g_Config.m_ClServerRainbow && !m_RainbowWasOn)
	{
		m_RainbowWasOn = false;
	}
	if(m_RainbowWasOn && !g_Config.m_ClServerRainbow)
	{
		GameClient()->SendInfo(false);
		GameClient()->SendDummyInfo(false);
		m_RainbowWasOn = false;
	}
	// Makes the slider look smoother
	static float Speed = 1.0f;
	Speed = Speed + m_RainbowSpeed * Client()->FrameTimeAverage() * 0.1f;

	if(Speed > 255.f * 10) // Reset if Value gets highish, why? why not :D
		Speed = 1.0f;

	float h = round_to_int(Speed) % 255 / 255.f;
	float s = abs(m_RainbowSat[g_Config.m_ClDummy] - 255);
	float l = abs(m_RainbowLht[g_Config.m_ClDummy] - 255);

	m_PreviewRainbowColor[g_Config.m_ClDummy] = getIntFromColor(h, s, l);

	if(Client()->State() == IClient::STATE_ONLINE)
	{
		if(g_Config.m_ClServerRainbow && m_RainbowDelay < time_get() && m_LastMovement + time_freq() * 30 > time_get() && !GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Afk)
		{
			if(m_RainbowBody[g_Config.m_ClDummy] || m_RainbowFeet[g_Config.m_ClDummy])
			{
				if(m_BothPlayers)
				{
					GameClient()->SendDummyInfo(false);
					GameClient()->SendInfo(false);
				}
				else if(g_Config.m_ClDummy)
					GameClient()->SendDummyInfo(false);
				else
					GameClient()->SendInfo(false);
			}
			m_RainbowDelay = time_get() + time_freq() * g_Config.m_SvInfoChangeDelay;
			m_RainbowColor[0] = m_RainbowColor[1] = getIntFromColor(h, s, l);
		}
	}
}

void CEClient::OnShutdown()
{
	// str_copy(g_Config.m_ClDummySkin, g_Config.m_ClSavedDummySkin, sizeof(g_Config.m_ClDummySkin));
	// str_copy(g_Config.m_ClDummyName, g_Config.m_ClSavedDummyName, sizeof(g_Config.m_ClDummyName));
	// str_copy(g_Config.m_ClDummyClan, g_Config.m_ClSavedDummyClan, sizeof(g_Config.m_ClDummyClan));
	// g_Config.m_ClDummyCountry = g_Config.m_ClSavedDummyCountry;
	// g_Config.m_ClDummyColorFeet = g_Config.m_ClSavedDummyColorFeet;
	if(g_Config.m_ClServerRainbow)
	{
		g_Config.m_ClDummyUseCustomColor = g_Config.m_ClSavedDummyUseCustomColor;
		g_Config.m_ClDummyColorBody = g_Config.m_ClSavedDummyColorBody;
	}

	// str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClSavedPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
	// str_copy(g_Config.m_PlayerName, g_Config.m_ClSavedName, sizeof(g_Config.m_PlayerName));
	// str_copy(g_Config.m_PlayerClan, g_Config.m_ClSavedClan, sizeof(g_Config.m_PlayerClan));
	// g_Config.m_PlayerCountry = g_Config.m_ClSavedCountry;
	// g_Config.m_ClPlayerColorFeet = g_Config.m_ClSavedPlayerColorFeet;
	if(g_Config.m_ClServerRainbow)
	{
		g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClSavedPlayerUseCustomColor;
		g_Config.m_ClPlayerColorBody = g_Config.m_ClSavedPlayerColorBody;
	}

	if(g_Config.m_ClDisableGoresOnShutdown)
	{
		g_Config.m_ClGoresMode = 0;
		int Key = g_Config.m_ClGoresModeKey;
		if(Key < KEY_FIRST || Key >= KEY_LAST)
			return;
		GameClient()->m_Binds.Bind(Key, g_Config.m_ClGoresModeSaved);
	}

	g_Config.m_ClKillCounter = m_KillCount;
}

void CEClient::OnInit()
{
	// On client load
	TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

	m_LastMovement = 0;

	m_JoinedTeam = false;
	m_AttempedJoinTeam = false;

	// Rainbow
	m_RainbowColor[0] = g_Config.m_ClPlayerColorBody;

	// Dummy Rainbow
	m_RainbowColor[1] = g_Config.m_ClDummyColorBody;

	if(g_Config.m_ClDisableGoresOnShutdown)
		GoresModeSave();

	// Set Kill Counter
	m_KillCount = g_Config.m_ClKillCounter;

	// First Launch
	if(g_Config.m_ClFirstLaunch)
	{
		m_FirstLaunch = true;
		g_Config.m_ClFirstLaunch = 0;
	}
}

void CEClient::OnNewSnapshot()
{
	UpdateTempPlayers();
	NotifyOnMove();
}

void CEClient::OnRender()
{
	UpdateRainbow();

	if(GameClient()->m_Menus.m_RPC_Ratelimit < time_get() && (GameClient()->m_Menus.m_RPC_Ratelimit - time_get()) / time_freq() > -1)
	{
		Client()->DiscordRPCchange();
		GameClient()->m_Menus.m_RPC_Ratelimit = -2;
	}

	if(Client()->State() == CClient::STATE_DEMOPLAYBACK)
		return;

	UpdateRainbow();
	GoresMode();

	if(m_SentKill)
	{
		GameClient()->m_AntiSpawnBlock.m_SentKill = true;
		m_KillCount++;
		m_SentKill = false;
	}

	if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
	{
		m_LastMovement = time_get();
	}
}
