#include "warlist.h"

#include <base/log.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/entity/entity.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

void CWarList::OnNewSnapshot()
{
	UpdateWarPlayers();
}

void CWarList::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::TCLIENTWARLIST);

	Console()->Register("update_war_group", "i[group_index] s[name] i[color]", CFGFLAG_CLIENT, ConUpsertWarType, this, "Update or add a specific war group");
	Console()->Register("add_war_entry", "s[group] s[name] s[clan] r[reason]", CFGFLAG_CLIENT, ConAddWarEntry, this, "Adds a specific war entry");
	Console()->Register("add_mute", "s[name]", CFGFLAG_CLIENT, ConAddMuteEntry, this, "Remove a clan war entry"); // EClient [Mutes]

	Console()->Register("war_name", "s[group] s[name] ?r[reason]", CFGFLAG_CLIENT, ConName, this, "Add a name war entry");
	Console()->Register("war_clan", "s[group] s[clan] ?r[reason]", CFGFLAG_CLIENT, ConClan, this, "Add a clan war entry");
	Console()->Register("remove_war_name", "s[group] s[name]", CFGFLAG_CLIENT, ConRemoveName, this, "Remove a name war entry");
	Console()->Register("remove_war_clan", "s[group] s[clan]", CFGFLAG_CLIENT, ConRemoveClan, this, "Remove a clan war entry");

	// In-game commands
	Console()->Register("war_name_index", "i[temp] i[group_index] s[name] ?r[reason]", CFGFLAG_CLIENT, ConNameIndex, this, "Remove a clan war entry");
	Console()->Register("war_clan_index", "i[temp] s[group_index] s[name] ?r[reason]", CFGFLAG_CLIENT, ConClanIndex, this, "Remove a clan war entry");
	Console()->Register("remove_war_name_index", "i[group_index] s[name]", CFGFLAG_CLIENT, ConRemoveNameIndex, this, "Remove a clan war entry");
	Console()->Register("remove_war_clan_index", "s[group_index] s[name]", CFGFLAG_CLIENT, ConRemoveClanIndex, this, "Remove a clan war entry");

	// EClient [Mutes]
	Console()->Register("addmute", "i[temp] s[name]", CFGFLAG_CLIENT, ConAddMute, this, "Remove a clan war entry");
	Console()->Register("delmute", "s[name]", CFGFLAG_CLIENT, ConDelMute, this, "Removes a Muted Name");

	Console()->Register("remove_entry_name", "s[name]", CFGFLAG_CLIENT, ConRemoveNameEntry, this, "Remove a clan war entry");
	Console()->Register("remove_entry_clan", "s[clan]", CFGFLAG_CLIENT, ConRemoveClanEntry, this, "Removes a Muted Name");
}

void CWarList::RebuildWarMaps()
{
	m_NameWarMap.clear();
	m_ClanWarMap.clear();
	m_MuteMap.clear();

	for(CWarType *pType : m_WarTypes)
		pType->m_NumEntries = 0;

	for(CWarEntry &Entry : m_vWarEntries)
	{
		if(Entry.m_aName[0])
			m_NameWarMap[Entry.m_aName] = &Entry;
		if(Entry.m_aClan[0])
			m_ClanWarMap[Entry.m_aClan] = &Entry;

		if(Entry.m_pWarType)
			Entry.m_pWarType->m_NumEntries++;
	}
	for(CMuteEntry &Entry : m_MuteEntries)
	{
		if(Entry.m_aMutedName[0])
			m_MuteMap[Entry.m_aMutedName] = &Entry;
	}
}

// In-game war Commands
void CWarList::ConNameIndex(IConsole::IResult *pResult, void *pUserData)
{
	int Temp = pResult->GetInteger(0);
	int Index = pResult->GetInteger(1);
	const char *pName = pResult->GetString(2);
	const char *pReason = pResult->GetString(3);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->AddWarEntryInGame(Index, pName, pReason, false, Temp);
}
void CWarList::ConClanIndex(IConsole::IResult *pResult, void *pUserData)
{
	int Temp = pResult->GetInteger(0);
	int Index = pResult->GetInteger(1);
	const char *pName = pResult->GetString(2);
	const char *pReason = pResult->GetString(3);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->AddWarEntryInGame(Index, pName, pReason, true, Temp);
}
void CWarList::ConRemoveNameIndex(IConsole::IResult *pResult, void *pUserData)
{
	int Index = pResult->GetInteger(0);
	const char *pName = pResult->GetString(1);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->RemoveWarEntryInGame(Index, pName, false);
}
void CWarList::ConRemoveClanIndex(IConsole::IResult *pResult, void *pUserData)
{
	int Index = pResult->GetInteger(0);
	const char *pName = pResult->GetString(1);
	CWarList *pThis = static_cast<CWarList *>(pUserData);
	pThis->RemoveWarEntryInGame(Index, pName, true);
}

