
#include <engine/console.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <base/system.h>

#include "entity.h"
#include <base/log.h>
#include <game/generated/protocol.h>

void CEClient::ConVotekick(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->Votekick(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CEClient::ConServerRainbowSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_RainbowSpeed);

	if(pResult->NumArguments() > 0)
	{
		pSelf->m_RainbowSpeed = pResult->GetInteger(0);
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::ConServerRainbowSaturation(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_RainbowSat[g_Config.m_ClDummy]);

	if(pResult->NumArguments() > 0)
	{
		int Dummy = g_Config.m_ClDummy;
		if(pResult->NumArguments() > 1)
		{
			if(pResult->GetInteger(1) == 0)
				Dummy = 0;
			else if(pResult->GetInteger(1) > 0)
				Dummy = 1;
		}

		pSelf->m_RainbowSat[Dummy] = pResult->GetInteger(0);
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::ConServerRainbowLightness(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_RainbowLht[g_Config.m_ClDummy]);

	if(pResult->NumArguments() > 0)
	{
		int Dummy = g_Config.m_ClDummy;
		if(pResult->NumArguments() > 1)
		{
			if(pResult->GetInteger(1) == 0)
				Dummy = 0;
			else if(pResult->GetInteger(1) > 0)
				Dummy = 1;
		}

		pSelf->m_RainbowLht[Dummy] = pResult->GetInteger(0);
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::ConServerRainbowBody(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_RainbowBody[g_Config.m_ClDummy]);

	if(pResult->NumArguments() > 0)
	{
		int Dummy = g_Config.m_ClDummy;
		if(pResult->NumArguments() > 1)
		{
			if(pResult->GetInteger(1) == 0)
				Dummy = 0;
			else if(pResult->GetInteger(1) > 0)
				Dummy = 1;
		}

		if(pResult->GetInteger(0) == 0)
			pSelf->m_RainbowBody[Dummy] = 0;
		else if(pResult->GetInteger(0) > 0)
			pSelf->m_RainbowBody[Dummy] = 1;
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::ConServerRainbowFeet(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_RainbowFeet[g_Config.m_ClDummy]);

	if(pResult->NumArguments() > 0)
	{
		int Dummy = g_Config.m_ClDummy;
		if(pResult->NumArguments() > 1)
		{
			if(pResult->GetInteger(1) == 0)
				Dummy = 0;
			else if(pResult->GetInteger(1) > 0)
				Dummy = 1;
		}

		if(pResult->GetInteger(0) == 0)
			pSelf->m_RainbowFeet[Dummy] = 0;
		else if(pResult->GetInteger(0) > 0)
			pSelf->m_RainbowFeet[Dummy] = 1;
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::ConServerRainbowBothPlayers(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	char aBuf[8];
	str_format(aBuf, sizeof(aBuf), "%d", pSelf->m_BothPlayers);

	if(pResult->NumArguments() > 0)
	{
		if(pResult->GetInteger(0) == 0)
			pSelf->m_BothPlayers = 0;
		else if(pResult->GetInteger(0) > 0)
			pSelf->m_BothPlayers = 1;
	}
	else
		log_info("E-Client", aBuf);
}

void CEClient::Votekick(const char *pName, const char *pReason)
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
		GameClient()->ClientMessage("No player with this name found.");
		return;
	}
}

// Temp War Commands
void CEClient::ConTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->TempWar(pResult->GetString(0), pResult->GetString(1));
}
void CEClient::ConUnTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->UnTempWar(pResult->GetString(0));
}

// Temp Helper Commands
void CEClient::ConTempHelper(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->TempHelper(pResult->GetString(0), pResult->GetString(1));
}
void CEClient::ConUnTempHelper(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->UnTempHelper(pResult->GetString(0));
}

// Mute Commands
void CEClient::ConTempMute(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->TempMute(pResult->GetString(0));
}
void CEClient::ConUnTempMute(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->UnTempMute(pResult->GetString(0));
}

// Saving and Restoring Skins
void CEClient::ConSaveSkin(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->SaveSkin();
}
void CEClient::ConRestoreSkin(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->RestoreSkin();
}

void CEClient::ConOnlineInfo(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->OnlineInfo();
}
void CEClient::ConPlayerInfo(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->PlayerInfo(pResult->GetString(0));
}
void CEClient::ConViewLink(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->Client()->ViewLink(pResult->GetString(0));
}

void CEClient::TempWar(const char *pName, const char *pReason, bool Silent)
{
	UnTempWar(pName, true); // Remove previous Reason

	CTempEntry Entry(0, pName, pReason);
	str_copy(Entry.m_aTempWar, pName);
	str_copy(Entry.m_aReason, pReason);

	m_TempEntries.push_back(Entry);
	UnTempHelper(pName, true);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp War List", pName, pReason);
	if(!Silent)
		GameClient()->ClientMessage(aBuf);

	UpdateTempPlayers();
}
bool CEClient::UnTempWar(const char *pName, bool Silent)
{
	bool Removed = false;

	if(!str_comp(pName, ""))
		return Removed;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "couldn't find \"%s\" on the Temp War List", pName);
	CTempEntry Entry(0, pName, "");

	auto it = std::find(m_TempEntries.begin(), m_TempEntries.end(), Entry);
	if(it != m_TempEntries.end())
	{
		for(auto it2 = m_TempEntries.begin(); it2 != m_TempEntries.end();)
		{
			bool IsDuplicate = !str_comp(it2->m_aTempWar, pName);

			if(IsDuplicate)
				it2 = m_TempEntries.erase(it2);
			else
				++it2;

			if(!str_comp(it2->m_aTempWar, pName))
			{
				str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Temp War List", pName);
				Removed = true;
			}
		}
	}
	if(!Silent)
		GameClient()->ClientMessage(aBuf);
	return Removed;
}

void CEClient::TempHelper(const char *pName, const char *pReason, bool Silent)
{
	UnTempHelper(pName, true); // Remove previous Reason

	CTempEntry Entry(1, pName, pReason);
	str_copy(Entry.m_aTempHelper, pName);
	str_copy(Entry.m_aReason, pReason);

	m_TempEntries.push_back(Entry);
	UnTempWar(pName, true);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp Helper List", pName);
	if(!Silent)
		GameClient()->ClientMessage(aBuf);

	UpdateTempPlayers();
}
bool CEClient::UnTempHelper(const char *pName, bool Silent)
{
	bool Removed = false;
	if(!str_comp(pName, ""))
		return Removed;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "couldn't find \"%s\" on the Temp Helper List", pName);
	CTempEntry Entry(1, pName, "");

	auto it = std::find(m_TempEntries.begin(), m_TempEntries.end(), Entry);
	if(it != m_TempEntries.end())
	{
		for(auto it2 = m_TempEntries.begin(); it2 != m_TempEntries.end();)
		{
			bool IsDuplicate = !str_comp(it2->m_aTempHelper, pName);

			if(IsDuplicate)
				it2 = m_TempEntries.erase(it2);
			else
				++it2;

			if(!str_comp(it2->m_aTempHelper, pName))
			{
				str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Temp Helper List", pName);
				Removed = true;
			}
		}
	}
	if(!Silent)
		GameClient()->ClientMessage(aBuf);
	return Removed;
}

void CEClient::TempMute(const char *pName, bool Silent)
{
	CTempEntry Entry(2, pName, "");
	str_copy(Entry.m_aTempMute, pName);

	m_TempEntries.push_back(Entry);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp Mute List", pName);
	if(!Silent)
		GameClient()->ClientMessage(aBuf);

	UpdateTempPlayers();
}
bool CEClient::UnTempMute(const char *pName, bool Silent)
{
	bool Removed = false;
	if(!str_comp(pName, ""))
		return Removed;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "couldn't find \"%s\" on the Temp Mute List", pName);
	CTempEntry Entry(2, pName, "");

	auto it = std::find(m_TempEntries.begin(), m_TempEntries.end(), Entry);
	if(it != m_TempEntries.end())
	{
		for(auto it2 = m_TempEntries.begin(); it2 != m_TempEntries.end();)
		{
			bool IsDuplicate = !str_comp(it2->m_aTempMute, pName);

			if(IsDuplicate)
				it2 = m_TempEntries.erase(it2);
			else
				++it2;

			if(!str_comp(it2->m_aTempMute, pName))
			{
				str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Temp Mute List", pName);
				Removed = true;
			}
		}
	}
	if(!Silent)
		GameClient()->ClientMessage(aBuf);
	return Removed;
}

void CEClient::RestoreSkin()
{
	if(g_Config.m_ClDummy)
	{
		str_copy(g_Config.m_ClDummySkin, g_Config.m_ClSavedDummySkin, sizeof(g_Config.m_ClDummySkin));
		str_copy(g_Config.m_ClDummyName, g_Config.m_ClSavedDummyName, sizeof(g_Config.m_ClDummyName));
		str_copy(g_Config.m_ClDummyClan, g_Config.m_ClSavedDummyClan, sizeof(g_Config.m_ClDummyClan));
		g_Config.m_ClDummyCountry = g_Config.m_ClSavedDummyCountry;
		g_Config.m_ClDummyUseCustomColor = g_Config.m_ClSavedDummyUseCustomColor;
		g_Config.m_ClDummyColorBody = g_Config.m_ClSavedDummyColorBody;
		g_Config.m_ClDummyColorFeet = g_Config.m_ClSavedDummyColorFeet;
		GameClient()->ClientMessage("Restored Dummy Skin");
		GameClient()->SendDummyInfo(false);
	}
	else
	{
		str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClSavedPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
		str_copy(g_Config.m_PlayerName, g_Config.m_ClSavedName, sizeof(g_Config.m_PlayerName));
		str_copy(g_Config.m_PlayerClan, g_Config.m_ClSavedClan, sizeof(g_Config.m_PlayerClan));
		g_Config.m_PlayerCountry = g_Config.m_ClSavedCountry;
		g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClSavedPlayerUseCustomColor;
		g_Config.m_ClPlayerColorBody = g_Config.m_ClSavedPlayerColorBody;
		g_Config.m_ClPlayerColorFeet = g_Config.m_ClSavedPlayerColorFeet;
		GameClient()->ClientMessage("Restored Main Skin");
		GameClient()->SendInfo(false);
	}
}
void CEClient::SaveSkin()
{
	if(!g_Config.m_ClServerRainbow)
	{
		if(g_Config.m_ClDummy)
		{
			str_copy(g_Config.m_ClSavedDummySkin, g_Config.m_ClDummySkin, sizeof(g_Config.m_ClSavedDummySkin));
			str_copy(g_Config.m_ClSavedDummyName, g_Config.m_ClDummyName, sizeof(g_Config.m_ClSavedDummyName));
			str_copy(g_Config.m_ClSavedDummyClan, g_Config.m_ClDummyClan, sizeof(g_Config.m_ClSavedDummyClan));
			g_Config.m_ClSavedDummyCountry = g_Config.m_ClDummyCountry;
			g_Config.m_ClSavedDummyUseCustomColor = g_Config.m_ClDummyUseCustomColor;
			g_Config.m_ClSavedDummyColorBody = g_Config.m_ClDummyColorBody;
			g_Config.m_ClSavedDummyColorFeet = g_Config.m_ClDummyColorFeet;
			GameClient()->ClientMessage("Saved Dummy Skin");
			GameClient()->SendDummyInfo(false);
		}
		else
		{
			str_copy(g_Config.m_ClSavedPlayerSkin, g_Config.m_ClPlayerSkin, sizeof(g_Config.m_ClSavedPlayerSkin));
			str_copy(g_Config.m_ClSavedName, g_Config.m_PlayerName, sizeof(g_Config.m_ClSavedName));
			str_copy(g_Config.m_ClSavedClan, g_Config.m_PlayerClan, sizeof(g_Config.m_ClSavedClan));
			g_Config.m_ClSavedCountry = g_Config.m_PlayerCountry;
			g_Config.m_ClSavedPlayerUseCustomColor = g_Config.m_ClPlayerUseCustomColor;
			g_Config.m_ClSavedPlayerColorBody = g_Config.m_ClPlayerColorBody;
			g_Config.m_ClSavedPlayerColorFeet = g_Config.m_ClPlayerColorFeet;
			GameClient()->ClientMessage("Saved Main Skin");
			GameClient()->SendInfo(false);
		}
	}
	else
		GameClient()->ClientMessage("Can't Save! Rainbow mode is enabled.");
}

