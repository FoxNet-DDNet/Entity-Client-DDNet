#include <game/client/gameclient.h>
#include <game/version.h>

#include <engine/shared/json.h>

#include "update.h"
#include <string.h>
#include <tuple>

// Stole Taters Update Stuff
static constexpr const char *ECLIENT_VER_URL = "https://www.entityclient.net/version.json";

void CUpdate::OnInit()
{
	FetchEClientInfo();
}

void CUpdate::OnRender()
{
	if(m_pAClientVerTask)
	{
		if(m_pAClientVerTask->State() == EHttpState::DONE)
		{
			FinishEClientInfo();
			ResetEClientInfoTask();
		}
	}
}
void CUpdate::ResetEClientInfoTask()
{
	if(m_pAClientVerTask)
	{
		m_pAClientVerTask->Abort();
		m_pAClientVerTask = NULL;
	}
}

void CUpdate::FetchEClientInfo()
{
	if(m_pAClientVerTask && !m_pAClientVerTask->Done())
		return;
	char aUrl[256];
	str_copy(aUrl, ECLIENT_VER_URL);
	m_pAClientVerTask = HttpGet(aUrl);
	m_pAClientVerTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pAClientVerTask->IpResolve(IPRESOLVE::V4);
	Http()->Run(m_pAClientVerTask);
}

typedef std::tuple<int, int, int> AVersion;
static const AVersion gs_InvalidACVersion = std::make_tuple(-1, -1, -1);

AVersion ToECVersion(char *pStr)
{
	int aVersion[3] = {0, 0, 0};
	const char *p = strtok(pStr, ".");

	for(int i = 0; i < 3 && p; ++i)
	{
		if(!str_isallnum(p))
			return gs_InvalidACVersion;

		aVersion[i] = str_toint(p);
		p = strtok(NULL, ".");
	}

	if(p)
		return gs_InvalidACVersion;

	return std::make_tuple(aVersion[0], aVersion[1], aVersion[2]);
}

void CUpdate::FinishEClientInfo()
{
	json_value *pJson = m_pAClientVerTask->ResultJson();
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

	json_value_free(pJson);
}
