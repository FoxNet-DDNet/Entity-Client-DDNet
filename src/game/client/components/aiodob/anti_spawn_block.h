#ifndef GAME_CLIENT_COMPONENTS_AIODOB_ANTISPAWNBLOCK_H
#define GAME_CLIENT_COMPONENTS_AIODOB_ANTISPAWNBLOCK_H
#include <game/client/component.h>

class CAntiSpawnBlock : public CComponent
{
public:
	bool m_SentKill;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif