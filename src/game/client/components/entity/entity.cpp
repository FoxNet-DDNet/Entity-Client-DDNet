#include "entity.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>
#include <base/thread.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/client/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>
#include <game/teamscore.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#if defined(CONF_FAMILY_WINDOWS)
#include <Windows.h>
#endif

#if defined(CONF_FAMILY_WINDOWS)
#include <TlHelp32.h>
#include <processthreadsapi.h>

static bool IsDiscordProcessName(const wchar_t *pProcessName)
{
	return _wcsicmp(pProcessName, L"Discord.exe") == 0 ||
	       _wcsicmp(pProcessName, L"DiscordCanary.exe") == 0 ||
	       _wcsicmp(pProcessName, L"DiscordPTB.exe") == 0 ||
	       _wcsicmp(pProcessName, L"DiscordSystemHelper.exe") == 0;
}
#endif

void CEClient::OnChatMessage(int ClientId, int Team, const char *pMsg)
{
	if(ClientId < 0 || ClientId > MAX_CLIENTS)
		return;

	bool Highlighted = GameClient()->m_Chat.LineHighlighted(ClientId, pMsg);

	if(Team == TEAM_WHISPER_RECV)
		Highlighted = true;

	if(!Highlighted)
		return;
	char aName[16];
	str_copy(aName, GameClient()->m_aClients[ClientId].m_aName, sizeof(aName));

	if(!str_comp(aName, GameClient()->m_aClients[GameClient()->m_aLocalIds[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, GameClient()->m_aClients[GameClient()->m_aLocalIds[1]].m_aName))
		return;

	bool HiddenMessage = GameClient()->m_WarList.m_WarPlayers[ClientId].m_IsMuted || (g_Config.m_ClHideEnemyChat && (GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1]));

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
		if(g_Config.m_ClReplyMuted && GameClient()->m_WarList.m_WarPlayers[ClientId].m_IsMuted)
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
				char TeamChar[8];
				str_format(TeamChar, sizeof(TeamChar), "%d", Team);

				int PrevTeam = -1;

				if(!GameClient()->m_Teams.SameTeam(Local, ClientId) && (Team > 0) && !m_JoinedTeam)
				{
					char aBuf[48] = "/team ";
					str_append(aBuf, TeamChar);
					GameClient()->m_Chat.SendChat(0, aBuf);

					char Joined[48] = "attempting to auto Join ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					m_JoinedTeam = true;
					m_AttemptedJoinTeam = true;
				}
				if(GameClient()->m_Teams.SameTeam(Local, ClientId) && m_JoinedTeam)
				{
					char Joined[48] = "Successfully Joined The Team of ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					LocalTeam = GameClient()->m_Teams.Team(Local);

					PrevTeam = Team;

					m_JoinedTeam = false;
				}
				if(!GameClient()->m_Teams.SameTeam(Local, ClientId) && m_AttemptedJoinTeam)
				{
					char Joined[48] = "Couldn't Join The Team of ";
					str_append(Joined, GameClient()->m_aClients[ClientId].m_aName);
					GameClient()->ClientMessage(Joined);

					m_AttemptedJoinTeam = false;
				}
				if(PrevTeam != Team && m_AttemptedJoinTeam)
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
					m_AttemptedJoinTeam = false;
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
		if(Core.m_aWeapons[WEAPON_GRENADE].m_Got || Core.m_aWeapons[WEAPON_LASER].m_Got || Core.m_aWeapons[WEAPON_SHOTGUN].m_Got)
			m_WeaponsGot = true;
		if((!Core.m_aWeapons[WEAPON_GRENADE].m_Got && !Core.m_aWeapons[WEAPON_LASER].m_Got && !Core.m_aWeapons[WEAPON_SHOTGUN].m_Got) && m_WeaponsGot)
			m_WeaponsGot = false;

		if(m_WeaponsGot)
			return;
	}

	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon == 0)
		GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_WantedWeapon = WEAPON_GUN + 1;
}

