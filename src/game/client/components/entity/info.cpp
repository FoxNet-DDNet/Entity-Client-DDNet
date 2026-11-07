#include "info.h"

#include <engine/shared/json.h>

#include <game/client/gameclient.h>
#include <game/version.h>

#include <cstdlib>
#include <string>
#include <tuple>

static constexpr const char *ECLIENT_INFO_FILE = "eclient-info.json";
static constexpr const char *ECLIENT_INFO_URL[2] = {"https://raw.githubusercontent.com/qxdFox/FoxSite/refs/heads/main/docs/info.json", "https://www.entityclient.net/info.json"};

void CEntityInfo::UpdateNews(const json_value &Json)
{
	const json_value &CurrentNews = Json["news"];
	if(CurrentNews.type != json_string)
		return;

	const char *pCurrentNews = CurrentNews;

	// Only mark the news button if something new was added to the news
	if(!m_News.empty() && m_News.find(pCurrentNews) == std::string::npos)
		g_Config.m_EcUnreadNews = true;

	m_News = pCurrentNews;
}

void CEntityInfo::OnInit()
{
	void *pBuf;
	unsigned Length;
	if(!Storage()->ReadFile(ECLIENT_INFO_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
		return;

	json_value *pJson = json_parse((const char *)pBuf, Length);
	free(pBuf);
	if(!pJson)
		return;

	UpdateNews(*pJson);

	json_value_free(pJson);
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
		else if(m_pEClientInfoTask->State() == EHttpState::ERROR && !m_Retried)
		{
			g_Config.m_ClInfoUrlType = !g_Config.m_ClInfoUrlType;
			ResetEClientInfoTask();
			FetchEClientInfo();
			m_Retried = true;
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
	const char *aUrl = ECLIENT_INFO_URL[g_Config.m_ClInfoUrlType];

	m_pEClientInfoTask = HttpGetFile(aUrl, Storage(), ECLIENT_INFO_FILE, IStorage::TYPE_SAVE);
	m_pEClientInfoTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pEClientInfoTask->IpResolve(IPRESOLVE::V4);
	m_pEClientInfoTask->SkipByFileTime(false);
	Http()->Run(m_pEClientInfoTask);
}

typedef std::tuple<int, int, int> EcVersion;
static const EcVersion gs_InvalidECVersion = std::make_tuple(-1, -1, -1);

static EcVersion ToECVersion(char *pStr)
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
	if(!Storage()->ReadFile(ECLIENT_INFO_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
		return;

	json_value *pJson = json_parse((const char *)pBuf, Length);
	free(pBuf);
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
	UpdateNews(Json);

	json_value_free(pJson);
}
