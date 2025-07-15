#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MAPCONFIG_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MAPCONFIG_H
#include <base/system.h>
#include <engine/console.h>
#include <game/client/component.h>
#include <vector>

class CMapConfig : public CComponent
{
public:
	static int MapConfigDir(const char *pName, int IsDir, int StorageType, void *pUser);

	bool GetConfigFile(const char *pMapName);
	char m_DefaultFile[64] = "";

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnMapLoad() override;
	virtual void OnInit() override;
};

#endif