void CEClient::OnConnect()
{
	if(g_Config.m_ClDummy)
		return;

	// if current server is type "Gores", turn the config on, else turn it off
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);
	static bool s_SentInfoMessage = false;
	if(m_FirstLaunch && !s_SentInfoMessage)
	{
		GameClient()->ClientMessage("╭──                 E-Client Info");
		GameClient()->ClientMessage("│ Seems like it's your first time running the client!");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ To view a list of Default Chat Commands do \".help\"");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ If you find a bug or have a Feature Request do \".github\"");
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("│ Chat Commands that start with \".\" are silent by default,");
		GameClient()->ClientMessage("│ which means no one will see them.");
		GameClient()->ClientMessage("│ Messages that start with \"!\" will be sent");
		GameClient()->ClientMessage("╰───────────────────────");
		s_SentInfoMessage = true;
	}
	else
	{
		if(g_Config.m_ClAutoEnableGoresMode)
		{
			if(str_find_nocase(CurrentServerInfo.m_aGameType, "Gores"))
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
	}
}

void CEClient::NotifyOnMove()
{
	if(!g_Config.m_ClNotifyOnMove)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
	if(!pGraphics || !Graphics())
		return;
	const CNetObj_Character *pLocalChar = GameClient()->m_Snap.m_pLocalCharacter;
	if(!pLocalChar)
		return;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	int LocalId = GameClient()->m_Snap.m_LocalClientId;

	vec2 LocalPos = GameClient()->m_aClients[LocalId].m_RenderPos;
	if(!pGraphics->WindowActive())
	{
		const float MaxDist = 27.5f;

		bool Moved = false;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
		{
			if(m_LastPos == LocalPos)
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

			float Dist = distance(LocalPos, OtherPos);
			if(Dist < MaxDist + MaxDist)
				Moved = true;

			// check if the player is hooked to the local player
			if(GameClient()->m_aClients[ClientId].m_RenderCur.m_HookedPlayer == LocalId)
				Moved = true;

			// Check for hammer firing
			bool Hammering = (pOtherChar->m_Weapon == WEAPON_HAMMER) && (pOtherChar->m_AttackTick + 2 > Client()->GameTick(g_Config.m_ClDummy));
			Dist = distance(vec2(pOtherChar->m_X, pOtherChar->m_Y), vec2(pLocalChar->m_X, pLocalChar->m_Y));
			if(Hammering && Dist < 70.0f)
				Moved = true;
		}

		if(Moved)
		{
			Client()->Notify("E-Client", "current tile changed");
			Graphics()->NotifyWindow();
		}
	}
	m_LastPos = LocalPos;
}

void CEClient::UpdateVolleyball()
{
	bool IsVolleyBall = false;
	if(g_Config.m_EcVolleyBallBetterBall > 0 && g_Config.m_EcVolleyBallBetterBallSkin[0] != '\0')
	{
		if(g_Config.m_EcVolleyBallBetterBall > 1)
			IsVolleyBall = true;
		else
			IsVolleyBall = str_startswith_nocase(GameClient()->Map()->BaseName(), "volleyball");
	};
	for(auto &Client : GameClient()->m_aClients)
	{
		Client.m_IsVolleyBall = IsVolleyBall && Client.m_DeepFrozen;
	}
}

void CEClient::UpdateRainbow()
{
	static bool s_RainbowWasOn = false;

	if(g_Config.m_ClServerRainbow && !s_RainbowWasOn)
	{
		s_RainbowWasOn = true;
	}
	if(s_RainbowWasOn && !g_Config.m_ClServerRainbow)
	{
		GameClient()->SendInfo(false);
		GameClient()->SendDummyInfo(false);
		s_RainbowWasOn = false;
	}
	// Makes the slider look smoother
	static float s_Speed = 1.0f;
	s_Speed = s_Speed + m_RainbowSpeed * Client()->FrameTimeAverage() * 0.1f;

	if(s_Speed > 255.f * 10) // Reset if Value gets highish, why? why not :D
		s_Speed = 1.0f;
	float h = round_to_int(s_Speed) % 255 / 255.f;
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

	g_Config.m_ClKillCounter = m_KillCount;
}

