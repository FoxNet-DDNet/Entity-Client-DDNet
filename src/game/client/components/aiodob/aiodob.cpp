#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/gamecore.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol.h>
#include "aiodob.h"
#include <base/system.h>
#include <base/math.h>
#include <cmath>
#include <engine/serverbrowser.h>

int CAiodob::IdWithName(const char *pName)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			return ClientId;
		}
	}
	return -1;
}

bool CAiodob::LineShouldHighlight(const char *pLine, const char *pName)
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

int CAiodob::Get128Name(const char *pMsg, char *pName)
{
	int i = 0;
	for(i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':' && pMsg[i + 1] == ' ')
		{
			str_copy(pName, pMsg, i + 1);
			return i;
		}
	}
	str_copy(pName, " ", 2);
	return -1;
}

void CAiodob::OnChatMessage(int ClientId, int Team, const char *pMsg)
{
	if(ClientId < 0 || ClientId > MAX_CLIENTS)
		return;

	bool Highlighted = false;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(m_pClient->m_aLocalIds[0] == -1)
			return;
		if(m_pClient->Client()->DummyConnected() && m_pClient->m_aLocalIds[1] == -1)
			return;
		if(ClientId >= 0 && ClientId != m_pClient->m_aLocalIds[0] && (!m_pClient->Client()->DummyConnected() || ClientId != m_pClient->m_aLocalIds[1]))
		{
			// main character
			Highlighted |= LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName);
			// dummy
			Highlighted |= m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName);
		}
	}
	else
	{
		if(m_pClient->m_Snap.m_LocalClientId == -1)
			return;
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= m_pClient->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_aName);
	}

	if(Team == 3) // whisper recv
		Highlighted = true;

	if(!Highlighted)
		return;
	char aName[16];
	str_copy(aName, m_pClient->m_aClients[ClientId].m_aName, sizeof(aName));
	if(ClientId == 63 && !str_comp_num(m_pClient->m_aClients[ClientId].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("aiodob", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientId].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName))
		return;

	// could iterate over ping history and also ignore older duplicates
	// ignore duplicated messages
	if(!str_comp(m_aLastPings[0].m_aMessage, pMsg))
		return;

	if(g_Config.m_ClReplyMuted && GameClient()->m_aClients[ClientId].m_IsMute) // || (GameClient()->m_WarList.IsWarlist(m_pClient->m_aClients[ClientId].m_aName) && g_Config.m_ClHideEnemyChat))
	{
		if(!GameClient()->m_Snap.m_pLocalCharacter)
			return;

		if(Team == 3) // whisper recv
		{
			char Text[2048];
			str_format(Text, sizeof(Text), "/w %s ", aName);
			str_append(Text, g_Config.m_ClAutoReplyMutedMsg);
			GameClient()->m_Chat.SendChat(0, Text);
		}
		else
		{
			char Text[2048];
			str_format(Text, sizeof(Text), "%s: ", aName);
			str_append(Text, g_Config.m_ClAutoReplyMutedMsg);
			GameClient()->m_Chat.SendChat(0, Text);
		}
	}
	if(g_Config.m_ClTabbedOutMsg && !GameClient()->m_aClients[ClientId].m_IsMute)
	{
		if(!GameClient()->m_Snap.m_pLocalCharacter)
			return;

		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			if(Team == 3) // whisper recv
			{
				char Text[2048];
				str_format(Text, sizeof(Text), "/w %s ", aName);
				str_append(Text, g_Config.m_ClAutoReplyMsg);
				GameClient()->m_Chat.SendChat(0, Text);
			}
			else
			{
				char Text[2048];
				str_format(Text, sizeof(Text), "%s: ", aName);
				str_append(Text, g_Config.m_ClAutoReplyMsg);
				GameClient()->m_Chat.SendChat(0, Text);
			}
		}
	}
}

void CAiodob::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
	}
}


void CAiodob::AutoJoinTeam()
{
	if(m_JoinTeam > time_get())
		return;

	if(!g_Config.m_ClAutoJoinTest)
		return;

	if(m_pClient->m_Chat.IsActive())
		return;

	if(GameClient()->CurrentRaceTime())
		return;

	int Local = m_pClient->m_Snap.m_LocalClientId;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pClient->m_Teams.Team(i))
		{
			if(str_comp(m_pClient->m_aClients[i].m_aName, g_Config.m_ClAutoJoinTeamName) == 0)
			{
				int LocalTeam = -1;


				if(i == Local)
					return;

				

				int Team = GameClient()->m_Teams.Team(i);
				char TeamChar[256];
				str_format(TeamChar, sizeof(TeamChar), "%d", Team);

				int PrevTeam = -1;

				if(!m_pClient->m_Teams.SameTeam(Local, i) && (Team > 0) && !m_JoinedTeam)
				{
					char aBuf[2048] = "/team ";
					str_append(aBuf, TeamChar);
					m_pClient->m_Chat.SendChat(0, aBuf);

					char Joined[2048] = "attempting to auto Join ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					m_JoinedTeam = true;
					m_AttempedJoinTeam = true;
				}
				if(m_pClient->m_Teams.SameTeam(Local, i) && m_JoinedTeam)
				{
					char Joined[2048] = "Successfully Joined The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					LocalTeam = GameClient()->m_Teams.Team(Local);

					PrevTeam = Team;

					m_JoinedTeam = false;
				}
				if(!m_pClient->m_Teams.SameTeam(Local, i) && m_AttempedJoinTeam)
				{
					char Joined[2048] = "Couldn't Join The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					m_AttempedJoinTeam = false;
				}
				if(PrevTeam != Team && m_AttempedJoinTeam)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "team has changed");
					m_JoinedTeam = false;
				}
				if(LocalTeam > 0)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "self team is bigger than 0");
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

void CAiodob::GoresMode()
{
	// if turning off kog mode and it was on before, rebind to previous bind

	CCharacterCore Core = GameClient()->m_PredictedPrevChar;

	if(g_Config.m_ClGoresModeDisableIfWeapons && g_Config.m_ClGoresMode)
	{

		if((Core.m_aWeapons[WEAPON_GRENADE].m_Got || Core.m_aWeapons[WEAPON_LASER].m_Got || Core.m_ExplosionGun || Core.m_ShortExplosionGun || Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && g_Config.m_ClGoresMode)
		{
			g_Config.m_ClGoresMode = 0;
			m_WeaponsGot = true;
			m_GoresModeWasOn = true;
		}
		if((!Core.m_aWeapons[WEAPON_GRENADE].m_Got && !Core.m_aWeapons[WEAPON_LASER].m_Got && !Core.m_ExplosionGun && !Core.m_ShortExplosionGun && !Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && m_WeaponsGot)
		{
			g_Config.m_ClGoresMode = 1;
			m_WeaponsGot = false;
		}
	}

	if(!g_Config.m_ClGoresMode && m_KogModeRebound)
	{
	
		GameClient()->m_Binds.Bind(KEY_MOUSE_1, g_Config.m_ClGoresModeSaved);
		m_KogModeRebound = false;
	}

	// if hasnt rebound yet and turning on kog mode, rebind to kog mode

	if(!m_KogModeRebound && g_Config.m_ClGoresMode)
	{
		GameClient()->m_Binds.Bind(KEY_MOUSE_1, "+fire;+prevweapon");
		m_KogModeRebound = true;
	}

	// if not local return

	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;

	// actual code lmfao

	bool GoresBind;
	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot("mouse1");
	if(!str_comp(GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key], "+fire;+prevweapon"))
		GoresBind = true;
	else
		GoresBind = false;

	if(g_Config.m_ClGoresMode && GoresBind)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter->m_Weapon == 0)
		{
			GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 2;
		}
	}

}

void CAiodob::OnConnect()
{
	// if dummy, return, so it doesnt display the info when joining with dummy

	if(g_Config.m_ClDummy)
		return;

	// if current server is type "Gores", turn the config on, else turn it off

	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);

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

	int Local = m_pClient->m_Snap.m_LocalClientId;

	char aBuf[512];

	if(g_Config.m_ClListsInfo)
	{
		int NumberWars = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool War = GameClient()->m_WarList.GetWarData(IdWithName(Client.m_aName)).m_WarGroupMatches[1];
			bool TempWar = m_TempPlayers[IdWithName(Client.m_aName)].IsTempWar;

			if(!Client.m_Active && !Local)
				continue;

			if((War && !TempWar) || (!War && TempWar))
				NumberWars++;
		}

		int NumberTeams = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool Team = GameClient()->m_WarList.GetWarData(IdWithName(Client.m_aName)).m_WarGroupMatches[2];

			if(!Client.m_Active && !Local)
				continue;

			if(Team)
				NumberTeams++;
		}

		int NumberHelpers = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool Helper = GameClient()->m_WarList.GetWarData(IdWithName(Client.m_aName)).m_WarGroupMatches[3];
			bool TempHelper = m_TempPlayers[IdWithName(Client.m_aName)].IsTempHelper;

			if(!Client.m_Active && !Local)
				continue;

			if((Helper && !TempHelper) || (!Helper && TempHelper))
				NumberHelpers++;
		}

		int NumberMutes = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool Mute = GameClient()->m_aClients[IdWithName(Client.m_aName)].m_Foe;
			bool TempMute = m_TempPlayers[IdWithName(Client.m_aName)].IsTempMute;

			if(!Client.m_Active && !Local)
				continue;

			if((Mute && !TempMute) || (!Mute && TempMute))
				NumberMutes++;
		}
		str_format(aBuf, sizeof(aBuf), "│ %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams, NumberWars, NumberHelpers, NumberMutes);
	}

	// info when joining a server of enabled components

	if(g_Config.m_ClEnabledInfo)
	{
		GameClient()->aMessage("╭──                  Aiodob Info");
		GameClient()->aMessage("│");
		
		if(g_Config.m_ClListsInfo)
		{
			GameClient()->aMessage(aBuf);
			GameClient()->aMessage("│");

		}
		if((g_Config.m_ClAutoKill && str_comp(Client()->GetCurrentMap(), "Multeasymap") == 0 && g_Config.m_ClAutoKillMultOnly) || (!g_Config.m_ClAutoKillMultOnly && g_Config.m_ClAutoKill))
		{
			GameClient()->aMessage("│ Auto Kill Enabled!");
			GameClient()->aMessage("│");
		}
		else if(g_Config.m_ClAutoKill && (g_Config.m_ClAutoKillMultOnly && str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0))
		{
			GameClient()->aMessage("│ Auto Kill Disabled, Not on Mult!");
			GameClient()->aMessage("│");
		}
		else if(!g_Config.m_ClAutoKill)
		{
			GameClient()->aMessage("│ Auto Kill Disabled!");
			GameClient()->aMessage("│");
		}

		// Freeze Kill

		if((g_Config.m_ClFreezeKill && str_comp(Client()->GetCurrentMap(), "Multeasymap") == 0 && g_Config.m_ClFreezeKillMultOnly) || (!g_Config.m_ClFreezeKillMultOnly && g_Config.m_ClFreezeKill))
		{
			GameClient()->aMessage("│ Freeze Kill Enabled!");
			GameClient()->aMessage("│");
		}
		else if(g_Config.m_ClFreezeKill && (g_Config.m_ClFreezeKillMultOnly && str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0))
		{
			GameClient()->aMessage("│ Freeze Kill Disabled, Not on Mult!");
			GameClient()->aMessage("│");
		}
		if(!g_Config.m_ClFreezeKill)
		{
			GameClient()->aMessage("│ Freeze Kill Disabled!");
			GameClient()->aMessage("│");
		}
		if(g_Config.m_ClGoresMode)
		{
			GameClient()->aMessage("│ Gores Mode: ON");
			GameClient()->aMessage("│");
		}
		else
		{
			GameClient()->aMessage("│ Gores Mode: OFF");
			GameClient()->aMessage("│");
		}
		if(g_Config.m_ClChatBubble)
		{
			GameClient()->aMessage("│ Chat Bubble is Currently: ON");
			GameClient()->aMessage("│");
		}
		else
		{
			GameClient()->aMessage("│ Chat Bubble is Currently: OFF");
			GameClient()->aMessage("│");
		}
		GameClient()->aMessage("╰───────────────────────");
	}

	// disables connected so it only does it once on join
}

void CAiodob::ChangeTileNotifyTick()
{
	if(!g_Config.m_ClChangeTileNotification)
		return;
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;

	float X = m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_X;
	float Y = m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_Y;
	int CurrentTile = Collision()->GetTileIndex(Collision()->GetPureMapIndex(X, Y));
	if(m_LastTile != CurrentTile && m_LastNotification + time_freq() * 10 < time_get())
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			Client()->Notify("aiodob", "current tile changed");
			Graphics()->NotifyWindow();
		}
		m_LastNotification = time_get();
	}
	m_LastTile = CurrentTile;
}

void CAiodob::RemoveWarEntryDuplicates(const char *pName)
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
}

void CAiodob::RemoveWarEntry(const char *pNameW, const char *pNameH, const char *pNameM)
{
	CTempEntry Entry(pNameW, pNameH, pNameM);
	auto it = std::find(m_TempEntries.begin(), m_TempEntries.end(), Entry);
	if(it != m_TempEntries.end())
		m_TempEntries.erase(it);
}

void CAiodob::UpdateTempPlayers()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		m_TempPlayers[i].IsTempWar = false;
		m_TempPlayers[i].IsTempHelper = false;
		m_TempPlayers[i].IsTempMute = false;

		for(CTempEntry &Entry : m_TempEntries)
		{
			if(str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempWar) == 0 && str_comp(Entry.m_aTempWar, "") != 0)
				m_TempPlayers[i].IsTempWar = true;
			if(str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempHelper) == 0 && str_comp(Entry.m_aTempHelper, "") != 0)
				m_TempPlayers[i].IsTempHelper = true;
			if(str_comp(GameClient()->m_aClients[i].m_aName, Entry.m_aTempMute) == 0 && str_comp(Entry.m_aTempMute, "") != 0)
				m_TempPlayers[i].IsTempMute = true;
		}
	}
}

void CAiodob::Rainbow()
{
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

	float h = (round_to_int(static_cast<float>(time_get()) / time_freq() * m_RainbowSpeed * 0.1f) % 255 / 255.f);
	float s = abs(m_Saturation - 255);
	float l = abs(m_Lightness - 255);

	if(m_ShowServerSide)
	{
		if(m_ServersideDelay < time_get())
		{
			m_PreviewRainbowColor = getIntFromColor(h, s, l);
			m_ServersideDelay = time_get() + time_freq() * 5.10;
		}
	}
	else
		m_PreviewRainbowColor = getIntFromColor(h, s, l);

	if(m_RainbowDelay < time_get() && g_Config.m_ClServerRainbow && m_LastMovement > time_get() && !m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_Afk)
	{
		if(g_Config.m_ClDummy)
			GameClient()->SendDummyInfo(false);
		else
			GameClient()->SendInfo(false);
		m_RainbowDelay = time_get() + time_freq() * 5.10;
		m_RainbowColor = getIntFromColor(h, s, l);
	}
}

void CAiodob::OnNewSnapshot()
{
	UpdateTempPlayers();
}

void CAiodob::OnInit()
{
	// on client load
	TextRender()->SetCustomFace(g_Config.m_ClCustomFont);
	
	const char *A = "Aiodob";

	if(str_comp(g_Config.m_ClSavedName, A) && str_comp(g_Config.m_ClSavedDummyName, A) && str_comp(g_Config.m_ClSavedClan, A)&& str_comp(g_Config.m_ClSavedDummyClan, A))
		SaveSkin();

	m_ServersideDelay = 0;
	m_RainbowSpeed = 10;
	m_LastMovement = 0;
	m_RainbowDelay = 0;
	m_Saturation = 200;
	m_Lightness = 30;
	m_LastTile = -1;
	m_JoinTeam = 0;
	m_JoinedTeam = false;
	m_RainbowWasOn = false;
	m_KogModeRebound = false;
	m_AttempedJoinTeam = false;
	dbg_msg("Aiodob", "Aiodob Client Features Loaded Successfully!");

	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot("mouse1");
	*g_Config.m_ClGoresModeSaved = *GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "Gores Mode Saved Bind Currently is: %s", g_Config.m_ClGoresModeSaved);
	dbg_msg("Aiodob", aBuf);

	GameClient()->m_Binds.Bind(KEY_MOUSE_1, g_Config.m_ClGoresModeSaved);
}

void CAiodob::OnShutdown()
{
	//str_copy(g_Config.m_ClDummySkin, g_Config.m_ClSavedDummySkin, sizeof(g_Config.m_ClDummySkin));
	//str_copy(g_Config.m_ClDummyName, g_Config.m_ClSavedDummyName, sizeof(g_Config.m_ClDummyName));
	//str_copy(g_Config.m_ClDummyClan, g_Config.m_ClSavedDummyClan, sizeof(g_Config.m_ClDummyClan));
	//g_Config.m_ClDummyCountry = g_Config.m_ClSavedDummyCountry;
	if(g_Config.m_ClServerRainbow)
	{
		g_Config.m_ClDummyUseCustomColor = g_Config.m_ClSavedDummyUseCustomColor;
		g_Config.m_ClDummyColorBody = g_Config.m_ClSavedDummyColorBody;
	}
	//g_Config.m_ClDummyColorFeet = g_Config.m_ClSavedDummyColorFeet;
	

	//str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClSavedPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
	//str_copy(g_Config.m_PlayerName, g_Config.m_ClSavedName, sizeof(g_Config.m_PlayerName));
	//str_copy(g_Config.m_PlayerClan, g_Config.m_ClSavedClan, sizeof(g_Config.m_PlayerClan));
	//g_Config.m_PlayerCountry = g_Config.m_ClSavedCountry;
	if(g_Config.m_ClServerRainbow)
	{
		g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClSavedPlayerUseCustomColor;
		g_Config.m_ClPlayerColorBody = g_Config.m_ClSavedPlayerColorBody;
	}
	//g_Config.m_ClPlayerColorFeet = g_Config.m_ClSavedPlayerColorFeet;

	if(g_Config.m_ClDisableGoresOnShutdown)
		g_Config.m_ClGoresMode = 0;
}

void CAiodob::OnRender()
{
	if(g_Config.m_ClServerRainbow)
		Rainbow();

	ChangeTileNotifyTick();
	GoresMode();
	AutoJoinTeam();

	// "secret" effect, makes a circle go around the player

	if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
	{
		m_LastMovement = time_get() + time_freq() * 30;
	}
}