#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <engine/serverbrowser.h>

#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol.h>
#include <game/gamecore.h>

#include <base/system.h>
#include <base/math.h>

#include <cmath>
#include "entity.h"

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

int CEClient::Get128Name(const char *pMsg, char *pName)
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

void CEClient::OnChatMessage(int ClientId, int Team, const char *pMsg)
{
	if(ClientId < 0 || ClientId > MAX_CLIENTS)
		return;

	bool Highlighted = false;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(GameClient()->m_aLocalIds[0] == -1)
			return;
		if(GameClient()->Client()->DummyConnected() && GameClient()->m_aLocalIds[1] == -1)
			return;
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && (!GameClient()->Client()->DummyConnected() || ClientId != GameClient()->m_aLocalIds[1]))
		{
			// main character
			Highlighted |= LineShouldHighlight(pMsg, GameClient()->m_aClients[GameClient()->m_aLocalIds[0]].m_aName);
			// dummy
			Highlighted |= GameClient()->Client()->DummyConnected() && LineShouldHighlight(pMsg, GameClient()->m_aClients[GameClient()->m_aLocalIds[1]].m_aName);
		}
	}
	else
	{
		if(GameClient()->m_Snap.m_LocalClientId == -1)
			return;
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pMsg, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}

	if(Team == 3) // whisper recv
		Highlighted = true;

	if(!Highlighted)
		return;
	char aName[16];
	str_copy(aName, GameClient()->m_aClients[ClientId].m_aName, sizeof(aName));
	if(ClientId == 63 && !str_comp_num(GameClient()->m_aClients[ClientId].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("E-Client", "fixname 128 player '%s' -> '%s'", GameClient()->m_aClients[ClientId].m_aName, aName);
	}
	// ignore own and dummys messages
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
	
	if(g_Config.m_ClReplyMuted && (GameClient()->m_WarList.m_WarPlayers[ClientId].IsMuted || m_TempPlayers[ClientId].IsTempMute))
	{
		if(!GameClient()->m_Snap.m_pLocalCharacter)
			return;

		char Text[2048];
		str_format(Text, sizeof(Text), "%s: %s", aName, g_Config.m_ClAutoReplyMutedMsg);

		if(Team == 3) // whisper recv
			str_format(Text, sizeof(Text), "/w %s %s", aName, g_Config.m_ClAutoReplyMutedMsg);

		GameClient()->m_Chat.SendChat(0, Text);
	}
	else if(g_Config.m_ClTabbedOutMsg)
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
			if(str_comp(GameClient()->m_aClients [ClientId].m_aName, g_Config.m_ClAutoJoinTeamName) == 0)
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

	CCharacterCore Core = GameClient()->m_PredictedPrevChar;

	if(g_Config.m_ClGoresModeDisableIfWeapons && g_Config.m_ClGoresMode)
	{

		if((Core.m_aWeapons[WEAPON_GRENADE].m_Got || Core.m_aWeapons[WEAPON_LASER].m_Got || Core.m_ExplosionGun || Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && g_Config.m_ClGoresMode)
		{
			g_Config.m_ClGoresMode = 0;
			m_WeaponsGot = true;
			m_GoresModeWasOn = true;
		}
		if((!Core.m_aWeapons[WEAPON_GRENADE].m_Got && !Core.m_aWeapons[WEAPON_LASER].m_Got && !Core.m_ExplosionGun && !Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && m_WeaponsGot)
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

	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;

	// actual code lmfao

	int Key = Input()->FindKeyByName(g_Config.m_ClGoresModeKey);
	if(Key == KEY_UNKNOWN)
	{
		g_Config.m_ClGoresMode = 0;
		dbg_msg("E-Client", "Invalid key: %s", g_Config.m_ClGoresModeKey);
	}
	else
	{
		bool GoresBind;
		const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot(g_Config.m_ClGoresModeKey);
		if(!str_comp(GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key], "+fire;+prevweapon"))
			GoresBind = true;
		else
			GoresBind = false;

		if(g_Config.m_ClGoresMode && GoresBind)
		{
			if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == 0)
			{
				GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = 2;
			}
		}
	}
}

void CEClient::OnConnect()
{
	// if dummy, return, so it doesnt display the info when joining with dummy

	if(g_Config.m_ClDummy)
		return;

	GameClient()->m_EClient.m_LastMovement = time_get() + time_freq() * 60;

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

void CEClient::ChangeTileNotifyTick()
{
	if(!g_Config.m_ClChangeTileNotification)
		return;
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;

	float X = GameClient()->m_Snap.m_aCharacters[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_X;
	float Y = GameClient()->m_Snap.m_aCharacters[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_Y;
	int CurrentTile = Collision()->GetTileIndex(Collision()->GetPureMapIndex(X, Y));
	if(m_LastTile != CurrentTile && m_LastNotification + time_freq() < time_get())
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			Client()->Notify("E-Client", "current tile changed");
			Graphics()->NotifyWindow();
		}
		m_LastNotification = time_get();
	}
	m_LastTile = CurrentTile;
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

void CEClient::Rainbow()
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
		if(g_Config.m_ClServerRainbow && m_RainbowDelay < time_get() && m_LastMovement > time_get() && !GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Afk)
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
		g_Config.m_ClGoresMode = 0;

	g_Config.m_ClKillCounter = m_KillCount;
}


void CEClient::OnInit()
{
	// On client load
	TextRender()->SetCustomFace(g_Config.m_ClCustomFont);
	
	m_LastMovement = 0;

	m_JoinedTeam = false;
	m_KogModeRebound = false;
	m_AttempedJoinTeam = false;

	// Rainbow
	m_RainbowColor[0] = g_Config.m_ClPlayerColorBody;

	// Dummy Rainbow
	m_RainbowColor[1] = g_Config.m_ClDummyColorBody;

	// Get Bindslot for Mouse1, default shoot bind
	int Key = Input()->FindKeyByName(g_Config.m_ClGoresModeKey);
	if(Key == KEY_UNKNOWN)
		dbg_msg("E-Client", "Invalid key: %s", g_Config.m_ClGoresModeKey);
	else
	{
		const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot(g_Config.m_ClGoresModeKey);
		*g_Config.m_ClGoresModeSaved = *GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];

		// Tells you what the bind is
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Gores Mode Saved Bind Currently is: %s", g_Config.m_ClGoresModeSaved);
		dbg_msg("E-Client", aBuf);

		// Binds the mouse to the saved bind, also doe
		GameClient()->m_Binds.Bind(Key, g_Config.m_ClGoresModeSaved);
	}

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
	ChangeTileNotifyTick();
	UpdateTempPlayers();
	// AutoJoinTeam();
}

void CEClient::OnRender()
{
	if(Client()->State() == CClient::STATE_DEMOPLAYBACK)
		return;

	Rainbow();
	GoresMode();

	// Set Offline RPC on Client start
	if(g_Config.m_ClDiscordRPC)
	{
		static int64_t Delay = time_get() + time_freq();
		static bool SetRpc = false;
		if(!SetRpc && Client()->State() != Client()->STATE_LOADING)
		{
			if(Delay < time_get())
			{
				Client()->DiscordRPCchange();
				SetRpc = true;
			}
		}
	}

	if(m_SentKill)
	{
		GameClient()->m_AntiSpawnBlock.m_SentKill = true;
		m_KillCount++;
		m_SentKill = false;
	}

	// "secret" effect, makes a circle go around the player
	if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
	{
		m_LastMovement = time_get() + time_freq() * 30;
	}

	if(GameClient()->m_Menus.m_RPC_Ratelimit < time_get() && (GameClient()->m_Menus.m_RPC_Ratelimit - time_get()) / time_freq() > -1)
	{
		Client()->DiscordRPCchange();
		GameClient()->m_Menus.m_RPC_Ratelimit = -2;
	}
}