void CWarList::ConRemoveNameWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveClanWar(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConNameTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConClanTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveNameTeam(IConsole::IResult *pResult, void *pUserData) {}
void CWarList::ConRemoveClanTeam(IConsole::IResult *pResult, void *pUserData) {}

// Generic Commands
void CWarList::ConName(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	const char *pReason = pResult->GetString(2);
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->AddWarEntry(pName, "", pReason, pType);
}
void CWarList::ConClan(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pClan = pResult->GetString(1);
	const char *pReason = pResult->GetString(2);
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->AddWarEntry("", pClan, pReason, pType);
}
void CWarList::ConRemoveName(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->RemoveWarEntry(pName, "", pType);
}
void CWarList::ConRemoveClan(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pClan = pResult->GetString(1);
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->RemoveWarEntry("", pClan, pType);
}

// Backend Commands for config file
void CWarList::ConAddWarEntry(IConsole::IResult *pResult, void *pUserData)
{
	const char *pType = pResult->GetString(0);
	const char *pName = pResult->GetString(1);
	const char *pClan = pResult->GetString(2);
	const char *pReason = pResult->GetString(3);
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->AddWarEntry(pName, pClan, pReason, pType);
}
void CWarList::ConUpsertWarType(IConsole::IResult *pResult, void *pUserData)
{
	int Index = pResult->GetInteger(0);
	const char *pType = pResult->GetString(1);
	unsigned int ColorInt = pResult->GetInteger(2);
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ColorInt));
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->UpsertWarType(Index, pType, Color);
}

// EClient [Mutes]
void CWarList::ConAddMuteEntry(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	const char *pName = pResult->GetString(0);
	pSelf->AddMuteEntry(pName);
}
void CWarList::ConAddMute(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	int Temp = pResult->GetInteger(0);
	const char *pName = pResult->GetString(1);
	pSelf->AddMute(pName, false, Temp);
}
void CWarList::ConDelMute(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	const char *pName = pResult->GetString(0);
	pSelf->RemoveMute(pName);
}
void CWarList::ConRemoveNameEntry(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	const char *pName = pResult->GetString(0);
	if(pSelf->RemoveWarEntryDuplicates(pName, ""))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Removed all entries for name \"%s\"", pName);
		pSelf->GameClient()->ClientMessage(aBuf);
	}
}
void CWarList::ConRemoveClanEntry(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	const char *pClan = pResult->GetString(0);
	if(pSelf->RemoveWarEntryDuplicates("", pClan))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Removed all entries for clan \"%s\"", pClan);
		pSelf->GameClient()->ClientMessage(aBuf);
	}
}

void CWarList::AddWarEntryInGame(int WarType, const char *pName, const char *pReason, bool IsClan, bool Temp)
{
	if(str_comp(pName, "") == 0)
		return;
	if(WarType >= (int)m_WarTypes.size())
		return;

	CWarType *pWarType = m_WarTypes[WarType];
	CWarEntry Entry(pWarType);
	str_copy(Entry.m_aReason, pReason);
	Entry.m_TempEntry = Temp; // EClient
	char aBuf[128];

	if(IsClan)
	{
		for(const auto &Client : GameClient()->m_aClients)
		{
			if(!Client.m_Active)
				continue;
			// Found user
			if(str_comp(Client.m_aName, pName) == 0)
			{
				if(str_comp(Client.m_aClan, "") != 0)
				{
					str_format(aBuf, sizeof(aBuf), "added \"%s's\" clan to '%s' list", pName, pWarType->m_aWarName);
					str_copy(Entry.m_aClan, Client.m_aClan);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "No clan found for user \"%s\"", pName);
					break;
				}
			}
		}
	}
	else
	{
		str_copy(Entry.m_aName, pName);
		str_format(aBuf, sizeof(aBuf), "added \"%s\" to %s'%s' list ", pName, Temp ? "Temp " : "", pWarType->m_aWarName);
	}
	if(!g_Config.m_ClWarListAllowDuplicates && !Entry.m_TempEntry) // Don't remove duplicates for temp entries
		RemoveWarEntryDuplicates(Entry.m_aName, Entry.m_aClan);

	GameClient()->ClientMessage(aBuf);

	AddWarEntry(Entry.m_aName, Entry.m_aClan, Entry.m_aReason, Entry.m_pWarType->m_aWarName, Entry.m_TempEntry);
}

void CWarList::RemoveWarEntryInGame(int WarType, const char *pName, bool IsClan)
{
	if(!str_comp(pName, ""))
		return;

	if(WarType >= (int)m_WarTypes.size())
		return;

	CWarType *pWarType = m_WarTypes[WarType];
	CWarEntry Entry(pWarType);
	char aBuf[128];

	if(IsClan)
	{
		for(const auto &Client : GameClient()->m_aClients)
		{
			if(!Client.m_Active)
				continue;
			// Found user
			if(str_comp(Client.m_aName, pName) == 0)
			{
				if(str_comp(Client.m_aClan, "") != 0)
				{
					str_format(aBuf, sizeof(aBuf), "removed \"%s's\" clan from the %s list", pName, pWarType->m_aWarName);
					str_copy(Entry.m_aClan, Client.m_aClan);
					break;
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "No clan found for user \"%s\"", pName);
					break;
				}
			}
		}
	}
	else
	{
		str_copy(Entry.m_aName, pName);
		str_format(aBuf, sizeof(aBuf), "removed \"%s\" from the %s list", pName, pWarType->m_aWarName);
	}
	GameClient()->ClientMessage(aBuf);
	RemoveWarEntry(Entry.m_aName, Entry.m_aClan, Entry.m_pWarType->m_aWarName);
}

void CWarList::AddMuteEntry(const char *pName, bool Temp)
{
	if(!str_comp(pName, ""))
		return;

	CMuteEntry Entry(pName);
	str_copy(Entry.m_aMutedName, pName);
	Entry.m_TempEntry = Temp;

	m_MuteEntries.push_back(Entry);

	RebuildWarMaps(); // EClient
}

void CWarList::AddMute(const char *pName, bool Quiet, bool Temp)
{
	if(!str_comp(pName, ""))
		return;

	CMuteEntry Entry(pName);
	str_copy(Entry.m_aMutedName, pName);
	Entry.m_TempEntry = Temp;

	if(!Quiet)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the %s List", pName, Temp ? "Temp Mute" : "Mute");
		GameClient()->ClientMessage(aBuf);
	}
	RemoveMute(pName, true);

	m_MuteEntries.push_back(Entry);

	RebuildWarMaps(); // EClient
}

void CWarList::RemoveMute(const char *pName, bool Silent)
{
	if(str_comp(pName, "") == 0)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "couldn't find \"%s\" in the Mute List", pName);
	CMuteEntry Entry(pName);

	auto It = std::find(m_MuteEntries.begin(), m_MuteEntries.end(), Entry);
	if(It != m_MuteEntries.end())
	{
		for(CMuteEntry &Entries : m_MuteEntries)
		{
			for(auto DupeIt = m_MuteEntries.begin(); DupeIt != m_MuteEntries.end();)
			{
				bool IsDuplicate = !str_comp(DupeIt->m_aMutedName, pName);

				if(IsDuplicate)
					DupeIt = m_MuteEntries.erase(DupeIt);
				else
					++DupeIt;

				if(!str_comp(Entries.m_aMutedName, pName))
					str_format(aBuf, sizeof(aBuf), "Removed \"%s\" from the Mute List", pName);
			}
		}
	}

	if(!Silent)
		GameClient()->ClientMessage(aBuf);

	RebuildWarMaps(); // EClient
}

void CWarList::UpdateWarEntry(int Index, const char *pName, const char *pClan, const char *pReason, CWarType *pType, bool Temp)
{
	if(Index >= 0 && Index < static_cast<int>(m_vWarEntries.size()))
	{
		str_copy(m_vWarEntries[Index].m_aName, pName);
		str_copy(m_vWarEntries[Index].m_aClan, pClan);
		str_copy(m_vWarEntries[Index].m_aReason, pReason);
		m_vWarEntries[Index].m_pWarType = pType;
		m_vWarEntries[Index].m_TempEntry = Temp; // EClient
	}
}

