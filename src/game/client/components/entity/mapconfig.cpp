#include "mapconfig.h"
#include <base/system.h>
#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

constexpr char aMapConfigDir[] = "Entity/MapConfigs";

void CMapConfig::OnInit()
{
	Storage()->ListDirectory(IStorage::TYPE_ALL, aMapConfigDir, MapConfigDir, this);
}

void CMapConfig::OnMapLoad()
{
	const char *pMapName = Client()->GetCurrentMap();
	GetConfigFile(pMapName);
}

bool CMapConfig::GetConfigFile(const char *pMapName)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s.cfg", aMapConfigDir, pMapName);

	if(Storage()->FileExists(aBuf, Storage()->TYPE_ALL))
	{
		Console()->ExecuteFile(aBuf);
		return true;
	}
	else if(str_comp(m_DefaultFile, "") != 0)
	{
		str_format(aBuf, sizeof(aBuf), "%s/%s.dflt", aMapConfigDir, m_DefaultFile);
		Console()->ExecuteFile(aBuf);
		return true;
	}
	else
		dbg_msg("MapConfig", "No Default Config Found");
	return false;
}

int CMapConfig::MapConfigDir(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CMapConfig *pSelf = (CMapConfig *)pUser;

	if(!IsDir && str_endswith(pName, ".cfg"))
	{
		char aConfigName[64];
		str_copy(aConfigName, pName, str_length(pName) - 3); // remove the .cfg

		dbg_msg("MapConfig", "Loading map config: %s", aConfigName);
	}
	if(!IsDir && str_endswith(pName, ".dflt"))
	{
		if(str_comp(pSelf->m_DefaultFile, "") != 0)
			return 0;

		char aConfigName[64];
		str_copy(aConfigName, pName, str_length(pName) - 4); // remove the .dflt

		dbg_msg("MapConfig", "Default Config Found: %s", aConfigName);
		str_copy(pSelf->m_DefaultFile, aConfigName);
	}

	return 0;
}