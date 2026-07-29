#include "map_finish_browser.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>

#include <memory>
#include <optional>

constexpr const float UpdateInterval = 0.08f;

void CMapFinishBrowser::ConAddMapFinish(IConsole::IResult *pResult, void *pUserData)
{
	const char *pPlayerName = pResult->GetString(0);
	const char *pMapName = pResult->GetString(1);
	const char *pCommunity = pResult->GetString(2);

	CMapFinishBrowser *pThis = static_cast<CMapFinishBrowser *>(pUserData);
	pThis->InsertEntry(pPlayerName, pMapName, pCommunity);
}

void CMapFinishBrowser::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CMapFinishBrowser *pThis = static_cast<CMapFinishBrowser *>(pUserData);

	for(const CMapFinishEntry &Entry : pThis->m_vMapFinishEntries)
	{
		char aBuf[(int)MAX_NAME_LENGTH + (int)MAX_MAP_LENGTH + (int)CServerInfo::MAX_COMMUNITY_ID_LENGTH + 32] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_map_finish \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Entry.PlayerName(), pEnd);
		str_append(aBuf, "\" \"");
		// Escape map
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Entry.MapName(), pEnd);
		str_append(aBuf, "\" \"");
		// Escape community
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Entry.Community(), pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYMAPFINISHES);
	}
}

void CMapFinishBrowser::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYMAPFINISHES);

	Console()->Register("add_map_finish", "s[name] s[map] s[community]", CFGFLAG_CLIENT, ConAddMapFinish, this, "Add a saved map finish entry");
}

void CMapFinishBrowser::InsertEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity)
{
	m_vMapFinishEntries.emplace(pPlayerName, pMapName, pCommunity);
}

void CMapFinishBrowser::RemoveEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity)
{
	m_vMapFinishEntries.erase(CMapFinishEntry(pPlayerName, pMapName, pCommunity));
}

void CMapFinishBrowser::ClearEntries()
{
	m_vMapFinishEntries.clear();
}

bool CMapFinishBrowser::HasEntry(const char *pPlayerName, const char *pMapName, const char *pCommunity) const
{
	return m_vMapFinishEntries.find(CMapFinishEntry(pPlayerName, pMapName, pCommunity)) != m_vMapFinishEntries.end();
}

void CMapFinishBrowser::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(m_LastUpdateTime + time_freq() * UpdateInterval > time_get())
		return;
	m_LastUpdateTime = time_get();

	if(!GameClient()->m_GameInfo.m_TimeScore)
		return;

	auto GetTimeScore = [&](int ClientId) -> std::optional<int> {
		if(GameClient()->m_ReceivedDDNetPlayerFinishTimes)
		{
			const int FinishTimeSeconds = GameClient()->m_aClients[ClientId].m_FinishTimeSeconds;
			if(FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS || FinishTimeSeconds == FinishTime::UNSET)
				return std::nullopt;
			return FinishTimeSeconds;
		}

		const CNetObj_PlayerInfo *pPlayerInfo = GameClient()->m_Snap.m_apPlayerInfos[ClientId];
		if(pPlayerInfo == nullptr || pPlayerInfo->m_Score == FinishTime::NOT_FINISHED_TIMESCORE || pPlayerInfo->m_Score == FinishTime::UNSET)
			return std::nullopt;
		return pPlayerInfo->m_Score;
	};

	const float ConnectedSince = Client()->LocalTime();

	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId < 0)
			continue;

		std::optional<int> TimeScore = GetTimeScore(LocalId);

		auto &FinishTime = m_aFinishTime[Dummy];

		if(FinishTime == TimeScore)
			continue;

		if(!TimeScore.has_value() && ConnectedSince < 3.0f)
			continue;

		const CServerInfo &CurrentServerInfo = Client()->ServerInfo();

		const char *pCommunityId = CurrentServerInfo.m_aCommunityId;
		const char *pMapName = CurrentServerInfo.m_aMap;

		if(pCommunityId[0] == '\0' && str_comp(pCommunityId, "None") != 0)
			continue;
		if(pMapName[0] == '\0')
			continue;
		const CCommunity *pCommunity = ServerBrowser()->Community(pCommunityId);
		if(!pCommunity || pCommunity->HasRanks())
			continue;

		const char *pPlayerName = Dummy == (int)IClient::CONN_MAIN ? g_Config.m_PlayerName : g_Config.m_ClDummyName;
		bool Exists = HasEntry(pPlayerName, pMapName, pCommunityId);

		if(!Exists && TimeScore.has_value())
		{
			InsertEntry(pPlayerName, pMapName, pCommunityId);
		}
		else if(Exists && !TimeScore.has_value())
		{
			RemoveEntry(pPlayerName, pMapName, pCommunityId);
		}

		FinishTime = TimeScore;
	}
}

void CMapFinishBrowser::OnReset()
{
	m_aFinishTime.fill(false);
}
