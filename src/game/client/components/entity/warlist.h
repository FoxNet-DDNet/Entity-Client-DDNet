#ifndef GAME_CLIENT_COMPONENTS_ENTITY_WARLIST_H
#define GAME_CLIENT_COMPONENTS_ENTITY_WARLIST_H

#include <base/color.h>
#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <string>
#include <unordered_map>
#include <vector>

enum
{
	MAX_WARLIST_TYPE_LENGTH = 10,
	MAX_WARLIST_IMPORT_ID_LENGTH = 16,
	MAX_WARLIST_REASON_LENGTH = 32
};

// TODO, add chat prefix
class CWarType
{
public:
	// Intentionally named redundantly because it was confusing otherwise
	char m_aWarName[MAX_WARLIST_TYPE_LENGTH] = "";
	ColorRGBA m_Color = ColorRGBA(1, 1, 1, 1);
	bool m_Removable = true;
	bool m_Imported = false;
	bool m_NoHookLine = false;
	bool m_NoEmotes = false;
	bool m_Mute = false;
	bool m_SettingsMigrated = false;
	char m_aPrefix[32] = "♦";
	bool m_ChatPrefix = true;
	bool m_ServerInfoPrefix = true;
	bool m_ChatColor = false;
	bool m_ScoreboardColor = true;
	bool m_SpecMenuColor = true;
	bool m_SpecMenuPrefix = false;
	bool m_NameplateColor = true;
	bool m_ShowClan = false;
	bool m_ShowReason = true;
	bool m_SwapNameReason = false;
	bool m_JoinLeaveColor = false;
	bool m_BrowserVisible = true;
	bool m_AutoAdd = true;
	bool m_AutoAddNotify = false;
	bool m_FrozenTee = false;
	// <E-Client
	int m_Index = 0;
	int m_NumEntries = 0;
	// EClient>

	CWarType(const char *pName, ColorRGBA Color = ColorRGBA(1, 1, 1, 1), bool Removable = true, bool IsImport = false)
	{
		str_copy(m_aWarName, pName);
		m_Color = Color;
		m_Removable = Removable;
		m_Imported = IsImport;
	}

	bool operator==(const CWarType &Other) const
	{
		return str_comp(m_aWarName, Other.m_aWarName) == 0;
	}
};

class CWarEntry
{
public:
	// a war entry can have a name, or clan, but not both
	// name matches the user with that name
	// clan matches all users with that clan
	char m_aName[MAX_NAME_LENGTH] = "";
	char m_aClan[MAX_CLAN_LENGTH] = "";
	char m_aReason[MAX_WARLIST_REASON_LENGTH] = "";

	CWarType *m_pWarType = nullptr;
	bool m_Imported = false;
	// <E-Client
	bool m_TempEntry = false;
	// EClient>

	CWarEntry(CWarType *pWarType)
	{
		m_pWarType = pWarType;
	}

	CWarEntry(CWarType *pWarType, const char *pName, const char *pClan, const char *pReason, bool IsImport = false)
	{
		m_pWarType = pWarType;
		str_copy(m_aName, pName);
		str_copy(m_aClan, pClan);
		str_copy(m_aReason, pReason);
		m_Imported = IsImport;
	}

	bool operator==(const CWarEntry &Other) const
	{
		bool NameMatch = str_comp(m_aName, Other.m_aName) == 0 && str_comp(m_aName, "") != 0;
		bool ClanMatch = str_comp(m_aClan, Other.m_aClan) == 0 && str_comp(m_aClan, "") != 0;
		return (NameMatch || ClanMatch) && m_pWarType == Other.m_pWarType;
	}
};

class CWarDataCache
{
public:
	ColorRGBA m_NameColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	ColorRGBA m_ClanColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	bool m_IsWarName = false;
	bool m_IsWarClan = false;

	bool m_IsMuted = false; // EClient [Mutes]
	CWarType *m_pNameType = nullptr;
	CWarType *m_pClanType = nullptr;
	CWarType *m_pMuteType = nullptr;
	bool m_NoHookLine = false;
	bool m_NoEmotes = false;

	std::vector<char> m_WarGroupMatches = {false, false, false, false, false};
	bool MatchesWarGroup(size_t Index) const { return Index < m_WarGroupMatches.size() && m_WarGroupMatches[Index]; }

	char m_aReason[MAX_WARLIST_REASON_LENGTH] = "";

	int m_WarTypeIndex = 0;
};

class CWarList : public CComponent
{
	// The war list will have these commands
	//
	// Preset Commands
	// .war [name] [reason]
	// .clanwar [name] [reason]
	// .team [name] [reason]
	// .clanteam [name] [reason]
	// .delwar [name]
	// .delteam [name]
	// .delclanwar [name]
	// .delclanteam [team]
	//
	// Generic Commands
	// .name [type] [name] [reason]
	// .clan [type] [clan] [reason]
	// .delname [type] [name]
	// .delclan [type] [clan]
	//
	// Backend commands
	// update_war_group [index] [type] [color]
	// update_war_group_meta [group] [property] [0|1]
	// add_war_entry [type] [name] [clan] [reason]

	// Preset war Commands
	static void ConNameIndex(IConsole::IResult *pResult, void *pUserData);
	static void ConClanIndex(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveNameIndex(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanIndex(IConsole::IResult *pResult, void *pUserData);

	static void ConNameTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConClanTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveNameWar(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveNameTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanWar(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanTeam(IConsole::IResult *pResult, void *pUserData);

	// Generic Commands
	static void ConName(IConsole::IResult *pResult, void *pUserData);
	static void ConClan(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveName(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClan(IConsole::IResult *pResult, void *pUserData);

	// Backend Commands for config file
	static void ConAddWarEntry(IConsole::IResult *pResult, void *pUserData);
	static void ConUpsertWarType(IConsole::IResult *pResult, void *pUserData);
	static void ConUpdateWarGroupMeta(IConsole::IResult *pResult, void *pUserData);
	static void ConLegacyHideEnemyChat(IConsole::IResult *pResult, void *pUserData);
	static void ConLegacyShowMutedInConsole(IConsole::IResult *pResult, void *pUserData);

	// EClient
	static void ConAddMute(IConsole::IResult *pResult, void *pUserData);
	static void ConDelMute(IConsole::IResult *pResult, void *pUserData);
	static void ConAddMuteEntry(IConsole::IResult *pResult, void *pUserData);

	static void ConRemoveNameEntry(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveClanEntry(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	CWarList();
	~CWarList() override;

	/*
	 * duplicate war types are NOT allowed
	 * 0 = none
	 * 1 = enemy
	 * 2 = team
	 * 3 = helper
	 */
	std::vector<CWarType *> m_WarTypes = {
		new CWarType("none", ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false), // 0
		new CWarType("enemy", ColorRGBA(1.0f, 0.2f, 0.2f, 1.0f), false), // 1
		new CWarType("team", ColorRGBA(0.0f, 0.9f, 0.2f, 1.0f), false), // 2
		new CWarType("helper", ColorRGBA(0.9f, 0.87f, 0.2f, 1.0f), false), // 3
		new CWarType("mute", ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false), // 4
	};

	// None type war entries will float to the top of the list, so they can be assigned a type
	CWarType *m_pWarTypeNone = m_WarTypes[0];
	CWarType *m_pWarTypeMute = m_WarTypes[4];
	bool m_LegacyGroupOrder = false;
	bool m_LegacyHideEnemyChat = false;

	// Duplicate war entries ARE allowed
	std::vector<CWarEntry> m_vWarEntries;
	// TODO: create an unordered map for war clans and war names, to speed up updating the WarPlayers cache
	// It should be updated when m_WarList changes

	CWarDataCache m_WarPlayers[MAX_CLIENTS];

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnNewSnapshot() override;
	void OnConsoleInit() override;

	// Fps Increase
	std::unordered_map<std::string, CWarEntry *> m_NameWarMap;
	std::unordered_map<std::string, CWarEntry *> m_ClanWarMap;
	std::unordered_map<std::string, CWarEntry *> m_MuteMap;
	std::unordered_map<std::string, std::vector<CWarType *>> m_NameWarTypeMatches;
	std::unordered_map<std::string, std::vector<CWarType *>> m_ClanWarTypeMatches;
	void RebuildWarMaps();
	void MigrateLegacySettings();
	void InitializeGroupSettings(CWarType *pType, int Index);

	void UpdateWarPlayers();

	void UpdateWarEntry(int Index, const char *pName, const char *pClan, const char *pReason, CWarType *pType, bool Temp = false);

	// Adds a new war entry if the specified index is not found
	void UpsertWarType(int Index, const char *pType, ColorRGBA Color);

	void AddWarEntryInGame(int WarType, const char *pName, const char *pReason, bool IsClan, bool Temp = false);
	void RemoveWarEntryInGame(int WarType, const char *pName, bool IsClan);

	// EClient
	void AddMuteEntry(const char *pName, bool Temp = false);
	void AddMute(const char *pName, bool Quiet, bool Temp = false);
	void RemoveMute(const char *pName, bool Silent = false);

	CWarEntry *FindWarEntryWithName(const char *pName);
	CWarEntry *FindWarEntryWithClan(const char *pClan);
	char *GetWarTypeName(int ClientId);

	void AddWarEntry(const char *pName, const char *pClan, const char *pReason, const char *pType, bool Temp = false);
	void AddWarType(const char *pType, ColorRGBA Color);

	void RemoveWarEntry(const char *pName, const char *pClan, const char *pType);
	void RemoveWarType(const char *pType);

	bool RemoveWarEntryDuplicates(const char *pName, const char *pClan);

	void RemoveWarEntry(CWarEntry *Entry);

	//void RemoveWarEntry(int Index);
	//void RemoveWarType(int Index);

	ColorRGBA GetPriorityColor(int ClientId) const;
	CWarType *GetPriorityType(int ClientId) const;
	CWarType *GetNameType(int ClientId) const;
	CWarType *GetClanType(int ClientId) const;
	ColorRGBA GetNameplateColor(int ClientId) const;
	ColorRGBA GetClanColor(int ClientId) const;
	bool GetAnyWar(int ClientId) const;
	bool GetNameWar(int ClientId) const;
	bool GetClanWar(int ClientId) const;

	void GetReason(char *pReason, int ClientId) const;
	CWarDataCache &GetWarData(int ClientId);

	void SortWarEntries();

	CWarType *FindWarType(const char *pType);
	CWarEntry *FindWarEntry(const char *pName, const char *pClan, const char *pType);
};

#endif
