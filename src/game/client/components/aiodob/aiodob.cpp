#include <engine/client.h>
#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>
#include <game/client/prediction/entities/character.h>
#include "aiodob.h"

void CAiodob::OnInit()
{
	// on client load
	TextRender()->SetCustomFace(g_Config.m_ClCustomFont);

	AttempedJoinTeam = false;
	JoinedTeam = false;
	m_SentKill = false;
	m_SentAutoKill = false;
	m_KogModeRebound = false;
	m_Mult = 0;
	m_JoinTeam = 0;
	m_LastTile = -1;
	m_LastFreeze = 0;
	m_LastMovement = 0;
	dbg_msg("Aiodob", "Aiodob Client Features Loaded Successfully!");

	const CBinds::CBindSlot BindSlot = GameClient()->m_Binds.GetBindSlot("mouse1");
	*g_Config.m_ClGoresModeSaved = *GameClient()->m_Binds.m_aapKeyBindings[BindSlot.m_ModifierMask][BindSlot.m_Key];
	
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "Gores Mode Saved Bind Currently is: %s", g_Config.m_ClGoresModeSaved);
	dbg_msg("Aiodob", aBuf);
}

void CAiodob::FreezeKill()
{
	float Time = g_Config.m_ClFreezeKillMs / 1000.0f;

	float TimeReset = time_get() + time_freq() * Time;

	// if freeze kill isnt turned on, stop

	if(!g_Config.m_ClFreezeKill)
	{
		m_LastFreeze = TimeReset;
		return;
	}

	// if player hasnt started the race, stop

	if(!GameClient()->CurrentRaceTime())
	{
		m_SentKill = false;
		m_LastFreeze = TimeReset + 3;
		return;
	}
	
	// if map name isnt "Multeasymap", stop

	if(g_Config.m_ClFreezeKillMultOnly)
		if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
			return;

	// debug

	if(g_Config.m_ClFreezeKillDebug)
	{
		float a = (m_LastFreeze - time_get()) / 1000000000.0f;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "until kill: %f", a );
		GameClient()->TextRender()->Text(50, 100, 10, aBuf);
	}
	
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// stuff

		int Local = m_pClient->m_Snap.m_LocalClientId;

		CCharacterCore *pCharacterOther = &m_pClient->m_aClients[i].m_Predicted;

		CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

		vec2 Position = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_RenderPos;
		CGameClient::CClientData OtherTee = m_pClient->m_aClients[i];
		int Distance = g_Config.m_ClFreezeKillTeamDistance * 100;

		// if tried to kill, stop

		if(m_SentKill == true)
			return;

		// stop when spectating

		if(m_pClient->m_aClients[Local].m_Paused || m_pClient->m_aClients[Local].m_Spec)
			m_LastFreeze = TimeReset;

		// dont kill if moving

		if((pCharacter->m_IsInFreeze || m_pClient->m_aClients[Local].m_FreezeEnd > 0) && i == Local && g_Config.m_ClFreezeDontKillMoving)
		{
			if(!m_pClient->m_Menus.IsActive() || !m_pClient->m_Chat.IsActive())
				if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
					m_LastFreeze = TimeReset;
		}

		// dont kill if teamate is in x * 2 blocks range

		if(g_Config.m_ClFreezeKillTeamClose && OtherTee.m_IsTeam && !OtherTee.m_Solo && OtherTee.m_Team == m_pClient->m_aClients[Local].m_Team && i != Local)
		{
			if(!((OtherTee.m_RenderPos.x < Position.x - Distance) || (OtherTee.m_RenderPos.x > Position.x + Distance) || (OtherTee.m_RenderPos.y > Position.y + Distance) || (OtherTee.m_RenderPos.y < Position.y - Distance)))
			{
				if(!pCharacterOther->m_IsInFreeze)
				{
					m_LastFreeze = TimeReset;
				}
			}
		}

		// wait x amount of seconds before killing

		if(g_Config.m_ClFreezeKillWaitMs)
		{

			// kill if frozen (without deep and live freeze)

			if(m_pClient->m_aClients[Local].m_FreezeEnd < 3 && !g_Config.m_ClFreezeKillOnlyFullFrozen && !pCharacter->m_IsInFreeze)
				m_LastFreeze = TimeReset;

			// only kill if player is in a freeze tile

			if(g_Config.m_ClFreezeKillOnlyFullFrozen )
			{
				if(!pCharacter->m_IsInFreeze)
					m_LastFreeze = TimeReset;

				// dont kill if not touching the ground (might crash not sure yet)

				//if(pCharacter->m_IsInFreeze && !pChar->IsGrounded() && g_Config.m_ClFreezeKillGrounded)
				//{
				//	m_LastFreeze = TimeReset;
				//}
			}

			// default kill protection timer
			if(m_LastFreeze <= time_get())
			{
				if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
				{
					m_pClient->m_Chat.SendChat(0, "/kill");
					m_SentKill = true;
					m_LastFreeze = time_get() + time_freq() * 5;
					return;
				}
				else if((pCharacter->m_IsInFreeze || m_pClient->m_aClients[Local].m_FreezeEnd > 0))
				{
					GameClient()->SendKill(Local);
					m_SentKill = true;
					return;
				}
			}
		}

		// if not wating for x amount of seconds

		else if(pCharacter->m_IsInFreeze)
		{
			if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
			{
				m_pClient->m_Chat.SendChat(0, "/kill");
				m_SentKill = true;
				m_LastFreeze = time_get() + time_freq() * 5;
			}
			else
			{
				GameClient()->SendKill(Local);
				m_SentKill = true;
			}
			return;
		}
	}
}

void CAiodob::AutoKill()
{
	const int Local = m_pClient->m_Snap.m_LocalClientId;
	CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

	if(g_Config.m_ClAutoKill)
	{

		// if sent kill, stop

		if(m_SentAutoKill == true)
			return;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharacterOther = &m_pClient->m_aClients[i].m_Predicted;
			
			// if not on "Multeasymap", stop

			if(g_Config.m_ClAutoKillMultOnly)
			{
				if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
				{
					if(g_Config.m_ClAutoKillDebug)
						TextRender()->Text(100, 50, 15, "Not on Mult");
					return;
				}
				else if(g_Config.m_ClAutoKillDebug)
					TextRender()->Text(100, 50, 15, "On Mult");
			}

			// stuff
			
			const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Prev;
			const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Cur;

			const CNetObj_Character *pPrevCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Prev;
			const CNetObj_Character *pCurCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Cur;

			auto IsWar = 0;
			bool EnemyFrozen = 0;

			// so it only takes info from other players and not self (could probably be made smarter :p)

			if(i != Local)
			{
				pPrevCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				pCurCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;
				IsWar = m_pClient->m_aClients[i].m_IsWar && (!m_pClient->m_aClients[i].m_IsAnyTeam || !m_pClient->m_aClients[i].m_IsHelper);
				EnemyFrozen = pCharacterOther->m_IsInFreeze;
			}

			// position and vel

			const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
			const vec2 SelfPos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

			const vec2 EnemyPos = vec2(pPrevCharO->m_X, pPrevCharO->m_Y) / 32.0f;
			const vec2 EnemyVel = mix(vec2(pPrevCharO->m_VelX, pPrevCharO->m_VelY), vec2(pCurCharO->m_VelX, pCurCharO->m_VelY), IntraTick) / 1000 / 1.5;

			const float RangeX = g_Config.m_ClAutoKillRangeX / 100.0f;
			const float RangeY = g_Config.m_ClAutoKillRangeY / 100.0f;

			// debug

			if(g_Config.m_ClAutoKillDebug)
			{
				char aBuf[100];
				str_format(aBuf, sizeof(aBuf), "%d", round_to_int(SelfPos.x));
				TextRender()->Text(100, 100, 15, aBuf);

				char bBuf[100];
				str_format(bBuf, sizeof(bBuf), "%d", round_to_int(EnemyPos.x));
				TextRender()->Text(200, 100, 15, bBuf);
			}
 
			// if in freeze tile

			if((pCharacter->m_IsInFreeze && !g_Config.m_ClAutoKillWarOnly) || (g_Config.m_ClAutoKillWarOnly && IsWar && pCharacter->m_IsInFreeze))
			{

				// check where other player is

				if((EnemyPos.y < SelfPos.y && EnemyPos.y > SelfPos.y - 1 - RangeY - EnemyVel.y) && ((EnemyPos.x <= SelfPos.x + RangeX) && (EnemyPos.x + RangeX >= SelfPos.x)))
				{
					// if other player is frozen, stop, why? idk

					if(EnemyFrozen)
						return;

					// if other player is unfrozen

					if(m_pClient->m_aClients[i].m_FreezeEnd > 0)
					{

						// if player is ontop of you and their x coordinate is close to own players

						if(EnemyPos.x <= SelfPos.x + 0.04f && EnemyPos.x + +0.04f >= SelfPos.x)
						{
							// kill

							if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
							{
								m_pClient->m_Chat.SendChat(0, "/kill");
								m_SentAutoKill = true;
								m_LastFreeze = time_get() + time_freq() * 5;
								return;
							}
							else
							{
								GameClient()->SendKill(Local);
								m_SentAutoKill = true;
								return;
							}
						}
						return;
					}

					// kill

					if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
					{
						m_pClient->m_Chat.SendChat(0, "/kill");
						m_SentAutoKill = true;
						return;
					}
					else
					{
						GameClient()->SendKill(Local);
						m_SentAutoKill = true;
						return;
					}
				}
			}
		}
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

	const int Local = m_pClient->m_Snap.m_LocalClientId;

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

				if(!m_pClient->m_Teams.SameTeam(Local, i) && (Team > 0) && !JoinedTeam)
				{
					char aBuf[2048] = "/team ";
					str_append(aBuf, TeamChar);
					m_pClient->m_Chat.SendChat(0, aBuf);

					char Joined[2048] = "attempting to auto Join ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					JoinedTeam = true;
					AttempedJoinTeam = true;
				}
				if(m_pClient->m_Teams.SameTeam(Local, i) && JoinedTeam)
				{
					char Joined[2048] = "Successfully Joined The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					LocalTeam = GameClient()->m_Teams.Team(Local);

					PrevTeam = Team;

					JoinedTeam = false;
				}
				if(!m_pClient->m_Teams.SameTeam(Local, i) && AttempedJoinTeam)
				{
					char Joined[2048] = "Couldn't Join The Team of ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);
					m_pClient->m_Chat.AddLine(-3, 0, Joined);

					AttempedJoinTeam = false;
				}
				if(PrevTeam != Team && AttempedJoinTeam)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "team has changed");
					JoinedTeam = false;
				}
				if(LocalTeam > 0)
				{
					m_pClient->m_Chat.AddLine(-3, 0, "self team is bigger than 0");
					JoinedTeam = false;
					LocalTeam = GameClient()->m_Teams.Team(Local);
				}
				if(LocalTeam != Team)
				{
					PrevTeam = Team;
					AttempedJoinTeam = false;
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

	if(g_Config.m_ClGoresModeDisableIfWeapons)
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

	if(!g_Config.m_ClGoresMode && m_KogModeRebound == true)
	{
	
		GameClient()->m_Binds.Bind(KEY_MOUSE_1, g_Config.m_ClGoresModeSaved);
		m_KogModeRebound = false;
		return;
	}

	// if hasnt rebound yet and turning on kog mode, rebind to kog mode

	if(m_KogModeRebound == false && g_Config.m_ClGoresMode)
	{
		GameClient()->m_Binds.Bind(KEY_MOUSE_1, "+fire;+prevweapon");
		m_KogModeRebound = true;
		return;
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

	// if current server is type "Gores", turn the config on, else turn it off, and only do it once at connection = m_Connected == true

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
	/*
	char aBuf[512];

	if(g_Config.m_ClListsInfo)
	{

		int NumberWars = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool War = GameClient()->m_WarList.IsAnyWar(Client.m_aName, Client.m_aClan);

			if(!Client.m_Active)
				continue;

			if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName) && !GameClient()->m_WarList.IsTeamlist(Client.m_aName) && !GameClient()->m_WarList.IsClanTeamlist(Client.m_aClan))
				if(War)
					NumberWars++;
		}

		int NumberTeams = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			bool Team = GameClient()->m_WarList.IsAnyTeam(Client.m_aName, Client.m_aClan);

			if(!Client.m_Active)
				continue;

			if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName) && !GameClient()->m_WarList.IsWarlist(Client.m_aName) && !GameClient()->m_WarList.IsTempWarlist(Client.m_aName) && !GameClient()->m_WarList.IsClanWarlist(Client.m_aClan))
				if(Team)
					NumberTeams++;
		}

		int NumberHelpers = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			if(!Client.m_Active)
				continue;
			if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName))
				continue;

			NumberHelpers++;
		}

		int NumberMutes = 0;
		for(auto &Client : GameClient()->m_aClients)
		{
			if(!Client.m_Active)
				continue;
			if(!GameClient()->m_WarList.IsMutelist(Client.m_aName))
				continue;

			NumberMutes++;
		}
		str_format(aBuf, sizeof(aBuf), "│ %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams, NumberWars, NumberHelpers, NumberMutes);
}
	*/

	// info when joining a server of enabled components

	if(g_Config.m_ClEnabledInfo)
	{
		GameClient()->aMessage("╭──                  Aiodob Info");
		GameClient()->aMessage("│");
		/*
		if(g_Config.m_ClListsInfo)
		{
			GameClient()->m_Chat.AddLine(-3, 0, aBuf);
			GameClient()->aMessage("│");

		}*/
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

	if(Client()->m_Connected == true)
		Client()->m_Connected = false;
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
			Client()->Notify("chillerbot-ux", "current tile changed");
			Graphics()->NotifyWindow();
		}
		m_LastNotification = time_get();
	}
	m_LastTile = CurrentTile;
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
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientId].m_aName, aName);
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

	
	if((g_Config.m_ClReplyMuted && GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[4]))// || (GameClient()->m_WarList.IsWarlist(m_pClient->m_aClients[ClientId].m_aName) && g_Config.m_ClHideEnemyChat))
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
	if(g_Config.m_ClTabbedOutMsg && !GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[4])
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

void CAiodob::ConVotekick(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->Votekick(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::Votekick(const char *pName, char *pReason)
{
	
	int ClientId;
	char Id[8];

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_format(Id, sizeof(Id), "%d", ClientId);
			GameClient()->m_Voting.Callvote("kick", Id, pReason);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->TempWar(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

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

void CAiodob::TempWar(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_IsTempWar, pName);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::OnConsoleInit()
{
	Console()->Register("votekick", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConVotekick, this, "Call a votekick");
	Console()->Register("tempwar", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempWar, this, "Call vote");
}

void CAiodob::OnRender()
{
	// const int Local = m_pClient->m_Snap.m_LocalClientId;
	
	// on join connction = true, after joining its false
	if(Client()->m_Connected == true)
		OnConnect();

	ChangeTileNotifyTick();
	GoresMode();
	AutoJoinTeam();
	AutoKill();
	FreezeKill();

	// "secret" effect, makes a circle go around the player

	if(GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Jump || (GameClient()->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] || GameClient()->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy]))
	{
		m_LastMovement = time_get() + time_freq() * 30;
	}
}