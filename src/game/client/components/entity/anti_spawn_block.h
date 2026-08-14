#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
#include <base/vmath.h>

#include <engine/client/enums.h>

#include <game/client/component.h>

#include <cstdint>

class CAntiSpawnBlock : public CComponent
{
	enum class EState
	{
		None = 0,
		InTeam,
		TeamZero,
	};

	class CConnState
	{
	public:
		int64_t m_Delay = 0;
		EState m_State = EState::None;
	};

	CConnState m_aConns[NUM_DUMMIES];

	void Reset(int Conn, EState State);
	void HandleConn(int Conn);
	bool GetPos(int Conn, int ClientId, vec2 &Pos) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnSelfDeath(bool Dummy) override { Reset(Dummy, EState::None); }
	void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_ANTI_SPAWN_BLOCK_H