void CEClient::OnlineInfo(bool Integrate)
{
	char aBuf[512];
	char active[512];

	int NumberWars = 0;
	int NumberHelpers = 0;
	int NumberTeams = 0;
	int NumberMutes = 0;

	int NumberHelpersAfk = 0;
	int NumberTeamsAfk = 0;
	int NumberMutesAfk = 0;
	int NumberWarsAfk = 0;

	for(auto &Client : GameClient()->m_aClients)
	{
		bool War = GameClient()->m_WarList.GetWarData(Client.ClientId()).m_WarGroupMatches[1];
		bool TempWar = m_TempPlayers[Client.ClientId()].IsTempWar;

		bool Team = GameClient()->m_WarList.GetWarData(Client.ClientId()).m_WarGroupMatches[2];

		bool Helper = GameClient()->m_WarList.GetWarData(Client.ClientId()).m_WarGroupMatches[3];
		bool TempHelper = m_TempPlayers[Client.ClientId()].IsTempHelper;

		bool Mute = GameClient()->m_WarList.m_WarPlayers[Client.ClientId()].IsMuted;
		bool TempMute = m_TempPlayers[Client.ClientId()].IsTempMute;

		if(!Client.m_Active && GameClient()->m_Teams.Team(Client.ClientId()) == 0)
			continue;

		if(Client.ClientId() == GameClient()->m_Snap.m_LocalClientId)
			continue;

		if(War || TempWar)
		{
			NumberWars++;
			if(Client.m_Afk)
				NumberWarsAfk++;
		}
		else if(Team)
		{
			NumberTeams++;
			if(Client.m_Afk)
				NumberTeamsAfk++;
		}
		else if(Helper || TempHelper)
		{
			NumberHelpers++;
			if(Client.m_Afk)
				NumberHelpersAfk++;
		}
		if(Mute || TempMute)
		{
			NumberMutes++;
			if(Client.m_Afk)
				NumberMutesAfk++;
		}
	}

	str_format(aBuf, sizeof(aBuf), "│ [online] %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams, NumberWars, NumberHelpers, NumberMutes);
	str_format(active, sizeof(active), "│ [active] %d Teams | %d Wars | %d Helpers | %d Mutes", NumberTeams - NumberTeamsAfk, NumberWars - NumberWarsAfk, NumberHelpers - NumberHelpersAfk, NumberMutes - NumberMutesAfk);
	if(!Integrate)
	{
		GameClient()->ClientMessage("╭──                  Entity Info");
		GameClient()->ClientMessage("│");
	}
	GameClient()->ClientMessage(aBuf);
	GameClient()->ClientMessage(active);
	if(!Integrate)
	{
		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("╰───────────────────────");
	}
}
void CEClient::PlayerInfo(const char *pName)
{
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "Couldnt Find a Player With The Name \"%s\"", pName);

	int Id = GameClient()->GetClientId(pName);

	if(Id >= 0)
	{
		GameClient()->ClientMessage("╭──                  Player Info");
		GameClient()->ClientMessage("│");

		str_format(aBuf, sizeof(aBuf), "│ Name: %s", pName);
		GameClient()->ClientMessage(aBuf);
		str_format(aBuf, sizeof(aBuf), "│ Clan: %s", GameClient()->m_aClients[Id].m_aClan);
		GameClient()->ClientMessage(aBuf);
		GameClient()->ClientMessage("│");
		if(!GameClient()->m_aClients[Id].m_UseCustomColor)
		{
			str_format(aBuf, sizeof(aBuf), "│ Custom Color: %s", "No");
			GameClient()->ClientMessage(aBuf);
			GameClient()->ClientMessage("│");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "│ Custom Color: %s", "Yes");
			GameClient()->ClientMessage(aBuf);
			str_format(aBuf, sizeof(aBuf), "│ Body Color: %d", GameClient()->m_aClients[Id].m_ColorBody);
			GameClient()->ClientMessage(aBuf);
			str_format(aBuf, sizeof(aBuf), "│ Feet Color: %d", GameClient()->m_aClients[Id].m_ColorFeet);
			GameClient()->ClientMessage(aBuf);
			GameClient()->ClientMessage("│");
		}
		str_format(aBuf, sizeof(aBuf), "│ Skin Name: %s", GameClient()->m_aClients[Id].m_aSkinName);
		GameClient()->ClientMessage(aBuf);
		GameClient()->ClientMessage("│");

		if(GameClient()->m_aClients[Id].m_AuthLevel > 0)
			str_format(aBuf, sizeof(aBuf), "│ Authed: Yes, Auth Level %d", GameClient()->m_aClients[Id].m_AuthLevel);
		else
			str_format(aBuf, sizeof(aBuf), "│ Authed: No", GameClient()->m_aClients[Id].m_AuthLevel);
		GameClient()->ClientMessage(aBuf);

		GameClient()->ClientMessage("│");
		GameClient()->ClientMessage("╰───────────────────────");
	}
	else
		GameClient()->ClientMessage(aBuf);
}

void CEClient::ConReplyLast(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	CLastPing LastPing = pSelf->m_aLastPing;

	if(!str_comp(LastPing.m_aName, "") || LastPing.m_aName[0] == '\0')
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "E-Client", "No one pinged you yet");
		return;
	}
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!str_comp(LastPing.m_aName, pSelf->GameClient()->m_aClients[ClientId].m_aName))
		{
			break;
		}
		if(ClientId == MAX_CLIENTS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "E-Client", "Player with that Name doesn't exist");
			return;
		}
	}

	char Text[2048];
	if(LastPing.m_Team == TEAM_WHISPER_RECV)
		str_format(Text, sizeof(Text), "/w %s %s", LastPing.m_aName, pResult->GetString(0));
	else
		str_format(Text, sizeof(Text), "%s: %s", LastPing.m_aName, pResult->GetString(0));
	pSelf->GameClient()->m_Chat.SendChat(0, Text);
}

void CEClient::ConCrash(IConsole::IResult *pResult, void *pUserData)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s", 1);
}

void CEClient::ConSpectateId(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	const char *pName = pSelf->GameClient()->GetClientName(pResult->GetInteger(0));

	char pCmd[64];
	str_format(pCmd, sizeof(pCmd), "/spec %s", pName);
	pSelf->GameClient()->m_Chat.SendChat(0, pCmd);
}

