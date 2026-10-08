#ifndef GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <cstdint>
#include <vector>

// The rendered quad is larger than the ball's physical body, the same way a tee's
// skin is larger than its physical size. Every collision radius in this component is
// derived from this single scale, so raising it (towards ~0.34) makes balls rest
// flush against the ground and against each other instead of visually overlapping.
constexpr float PhysicBallRadiusScale = 0.25f;
constexpr float PhysicBallDefaultSize = 60.0f;

class CBall
{
public:
	float m_Size;
	vec2 m_Pos;
	vec2 m_PrevPos;
	vec2 m_Vel;
	// Movement produced by this step before ball-pair pushout, in units per tick.
	vec2 m_StepVel = vec2(0.0f, 0.0f);
	vec2 m_MapContactNormal = vec2(0.0f, 0.0f);

	bool m_Grounded = false;
	// Resting on another ball that is itself grounded or asleep, so stacks can sleep too.
	bool m_Supported = false;
	bool m_TouchingBall = false;
	bool m_Asleep = false;
	bool m_Dead = false;
	float m_RestTime = 0.0f;

	int m_TuneZone = 0;
	float m_Rotation = 0.0f;
	float m_AngularVelocity = 0.0f; // Radians per physics tick.

	CBall(vec2 Pos, vec2 Vel, float Size) :
		m_Size(Size), m_Pos(Pos), m_PrevPos(Pos), m_Vel(Vel)
	{
	}

	float Radius() const { return m_Size * PhysicBallRadiusScale; }
	float RenderRadius() const { return m_Size * 0.5f; }

	void WakeUp()
	{
		m_Asleep = false;
		m_RestTime = 0.0f;
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

	vec2 PlayerPos() const;

	void RenderBalls();
	void Update(float Dt);
	void DoBallPhysics(CBall *pBall, float Dt, float Elasticity);
	void DoMapCollisions(CBall *pBall, float Dt, float Elasticity) const;
	void DoPlayerCollisions(CBall *pBall, float Elasticity) const;
	void DoBallCollisions(float Elasticity);
	bool ResolveBallPair(CBall *pA, CBall *pB, float Elasticity, bool ApplyImpulse) const;
	void DoWeaponFireEffects(CBall *pBall, float Dt) const;
	void UpdateSleepState(CBall *pBall, float Dt, float MovedDistance) const;

	bool KillBall(CBall &Ball);
	void PruneDeadBalls();
	void WakeAll();

	bool GetNearestAirPos(vec2 Pos, vec2 *pOutPos, float Size) const;

	int64_t m_LastPhysicsTime = 0;

	bool HoldingHook() const;
	bool HoldingFire() const;
	int CurrentWeapon() const;

	// Half extent of the axis aligned box, i.e. the ball radius.
	bool TestBox(vec2 Pos, float HalfExtent) const;

	// State that is identical for every ball, resolved once per physics step instead
	// of once per ball.
	struct CPlayerCollider
	{
		vec2 m_Pos;
		vec2 m_Vel;
	};
	std::vector<CPlayerCollider> m_vPlayerColliders;
	vec2 m_CursorWorldPos = vec2(0.0f, 0.0f);
	float m_Gravity = 0.0f;
	float m_GroundFriction = 1.0f;
	float m_AirFriction = 1.0f;
	float m_AirAngularDamping = 1.0f;
	int m_Weapon = -1;
	bool m_FireHeld = false;
	bool m_FirePressed = false;
	int m_LastObservedFire = 0;
	int m_LastObservedDummy = -1;
	bool m_FireCounterInitialized = false;
	bool m_HammerHitValid = false;
	vec2 m_HammerHitPos = vec2(0.0f, 0.0f);
	vec2 m_HammerPlayerPos = vec2(0.0f, 0.0f);
	float m_HammerStrength = 0.0f;
	void UpdateStepState();
	void WakeForWeaponFire(CBall &Ball) const;

	// Uniform grid broadphase, hashed into a flat bucket table so it does not depend
	// on the map size and needs no per-step allocations once warmed up.
	std::vector<uint64_t> m_vBallCellKeys;
	std::vector<uint32_t> m_vBucketStart;
	std::vector<uint32_t> m_vBucketCursor;
	std::vector<int> m_vBucketBalls;
	std::vector<vec2> m_vPassStartPos;
	float m_GridCellSize = 1.0f;
	uint32_t m_BucketMask = 0;
	void BuildBroadphase();
	void CellOf(vec2 Pos, int *pCellX, int *pCellY) const;

	std::vector<uint32_t> m_vVisibleBallIndices;

public:
	size_t GetBallCount() const { return m_vBalls.size(); }

	void NewBall(vec2 Pos, float Size = PhysicBallDefaultSize, int Amount = 1);
	void NewBallPlayer(int Amount = 1, float Size = PhysicBallDefaultSize);
	void NewBallCursor(int Amount = 1, float Size = PhysicBallDefaultSize);

	void OnExplosion(vec2 Pos, bool SameTeam);

	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override { Reset(); }
	void OnRender() override;
	void OnConsoleInit() override;
	void OnStateChange(int NewState, int OldState) override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_PHYSICBALL_H
