#include "warlist.h"

#include <base/log.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/entity/entity.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>

void CWarList::OnInit()
{
	MigrateLegacySettings();
	if(g_Config.m_ClFrozenHudUseWarlist < 0)
	{
		g_Config.m_ClFrozenHudUseWarlist = std::any_of(m_WarTypes.begin(), m_WarTypes.end(), [](const CWarType *pType) {
			return pType->m_FrozenTee;
		});
	}
}

void CWarList::OnNewSnapshot()
{
	UpdateWarPlayers();
}

void CWarList::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ECLIENTWARLIST);

	Console()->Register("update_war_group", "i[group_index] s[name] i[color]", CFGFLAG_CLIENT, ConUpsertWarType, this, "Update or add a specific war group");
	Console()->Register("update_war_group_meta", "s[group] s[property] ?r[data]", CFGFLAG_CLIENT, ConUpdateWarGroupMeta, this, "Get or set a war group property (prefix text or a 0/1 option)");
	Console()->Register("ec_hide_enemy_chat", "i[enabled]", CFGFLAG_CLIENT, ConLegacyHideEnemyChat, this, "Legacy setting migrated to the enemy group mute option");
	Console()->Register("ec_show_muted_in_console", "i[enabled]", CFGFLAG_CLIENT, ConLegacyShowMutedInConsole, this, "Legacy setting; muted messages are always logged");
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
	Console()->Register("addmute", "i[temp] r[name]", CFGFLAG_CLIENT, ConAddMute, this, "Remove a clan war entry");
	Console()->Register("delmute", "r[name]", CFGFLAG_CLIENT, ConDelMute, this, "Removes a Muted Name");

	Console()->Register("remove_entry_name", "r[name]", CFGFLAG_CLIENT, ConRemoveNameEntry, this, "Remove a clan war entry");
	Console()->Register("remove_entry_clan", "r[clan]", CFGFLAG_CLIENT, ConRemoveClanEntry, this, "Removes a Muted Name");
}

void CWarList::RebuildWarMaps()
{
	m_NameWarMap.clear();
	m_ClanWarMap.clear();
	m_MuteMap.clear();
	m_NameWarTypeMatches.clear();
	m_ClanWarTypeMatches.clear();

	for(CWarType *pType : m_WarTypes)
		pType->m_NumEntries = 0;

	for(CWarEntry &Entry : m_vWarEntries)
	{
		if(Entry.m_pWarType && Entry.m_aName[0])
			m_NameWarTypeMatches[Entry.m_aName].push_back(Entry.m_pWarType);
		if(Entry.m_pWarType && Entry.m_aClan[0])
			m_ClanWarTypeMatches[Entry.m_aClan].push_back(Entry.m_pWarType);
		if(Entry.m_pWarType == m_pWarTypeMute && Entry.m_aName[0])
			m_MuteMap[Entry.m_aName] = &Entry;
		else if(Entry.m_aName[0])
			m_NameWarMap[Entry.m_aName] = &Entry;
		if(Entry.m_pWarType != m_pWarTypeMute && Entry.m_aClan[0])
			m_ClanWarMap[Entry.m_aClan] = &Entry;

		if(Entry.m_pWarType)
			Entry.m_pWarType->m_NumEntries++;
	}
}

void CWarList::InitializeGroupSettings(CWarType *pType, int Index)
{
	if(pType->m_SettingsMigrated)
		return;
	str_copy(pType->m_aPrefix, g_Config.m_ClWarlistPrefix);
	str_clean_whitespaces(pType->m_aPrefix);
	pType->m_ChatPrefix = g_Config.m_ClWarlistPrefixes;
	if(Index == 1)
		pType->m_Mute = m_LegacyHideEnemyChat;
	pType->m_ServerInfoPrefix = g_Config.m_ClWarlistPrefixesServerInfo;
	pType->m_ChatColor = g_Config.m_ClWarListChat;
	pType->m_ScoreboardColor = g_Config.m_ClWarListScoreboard;
	pType->m_SpecMenuColor = g_Config.m_ClWarListSpecMenu;
	pType->m_SpecMenuPrefix = g_Config.m_ClSpecMenuPrefixes;
	pType->m_NameplateColor = true;
	pType->m_ShowClan = g_Config.m_ClWarListShowClan;
	pType->m_ShowReason = g_Config.m_ClWarListReason;
	pType->m_SwapNameReason = g_Config.m_ClWarListSwapNameReason;
	pType->m_JoinLeaveColor = g_Config.m_ClWarListColorJoinLeave;
	pType->m_BrowserVisible = Index >= 31 || !IsFlagSet(g_Config.m_ClWarlistBrowserFlags, Index);
	pType->m_AutoAdd = Index != 0 && g_Config.m_ClAutoAddOnNameChange != 0;
	pType->m_AutoAddNotify = Index != 0 && g_Config.m_ClAutoAddOnNameChange == 2;
	pType->m_FrozenTee = Index < 31 && IsFlagSet(g_Config.m_ClWarlistFrozenTeeFlags, Index);
	if(pType == m_pWarTypeMute)
	{
		pType->m_Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMutedColor));
		str_copy(pType->m_aPrefix, "♦");
		pType->m_Mute = true;
		pType->m_ChatPrefix = false;
		pType->m_ServerInfoPrefix = false;
		pType->m_ChatColor = false;
		pType->m_ScoreboardColor = false;
		pType->m_SpecMenuColor = false;
		pType->m_SpecMenuPrefix = false;
		pType->m_NameplateColor = false;
		pType->m_ShowClan = false;
		pType->m_ShowReason = false;
		pType->m_SwapNameReason = false;
		pType->m_JoinLeaveColor = false;
		pType->m_BrowserVisible = false;
		pType->m_FrozenTee = false;
	}
	pType->m_SettingsMigrated = true;
}

void CWarList::MigrateLegacySettings()
{
	for(size_t Index = 0; Index < m_WarTypes.size(); ++Index)
		InitializeGroupSettings(m_WarTypes[Index], Index);
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

void CWarList::ConLegacyHideEnemyChat(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->m_LegacyHideEnemyChat = pResult->GetInteger(0) != 0;
	if(pSelf->m_WarTypes[1]->m_SettingsMigrated)
	{
		pSelf->m_WarTypes[1]->m_Mute = pSelf->m_LegacyHideEnemyChat;
		pSelf->UpdateWarPlayers();
	}
}

void CWarList::ConLegacyShowMutedInConsole(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	(void)pUserData;
}

void CWarList::ConUpdateWarGroupMeta(IConsole::IResult *pResult, void *pUserData)
{
	CWarList *pSelf = static_cast<CWarList *>(pUserData);
	pSelf->MigrateLegacySettings();
	const char *pGroup = pResult->GetString(0);
	const char *pProperty = pResult->GetString(1);
	const char *pData = pResult->GetString(2);
	CWarType *pType = pSelf->FindWarType(pGroup);
	if(pType == pSelf->m_pWarTypeNone && str_comp(pGroup, "none") != 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", "Unknown war group");
		return;
	}
	if(str_comp(pProperty, "prefix") == 0)
	{
		if(pResult->NumArguments() >= 3)
		{
			str_copy(pType->m_aPrefix, pData);
			str_clean_whitespaces(pType->m_aPrefix);
		}
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s prefix: \"%s\"", pType->m_aWarName, pType->m_aPrefix);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
		return;
	}

	bool *pValue = nullptr;
	if(str_comp(pProperty, "no_hook_line") == 0)
		pValue = &pType->m_NoHookLine;
	else if(str_comp(pProperty, "no_emotes") == 0)
		pValue = &pType->m_NoEmotes;
	else if(str_comp(pProperty, "mute") == 0)
		pValue = &pType->m_Mute;
	else if(str_comp(pProperty, "chat_prefix") == 0)
		pValue = &pType->m_ChatPrefix;
	else if(str_comp(pProperty, "server_info_prefix") == 0)
		pValue = &pType->m_ServerInfoPrefix;
	else if(str_comp(pProperty, "chat_color") == 0)
		pValue = &pType->m_ChatColor;
	else if(str_comp(pProperty, "scoreboard_color") == 0)
		pValue = &pType->m_ScoreboardColor;
	else if(str_comp(pProperty, "specmenu_color") == 0)
		pValue = &pType->m_SpecMenuColor;
	else if(str_comp(pProperty, "specmenu_prefix") == 0)
		pValue = &pType->m_SpecMenuPrefix;
	else if(str_comp(pProperty, "nameplate_color") == 0)
		pValue = &pType->m_NameplateColor;
	else if(str_comp(pProperty, "show_clan") == 0)
		pValue = &pType->m_ShowClan;
	else if(str_comp(pProperty, "show_reason") == 0)
		pValue = &pType->m_ShowReason;
	else if(str_comp(pProperty, "swap_name_reason") == 0)
		pValue = &pType->m_SwapNameReason;
	else if(str_comp(pProperty, "join_leave_color") == 0)
		pValue = &pType->m_JoinLeaveColor;
	else if(str_comp(pProperty, "browser_visible") == 0)
		pValue = &pType->m_BrowserVisible;
	else if(str_comp(pProperty, "auto_add") == 0)
		pValue = &pType->m_AutoAdd;
	else if(str_comp(pProperty, "auto_add_notify") == 0)
		pValue = &pType->m_AutoAddNotify;
	else if(str_comp(pProperty, "frozen_tee") == 0)
		pValue = &pType->m_FrozenTee;
	if(!pValue)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", "Unknown war group property");
		return;
	}

	if(pResult->NumArguments() >= 3)
	{
		if(pType == pSelf->m_pWarTypeMute && pValue == &pType->m_Mute && str_comp(pData, "1") != 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", "The built-in mute group cannot be unmuted");
			return;
		}
		if(str_comp(pData, "0") != 0 && str_comp(pData, "1") != 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", "Property value must be 0 or 1");
			return;
		}
		*pValue = pData[0] == '1';
		pSelf->UpdateWarPlayers();
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s %s: %d", pType->m_aWarName, pProperty, *pValue);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
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
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
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
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "warlist", aBuf);
	}
}

void CWarList::AddWarEntryInGame(int WarType, const char *pName, const char *pReason, bool IsClan, bool Temp)
{
	if(str_comp(pName, "") == 0)
		return;
	if(WarType < 0 || WarType >= (int)m_WarTypes.size())
		return;

	CWarType *pWarType = m_WarTypes[WarType];
	if(pWarType == m_pWarTypeMute && IsClan)
		return;
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
	if(pWarType != m_pWarTypeMute && !g_Config.m_ClWarListAllowDuplicates && !Entry.m_TempEntry) // Don't remove duplicates for temp entries
		RemoveWarEntryDuplicates(Entry.m_aName, Entry.m_aClan);

	GameClient()->ClientMessage(aBuf);

	AddWarEntry(Entry.m_aName, Entry.m_aClan, Entry.m_aReason, Entry.m_pWarType->m_aWarName, Entry.m_TempEntry);
}

void CWarList::RemoveWarEntryInGame(int WarType, const char *pName, bool IsClan)
{
	if(!str_comp(pName, ""))
		return;

	if(WarType < 0 || WarType >= (int)m_WarTypes.size())
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
	AddWarEntry(pName, "", "", m_pWarTypeMute->m_aWarName, Temp);
}

void CWarList::AddMute(const char *pName, bool Quiet, bool Temp)
{
	if(!str_comp(pName, ""))
		return;

	if(!Quiet)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Added \"%s\" to the %s List", pName, Temp ? "Temp Mute" : "Mute");
		GameClient()->ClientMessage(aBuf);
	}
	RemoveMute(pName, true);
	AddMuteEntry(pName, Temp);
}