void CEClient::SetDDNetProcessPriority(bool Set)
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!SetPriorityClass(GetCurrentProcess(), Set ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS))
	{
		log_info("entity-client", Set ? "Failed to set process priority" : "Failed to reset process priority");
		return;
	}
	if(!SetThreadPriority(GetCurrentThread(), Set ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_NORMAL))
	{
		log_info("entity-client", Set ? "Failed to set thread priority" : "Failed to reset thread priority");
		return;
	}
#endif
}

void CEClient::DiscordPriorityThread(void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->SetDiscordProcessesNormalPriority();
	pSelf->m_DiscordPriorityDelay.store(time_get() + time_freq() * 30);
	pSelf->m_DiscordPriorityThreadRunning.store(false);
}

void CEClient::StartDiscordPriorityThread()
{
	// Don't start the thread if it's already running
	if(m_DiscordPriorityThreadRunning.load())
		return;

	if(m_pDiscordPriorityThread)
	{
		thread_wait(m_pDiscordPriorityThread);
		m_pDiscordPriorityThread = nullptr;
	}

	m_DiscordPriorityDelay.store(0);
	m_DiscordPriorityThreadRunning.store(true);
	m_pDiscordPriorityThread = thread_init(DiscordPriorityThread, this, "discord-priority");
}

void CEClient::SetDiscordProcessesNormalPriority()
{
#if defined(CONF_FAMILY_WINDOWS)
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnapshot == INVALID_HANDLE_VALUE)
	{
		log_info("entity-client", "Failed to create process snapshot");
		return;
	}

	PROCESSENTRY32 Entry;
	mem_zero(&Entry, sizeof(Entry));
	Entry.dwSize = sizeof(Entry);

	int Changed = 0;
	int Failed = 0;

	if(Process32First(hSnapshot, &Entry))
	{
		do
		{
			if(!IsDiscordProcessName(Entry.szExeFile))
				continue;

			HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Entry.th32ProcessID);
			if(!hProcess)
			{
				Failed++;
				continue;
			}

			if(SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS))
				Changed++;
			else
				Failed++;

			CloseHandle(hProcess);
		} while(Process32Next(hSnapshot, &Entry));
	}

	CloseHandle(hSnapshot);
#endif
}

void CEClient::OnInit()
{
	SetDDNetProcessPriority(g_Config.m_ClHighProcessPriority);
	if(g_Config.m_ClDiscordNormalProcessPriority)
		StartDiscordPriorityThread();

	// On client load
	TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

	m_LastMovement = 0;

	m_JoinedTeam = false;
	m_AttemptedJoinTeam = false;

	// Rainbow
	m_RainbowColor[0] = g_Config.m_ClPlayerColorBody;

	// Dummy Rainbow
	m_RainbowColor[1] = g_Config.m_ClDummyColorBody;

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
	NotifyOnMove();
	UpdateVolleyball();
}

void CEClient::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
	{
		m_JoinedTeam = false;
		m_AttemptedJoinTeam = false;
		m_LastReplyId = -1;
		m_aLastPing = CLastPing();
	}
}

void CEClient::OnRender()
{
	const int64_t DiscordPriorityDelay = m_DiscordPriorityDelay.load();
	if(g_Config.m_ClDiscordNormalProcessPriority && !m_DiscordPriorityThreadRunning.load() && DiscordPriorityDelay < time_get())
	{
		StartDiscordPriorityThread();
	}

	if(Client()->State() == CClient::STATE_DEMOPLAYBACK)
		return;

	UpdateRainbow();
	GoresMode();

	if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
	{
		m_LastMovement = time_get();
	}
}

void CEClient::OnSelfDeath()
{
	m_KillCount++;
}

void CEClient::OnFocusChange(bool IsFocused)
{
	SetDDNetProcessPriority(g_Config.m_ClHighProcessPriority);
}
