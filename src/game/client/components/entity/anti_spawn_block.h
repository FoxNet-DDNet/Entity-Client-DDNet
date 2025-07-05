#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#include <game/client/component.h>

class CAntiSpawnBlock : public CComponent
{
	int64_t m_Delay = 0;

public:
	bool m_SentKill;
	bool m_SentTeamRequest;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif