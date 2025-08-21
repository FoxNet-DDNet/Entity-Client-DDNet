#include <game/client/gameclient.h>
#include <game/version.h>

#include <engine/shared/json.h>

#include "info.h"
#include <string.h>
#include <tuple>

static constexpr const char *ECLIENT_INFO_FILE = "eclient-info.json";
static constexpr const char *ECLIENT_INFO_URL = "https://www.entityclient.net/info.json";

void CEntityInfo::OnInit()
{
	void *pBuf;
	unsigned Length;
	if(!Storage()->ReadFile(ECLIENT_INFO_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
		return;

	json_value *pJson = json_parse((const char *)pBuf, Length);
	const json_value &Json = *pJson;
	const json_value &CurrentNews = Json["news"];

	if(CurrentNews.type == json_string)
	{
		str_copy(m_aNews, CurrentNews);
	}
}

void CEntityInfo::OnRender()
{
	if(m_pEClientInfoTask)
	{
		if(m_pEClientInfoTask->State() == EHttpState::DONE)
		{
			FinishEClientInfo();
			ResetEClientInfoTask();
		}
	}
}
void CEntityInfo::ResetEClientInfoTask()
{
	if(m_pEClientInfoTask)
	{
		m_pEClientInfoTask->Abort();
		m_pEClientInfoTask = nullptr;
	}
}

void CEntityInfo::FetchEClientInfo()
{
	if(m_pEClientInfoTask && !m_pEClientInfoTask->Done())
		return;
	const char *aUrl = ECLIENT_INFO_URL;

	m_pEClientInfoTask = HttpGetFile(aUrl, Storage(), ECLIENT_INFO_FILE, IStorage::TYPE_SAVE);
	m_pEClientInfoTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pEClientInfoTask->IpResolve(IPRESOLVE::V4);
	m_pEClientInfoTask->ExpectSha256(SHA256_ZEROED);
	m_pEClientInfoTask->SkipByFileTime(false);
	Http()->Run(m_pEClientInfoTask);
}

typedef std::tuple<int, int, int> EcVersion;
static const EcVersion gs_InvalidECVersion = std::make_tuple(-1, -1, -1);

EcVersion ToECVersion(char *pStr)
{
	int aVersion[3] = {0, 0, 0};
	const char *p = strtok(pStr, ".");

	for(int i = 0; i < 3 && p; ++i)
	{
		if(!str_isallnum(p))
			return gs_InvalidECVersion;

		aVersion[i] = str_toint(p);
		p = strtok(nullptr, ".");
	}

	if(p)
		return gs_InvalidECVersion;

	return std::make_tuple(aVersion[0], aVersion[1], aVersion[2]);
}

void CEntityInfo::FinishEClientInfo()
{
	void *pBuf;
	unsigned Length;
	json_value *pJson = nullptr;
	if(Storage()->ReadFile(ECLIENT_INFO_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
		pJson = json_parse((const char *)pBuf, Length);
	if(!pJson)
		return;
	const json_value &Json = *pJson;
	const json_value &CurrentVersion = Json["version"];

	if(CurrentVersion.type == json_string)
	{
		char aNewVersionStr[64];
		str_copy(aNewVersionStr, CurrentVersion);
		char aCurVersionStr[64];
		str_copy(aCurVersionStr, ECLIENT_VERSION);
		if(ToECVersion(aNewVersionStr) > ToECVersion(aCurVersionStr))
		{
			str_copy(m_aVersionStr, CurrentVersion);
		}
		else
		{
			m_aVersionStr[0] = '0';
			m_aVersionStr[1] = '\0';
		}
	}
	const json_value &CurrentNews = Json["news"];

	if(CurrentNews.type == json_string)
	{
		if(m_aNews[0] && !str_find(m_aNews, CurrentNews))
			g_Config.m_EcUnreadNews = true;

		str_copy(m_aNews, CurrentNews);
	}
		 
	json_value_free(pJson);
}
