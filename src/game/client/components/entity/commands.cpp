
#include "entity.h"

#include <base/log.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/client/components/tclient/warlist.h>
#include <game/client/gameclient.h>

#include <cstdlib>
#include <vector>

void CEClient::ConVotekick(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->Votekick(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CEClient::ConServerRainbowSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	if(pResult->NumArguments() > 0)
	{
		pSelf->m_RainbowSpeed = pResult->GetInteger(0);
	}
	else
		log_info("E-Client", "%d", pSelf->m_RainbowSpeed);
}

void CEClient::ConServerRainbowSaturation(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

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
		log_info("E-Client", "%d", pSelf->m_RainbowSat[g_Config.m_ClDummy]);
}

void CEClient::ConServerRainbowLightness(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

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
		log_info("E-Client", "%d", pSelf->m_RainbowLht[g_Config.m_ClDummy]);
}

void CEClient::ConServerRainbowBody(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

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
		log_info("E-Client", "%d", pSelf->m_RainbowBody[g_Config.m_ClDummy]);
}

void CEClient::ConServerRainbowFeet(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

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
		log_info("E-Client", "%d", pSelf->m_RainbowFeet[g_Config.m_ClDummy]);
}

void CEClient::ConServerRainbowBothPlayers(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;

	if(pResult->NumArguments() > 0)
	{
		if(pResult->GetInteger(0) == 0)
			pSelf->m_BothPlayers = 0;
		else if(pResult->GetInteger(0) > 0)
			pSelf->m_BothPlayers = 1;
	}
	else
		log_info("E-Client", "%d", pSelf->m_BothPlayers);
}

void CEClient::Votekick(const char *pName, const char *pReason)
{
	const int ClientId = GameClient()->GetClientId(pName);

	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
	{
		GameClient()->ClientMessage("No player with this name found.");
		return;
	}

	if(!str_comp(pName, GameClient()->m_aClients[ClientId].m_aName))
	{
		char Id[8];
		str_format(Id, sizeof(Id), "%d", ClientId);
		GameClient()->m_Voting.Callvote("kick", Id, pReason);
	}
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

void CEClient::OnlineInfo()
{
	int Mutes = 0;

	class COnlineInfo
	{
	public:
		char m_aLabel[16];
		int m_Amount;
		int m_ActiveAmount;
	};

	std::vector<COnlineInfo> OnlineInfos;

	bool HasAny = false;
	for(size_t WarlistType = 1; WarlistType < GameClient()->m_WarList.m_WarTypes.size(); ++WarlistType)
	{
		int Amount = 0;
		int AmountActive = 0;
		for(const CGameClient::CClientData &Client : GameClient()->m_aClients)
		{
			if(!Client.m_Active && GameClient()->m_Teams.Team(Client.ClientId()) == 0)
				continue;

			if(Client.ClientId() == GameClient()->m_Snap.m_LocalClientId)
				continue;

			CWarDataCache &WarData = GameClient()->m_WarList.GetWarData(Client.ClientId());
			bool Matches = WarData.m_WarGroupMatches[WarlistType];
			bool Muted = WarData.m_IsMuted;

			if(Muted && WarlistType == 1)
				Mutes++;
			if(Matches)
			{
				Amount++;
				if(!Client.m_Afk)
					AmountActive++;
				HasAny = true;
			}
		}
		COnlineInfo Info;

		char Label[16] = "";
		str_copy(Label, GameClient()->m_WarList.m_WarTypes[WarlistType]->m_aWarName);
		if(Amount != 1)
		{
			if(str_endswith(Label, "s"))
				str_append(Label, "'", sizeof(Label));
			else if(str_endswith(Label, "y"))
			{
				const int Len = str_length(Label);
				if(Len >= 1)
				{
					Label[Len - 1] = '\0';
					str_append(Label, "ies", sizeof(Label));
				}
			}
			else
				str_append(Label, "s", sizeof(Label));
		}

		if(Label[0] == '\0')
			continue;

		str_copy(Info.m_aLabel, Label);
		Info.m_Amount = Amount;
		Info.m_ActiveAmount = AmountActive;
		OnlineInfos.push_back(Info);
	}
	if(Mutes > 0)
		HasAny = true;

	if(!HasAny)
	{
		GameClient()->ClientMessage("╭──         Online Info");
		GameClient()->ClientMessage("│ No one from any group online.");
		GameClient()->ClientMessage("╰───────────────────────");
		return; // No one from warlist online
	}

	GameClient()->ClientMessage("╭──         Online Info");

	char aBuf[64] = "";
	if(Mutes > 0)
		str_format(aBuf, sizeof(aBuf), "│ %d Mutes", Mutes);
	GameClient()->ClientMessage(aBuf);
	HasAny = false;
	for(const COnlineInfo &Info : OnlineInfos)
	{
		const int ActiveAmount = Info.m_ActiveAmount;
		const int AfkAmount = Info.m_Amount - Info.m_ActiveAmount;

		if(ActiveAmount == 0)
			continue;
		char aTemp[32] = "";

		str_format(aBuf, sizeof(aBuf), "│ %d %s", ActiveAmount, Info.m_aLabel);

		if(AfkAmount > 0)
			str_format(aTemp, sizeof(aTemp), " (+%d afk)", AfkAmount);
		str_append(aBuf, aTemp, sizeof(aBuf));

		GameClient()->ClientMessage(aBuf);
		HasAny = true;
	}
	if(!HasAny)
		GameClient()->ClientMessage("│ No Active players online.");

	GameClient()->ClientMessage("╰───────────────────────");
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
		str_format(aBuf, sizeof(aBuf), "│ Custom Color: %s", GameClient()->m_aClients[Id].m_UseCustomColor ? "Yes" : "No");
		str_format(aBuf, sizeof(aBuf), "│ Body Color: %d", GameClient()->m_aClients[Id].m_ColorBody);
		GameClient()->ClientMessage(aBuf);
		str_format(aBuf, sizeof(aBuf), "│ Feet Color: %d", GameClient()->m_aClients[Id].m_ColorFeet);
		GameClient()->ClientMessage(aBuf);
		GameClient()->ClientMessage("│");

		str_format(aBuf, sizeof(aBuf), "│ Skin Name: %s", GameClient()->m_aClients[Id].m_aSkinName);
		GameClient()->ClientMessage(aBuf);
		GameClient()->ClientMessage("│");

		if(GameClient()->m_aClients[Id].m_AuthLevel > 0)
			str_format(aBuf, sizeof(aBuf), "│ Authed: Yes, Auth Level %d", GameClient()->m_aClients[Id].m_AuthLevel);
		else
			str_copy(aBuf, "│ Authed: No");
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

void CEClient::ConSpectateId(IConsole::IResult *pResult, void *pUserData)
{
	CEClient *pSelf = (CEClient *)pUserData;
	const char *pName = pSelf->GameClient()->GetClientName(pResult->GetInteger(0));

	char pCmd[64];
	str_format(pCmd, sizeof(pCmd), "/spec %s", pName);
	pSelf->GameClient()->m_Chat.SendChat(0, pCmd);
}

void CEClient::ConCrash(IConsole::IResult *pResult, void *pUserData)
{
	exit(666);
}

void CEClient::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();

	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITY);

	// Misc
	Console()->Register("votekick", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConVotekick, this, "Call a votekick");
	Console()->Register("onlineinfo", "", CFGFLAG_CLIENT, ConOnlineInfo, this, "Shows you how many people of default lists are on the current server");
	Console()->Register("playerinfo", "s[name]", CFGFLAG_CLIENT, ConPlayerInfo, this, "Get Info of a Player");

	// Temporary Lists
	//Console()->Register("addtempwar", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempWar, this, "temporary War");
	//Console()->Register("deltempwar", "s[name]", CFGFLAG_CLIENT, ConUnTempWar, this, "remove temporary War");

	//Console()->Register("addtemphelper", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempHelper, this, "temporary Helper");
	//Console()->Register("deltemphelper", "s[name]", CFGFLAG_CLIENT, ConUnTempHelper, this, "remove temporary Helper");

	//Console()->Register("addtempmute", "s[name]", CFGFLAG_CLIENT, ConTempMute, this, "temporary Mute");
	//Console()->Register("deltempmute", "s[name]", CFGFLAG_CLIENT, ConUnTempMute, this, "remove temporary Mute");

	// Skin Saving/Restoing
	Console()->Register("restoreskin", "", CFGFLAG_CLIENT, ConRestoreSkin, this, "Restore Your Saved Info");
	Console()->Register("saveskin", "", CFGFLAG_CLIENT, ConSaveSkin, this, "Save Your Current Info (Skin, name, etc.)");

	// View Link
	Console()->Register("view_link", "r[url]", CFGFLAG_CLIENT, ConViewLink, this, "Opens a new Browser tab with that Link");

	// Rainbow Commands
	Console()->Register("server_rainbow_speed", "?s[speed]", CFGFLAG_CLIENT, ConServerRainbowSpeed, this, "Rainbow Speed of Server side rainbow mode (default = 10)");
	Console()->Register("server_rainbow_both_players", "?i[int]", CFGFLAG_CLIENT, ConServerRainbowBothPlayers, this, "Rainbow Both Players at the same time");
	Console()->Register("server_rainbow_sat", "?i[sat] ?i[0 | 1(dummy)]", CFGFLAG_CLIENT, ConServerRainbowSaturation, this, "Rainbow Saturation of Server side rainbow mode (default = 200)");
	Console()->Register("server_rainbow_lht", "?i[lht] ?i[0 | 1(dummy)]", CFGFLAG_CLIENT, ConServerRainbowLightness, this, "Rainbow Lightness of Server side rainbow mode (default = 30)");

	Console()->Register("server_rainbow_body", "?i[int] ?i[0 | 1(dummy)]", CFGFLAG_CLIENT, ConServerRainbowBody, this, "Rainbow Body");
	Console()->Register("server_rainbow_feet", "?i[int] ?i[0 | 1(dummy)]", CFGFLAG_CLIENT, ConServerRainbowFeet, this, "Rainbow Feet");

	Console()->Register("reply_last", "?r[message]", CFGFLAG_CLIENT, ConReplyLast, this, "Reply to the last ping");
	Console()->Register("specid", "i[id]", CFGFLAG_CLIENT, ConSpectateId, this, "Spectate Id");
	Console()->Register("crash", "", CFGFLAG_CLIENT, ConCrash, this, "Crash your own client");

	Console()->Chain("ec_fast_input", ConchainFastInputs, this);
	Console()->Chain("ec_fast_input_amount", ConchainFastInputs, this);

	Console()->Chain("ec_discord_rpc", ConchainDiscordUpdate, this);
	Console()->Chain("ec_discord_map_status", ConchainDiscordUpdate, this);
	Console()->Chain("ec_discord_online_status", ConchainDiscordUpdate, this);
	Console()->Chain("ec_high_process_priority", ConchainDDNetProcessPriority, this);
	Console()->Chain("ec_discord_normal_process_priority", ConchainDiscordProcessPriority, this);
}

void CEClient::ConchainFastInputs(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CEClient *pSelf = (CEClient *)pUserData;
	if(pResult->NumArguments())
		pSelf->Client()->SendFastInputsInfo(g_Config.m_ClDummy);
}

void CEClient::ConchainDiscordUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CEClient *pSelf = (CEClient *)pUserData;
	pSelf->Client()->DiscordRPCchange();
}

