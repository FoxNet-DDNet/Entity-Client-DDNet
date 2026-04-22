#ifndef GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <cstdint>
#include <vector>

class CBall
{
public:
	float m_Size;
	vec2 m_Pos;
	vec2 m_PrevPos;
	vec2 m_Vel;

	bool m_Grounded = false;

	int m_TuneZone = 0;
	float m_Rotation = 0.0f;

	CBall(vec2 Pos, vec2 Vel, float Size)
	{
		m_Pos = Pos;
		m_Vel = Vel;
		m_Size = Size;
	}
};

class CPhysicBalls : public CComponent
{
	std::vector<CBall> m_vBalls;
	void Reset();

	static void ConNewPhysicBall(IConsole::IResult *pResult, void *pUserData);
	static void ConNewPhysicBallAtCursor(IConsole::IResult *pResult, void *pUserData);
	static void ConRemovePhysicBallsAtCursor(IConsole::IResult *pResult, void *pUserData);
	static void ConResetPhysicBalls(IConsole::IResult *pResult, void *pUserData);

	vec2 PlayerPos(float BallSize) const;

	bool IsBallVisible(const CBall *pBall);

	void RenderBall(const CBall *pBall);
	void DoBallPhysics(CBall *pBall, float Dt);
	void DoMapCollisions(CBall *pBall, float Dt, float Elasticity) const;
	void DoPlayerCollisions(CBall *pBall, float Dt, float Elasticity) const;
	void DoBallCollisions(CBall *pBall, float Dt, float Elasticity);
	void DoWeaponFireEffects(CBall *pBall, float Dt) const;

	bool KillBall(const CBall *pBall);

	bool GetNearestAirPos(vec2 Pos, vec2 PrevPos, vec2 *pOutPos, float Size) const;

	int64_t m_LastPhysicsTime = 0;

	bool HoldingHook() const;
	bool HoldingFire() const;
	bool PressedFire() const;
	int CurrentWeapon() const;

	bool TestBox(vec2 Pos, float Size) const;

public:
	size_t GetBallCount() const { return m_vBalls.size(); }

	void NewBallPlayer(float Size);
	void NewBallCursor(float Size);

	void OnExplosion(vec2 Pos, bool SameTeam);

	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override { Reset(); }
	void OnRender() override;
	void OnConsoleInit() override;
	void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H
