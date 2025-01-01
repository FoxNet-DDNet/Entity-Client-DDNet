
#include <engine/console.h>

#include <game/client/gameclient.h>

#include <base/system.h>

#include "aiodob.h"

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

void CAiodob::TempWar(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempWarName, pName);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp War List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConUnTempWar(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->UnTempWar(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::UnTempWar(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempWarName, "\0");
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Temp War List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
		else
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Name \"%s\" not found in the Temp War List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConTempHelper(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->TempHelper(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::TempHelper(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempHelperName, pName);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp Helper List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConUnTempHelper(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->UnTempHelper(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::UnTempHelper(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempHelperName, "\0");
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from  the Temp Helper List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConTempMute(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->TempMute(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::TempMute(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempMuteName, pName);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the Temp Mute List", pName);
			GameClient()->aMessage(aBuf);
			return;
		}
	}
	if(ClientId == MAX_CLIENTS)
	{
		GameClient()->aMessage("No player with this name found.");
		return;
	}
}

void CAiodob::ConUnTempMute(IConsole::IResult *pResult, void *pUserData)
{
	CAiodob *pSelf = (CAiodob *)pUserData;
	pSelf->UnTempMute(pResult->GetString(0), pResult->NumArguments() > 1 ? pResult->GetString(1) : "");
}

void CAiodob::UnTempMute(const char *pName, char *pReason)
{
	int ClientId;

	for(ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(str_comp(pName, GameClient()->m_aClients[ClientId].m_aName) == 0)
		{
			str_copy(GameClient()->m_aClients[ClientId].m_TempMuteName, "\0");
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from  the Temp Mute List", pName);
			GameClient()->aMessage(aBuf);
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

	Console()->Register("addtempwar", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempWar, this, "temporary War");
	Console()->Register("deltempwar", "s[name]", CFGFLAG_CLIENT, ConUnTempWar, this, "remove temporary War");

	Console()->Register("addtemphelper", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempHelper, this, "temporary Helper");
	Console()->Register("deltemphelper", "s[name]", CFGFLAG_CLIENT, ConUnTempHelper, this, "remove temporary Helper");

	Console()->Register("addtempmute", "s[name] ?r[reason]", CFGFLAG_CLIENT, ConTempMute, this, "temporary Mute");
	Console()->Register("deltempmute", "s[name]", CFGFLAG_CLIENT, ConUnTempMute, this, "remove temporary Mute");
}