void CEClient::ConchainDDNetProcessPriority(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CEClient *pSelf = (CEClient *)pUserData;
	if(pResult->NumArguments())
	{
		bool Value = pResult->GetInteger(0) != 0;
		pSelf->SetDDNetProcessPriority(Value);
	}
}

void CEClient::ConchainDiscordProcessPriority(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CEClient *pSelf = (CEClient *)pUserData;
	if(pResult->NumArguments())
	{
		if(pResult->GetInteger(0) != 0)
			pSelf->StartDiscordPriorityThread();
	}
}

void CEClient::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CEClient *pThis = (CEClient *)pUserData;

	char aBuf[128];

	str_format(aBuf, sizeof(aBuf), "server_rainbow_speed %d", pThis->m_RainbowSpeed);
	pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);
	str_format(aBuf, sizeof(aBuf), "server_rainbow_both_players %d", pThis->m_BothPlayers);
	pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);

	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		str_format(aBuf, sizeof(aBuf), "server_rainbow_sat %d %d", pThis->m_RainbowSat[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);
		str_format(aBuf, sizeof(aBuf), "server_rainbow_lht %d %d", pThis->m_RainbowLht[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);

		str_format(aBuf, sizeof(aBuf), "server_rainbow_body %d %d", pThis->m_RainbowBody[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);
		str_format(aBuf, sizeof(aBuf), "server_rainbow_feet %d %d", pThis->m_RainbowFeet[Dummy], Dummy);
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITY);
	}
}