void CWarList::UpsertWarType(int Index, const char *pType, ColorRGBA Color)
{
	if(str_comp(pType, "none") == 0)
		return;

	if(Index >= 0 && Index < static_cast<int>(m_WarTypes.size()))
	{
		str_copy(m_WarTypes[Index]->m_aWarName, pType);
		m_WarTypes[Index]->m_Color = Color;
	}
	else
	{
		AddWarType(pType, Color);
	}
}

void CWarList::AddWarEntry(const char *pName, const char *pClan, const char *pReason, const char *pType, bool Temp)
{
	if(str_comp(pName, "") == 0 && str_comp(pClan, "") == 0)
		return;

	CWarType *WarType = FindWarType(pType);
	if(WarType == m_pWarTypeNone)
	{
		AddWarType(pType, ColorRGBA(0, 0, 0, 1));
		WarType = FindWarType(pType);
	}

	CWarEntry Entry(WarType);
	str_copy(Entry.m_aReason, pReason);
	Entry.m_TempEntry = Temp; // EClient

	if(str_comp(pClan, "") != 0)
		str_copy(Entry.m_aClan, pClan);
	else if(str_comp(pName, "") != 0)
		str_copy(Entry.m_aName, pName);

	if(!g_Config.m_ClWarListAllowDuplicates && !Temp)
		RemoveWarEntryDuplicates(pName, pClan);
	m_vWarEntries.push_back(Entry);

	RebuildWarMaps(); // EClient
}

bool CWarList::RemoveWarEntryDuplicates(const char *pName, const char *pClan)
{
	if(str_comp(pName, "") == 0 && str_comp(pClan, "") == 0)
		return false;

	bool Found = false;

	for(auto It = m_vWarEntries.begin(); It != m_vWarEntries.end();)
	{
		bool IsDuplicate =
			(str_comp(It->m_aName, pName) == 0) &&
			(str_comp(It->m_aClan, pClan) == 0);
		if(IsDuplicate)
		{
			It = m_vWarEntries.erase(It);
			Found = true;
		}
		else
			++It;
	}

	RebuildWarMaps(); // EClient
	return Found;
}

void CWarList::AddWarType(const char *pType, ColorRGBA Color)
{
	if(str_comp(pType, "none") == 0)
		return;

	CWarType *Type = FindWarType(pType);
	if(Type == m_pWarTypeNone)
	{
		CWarType *NewType = new CWarType(pType, Color);
		m_WarTypes.push_back(NewType);
	}
	else
	{
		Type->m_Color = Color;
	}
}

void CWarList::RemoveWarEntry(const char *pName, const char *pClan, const char *pType)
{
	CWarType *WarType = FindWarType(pType);
	CWarEntry Entry(WarType, pName, pClan, "");
	auto It = std::find(m_vWarEntries.begin(), m_vWarEntries.end(), Entry);
	if(It != m_vWarEntries.end())
	{
		m_vWarEntries.erase(It);
		RebuildWarMaps(); // EClient
	}
}

void CWarList::RemoveWarEntry(CWarEntry *Entry)
{
	auto It = std::find_if(m_vWarEntries.begin(), m_vWarEntries.end(),
		[Entry](const CWarEntry &WarEntry) { return &WarEntry == Entry; });
	if(It != m_vWarEntries.end())
	{
		m_vWarEntries.erase(It);
		RebuildWarMaps(); // EClient
	}
}

void CWarList::RemoveWarType(const char *pType)
{
	CWarType Type(pType);

	auto It = std::find_if(m_WarTypes.begin(), m_WarTypes.end(),
		[&Type](CWarType *WarTypePtr) { return *WarTypePtr == Type; });
	if(It != m_WarTypes.end())
	{
		// Don't remove default war types
		if(!(*It)->m_Removable)
			return;

		// Find all war entries and set them to None if they are using this type
		for(CWarEntry &Entry : m_vWarEntries)
		{
			if(*Entry.m_pWarType == **It)
			{
				Entry.m_pWarType = m_pWarTypeNone;
			}
		}
		m_WarTypes.erase(It);
		RebuildWarMaps(); // EClient
	}
}

CWarEntry *CWarList::FindWarEntryWithName(const char *pName)
{
	for(CWarEntry &Entry : m_vWarEntries)
	{
		if(str_comp(pName, Entry.m_aName) == 0 && str_comp(Entry.m_aName, "") != 0)
		{
			return &Entry;
		}
	}
	return nullptr;
}

CWarEntry *CWarList::FindWarEntryWithClan(const char *pClan)
{
	for(CWarEntry &Entry : m_vWarEntries)
	{
		if(str_comp(pClan, Entry.m_aClan) == 0 && str_comp(Entry.m_aClan, "") != 0)
		{
			return &Entry;
		}
	}
	return nullptr;
}

char *CWarList::GetWarTypeName(int ClientId)
{
	for(CWarEntry &Entry : m_vWarEntries)
	{
		if(!str_comp(GameClient()->m_aClients[ClientId].m_aName, Entry.m_aName) && str_comp(Entry.m_aName, "") != 0)
		{
			return Entry.m_pWarType->m_aWarName;
		}
		else if(!str_comp(GameClient()->m_aClients[ClientId].m_aClan, Entry.m_aClan) && str_comp(Entry.m_aClan, "") != 0)
		{
			return Entry.m_pWarType->m_aWarName;
		}
	}
	return nullptr;
}

CWarType *CWarList::FindWarType(const char *pType)
{
	CWarType Type(pType);
	auto It = std::find_if(m_WarTypes.begin(), m_WarTypes.end(),
		[&Type](CWarType *WarTypePtr) { return *WarTypePtr == Type; });
	if(It != m_WarTypes.end())
		return *It;
	else
		return m_pWarTypeNone;
}

CWarEntry *CWarList::FindWarEntry(const char *pName, const char *pClan, const char *pType)
{
	CWarType *WarType = FindWarType(pType);
	CWarEntry Entry(WarType, pName, pClan, "");
	auto It = std::find(m_vWarEntries.begin(), m_vWarEntries.end(), Entry);

	if(It != m_vWarEntries.end())
		return &(*It);
	else
		return nullptr;
}

ColorRGBA CWarList::GetPriorityColor(int ClientId) const
{
	if(m_WarPlayers[ClientId].m_IsWarClan && !m_WarPlayers[ClientId].m_IsWarName)
		return m_WarPlayers[ClientId].m_ClanColor;
	else
		return m_WarPlayers[ClientId].m_NameColor;
}

ColorRGBA CWarList::GetNameplateColor(int ClientId) const
{
	return m_WarPlayers[ClientId].m_NameColor;
}

ColorRGBA CWarList::GetClanColor(int ClientId) const
{
	return m_WarPlayers[ClientId].m_ClanColor;
}

bool CWarList::GetAnyWar(int ClientId) const
{
	if(ClientId < 0)
		return false;
	return m_WarPlayers[ClientId].m_IsWarClan || m_WarPlayers[ClientId].m_IsWarName;
}

bool CWarList::GetNameWar(int ClientId) const
{
	if(ClientId < 0)
		return false;
	return m_WarPlayers[ClientId].m_IsWarName;
}
bool CWarList::GetClanWar(int ClientId) const
{
	if(ClientId < 0)
		return false;
	return m_WarPlayers[ClientId].m_IsWarClan;
}

void CWarList::GetReason(char *pReason, int ClientId) const
{
	str_copy(pReason, m_WarPlayers[ClientId].m_aReason, sizeof(m_WarPlayers[ClientId].m_aReason));
}

CWarDataCache &CWarList::GetWarData(int ClientId)
{
	return m_WarPlayers[ClientId];
}

void CWarList::SortWarEntries()
{
	// TODO
}

