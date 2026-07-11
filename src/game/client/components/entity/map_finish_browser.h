#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MAP_FINISH_BROWSER_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MAP_FINISH_BROWSER_H

#include <base/str.h>

#include <engine/client/enums.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>

class CMapFinishEntry
{
	char m_aPlayerName[MAX_NAME_LENGTH];
	char m_aMapName[MAX_MAP_LENGTH];
	char m_aCommunity[CServerInfo::MAX_COMMUNITY_ID_LENGTH];

public:
	CMapFinishEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity)
	{
		str_copy(m_aPlayerName, pPlayerName, sizeof(m_aPlayerName));
		str_copy(m_aMapName, pMapName, sizeof(m_aMapName));
		str_copy(m_aCommunity, pCommunity, sizeof(m_aCommunity));
	}

	bool operator==(const CMapFinishEntry &Other) const
	{
		return str_comp(m_aPlayerName, Other.m_aPlayerName) == 0 &&
		       str_comp(m_aMapName, Other.m_aMapName) == 0 &&
		       str_comp(m_aCommunity, Other.m_aCommunity) == 0;
	}

	const char *PlayerName() const { return m_aPlayerName; }
	const char *MapName() const { return m_aMapName; }
	const char *Community() const { return m_aCommunity; }
};

struct CMapFinishEntryHash
{
	size_t operator()(const CMapFinishEntry &Entry) const
	{
		size_t Hash = static_cast<size_t>(str_quickhash(Entry.PlayerName()));
		Hash ^= static_cast<size_t>(str_quickhash(Entry.MapName())) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		Hash ^= static_cast<size_t>(str_quickhash(Entry.Community())) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		return Hash;
	}
};

class CMapFinishBrowser : public CComponent
{
	int64_t m_LastUpdateTime = 0;

	std::unordered_set<CMapFinishEntry, CMapFinishEntryHash> m_vMapFinishEntries;

	std::array<std::optional<int>, NUM_DUMMIES> m_aFinishTime;

	static void ConAddMapFinish(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	void InsertEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity);
	void RemoveEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity);
	void ClearEntries();

	bool HasEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity) const;

	void OnRender() override;
	void OnConsoleInit() override;
	void OnReset() override;

	int Sizeof() const override { return sizeof(*this); }
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_MAP_FINISH_BROWSER_H