void CEClient::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();

	if(pConfigManager)
		pConfigManager->RegisterECallback(ConfigSaveCallback, this);

	// Misc
	Console()->Register("votekick", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConVotekick, this, "Call a votekick");
	Console()->Register("onlineinfo", "", CFGFLAG_CLIENT, ConOnlineInfo, this, "Shows you how many people of default lists are on the current server");
	Console()->Register("playerinfo", "s[name]", CFGFLAG_CLIENT, ConPlayerInfo, this, "Get Info of a Player");

	// Temporary Lists
	Console()->Register("addtempwar", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempWar, this, "temporary War");
	Console()->Register("deltempwar", "s[name]", CFGFLAG_CLIENT, ConUnTempWar, this, "remove temporary War");

	Console()->Register("addtemphelper", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempHelper, this, "temporary Helper");
	Console()->Register("deltemphelper", "s[name]", CFGFLAG_CLIENT, ConUnTempHelper, this, "remove temporary Helper");

	Console()->Register("addtempmute", "s[name]", CFGFLAG_CLIENT, ConTempMute, this, "temporary Mute");
	Console()->Register("deltempmute", "s[name]", CFGFLAG_CLIENT, ConUnTempMute, this, "remove temporary Mute");

	// Skin Saving/Restoing
	Console()->Register("restoreskin", "", CFGFLAG_CLIENT, ConRestoreSkin, this, "Restore Your Saved Info");
	Console()->Register("saveskin", "", CFGFLAG_CLIENT, ConSaveSkin, this, "Save Your Current Info (Skin, name, etc.)");

	// View Link
	Console()->Register("view_link", "r[Url]", CFGFLAG_CLIENT, ConViewLink, this, "Opens a new Browser tab with that Link");

	// Rainbow Commands
	Console()->Register("server_rainbow_speed", "?s[speed]", CFGFLAG_CLIENT, ConServerRainbowSpeed, this, "Rainbow Speed of Server side rainbow mode (default = 10)");
	Console()->Register("server_rainbow_both_players", "?i[int]", CFGFLAG_CLIENT, ConServerRainbowBothPlayers, this, "Rainbow Both Players at the same time");
	Console()->Register("server_rainbow_sat", "?i[Sat] ?i[0 | 1(Dummy)]", CFGFLAG_CLIENT, ConServerRainbowSaturation, this, "Rainbow Saturation of Server side rainbow mode (default = 200)");
	Console()->Register("server_rainbow_lht", "?i[Lht] ?i[0 | 1(Dummy)]", CFGFLAG_CLIENT, ConServerRainbowLightness, this, "Rainbow Lightness of Server side rainbow mode (default = 30)");

	Console()->Register("server_rainbow_body", "?i[int] ?i[0 | 1(Dummy)]", CFGFLAG_CLIENT, ConServerRainbowBody, this, "Rainbow Body");
	Console()->Register("server_rainbow_feet", "?i[int] ?i[0 | 1(Dummy)]", CFGFLAG_CLIENT, ConServerRainbowFeet, this, "Rainbow Feet");

	Console()->Register("reply_last", "?r[Message]", CFGFLAG_CLIENT, ConReplyLast, this, "Reply to the last ping");
	Console()->Register("specid", "i[Id]", CFGFLAG_CLIENT, ConSpectateId, this, "Spectate Id");
	Console()->Register("crash", "", CFGFLAG_CLIENT, ConCrash, this, "Crash your own client");

	Console()->Chain("ec_gores_mode", ConchainGoresMode, this);
}

void CEClient::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CEClient *pThis = (CEClient *)pUserData;

	char aBuf[128];

	str_format(aBuf, sizeof(aBuf), "server_rainbow_speed %d", pThis->m_RainbowSpeed);
	pConfigManager->WriteLine(aBuf);
	str_format(aBuf, sizeof(aBuf), "server_rainbow_both_players %d", pThis->m_BothPlayers);
	pConfigManager->WriteLine(aBuf);

	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		str_format(aBuf, sizeof(aBuf), "server_rainbow_sat %d %d", pThis->m_RainbowSat[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf);
		str_format(aBuf, sizeof(aBuf), "server_rainbow_lht %d %d", pThis->m_RainbowLht[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf);

		str_format(aBuf, sizeof(aBuf), "server_rainbow_body %d %d", pThis->m_RainbowBody[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf);
		str_format(aBuf, sizeof(aBuf), "server_rainbow_feet %d %d", pThis->m_RainbowFeet[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf);
	}
}
