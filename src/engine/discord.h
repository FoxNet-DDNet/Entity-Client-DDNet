#ifndef ENGINE_DISCORD_H
#define ENGINE_DISCORD_H

#include "kernel.h"
#include <base/types.h>

class IDiscord : public IInterface
{
	MACRO_INTERFACE("discord")
public:
	virtual void Update(bool RPC) = 0;

	virtual void ClearGameInfo(const char *pDetail, bool Timestamp) = 0;
	virtual void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, const char *pDetail, bool ShowMap, bool Timestamp, bool AnnounceAddr) = 0;
};

IDiscord *CreateDiscord();

#endif // ENGINE_DISCORD_H
