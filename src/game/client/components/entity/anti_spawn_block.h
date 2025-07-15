#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ANTISPAWNBLOCK_H
#include <game/client/component.h>

class CAntiSpawnBlock : public CComponent
{
	int64_t m_Delay = 0;

public:
	enum States
	{
		STATE_NONE = 0,
		STATE_IN_TEAM,
		STATE_TEAM_ZERO,
	};

	bool m_SentKill;
	int m_State;

	void Reset(int State = -1);

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