void CWarList::RemoveMute(const char *pName, bool Silent)
{
	if(str_comp(pName, "") == 0)
		return;

	const size_t OldSize = m_vWarEntries.size();
	std::erase_if(m_vWarEntries, [&](const CWarEntry &Entry) {
		return Entry.m_pWarType == m_pWarTypeMute && str_comp(Entry.m_aName, pName) == 0;
	});
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), OldSize == m_vWarEntries.size() ? "couldn't find \"%s\" in the Mute List" : "Removed \"%s\" from the Mute List", pName);

	if(!Silent)
		GameClient()->ClientMessage(aBuf);

	RebuildWarMaps();
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
	MigrateLegacySettings();
	if(str_comp(pType, "none") == 0)
		return;
	if(str_comp(pType, "mute") == 0)
	{
		if(Index >= 4)
			m_pWarTypeMute->m_Color = Color;
		return;
	}
	// Old configs put the first custom group at index 4. The mute group now owns
	// that index, so translate legacy custom indices while loading them.
	if(Index == 4)
		m_LegacyGroupOrder = true;
	const int TargetIndex = Index >= 4 && m_LegacyGroupOrder ? Index + 1 : Index;

	if(TargetIndex >= 0 && TargetIndex < static_cast<int>(m_WarTypes.size()))
	{
		str_copy(m_WarTypes[TargetIndex]->m_aWarName, pType);
		m_WarTypes[TargetIndex]->m_Color = Color;
	}
	else
	{
		CWarType *pExistingType = FindWarType(pType);
		if(pExistingType != m_pWarTypeNone)
		{
			pExistingType->m_Color = Color;
			return;
		}
		CWarType *pNewType = new CWarType(pType, Color);
		InitializeGroupSettings(pNewType, Index);
		m_WarTypes.push_back(pNewType);
	}
}

void CWarList::AddWarEntry(const char *pName, const char *pClan, const char *pReason, const char *pType, bool Temp)
{
	MigrateLegacySettings();
	if(str_comp(pName, "") == 0 && str_comp(pClan, "") == 0)
		return;

	CWarType *WarType = FindWarType(pType);
	if(WarType == m_pWarTypeNone)
	{
		AddWarType(pType, ColorRGBA(0, 0, 0, 1));
		WarType = FindWarType(pType);
	}

	CWarEntry Entry(WarType);
	if(WarType == m_pWarTypeMute && (pName[0] == '\0' || pClan[0] != '\0'))
		return;
	str_copy(Entry.m_aReason, pReason);
	Entry.m_TempEntry = Temp; // EClient

	if(str_comp(pClan, "") != 0)
		str_copy(Entry.m_aClan, pClan);
	else if(str_comp(pName, "") != 0)
		str_copy(Entry.m_aName, pName);

	if(WarType == m_pWarTypeMute)
	{
		std::erase_if(m_vWarEntries, [&](const CWarEntry &Existing) {
			return Existing.m_pWarType == m_pWarTypeMute && str_comp(Existing.m_aName, pName) == 0;
		});
	}
	else if(!g_Config.m_ClWarListAllowDuplicates && !Temp)
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
		bool IsDuplicate = It->m_pWarType != m_pWarTypeMute &&
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
	MigrateLegacySettings();
	if(str_comp(pType, "none") == 0 || str_comp(pType, "mute") == 0)
		return;

	CWarType *Type = FindWarType(pType);
	if(Type == m_pWarTypeNone)
	{
		CWarType *NewType = new CWarType(pType, Color);
		NewType->m_SettingsMigrated = true;
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
		if(Entry.m_pWarType == m_pWarTypeMute)
			continue;
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
		if(Entry.m_pWarType == m_pWarTypeMute)
			continue;
		if(!str_comp(GameClient()->m_aClients[ClientId].m_aName, Entry.m_aName) && str_comp(Entry.m_aName, "") != 0)
		{
			return Entry.m_pWarType->m_aWarName;
		}
		else if(!str_comp(GameClient()->m_aClients[ClientId].m_aClan, Entry.m_aClan) && str_comp(Entry.m_aClan, "") != 0)
		{
			return Entry.m_pWarType->m_aWarName;
		}
	}
	if(m_MuteMap.contains(GameClient()->m_aClients[ClientId].m_aName))
		return m_pWarTypeMute->m_aWarName;
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

CWarType *CWarList::GetPriorityType(int ClientId) const
{
	const CWarDataCache &Cache = m_WarPlayers[ClientId];
	if(Cache.m_pNameType)
		return Cache.m_pNameType;
	if(Cache.m_pClanType)
		return Cache.m_pClanType;
	return Cache.m_pMuteType;
}

CWarType *CWarList::GetNameType(int ClientId) const
{
	return m_WarPlayers[ClientId].m_pNameType;
}

CWarType *CWarList::GetClanType(int ClientId) const
{
	return m_WarPlayers[ClientId].m_pClanType;
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
	MigrateLegacySettings();
	for(int i = 0; i < (int)m_WarTypes.size(); ++i)
		m_WarTypes[i]->m_Index = i;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;

		auto &Client = GameClient()->m_aClients[i];
		auto &Cache = m_WarPlayers[i];

		Cache.m_IsMuted = false;
		Cache.m_pNameType = nullptr;
		Cache.m_pClanType = nullptr;
		Cache.m_pMuteType = nullptr;
		Cache.m_NoHookLine = false;
		Cache.m_NoEmotes = false;
		Cache.m_IsWarName = false;
		Cache.m_IsWarClan = false;
		memset(Cache.m_aReason, 0, sizeof(Cache.m_aReason));
		Cache.m_NameColor = ColorRGBA(1, 1, 1, 1);
		Cache.m_ClanColor = ColorRGBA(1, 1, 1, 1);
		Cache.m_WarGroupMatches.clear();
		Cache.m_WarGroupMatches.resize((int)m_WarTypes.size(), false);
		Cache.m_WarTypeIndex = -1;
		auto AddMatches = [&](const std::vector<CWarType *> &vTypes) {
			for(const CWarType *pType : vTypes)
			{
				Cache.m_WarGroupMatches[pType->m_Index] = true;
				Cache.m_IsMuted |= pType->m_Mute;
				Cache.m_NoHookLine |= pType->m_NoHookLine;
				Cache.m_NoEmotes |= pType->m_NoEmotes;
			}
		};
		if(const auto It = m_NameWarTypeMatches.find(Client.m_aName); It != m_NameWarTypeMatches.end())
			AddMatches(It->second);
		if(const auto It = m_ClanWarTypeMatches.find(Client.m_aClan); It != m_ClanWarTypeMatches.end())
			AddMatches(It->second);

		// Name war
		auto ItName = m_NameWarMap.find(Client.m_aName);
		if(ItName != m_NameWarMap.end())
		{
			CWarEntry *Entry = ItName->second;
			str_copy(Cache.m_aReason, Entry->m_aReason);
			Cache.m_IsWarName = true;
			Cache.m_pNameType = Entry->m_pWarType;
			Cache.m_NameColor = Entry->m_pWarType->m_Color;
			Cache.m_WarGroupMatches[Entry->m_pWarType->m_Index] = true;
			Cache.m_WarTypeIndex = Entry->m_pWarType->m_Index;
			Cache.m_NoHookLine |= Entry->m_pWarType->m_NoHookLine;
			Cache.m_NoEmotes |= Entry->m_pWarType->m_NoEmotes;
			Cache.m_IsMuted |= Entry->m_pWarType->m_Mute;
		}

		// Clan war (only if not already a name war)
		auto ItClan = m_ClanWarMap.find(Client.m_aClan);
		if(ItClan != m_ClanWarMap.end())
		{
			CWarEntry *Entry = ItClan->second;
			if(!Cache.m_IsWarName)
				str_copy(Cache.m_aReason, Entry->m_aReason);
			Cache.m_IsWarClan = true;
			Cache.m_pClanType = Entry->m_pWarType;
			Cache.m_ClanColor = Entry->m_pWarType->m_Color;
			Cache.m_WarGroupMatches[Entry->m_pWarType->m_Index] = true;
			Cache.m_WarTypeIndex = Entry->m_pWarType->m_Index;
			Cache.m_NoHookLine |= Entry->m_pWarType->m_NoHookLine;
			Cache.m_NoEmotes |= Entry->m_pWarType->m_NoEmotes;
			Cache.m_IsMuted |= Entry->m_pWarType->m_Mute;
		}

		// Mute
		auto ItMute = m_MuteMap.find(Client.m_aName);
		if(ItMute != m_MuteMap.end())
		{
			Cache.m_IsMuted = true;
			Cache.m_pMuteType = m_pWarTypeMute;
			Cache.m_WarGroupMatches[m_pWarTypeMute->m_Index] = true;
			Cache.m_NoHookLine |= m_pWarTypeMute->m_NoHookLine;
			Cache.m_NoEmotes |= m_pWarTypeMute->m_NoEmotes;
			if(!Cache.m_IsWarName && !Cache.m_IsWarClan)
				Cache.m_WarTypeIndex = m_pWarTypeMute->m_Index;
			if(!Cache.m_IsWarName && !Cache.m_IsWarClan &&
				(m_pWarTypeMute->m_ChatPrefix || m_pWarTypeMute->m_ChatColor || m_pWarTypeMute->m_ScoreboardColor ||
					m_pWarTypeMute->m_SpecMenuColor || m_pWarTypeMute->m_SpecMenuPrefix || m_pWarTypeMute->m_NameplateColor ||
					m_pWarTypeMute->m_ShowReason || m_pWarTypeMute->m_JoinLeaveColor))
			{
				Cache.m_IsWarName = true;
				Cache.m_pNameType = m_pWarTypeMute;
				Cache.m_NameColor = m_pWarTypeMute->m_Color;
				Cache.m_WarTypeIndex = m_pWarTypeMute->m_Index;
			}
		}
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
	pThis->MigrateLegacySettings();

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
		pConfigManager->WriteLine(aBuf, ConfigDomain::ECLIENTWARLIST);
		char aEscapePrefix[sizeof(WarType.m_aPrefix) * 2];
		str_clean_whitespaces(WarType.m_aPrefix);
		EscapeParam(aEscapePrefix, WarType.m_aPrefix, sizeof(aEscapePrefix));
		str_format(aBuf, sizeof(aBuf), "update_war_group_meta \"%s\" prefix \"%s\"", aEscapeType, aEscapePrefix);
		pConfigManager->WriteLine(aBuf, ConfigDomain::ECLIENTWARLIST);
		const char *apProperties[] = {"no_hook_line", "no_emotes", "mute", "chat_prefix", "server_info_prefix", "chat_color", "scoreboard_color", "specmenu_color", "specmenu_prefix", "nameplate_color", "show_clan", "show_reason", "swap_name_reason", "join_leave_color", "browser_visible", "auto_add", "auto_add_notify", "frozen_tee"};
		const bool aValues[] = {WarType.m_NoHookLine, WarType.m_NoEmotes, WarType.m_Mute, WarType.m_ChatPrefix, WarType.m_ServerInfoPrefix, WarType.m_ChatColor, WarType.m_ScoreboardColor, WarType.m_SpecMenuColor, WarType.m_SpecMenuPrefix, WarType.m_NameplateColor, WarType.m_ShowClan, WarType.m_ShowReason, WarType.m_SwapNameReason, WarType.m_JoinLeaveColor, WarType.m_BrowserVisible, WarType.m_AutoAdd, WarType.m_AutoAddNotify, WarType.m_FrozenTee};
		for(size_t Property = 0; Property < std::size(apProperties); ++Property)
		{
			str_format(aBuf, sizeof(aBuf), "update_war_group_meta \"%s\" %s %d", aEscapeType, apProperties[Property], aValues[Property]);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ECLIENTWARLIST);
		}
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
		pConfigManager->WriteLine(aBuf, ConfigDomain::ECLIENTWARLIST);
	}
}
