#ifndef GAME_CLIENT_COMPONENTS_AIODOB_ANTISPAWNBLOCK_H
#define GAME_CLIENT_COMPONENTS_AIODOB_ANTISPAWNBLOCK_H
#include <game/client/component.h>
#include <cstdint>

class CAntiSpawnBlock : public CComponent
{
public:
	bool m_SentKill;
	bool m_SentTeamRequest;
	bool m_Locked;
	bool m_SentTeam0Request;
	int m_Team;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif