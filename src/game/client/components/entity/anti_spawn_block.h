#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#include <game/client/component.h>

class CAntiSpawnBlock : public CComponent
{
public:
	bool m_SentKill;
	bool m_SentTeamRequest;
	bool m_Team0Request;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif