// ChillerDragon 2021 - chillerbot ux

#include <engine/config.h>
#include <engine/shared/linereader.h>
#include <engine/textrender.h>
#include <game/client/gameclient.h>

#include <base/system.h>

#include "warlist.h"
#include <engine/client.h>

void CWarList::AddSimpleClanWar(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		char *pClan = m_pClient->m_aClients[Index].m_aClan;

			if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
			{

				if(!pClan || pClan[0] == '\0')
				{
					m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
					return;
				}
				if(!Storage()->CreateFolder("chillerbot/warlist/war/war", IStorage::TYPE_SAVE))
				{
					m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create temp folder");
					return;
				}
				if(!Storage()->CreateFolder("chillerbot/warlist/war/war", IStorage::TYPE_SAVE))
				{
					m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create war/war folder");
					return;
				}
				AddClanWar("clanwar", pClan);

				RemoveClanTeamNoMsg(pClan);
			}
	}
}

void CWarList::AddSimpleClanTeam(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		char *pClan = m_pClient->m_aClients[Index].m_aClan;

		if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
		{
			if(!pClan || pClan[0] == '\0')
			{
				m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
				return;
			}
			if(!Storage()->CreateFolder("chillerbot/warlist/team/team", IStorage::TYPE_SAVE))
			{
				m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create temp folder");
				return;
			}
			if(!Storage()->CreateFolder("chillerbot/warlist/team/team", IStorage::TYPE_SAVE))
			{
				m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create team/team folder");
				return;
			}
			AddClanTeam("clanteam", pClan);

			RemoveClanWarNoMsg(pClan);

			ReloadList();
		}
	}
}

void CWarList::RemoveSimpleClanTeam(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		char *pClan = m_pClient->m_aClients[Index].m_aClan;

		if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
		{
			char aBuf[512];
			if(!RemoveClanTeamNameFromVector("chillerbot/warlist/team/team", pClan))
			{
				str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the team list", pClan);
				m_pClient->m_Chat.AddLine(-3, 0, aBuf);
				return;
			}
			if(!WriteClanTeamNames("chillerbot/warlist/team/team"))
			{
				m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write team names");
			}
			str_format(aBuf, sizeof(aBuf), "Removed '%s' from the team list", pClan);
			m_pClient->m_Chat.AddLine(-3, 0, aBuf);
			ReloadList();
		}
	}
}

void CWarList::RemoveSimpleClanWar(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		char *pClan = m_pClient->m_aClients[Index].m_aClan;

		if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
		{
			char aBuf[512];
			if(!RemoveClanWarNameFromVector("chillerbot/warlist/war/war", pClan))
			{
				str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the war list", pClan);
				m_pClient->m_Chat.AddLine(-3, 0, aBuf);
				return;
			}
			if(!WriteClanWarNames("chillerbot/warlist/war/war"))
			{
				m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write war names");
			}
			str_format(aBuf, sizeof(aBuf), "Removed '%s' from the war list", pClan);
			m_pClient->m_Chat.AddLine(-3, 0, aBuf);

			RemoveClanTeamNameFromVector("chillerbot/warlist/team/team", pClan);
	
			ReloadList();
		}
	}
}

void CWarList::AddSimpleMute(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/mutes", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create mutes folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/mutes/mutes", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create mutes/mutes folder");
		return;
	}
	AddMute("mute", pName);
}

void CWarList::AddSimpleHelper(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/helper", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create helper folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/helper/helper", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create helper/helper folder");
		return;
	}

	AddHelper("helper", pName);
	RemoveTeamNoMsg(pName);
	RemoveWarNoMsg(pName);
	RemoveTempWarNoMsg(pName);
}

void CWarList::AddSimpleWar(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/war", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create war folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/war/war", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create war/war folder");
		return;
	}

	AddWar("war", pName);
	RemoveTeamNoMsg(pName);
	RemoveHelperNoMsg(pName);
	RemoveTempWarNoMsg(pName);
}

void CWarList::AddSimpleTeam(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/team", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create team folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/warlist/team/team", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create team/team folder");
		return;
	}

	AddTeam("team", pName);
	RemoveWarNoMsg(pName);
	RemoveTempWarNoMsg(pName);
	RemoveHelperNoMsg(pName);
}

void CWarList::AddSimpleTempWar(const char *pName)
{
	if(!pName || pName[0] == '\0')
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: missing argument <name>");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/templist/temp", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create temp folder");
		return;
	}
	if(!Storage()->CreateFolder("chillerbot/templist/temp/tempwar", IStorage::TYPE_SAVE))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to create temp/tempwar folder");
		return;
	}
	AddTempWar("tempwar", pName);
	RemoveTeamNoMsg(pName);
	RemoveHelperNoMsg(pName);
}

void CWarList::RemoveSimpleTempWar(const char *pName)
{
	char aBuf[512];
	if(!RemoveTempWarNameFromVector("chillerbot/templist/temp/tempwar", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the temp war list", pName);
		m_pClient->m_Chat.AddLine(-3, 0, aBuf);
		return;
	}
	if(!WriteTempWarNames("chillerbot/templist/temp/tempwar"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write temp war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the temp war list", pName);
	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	ReloadList();
}

void CWarList::RemoveSimpleHelper(const char *pName)
{
	char aBuf[512];
	if(!RemoveHelperNameFromVector("chillerbot/warlist/helper/helper", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the helper list", pName);
		m_pClient->m_Chat.AddLine(-3, 0, aBuf);
		return;
	}
	if(!WriteHelperNames("chillerbot/warlist/helper/helper"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write helper names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the helper list", pName);
	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	ReloadList();
}

void CWarList::RemoveSimpleWar(const char *pName)
{
	char aBuf[512];
	if(!RemoveWarNameFromVector("chillerbot/warlist/war/war", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the war list", pName);
		m_pClient->m_Chat.AddLine(-3, 0, aBuf);
		RemoveTempWarNoMsg(pName);
		return;
	}
	if(!WriteWarNames("chillerbot/warlist/war/war"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the war list", pName);
	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	ReloadList();
}

void CWarList::RemoveSimpleMute(const char *pName)
{
	char aBuf[512];
	if(!RemoveMuteNameFromVector("chillerbot/warlist/mutes/mutes", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the mute list", pName);
		m_pClient->m_Chat.AddLine(-3, 0, aBuf);
		return;
	}
	if(!WriteMuteNames("chillerbot/warlist/mutes/mutes"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write war names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the mutes list", pName);
	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	ReloadList();
}

void CWarList::OnlineInfo()
{
	char aBuf[512];

	char active[512];

	int NumberWars = 0;
	int NumberWarsAfk = 0;
	for(auto &Client : GameClient()->m_aClients)
	{
		bool War = GameClient()->m_WarList.IsAnyWar(Client.m_aName, Client.m_aClan);

		if(!Client.m_Active)
			continue;

		if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName) && !GameClient()->m_WarList.IsTeamlist(Client.m_aName) && !GameClient()->m_WarList.IsClanTeamlist(Client.m_aClan))
			if(War)
				NumberWars++;


		if(Client.m_Afk && War)
			NumberWarsAfk++;
	}

	int NumberTeams = 0;
	int NumberTeamsAfk = 0;
	for(auto &Client : GameClient()->m_aClients)
	{
		bool Team = GameClient()->m_WarList.IsAnyTeam(Client.m_aName, Client.m_aClan);

		if(!Client.m_Active)
			continue;

		if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName) && !GameClient()->m_WarList.IsWarlist(Client.m_aName) && !GameClient()->m_WarList.IsTempWarlist(Client.m_aName) && !GameClient()->m_WarList.IsClanWarlist(Client.m_aClan))
			if(Team)
				NumberTeams++;

		if(Client.m_Afk && Team)
			NumberTeamsAfk++;
	}

	int NumberHelpers = 0;
	int NumberHelpersAfk = 0;
	for(auto &Client : GameClient()->m_aClients)
	{
		if(!Client.m_Active)
			continue;
		if(!GameClient()->m_WarList.IsHelperlist(Client.m_aName))
			continue;

		NumberHelpers++;

		if(Client.m_Afk)
			NumberHelpersAfk++;
	}

	int NumberMutes = 0;
	int NumberMutesAfk = 0;
	for(auto &Client : GameClient()->m_aClients)
	{
		if(!Client.m_Active)
			continue;
		if(!GameClient()->m_WarList.IsMutelist(Client.m_aName))
			continue;

		NumberMutes++;

		if(Client.m_Afk)
			NumberMutes++;
	}
	str_format(aBuf, sizeof(aBuf), "[online] %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams, NumberWars, NumberHelpers, NumberMutes);

	str_format(active, sizeof(active), "[active] %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams - NumberTeamsAfk, NumberWars - NumberWarsAfk, NumberHelpers - NumberHelpersAfk, NumberMutes - NumberMutesAfk);

	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	m_pClient->m_Chat.AddLine(-3, 0, active);
}

void CWarList::DelClone()
{
	if(!g_Config.m_ClDummy)
	{
		g_Config.m_ClCopyingSkin = 0;
		g_Config.m_ClCloningPerson = 0;

		str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClSavedPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
		str_copy(g_Config.m_PlayerName, g_Config.m_ClSavedName, sizeof(g_Config.m_PlayerName));
		str_copy(g_Config.m_PlayerClan, g_Config.m_ClSavedClan, sizeof(g_Config.m_PlayerClan));

		g_Config.m_PlayerCountry = g_Config.m_ClSavedCountry;
		g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClSavedPlayerUseCustomColor;
		g_Config.m_ClPlayerColorBody = g_Config.m_ClSavedPlayerColorBody;
		g_Config.m_ClPlayerColorFeet = g_Config.m_ClSavedPlayerColorFeet;

		m_pClient->m_Chat.AddLine(-3, 0, "Deleted Main Clone");
	}
	else if(g_Config.m_ClDummy)
	{
		g_Config.m_ClCopyingSkinDummy = 0;
		g_Config.m_ClCloningPersonDummy = 0;

		str_copy(g_Config.m_ClDummySkin, g_Config.m_ClSavedDummySkin, sizeof(g_Config.m_ClDummySkin));
		str_copy(g_Config.m_ClDummyName, g_Config.m_ClSavedDummyName, sizeof(g_Config.m_ClDummyName));
		str_copy(g_Config.m_ClDummyClan, g_Config.m_ClSavedDummyClan, sizeof(g_Config.m_ClDummyClan));

		g_Config.m_ClDummyCountry = g_Config.m_ClSavedDummyCountry;
		g_Config.m_ClDummyUseCustomColor = g_Config.m_ClSavedDummyUseCustomColor;
		g_Config.m_ClDummyColorBody = g_Config.m_ClSavedDummyColorBody;
		g_Config.m_ClDummyColorFeet = g_Config.m_ClSavedDummyColorFeet;

		m_pClient->m_Chat.AddLine(-3, 0, "Deleted Dummy Clone");
	}
}

void CWarList::VoteKick(const char *pName, const char *pReason)
{
	/*
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp(m_pClient->m_aClients[i].m_aName, pName) == 0)
		{
			GameClient()->Friends()->AddFriend(pName, m_pClient->m_aClients[i].m_aClan);
			char aBuf[128];

			if(GameClient()->Friends()->IsFriend(pName, "", true))
				str_format(aBuf, sizeof(aBuf), "added \"%s\" to the Friendlist", pName);
			else
				str_format(aBuf, sizeof(aBuf), "Couldn't add \"%s\" to the Friendlist", pName);

			m_pClient->m_Chat.AddLine(-3, 0, aBuf);
			return;
		}
	}*/

	m_pClient->m_Chat.AddLine(-3, 0, pName);
	m_pClient->m_Chat.AddLine(-3, 0, pReason);
}

void CWarList::AddFriend(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp(m_pClient->m_aClients[i].m_aName, pName) == 0)
		{
			GameClient()->Friends()->AddFriend(pName, m_pClient->m_aClients[i].m_aClan);
			char aBuf[128];

			if(GameClient()->Friends()->IsFriend(pName, "", true))
				str_format(aBuf, sizeof(aBuf), "added \"%s\" to the Friendlist", pName);
			else
				str_format(aBuf, sizeof(aBuf), "Couldn't add \"%s\" to the Friendlist", pName);

			m_pClient->m_Chat.AddLine(-3, 0, aBuf);
			return;
		}
	}
}

void CWarList::RemoveFriend(const char *pName)
{
	int Clients = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(str_comp(m_pClient->m_aClients[i].m_aName, pName) == 0)
		{
			char aBuf[128];
	
			if(GameClient()->Friends()->IsFriend(pName, "", true))
			{
				str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Friendlist", pName);
				GameClient()->Friends()->RemoveFriend(pName, m_pClient->m_aClients[i].m_aClan);
			}
			else
				str_format(aBuf, sizeof(aBuf), "\"%s\" wasn't on the Friendlist", pName);

			m_pClient->m_Chat.AddLine(-3, 0, aBuf);
			return;
		}
	}
}

void CWarList::Clone(const char *pName)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		if(!g_Config.m_ClDummy)
		{
			if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
			{
				if(g_Config.m_ClCloningPerson == 0)
				{
					// save skins then load new ones

					if(!g_Config.m_ClCopyingSkin)
					{
						str_copy(g_Config.m_ClSavedPlayerSkin, g_Config.m_ClPlayerSkin, sizeof(g_Config.m_ClSavedPlayerSkin));
						str_copy(g_Config.m_ClSavedName, g_Config.m_PlayerName, sizeof(g_Config.m_ClSavedName));
						str_copy(g_Config.m_ClSavedClan, g_Config.m_PlayerClan, sizeof(g_Config.m_ClSavedClan));

						g_Config.m_ClSavedCountry = g_Config.m_PlayerCountry;
						g_Config.m_ClSavedPlayerUseCustomColor = g_Config.m_ClPlayerUseCustomColor;
						g_Config.m_ClSavedPlayerColorBody = g_Config.m_ClPlayerColorBody;
						g_Config.m_ClSavedPlayerColorFeet = g_Config.m_ClPlayerColorFeet;
					}
					else
					{
						str_copy(g_Config.m_ClSavedName, g_Config.m_PlayerName, sizeof(g_Config.m_ClSavedName));
						str_copy(g_Config.m_ClSavedClan, g_Config.m_PlayerClan, sizeof(g_Config.m_ClSavedClan));

						g_Config.m_ClSavedCountry = g_Config.m_PlayerCountry;
					}
					g_Config.m_ClCloningPerson = 1;
					g_Config.m_ClCopyingSkin = 1;
				}

				char aBuf[2048] = "Main Player Cloning: ";
				str_append(aBuf, m_pClient->m_aClients[Index].m_aName);
				m_pClient->m_Chat.AddLine(-3, 0, aBuf);

				char Dot[2048] = ".";

				str_append(Dot, m_pClient->m_aClients[Index].m_aName);


				str_copy(g_Config.m_ClPlayerSkin, m_pClient->m_aClients[Index].m_aSkinName, sizeof(g_Config.m_ClPlayerSkin));
				str_copy(g_Config.m_PlayerName, Dot, sizeof(g_Config.m_PlayerName));
				str_copy(g_Config.m_PlayerClan, m_pClient->m_aClients[Index].m_aClan, sizeof(g_Config.m_PlayerClan));

				g_Config.m_PlayerCountry = m_pClient->m_aClients[Index].m_Country;

				g_Config.m_ClPlayerUseCustomColor = m_pClient->m_aClients[Index].m_UseCustomColor;
				g_Config.m_ClPlayerColorBody = m_pClient->m_aClients[Index].m_ColorBody;
				g_Config.m_ClPlayerColorFeet = m_pClient->m_aClients[Index].m_ColorFeet;



			}
		}
		else
		{
			if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
			{
				if(g_Config.m_ClCloningPersonDummy == 0)
				{
					// save skins then load new ones
					if(!g_Config.m_ClCopyingSkin)
					{
						str_copy(g_Config.m_ClSavedDummySkin, g_Config.m_ClDummySkin, sizeof(g_Config.m_ClSavedDummySkin));
						str_copy(g_Config.m_ClSavedDummyName, g_Config.m_ClDummyName, sizeof(g_Config.m_ClSavedDummyName));
						str_copy(g_Config.m_ClSavedDummyClan, g_Config.m_ClDummyClan, sizeof(g_Config.m_ClSavedDummyClan));

						g_Config.m_ClSavedDummyCountry = g_Config.m_ClDummyCountry;
						g_Config.m_ClSavedDummyUseCustomColor = g_Config.m_ClDummyUseCustomColor;
						g_Config.m_ClSavedDummyColorBody = g_Config.m_ClDummyColorBody;
						g_Config.m_ClSavedDummyColorFeet = g_Config.m_ClDummyColorFeet;
					}
					else
					{
						str_copy(g_Config.m_ClSavedDummyName, g_Config.m_ClDummyName, sizeof(g_Config.m_ClSavedDummyName));
						str_copy(g_Config.m_ClSavedDummyClan, g_Config.m_ClDummyClan, sizeof(g_Config.m_ClSavedDummyClan));

						g_Config.m_ClSavedDummyCountry = g_Config.m_ClDummyCountry;
					}
					g_Config.m_ClCloningPersonDummy = 1;
					g_Config.m_ClCopyingSkinDummy = 1;
				}

				char aBuf[2048] = "Dummy Player Cloning: ";
					str_append(aBuf, m_pClient->m_aClients[Index].m_aName);
						m_pClient->m_Chat.AddLine(-3, 0, aBuf);

				char Dot[2048] = ".";

				str_append(Dot, m_pClient->m_aClients[Index].m_aName);

				str_copy(g_Config.m_ClDummySkin, m_pClient->m_aClients[Index].m_aSkinName, sizeof(g_Config.m_ClDummySkin));
				str_copy(g_Config.m_ClDummyName, Dot, sizeof(g_Config.m_ClDummyName));
				str_copy(g_Config.m_ClDummyClan, m_pClient->m_aClients[Index].m_aClan, sizeof(g_Config.m_ClDummyClan));

				g_Config.m_ClDummyCountry = m_pClient->m_aClients[Index].m_Country;



				g_Config.m_ClDummyUseCustomColor = m_pClient->m_aClients[Index].m_UseCustomColor;

				g_Config.m_ClDummyColorBody = m_pClient->m_aClients[Index].m_ColorBody;
				g_Config.m_ClDummyColorFeet = m_pClient->m_aClients[Index].m_ColorFeet;
			}
		}
	}
}

void CWarList::Skin(const char *pName)
{
	int Clients = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		if(!g_Config.m_ClDummy)
		{
			if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
			{
				if(g_Config.m_ClCopyingSkin == 0)
				{
					// save skins then load new ones

					str_copy(g_Config.m_ClSavedPlayerSkin, g_Config.m_ClPlayerSkin, sizeof(g_Config.m_ClSavedPlayerSkin));
					g_Config.m_ClSavedPlayerUseCustomColor = g_Config.m_ClPlayerUseCustomColor;
					g_Config.m_ClSavedPlayerColorBody = g_Config.m_ClPlayerColorBody;
					g_Config.m_ClSavedPlayerColorFeet = g_Config.m_ClPlayerColorFeet;

					g_Config.m_ClCopyingSkin = 1;
				}

				char aBuf[2048] = "Copying Skin of ";
				str_append(aBuf, m_pClient->m_aClients[Index].m_aName);
				m_pClient->m_Chat.AddLine(-3, 0, aBuf);

				str_copy(g_Config.m_ClPlayerSkin, m_pClient->m_aClients[Index].m_aSkinName, sizeof(g_Config.m_ClPlayerSkin));
				g_Config.m_ClPlayerUseCustomColor = m_pClient->m_aClients[Index].m_UseCustomColor;
				g_Config.m_ClPlayerColorBody = m_pClient->m_aClients[Index].m_ColorBody;
				g_Config.m_ClPlayerColorFeet = m_pClient->m_aClients[Index].m_ColorFeet;
			}
		}
		else
		{
			if(str_comp(m_pClient->m_aClients[Index].m_aName, pName) == 0)
			{
				if(g_Config.m_ClCopyingSkinDummy == 0)
				{
					// save skins then load new ones

					str_copy(g_Config.m_ClSavedDummySkin, g_Config.m_ClDummySkin, sizeof(g_Config.m_ClSavedDummySkin));
					g_Config.m_ClSavedDummyUseCustomColor = g_Config.m_ClDummyUseCustomColor;
					g_Config.m_ClSavedDummyColorBody = g_Config.m_ClDummyColorBody;
					g_Config.m_ClSavedDummyColorFeet = g_Config.m_ClDummyColorFeet;

					g_Config.m_ClCopyingSkinDummy = 1;
				}

				char aBuf[2048] = "Copying Skin of ";
				str_append(aBuf, m_pClient->m_aClients[Index].m_aName);
				m_pClient->m_Chat.AddLine(-3, 0, aBuf);

				str_copy(g_Config.m_ClDummySkin, m_pClient->m_aClients[Index].m_aSkinName, sizeof(g_Config.m_ClDummySkin));
				g_Config.m_ClDummyUseCustomColor = m_pClient->m_aClients[Index].m_UseCustomColor;
				g_Config.m_ClDummyColorBody = m_pClient->m_aClients[Index].m_ColorBody;
				g_Config.m_ClDummyColorFeet = m_pClient->m_aClients[Index].m_ColorFeet;
			}
		}
		Clients++;
		if(Clients >= 63)
			m_pClient->m_Chat.AddLine(-3, 0, "aBuf");
	}
}

void CWarList::RemoveSimpleTeam(const char *pName)
{
	char aBuf[512];
	if(!RemoveTeamNameFromVector("chillerbot/warlist/team/team", pName))
	{
		str_format(aBuf, sizeof(aBuf), "Name '%s' not found in the team list", pName);
		m_pClient->m_Chat.AddLine(-3, 0, aBuf);
		return;
	}
	if(!WriteTeamNames("chillerbot/warlist/team/team"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Error: failed to write team names");
	}
	str_format(aBuf, sizeof(aBuf), "Removed '%s' from the team list", pName);
	m_pClient->m_Chat.AddLine(-3, 0, aBuf);
	ReloadList();
}

// Send no message if removing a name from the list

void CWarList::RemoveClanTeamNoMsg(const char *pClan)
{
	if(!RemoveClanTeamNameFromVector("chillerbot/warlist/team/team", pClan))
	{
		return;
	}
	if(!WriteClanTeamNames("chillerbot/warlist/team/team"))
	{
	}
	ReloadList();
}

void CWarList::RemoveClanWarNoMsg(const char *pClan)
{
	if(!RemoveClanWarNameFromVector("chillerbot/warlist/war/war", pClan))
	{
		return;
	}
	if(!WriteClanWarNames("chillerbot/warlist/war/war"))
	{
	}
	ReloadList();
}

void CWarList::RemoveTeamNoMsg(const char *pName)
{
	if(!RemoveTeamNameFromVector("chillerbot/warlist/team/team", pName))
	{
		return;
	}
	if(!WriteTeamNames("chillerbot/warlist/team/team"))
	{
	}
	ReloadList();
}

void CWarList::RemoveTempWarNoMsg(const char *pName)
{
	if(!RemoveTempWarNameFromVector("chillerbot/templist/temp/tempwar", pName))
	{
	}
	if(!WriteTempWarNames("chillerbot/templist/temp/tempwar"))
	{
	}
	ReloadList();
}

void CWarList::RemoveWarNoMsg(const char *pName)
{
	if(!RemoveWarNameFromVector("chillerbot/warlist/war/war", pName))
	{
		return;
	}
	if(!WriteWarNames("chillerbot/warlist/war/war"))
	{
	}
	ReloadList();
}

void CWarList::RemoveHelperNoMsg(const char *pName)
{
	if(!RemoveHelperNameFromVector("chillerbot/warlist/helper/helper", pName))
	{
		return;
	}
	if(!WriteHelperNames("chillerbot/warlist/helper/helper"))
	{
	}
	ReloadList();
}

void CWarList::JoinTeamName(const char *pName)
{
	if (GameClient()->CurrentRaceTime())
	{
		m_pClient->m_Chat.AddLine(-3, 0, "Can't Join Team When Race is Started");
		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Number = -1;

			if(str_comp(m_pClient->m_aClients[i].m_aName, pName) == 0)
			{		

				if(m_pClient->m_Teams.Team(i))
				{
					int Team = GameClient()->m_Teams.Team(i);
					char TeamChar[256];
					str_format(TeamChar, sizeof(TeamChar), "%d", Team);

					char aBuf[2048] = "/team ";
					str_append(aBuf, TeamChar);
					m_pClient->m_Chat.SendChat(0, aBuf);

					Number++;
					return;
				}
				else
					return m_pClient->m_Chat.AddLine(-3, 0, "Player isn't in a Team");
			}
	}
}

bool CWarList::OnChatCmdSimple(char Prefix, int ClientId, int Team, const char *pCmd, int NumArgs, const char **ppArgs, const char *pRawArgLine)
{
	char aBuf[512];

	if(!str_comp(pCmd, "help"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "=== Aiodob Warlist ===");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)war <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)team <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)helper <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)mute <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)clanwar <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)clanteam <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(del)tempwar <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!extra");
	}
	else if(!str_comp(pCmd, "extra"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "=== Aiodob Extra ===");
		m_pClient->m_Chat.AddLine(-3, 0, "!info");
		m_pClient->m_Chat.AddLine(-3, 0, "!delclone");
		m_pClient->m_Chat.AddLine(-3, 0, "!skin <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!clone <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!join <name>");
		m_pClient->m_Chat.AddLine(-3, 0, "!kick <name> <reason>");
		m_pClient->m_Chat.AddLine(-3, 0, "!(un)friend <name>");
	}
	/*
	else if(!str_comp(pCmd, "kick") || (!str_comp(pCmd, "votekick")))
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!str_find_nocase(pRawArgLine, m_pClient->m_aClients[i].m_aName))
			{

				const char *aName = m_pClient->m_aClients[i].m_aName;
				const char *WholeArgs = pRawArgLine;
				char Reason[16];

				{
					using namespace std;

					int NameLength = str_length(aName) + 1; // + 1 so it removes the space
					string Rsn(WholeArgs);
					for(int i = 0; i < NameLength; i++)
						Rsn.erase(Rsn.begin());

					strcpy(Reason, Rsn.c_str());
				}
					char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d", str_length(aName));

				VoteKick(aName, Reason);
			}
		}
	}*/
	else if(!str_comp(pCmd, "addfriend") || (!str_comp(pCmd, "friend")))
	{
		AddFriend(pRawArgLine);
	}
	else if(!str_comp(pCmd, "unfriend") || (!str_comp(pCmd, "delfriend")))
	{
		RemoveFriend(pRawArgLine);
	}
	else if(!str_comp(pCmd, "join"))
	{
		JoinTeamName(pRawArgLine);
	}
	else if(!str_comp(pCmd, "discord"))
	{
		m_pClient->m_Chat.AddLine(-3, 0, "https://discord.gg/hAWVx9YJxA");
	}
	else if(!str_comp(pCmd, "github"))
	{
		open_link("https://github.com/qxdFox/Aiodob-Client-DDNet");
	}
	else if(!str_comp(pCmd, "kill"))
	{
		m_pClient->m_Chat.SendChat(0, "/kill");
	}
	else if(!str_comp(pCmd, "clanwar") || !str_comp(pCmd, "addclanwar") || !str_comp(pCmd, g_Config.m_ClAddClanWarString)) // "team <name>"
	{
		AddSimpleClanWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delclanwar") || !str_comp(pCmd, "unclanwar") || !str_comp(pCmd, g_Config.m_ClDelClanWarString)) // "team <name>"
	{
		RemoveSimpleClanWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "clanteam") || !str_comp(pCmd, "addclanteam") || !str_comp(pCmd, g_Config.m_ClAddClanTeamString)) // "team <name>"
	{
		AddSimpleClanTeam(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delclanteam") || !str_comp(pCmd, "unclanteam") || !str_comp(pCmd, g_Config.m_ClDelClanTeamString)) // "team <name>"
	{
		RemoveSimpleClanTeam(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "tempwar") || !str_comp(pCmd, "addtempwar") || (!str_comp(pCmd, "temp") || !str_comp(pCmd, g_Config.m_ClAddTempWarString))) // "team <name>"
	{
		AddSimpleTempWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "helper") || (!str_comp(pCmd, "hepler")|| (!str_comp(pCmd, "ehlper") || !str_comp(pCmd, "addhelper") || !str_comp(pCmd, g_Config.m_ClAddHelperString)))) // "team <name>"
	{
		AddSimpleHelper(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "mute") || (!str_comp(pCmd, "mtue") || !str_comp(pCmd, "addmute") || !str_comp(pCmd, g_Config.m_ClAddMuteString))) // "team <name>"
	{
		AddSimpleMute(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "war") || (!str_comp(pCmd, "awr") || (!str_comp(pCmd, "wra") || !str_comp(pCmd, "addwar") || !str_comp(pCmd, g_Config.m_ClAddWarString)))) // "war <name>"
	{
		AddSimpleWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "team")|| (!str_comp(pCmd, "tema")|| (!str_comp(pCmd, "tmea") || !str_comp(pCmd, "addteam") || !str_comp(pCmd, g_Config.m_ClAddTeamString)))) // "team <name>"
	{
		AddSimpleTeam(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delhelper") || !str_comp(pCmd, "unhelper") || !str_comp(pCmd, g_Config.m_ClRemoveHelperString))
	{
		RemoveSimpleHelper(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delmute") || !str_comp(pCmd, "unmute") || !str_comp(pCmd, g_Config.m_ClRemoveMuteString))
	{
		RemoveSimpleMute(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "deltempwar") || !str_comp(pCmd, "deltemp") || !str_comp(pCmd, "untempwar") || !str_comp(pCmd, "untemp") || !str_comp(pCmd, g_Config.m_ClRemoveTempWarString))
	{
		RemoveSimpleTempWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delwar")|| (!str_comp(pCmd, "unwra") || !str_comp(pCmd, "unwar") || !str_comp(pCmd, "peace") || !str_comp(pCmd, g_Config.m_ClRemoveWarString)))
	{
		RemoveSimpleWar(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "delteam")|| (!str_comp(pCmd, "untema") || !str_comp(pCmd, "unteam") || !str_comp(pCmd, "unfriend") || !str_comp(pCmd, g_Config.m_ClRemoveTeamString)))
	{
		RemoveSimpleTeam(pRawArgLine);
		return true;
	}
	else if(!str_comp(pCmd, "info"))
	{
		OnlineInfo();
		return true;
	}
	else if(!str_comp(pCmd, "delclone") || !str_comp(pCmd, "unclone") || !str_comp(pCmd, "unskin") || !str_comp(pCmd, "delskin"))
	{
		DelClone();
		if(!g_Config.m_ClDummy)
			m_pClient->SendInfo(false);
		else if(g_Config.m_ClDummy)
			m_pClient->SendDummyInfo(false);
		return true;
	}
	else if(!str_comp(pCmd, "skin"))
	{
		Skin(pRawArgLine);
		if(!g_Config.m_ClDummy)
			m_pClient->SendInfo(false);
		else if(g_Config.m_ClDummy)
			m_pClient->SendDummyInfo(false);
		return true;
	}
	else if(!str_comp(pCmd, "clone") || !str_comp(pCmd, "copy"))
	{
		Clone(pRawArgLine);
		if(!g_Config.m_ClDummy)
			m_pClient->SendInfo(false);
		else if(g_Config.m_ClDummy)
			m_pClient->SendDummyInfo(false);
		return true;
	}
	else if(!str_comp(pCmd, "search")) // "search <name can contain spaces>"
	{
		if(NumArgs != 1)
		{
			str_format(aBuf, sizeof(aBuf), "Error: expected 1 argument but got %d", NumArgs);
			m_pClient->m_Chat.AddLine(-3, 0, aBuf);
			return true;
		}
		m_pClient->m_Chat.AddLine(-3, 0, "[search] fullmatch:");
		if(!SearchName(ppArgs[0], false))
		{
			m_pClient->m_Chat.AddLine(-3, 0, "[search] partial:");
			SearchName(ppArgs[0], true);
		}
	}
	else
	{
		return false;
	}
	return true;
}