void CWarList::UpdateWarPlayers()
{
	for(int i = 0; i < (int)m_WarTypes.size(); ++i)
		m_WarTypes[i]->m_Index = i;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		auto &Client = GameClient()->m_aClients[i];
		auto &Cache = m_WarPlayers[i];

		Cache.m_IsMuted = false;
		Cache.m_IsWarName = false;
		Cache.m_IsWarClan = false;
		memset(Cache.m_aReason, 0, sizeof(Cache.m_aReason));
		Cache.m_NameColor = ColorRGBA(1, 1, 1, 1);
		Cache.m_ClanColor = ColorRGBA(1, 1, 1, 1);
		Cache.m_WarGroupMatches.clear();
		Cache.m_WarGroupMatches.resize((int)m_WarTypes.size(), false);
		Cache.m_WarTypeIndex = -1;

		// Name war
		auto ItName = m_NameWarMap.find(Client.m_aName);
		if(ItName != m_NameWarMap.end())
		{
			CWarEntry *Entry = ItName->second;
			str_copy(Cache.m_aReason, Entry->m_aReason);
			Cache.m_IsWarName = true;
			Cache.m_NameColor = Entry->m_pWarType->m_Color;
			Cache.m_WarGroupMatches[Entry->m_pWarType->m_Index] = true;
			Cache.m_WarTypeIndex = Entry->m_pWarType->m_Index;
		}

		// Clan war (only if not already a name war)
		auto ItClan = m_ClanWarMap.find(Client.m_aClan);
		if(ItClan != m_ClanWarMap.end())
		{
			CWarEntry *Entry = ItClan->second;
			if(!Cache.m_IsWarName)
				str_copy(Cache.m_aReason, Entry->m_aReason);
			Cache.m_IsWarClan = true;
			Cache.m_ClanColor = Entry->m_pWarType->m_Color;
			Cache.m_WarGroupMatches[Entry->m_pWarType->m_Index] = true;
			Cache.m_WarTypeIndex = Entry->m_pWarType->m_Index;
		}

		// Mute
		auto ItMute = m_MuteMap.find(Client.m_aName);
		if(ItMute != m_MuteMap.end())
			Cache.m_IsMuted = true;
	}
}

CWarList::~CWarList()
{
	for(CWarType *WarType : m_WarTypes)
		delete WarType;
	m_WarTypes.clear();
}

CWarList::CWarList()
{
	str_copy(m_WarTypes[0]->m_aWarName, "none");
	m_WarTypes[0]->m_Color = ColorRGBA(1, 1, 1, 1);
}

static void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

void CWarList::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CWarList *pThis = (CWarList *)pUserData;

	char aBuf[1024];
	for(int i = 0; i < static_cast<int>(pThis->m_WarTypes.size()); i++)
	{
		CWarType &WarType = *pThis->m_WarTypes[i];

		// Imported wartypes don't get saved
		if(WarType.m_Imported)
			continue;

		char aEscapeType[MAX_WARLIST_TYPE_LENGTH * 2];
		EscapeParam(aEscapeType, WarType.m_aWarName, sizeof(aEscapeType));
		ColorHSLA Color = color_cast<ColorHSLA>(WarType.m_Color);

		str_format(aBuf, sizeof(aBuf), "update_war_group %d \"%s\" %d", i, aEscapeType, Color.Pack(false));
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTWARLIST);
	}
	for(CWarEntry &Entry : pThis->m_vWarEntries)
	{
		// Imported entries don't get saved
		if(Entry.m_Imported)
			continue;
		if(Entry.m_TempEntry)
			continue;

		char aEscapeType[MAX_WARLIST_TYPE_LENGTH * 2];
		char aEscapeName[MAX_NAME_LENGTH * 2];
		char aEscapeClan[MAX_CLAN_LENGTH * 2];
		char aEscapeReason[MAX_WARLIST_REASON_LENGTH * 2];
		EscapeParam(aEscapeType, Entry.m_pWarType->m_aWarName, sizeof(aEscapeType));
		EscapeParam(aEscapeName, Entry.m_aName, sizeof(aEscapeName));
		EscapeParam(aEscapeClan, Entry.m_aClan, sizeof(aEscapeClan));
		EscapeParam(aEscapeReason, Entry.m_aReason, sizeof(aEscapeReason));

		str_format(aBuf, sizeof(aBuf), "add_war_entry \"%s\" \"%s\" \"%s\" \"%s\"", aEscapeType, aEscapeName, aEscapeClan, aEscapeReason);
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTWARLIST);
	}
	for(CMuteEntry &Entry : pThis->m_MuteEntries)
	{
		if(Entry.m_TempEntry)
			continue;

		char aEscapeName[MAX_NAME_LENGTH * 2];
		EscapeParam(aEscapeName, Entry.m_aMutedName, sizeof(aEscapeName));

		str_format(aBuf, sizeof(aBuf), "add_mute \"%s\"", aEscapeName);
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTWARLIST);
	}
}
