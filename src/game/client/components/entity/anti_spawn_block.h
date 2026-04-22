#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
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

	int m_State;

	void Reset(int Stat);

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnSelfDeath() override { Reset(STATE_NONE); }
	void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
