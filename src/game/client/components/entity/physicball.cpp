#include "physicball.h"

#include <base/color.h>
#include <base/log.h>
#include <base/math.h>
#include <base/time.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/client_data.h>
#include <generated/protocol.h>

#include <game/client/components/particles.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

constexpr float PhysicBallKillMargin = 200.0f * 32.0f;

constexpr float PhysicBallRestSpeed = 30.0f;
constexpr float PhysicBallRestDuration = 0.4f;
constexpr float PhysicBallMaxAngularSpeed = 1.5f; // Radians per physics tick.
constexpr float PhysicBallHammerReach = 0.95f;
constexpr float PhysicBallHammerHitRadius = 0.85f;
constexpr float PhysicBallHammerStrengthScale = 1.35f;
constexpr size_t MaxPhysicBalls = 4096;
constexpr float MinPhysicBallSize = 1.0f;
constexpr float MaxPhysicBallSize = 512.0f;

static uint64_t PackCell(int CellX, int CellY)
{
	return ((uint64_t)(uint32_t)CellX << 32) | (uint64_t)(uint32_t)CellY;
}

static uint32_t HashCell(int CellX, int CellY)
{
	return (uint32_t)CellX * 73856093u ^ (uint32_t)CellY * 19349663u;
}

void CPhysicBalls::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
		Reset();
}

void CPhysicBalls::Reset()
{
	std::vector<CBall>().swap(m_vBalls);
	std::vector<CPlayerCollider>().swap(m_vPlayerColliders);
	std::vector<uint64_t>().swap(m_vBallCellKeys);
	std::vector<uint32_t>().swap(m_vBucketStart);
	std::vector<uint32_t>().swap(m_vBucketCursor);
	std::vector<int>().swap(m_vBucketBalls);
	std::vector<vec2>().swap(m_vPassStartPos);
	std::vector<uint32_t>().swap(m_vVisibleBallIndices);
	m_LastPhysicsTime = 0;
	m_FireCounterInitialized = false;
	m_LastObservedDummy = -1;
	m_FirePressed = false;
	m_HammerHitValid = false;
}

void CPhysicBalls::OnConsoleInit()
{
	Console()->Register("physic_ball_new", "?f[size] ?i[amount]", CFGFLAG_CLIENT, ConNewPhysicBall, this, "Summon a new physic ball");
	Console()->Register("physic_ball_new_cursor", "?f[size] ?i[amount]", CFGFLAG_CLIENT, ConNewPhysicBallAtCursor, this, "Summon a new physic ball at the cursor");
	Console()->Register("physic_balls_remove_cursor", "?f[radius]", CFGFLAG_CLIENT, ConRemovePhysicBallsAtCursor, this, "Removes ball at cursor");
	Console()->Register("physic_balls_reset", "", CFGFLAG_CLIENT, ConResetPhysicBalls, this, "Reset all physic balls");
}

void CPhysicBalls::NewBall(vec2 Pos, float Size, int Amount)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	if(!std::isfinite(Pos.x) || !std::isfinite(Pos.y) || !std::isfinite(Size) ||
		Size < MinPhysicBallSize || Size > MaxPhysicBallSize || Amount <= 0)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "physic balls", "Invalid ball position, size, or amount");
		return;
	}
	if(static_cast<size_t>(Amount) > MaxPhysicBalls - m_vBalls.size())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "physic balls", "Ball limit reached");
		return;
	}

	vec2 OutPos;
	if(GetNearestAirPos(Pos, &OutPos, Size))
		Pos = OutPos;

	m_vBalls.reserve(m_vBalls.size() + static_cast<size_t>(Amount));
	for(int i = 0; i < Amount; i++)
		m_vBalls.emplace_back(Pos, vec2(), Size);
	WakeAll();
}

void CPhysicBalls::NewBallPlayer(int Amount, float Size)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	NewBall(PlayerPos(), Size, Amount);
}

void CPhysicBalls::NewBallCursor(int Amount, float Size)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	NewBall(GameClient()->GetCursorWorldPos(), Size, Amount);
}

void CPhysicBalls::ConNewPhysicBall(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);
	float Size = pResult->NumArguments() >= 1 ? pResult->GetFloat(0) : PhysicBallDefaultSize;
	int Amount = pResult->NumArguments() >= 2 ? pResult->GetInteger(1) : 1;

	pSelf->NewBallPlayer(Amount, Size);
}

void CPhysicBalls::ConNewPhysicBallAtCursor(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);
	float Size = pResult->NumArguments() >= 1 ? pResult->GetFloat(0) : PhysicBallDefaultSize;
	int Amount = pResult->NumArguments() >= 2 ? pResult->GetInteger(1) : 1;

	pSelf->NewBallCursor(Amount, Size);
}

void CPhysicBalls::ConRemovePhysicBallsAtCursor(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);

	if(pSelf->Client()->State() != IClient::STATE_ONLINE)
		return;

	const float Radius = pResult->NumArguments() > 0 ? pResult->GetFloat(0) : 40.0f;
	const vec2 CursorPos = pSelf->GameClient()->GetCursorWorldPos();

	for(CBall &Ball : pSelf->m_vBalls)
	{
		const float Distance = length(Ball.m_Pos - CursorPos);
		if(Distance < Radius + Ball.m_Size * 0.5f)
			pSelf->KillBall(Ball);
	}
	pSelf->PruneDeadBalls();
}

void CPhysicBalls::ConResetPhysicBalls(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);
	pSelf->Reset();
}

vec2 CPhysicBalls::PlayerPos() const
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return GameClient()->m_Camera.m_Center;

	auto GetPos = [&]() -> vec2 {
		if(GameClient()->m_Snap.m_SpecInfo.m_Active)
			return GameClient()->m_Camera.m_Center;

		int ClientId = GameClient()->m_Snap.m_LocalClientId;
		if(ClientId == -1)
			return GameClient()->m_Camera.m_Center;

		return GameClient()->m_aClients[ClientId].m_RenderPos;
	};

	return GetPos() - vec2(0, 2.0f);
}

void CPhysicBalls::RenderBalls()
{
	if(m_vBalls.empty())
		return;

	const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_ClPhysicBallsSkin);
	if(!pSkin)
		return;

	const CScreenRect ScreenRect = Graphics()->GetScreen();

	m_vVisibleBallIndices.clear();
	for(size_t i = 0; i < m_vBalls.size(); i++)
	{
		const CBall &Ball = m_vBalls[i];
		const float HalfSize = Ball.m_Size * 0.75f;
		if(Ball.m_Pos.x + HalfSize < ScreenRect.m_TopLeft.x || Ball.m_Pos.x - HalfSize > ScreenRect.m_BottomRight.x ||
			Ball.m_Pos.y + HalfSize < ScreenRect.m_TopLeft.y || Ball.m_Pos.y - HalfSize > ScreenRect.m_BottomRight.y)
			continue;
		m_vVisibleBallIndices.push_back(static_cast<uint32_t>(i));
	}

	if(m_vVisibleBallIndices.empty())
		return;

	for(int Pass = 0; Pass < 2; Pass++)
	{
		Graphics()->TextureSet(Pass == 0 ? pSkin->m_OriginalSkin.m_BodyOutline : pSkin->m_OriginalSkin.m_Body);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		for(uint32_t Index : m_vVisibleBallIndices)
		{
			const CBall &Ball = m_vBalls[Index];
			Graphics()->QuadsSetRotation(Ball.m_Rotation);
			IEngineGraphics::CQuadItem Quad{Ball.m_Pos.x, Ball.m_Pos.y, Ball.m_Size, Ball.m_Size};
			Graphics()->QuadsDraw(&Quad, 1);
		}
		Graphics()->QuadsEnd();
	}
}

void CPhysicBalls::UpdateStepState()
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;

	m_Gravity = LocalId >= 0 ?
			    (float)GameClient()->m_aClients[LocalId].m_Predicted.m_Tuning.m_Gravity :
			    (float)GameClient()->m_aTuning[g_Config.m_ClDummy].m_Gravity;

	m_CursorWorldPos = GameClient()->GetCursorWorldPos();
	m_FireHeld = HoldingFire();
	const int Dummy = g_Config.m_ClDummy;
	const int CurrentFire = GameClient()->m_Controls.m_aInputData[Dummy].m_Fire;
	m_FirePressed = m_FireCounterInitialized && m_LastObservedDummy == Dummy &&
			CountInput(m_LastObservedFire, CurrentFire).m_Presses > 0;
	m_LastObservedFire = CurrentFire;
	m_LastObservedDummy = Dummy;
	m_FireCounterInitialized = true;
	m_Weapon = GameClient()->m_Snap.m_SpecInfo.m_Active ? WEAPON_GUN : CurrentWeapon();
	m_HammerHitValid = false;
	if(m_FirePressed && m_Weapon == WEAPON_HAMMER &&
		LocalId >= 0 && GameClient()->m_Snap.m_apPlayerInfos[LocalId])
	{
		const CGameClient::CClientData &LocalClient = GameClient()->m_aClients[LocalId];
		if(!LocalClient.m_Predicted.m_HammerHitDisabled &&
			(g_Config.m_SvDeepfly || !LocalClient.m_Predicted.m_DeepFrozen))
		{
			const vec2 HammerInputDir = normalize(vec2(
				(float)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x,
				(float)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y));
			m_HammerPlayerPos = LocalClient.m_Predicted.m_Pos;
			m_HammerHitPos = m_HammerPlayerPos + HammerInputDir * CCharacterCore::PhysicalSize() * PhysicBallHammerReach;
			m_HammerStrength = LocalClient.m_Predicted.m_Tuning.m_HammerStrength * PhysicBallHammerStrengthScale;
			m_HammerHitValid = true;
		}
	}

	m_vPlayerColliders.clear();
	if(LocalId < 0 || !GameClient()->m_Snap.m_apPlayerInfos[LocalId])
		return;

	const CGameClient::CClientData &LocalClient = GameClient()->m_aClients[LocalId];

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId])
			continue;

		const CGameClient::CClientData &Client = GameClient()->m_aClients[ClientId];
		if(!Client.m_Active || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;
		if(ClientId != LocalId)
		{
			if(Client.m_CollisionDisabled || Client.m_Spec || Client.m_Predicted.m_Id < 0)
				continue;
			if(Client.m_Team != LocalClient.m_Team)
				continue;
			if(Client.m_Solo || LocalClient.m_Solo)
				continue;
		}

		m_vPlayerColliders.push_back({Client.m_RenderPos, Client.m_Predicted.m_Vel});
	}
}

void CPhysicBalls::DoPlayerCollisions(CBall *pBall, float Elasticity) const
{
	if(m_vPlayerColliders.empty())
		return;

	const float BallRadius = pBall->Radius();
	const float CombinedRadius = BallRadius + CCharacterCore::PhysicalSize() * 0.75f;
	const float SeparationPadding = 0.5f;

	for(const CPlayerCollider &Player : m_vPlayerColliders)
	{
		const vec2 Delta = pBall->m_Pos - Player.m_Pos;
		const float DistanceSq = dot(Delta, Delta);
		if(DistanceSq >= CombinedRadius * CombinedRadius)
			continue;

		const float Distance = std::sqrt(DistanceSq);
		const vec2 Normal = Distance > 0.0001f ? Delta / Distance : vec2(0.0f, -1.0f);
		const float Penetration = CombinedRadius - Distance + SeparationPadding;

		pBall->WakeUp();

		vec2 NewPos = pBall->m_Pos + Normal * Penetration;

		if(TestBox(vec2(NewPos.x, pBall->m_Pos.y), BallRadius))
			NewPos.x = pBall->m_Pos.x;
		if(TestBox(vec2(pBall->m_Pos.x, NewPos.y), BallRadius))
			NewPos.y = pBall->m_Pos.y;

		const vec2 RelativeVel = pBall->m_Vel - Player.m_Vel;
		const float RelativeNormalVel = dot(RelativeVel, Normal);
		if(RelativeNormalVel < 0.0f)
			pBall->m_Vel -= Normal * ((1.0f + Elasticity) * RelativeNormalVel);

		pBall->m_Pos = NewPos;
	}
}

void CPhysicBalls::CellOf(vec2 Pos, int *pCellX, int *pCellY) const
{
	const float x = std::isfinite(Pos.x) ? std::clamp(Pos.x / m_GridCellSize, -1.0e6f, 1.0e6f) : 0.0f;
	const float y = std::isfinite(Pos.y) ? std::clamp(Pos.y / m_GridCellSize, -1.0e6f, 1.0e6f) : 0.0f;
	*pCellX = (int)std::floor(x);
	*pCellY = (int)std::floor(y);
}

void CPhysicBalls::BuildBroadphase()
{
	const size_t NumBalls = m_vBalls.size();

	float MaxRadius = 1.0f;
	for(const CBall &Ball : m_vBalls)
		MaxRadius = std::max(MaxRadius, Ball.Radius());
	m_GridCellSize = MaxRadius * 2.0f;

	uint32_t NumBuckets = 64;
	while(NumBuckets < NumBalls * 2)
		NumBuckets <<= 1;
	m_BucketMask = NumBuckets - 1;

	m_vBallCellKeys.resize(NumBalls);
	m_vBucketBalls.resize(NumBalls);
	m_vBucketStart.assign(NumBuckets + 1, 0);

	for(size_t i = 0; i < NumBalls; i++)
	{
		int CellX, CellY;
		CellOf(m_vBalls[i].m_Pos, &CellX, &CellY);
		m_vBallCellKeys[i] = PackCell(CellX, CellY);
		m_vBucketStart[(HashCell(CellX, CellY) & m_BucketMask) + 1]++;
	}
	for(uint32_t Bucket = 0; Bucket < NumBuckets; Bucket++)
		m_vBucketStart[Bucket + 1] += m_vBucketStart[Bucket];

	m_vBucketCursor = m_vBucketStart;
	for(size_t i = 0; i < NumBalls; i++)
	{
		const uint64_t Key = m_vBallCellKeys[i];
		const uint32_t Bucket = HashCell((int)(uint32_t)(Key >> 32), (int)(uint32_t)Key) & m_BucketMask;
		m_vBucketBalls[m_vBucketCursor[Bucket]++] = (int)i;
	}
}

bool CPhysicBalls::ResolveBallPair(CBall *pA, CBall *pB, float Elasticity, bool ApplyImpulse) const
{
	const float RadiusA = pA->Radius();
	const float RadiusB = pB->Radius();
	const float ContactDistance = RadiusA + RadiusB;

	const float TouchDistance = ContactDistance + 1.0f;

	const vec2 Delta = pA->m_Pos - pB->m_Pos;
	if(std::abs(Delta.x) >= TouchDistance || std::abs(Delta.y) >= TouchDistance)
		return false;
	const float DistanceSq = dot(Delta, Delta);
	if(DistanceSq >= TouchDistance * TouchDistance)
		return false;

	const float Distance = std::sqrt(DistanceSq);
	vec2 Normal;
	if(Distance > 0.0001f)
		Normal = Delta / Distance;
	else
	{
		// A batch spawn can place thousands of balls at exactly the same point.
		// Give coincident pairs stable directions instead of stacking them all vertically.
		constexpr float Diagonal = 0.70710678f;
		static const vec2 aDirections[8] = {
			vec2(1.0f, 0.0f), vec2(Diagonal, Diagonal), vec2(0.0f, 1.0f), vec2(-Diagonal, Diagonal),
			vec2(-1.0f, 0.0f), vec2(-Diagonal, -Diagonal), vec2(0.0f, -1.0f), vec2(Diagonal, -Diagonal)};
		const uint32_t IndexA = static_cast<uint32_t>(pA - m_vBalls.data());
		const uint32_t IndexB = static_cast<uint32_t>(pB - m_vBalls.data());
		Normal = aDirections[(IndexA * 73856093u ^ IndexB * 19349663u) & 7u];
	}

	const bool BSupportsA = Normal.y < -0.4f && (pB->m_Grounded || pB->m_Supported || pB->m_Asleep);
	const bool ASupportsB = Normal.y > 0.4f && (pA->m_Grounded || pA->m_Supported || pA->m_Asleep);
	if(BSupportsA)
		pA->m_Supported = true;
	if(ASupportsB)
		pB->m_Supported = true;

	if(Distance >= ContactDistance)
		return false;

	if(pA->m_Asleep && pB->m_Asleep)
		return false;
	constexpr float Slop = 0.5f;
	// A resting contact within the allowed slop needs neither correction nor
	// wakeup. This also avoids running the second pass for untouched piles.
	if(ContactDistance - Distance <= Slop &&
		(!ApplyImpulse || dot(pA->m_Vel - pB->m_Vel, Normal) >= 0.0f))
		return false;

	pA->WakeUp();
	pB->WakeUp();

	const float MassA = pA->m_Size;
	const float MassB = pB->m_Size;
	const float MassSum = MassA + MassB;
	if(MassSum <= 0.0001f)
		return false;
	const float ShareA = MassB / MassSum;
	const float ShareB = MassA / MassSum;
	// A supported lower ball should not be pushed into the pile by the
	// positional correction. The velocity impulse still uses both masses.
	const float PositionShareA = BSupportsA ? 1.0f : ASupportsB ? 0.0f :
								      ShareA;
	const float PositionShareB = ASupportsB ? 1.0f : BSupportsA ? 0.0f :
								      ShareB;

	constexpr float CorrectionRate = 0.95f;
	const float Penetration = std::max(ContactDistance - Distance - Slop, 0.0f) * CorrectionRate;
	if(Penetration > 0.0f)
	{
		const vec2 Correction = Normal * Penetration;
		pA->m_Pos += Correction * PositionShareA;
		pB->m_Pos -= Correction * PositionShareB;
	}

	if(ApplyImpulse)
	{
		const vec2 RelativeVel = pA->m_Vel - pB->m_Vel;
		const float RelativeNormalVel = dot(RelativeVel, Normal);
		if(RelativeNormalVel < 0.0f)
		{
			const vec2 Impulse = Normal * ((1.0f + Elasticity) * RelativeNormalVel);
			pA->m_Vel -= Impulse * ShareA;
			pB->m_Vel += Impulse * ShareB;
		}

		// Tangential motion at the contact point makes the balls spin as they rub.
		const vec2 Tangent(-Normal.y, Normal.x);
		const float SpinRadiusA = pA->RenderRadius();
		const float SpinRadiusB = pB->RenderRadius();
		const float Slip = dot(RelativeVel, Tangent) - pA->m_AngularVelocity * SpinRadiusA - pB->m_AngularVelocity * SpinRadiusB;
		const float SpinChange = Slip * 0.2f / (SpinRadiusA + SpinRadiusB);
		pA->m_AngularVelocity = std::clamp(pA->m_AngularVelocity + SpinChange, -PhysicBallMaxAngularSpeed, PhysicBallMaxAngularSpeed);
		pB->m_AngularVelocity = std::clamp(pB->m_AngularVelocity + SpinChange, -PhysicBallMaxAngularSpeed, PhysicBallMaxAngularSpeed);
	}

	return true;
}

void CPhysicBalls::DoBallCollisions(float Elasticity)
{
	const int NumBalls = (int)m_vBalls.size();
	if(NumBalls < 2)
		return;

	for(int Iteration = 0; Iteration < 2; Iteration++)
	{
		// Position corrections can move a ball into another cell between passes.
		BuildBroadphase();
		m_vPassStartPos.resize(NumBalls);
		for(int i = 0; i < NumBalls; i++)
			m_vPassStartPos[i] = m_vBalls[i].m_Pos;
		const bool ApplyImpulse = Iteration == 0;
		bool AnyContact = false;
		// Visit each neighboring cell pair once: current, right, and the row below.
		static constexpr int aNeighbourOffsets[5][2] = {{0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

		for(int i = 0; i < NumBalls; i++)
		{
			const uint64_t Key = m_vBallCellKeys[i];
			const int CellX = (int)(uint32_t)(Key >> 32);
			const int CellY = (int)(uint32_t)Key;

			for(const auto &Offset : aNeighbourOffsets)
			{
				const int NeighbourX = CellX + Offset[0];
				const int NeighbourY = CellY + Offset[1];
				const uint64_t NeighbourKey = PackCell(NeighbourX, NeighbourY);
				const uint32_t Bucket = HashCell(NeighbourX, NeighbourY) & m_BucketMask;

				for(uint32_t k = m_vBucketStart[Bucket]; k < m_vBucketStart[Bucket + 1]; k++)
				{
					const int j = m_vBucketBalls[k];
					if((Offset[0] == 0 && Offset[1] == 0 && j <= i) || m_vBallCellKeys[j] != NeighbourKey)
						continue;

					if(ResolveBallPair(&m_vBalls[i], &m_vBalls[j], Elasticity, ApplyImpulse))
						AnyContact = true;
				}
			}
		}
		if(!AnyContact)
			break;

		// Map checks scale with moved balls, not with the number of ball contacts.
		for(int i = 0; i < NumBalls; i++)
		{
			CBall &Ball = m_vBalls[i];
			const vec2 StartPos = m_vPassStartPos[i];
			if(Ball.m_Pos == StartPos || !TestBox(Ball.m_Pos, Ball.Radius()))
				continue;

			const vec2 TargetPos = Ball.m_Pos;
			Ball.m_Pos = StartPos;
			if(!TestBox(vec2(TargetPos.x, StartPos.y), Ball.Radius()))
				Ball.m_Pos.x = TargetPos.x;
			if(!TestBox(vec2(Ball.m_Pos.x, TargetPos.y), Ball.Radius()))
				Ball.m_Pos.y = TargetPos.y;
		}
	}
}

void CPhysicBalls::DoMapCollisions(CBall *pBall, float Dt, float Elasticity) const
{
	const float BallRadius = pBall->Radius();

	const vec2 StartPos = pBall->m_PrevPos;
	const vec2 TargetPos = pBall->m_Pos + pBall->m_Vel * Dt;
	const vec2 FullDelta = TargetPos - StartPos;
	const float Distance = length(FullDelta);

	if(Distance < 0.0001f)
	{
		pBall->m_Pos = StartPos;
		pBall->m_MapContactNormal = vec2(0.0f, 0.0f);
		return;
	}

	const float MaxStep = std::clamp(BallRadius, 4.0f, 16.0f);
	const int Steps = std::clamp((int)std::ceil(Distance / MaxStep), 1, 32);

	bool Grounded = false;
	vec2 HorizontalContactNormal(0.0f, 0.0f);
	vec2 VerticalContactNormal(0.0f, 0.0f);

	vec2 Pos = StartPos;
	vec2 Vel = pBall->m_Vel;

	for(int i = 0; i < Steps; i++)
	{
		vec2 NextPos = StartPos + FullDelta * ((float)(i + 1) / (float)Steps);

		if(NextPos == Pos)
			break;

		if(TestBox(NextPos, BallRadius))
		{
			int Hits = 0;

			// Y axis
			if(TestBox(vec2(Pos.x, NextPos.y), BallRadius))
			{
				const bool MovingDown = NextPos.y > Pos.y || (NextPos.y == Pos.y && Vel.y > 0.0f);
				if(MovingDown)
					Grounded = true;
				VerticalContactNormal = vec2(0.0f, MovingDown ? -1.0f : 1.0f);
				NextPos.y = Pos.y;
				Vel.y *= -Elasticity;
				Hits++;
			}

			// X axis
			if(TestBox(vec2(NextPos.x, Pos.y), BallRadius))
			{
				const bool MovingRight = NextPos.x > Pos.x || (NextPos.x == Pos.x && Vel.x > 0.0f);
				HorizontalContactNormal = vec2(MovingRight ? -1.0f : 1.0f, 0.0f);
				NextPos.x = Pos.x;
				Vel.x *= -Elasticity;
				Hits++;
			}

			// Corner hit
			if(Hits == 0)
			{
				const bool MovingDown = NextPos.y > Pos.y || (NextPos.y == Pos.y && Vel.y > 0.0f);
				const bool MovingRight = NextPos.x > Pos.x || (NextPos.x == Pos.x && Vel.x > 0.0f);
				if(MovingDown)
					Grounded = true;
				VerticalContactNormal = vec2(0.0f, MovingDown ? -1.0f : 1.0f);
				HorizontalContactNormal = vec2(MovingRight ? -1.0f : 1.0f, 0.0f);
				NextPos.y = Pos.y;
				Vel.y *= -Elasticity;
				NextPos.x = Pos.x;
				Vel.x *= -Elasticity;
			}
		}

		Pos = NextPos;
	}

	pBall->m_Pos = Pos;
	pBall->m_Vel = Vel;
	pBall->m_Grounded = Grounded;
	const vec2 Travel = Pos - StartPos;
	if(VerticalContactNormal.y != 0.0f &&
		(HorizontalContactNormal.x == 0.0f || std::abs(Travel.x) >= std::abs(Travel.y)))
		pBall->m_MapContactNormal = VerticalContactNormal;
	else if(HorizontalContactNormal.x != 0.0f)
		pBall->m_MapContactNormal = HorizontalContactNormal;
	else
	{
		// Keep a sliding contact when there is tangent motion but no new impact.
		const vec2 PreviousNormal = pBall->m_MapContactNormal;
		const float TangentialTravel = Travel.y * PreviousNormal.x - Travel.x * PreviousNormal.y;
		if(std::abs(TangentialTravel) <= 0.01f || !TestBox(Pos - PreviousNormal, BallRadius))
			pBall->m_MapContactNormal = vec2(0.0f, 0.0f);
	}
}

void CPhysicBalls::DoWeaponFireEffects(CBall *pBall, float Dt) const
{
	if(m_Weapon == -1)
		return;

	if(m_Weapon == WEAPON_GUN && m_FireHeld)
	{
		const vec2 ForceDir = normalize(m_CursorWorldPos - pBall->m_Pos);

		const float ForceStrength = 2.0f;
		pBall->m_Vel += (ForceDir * (ForceStrength * Dt));
	}
	else if(m_Weapon == WEAPON_HAMMER && m_HammerHitValid)
	{
		const vec2 Delta = pBall->m_Pos - m_HammerHitPos;
		const float CombinedRadius = pBall->Radius() + CCharacterCore::PhysicalSize() * PhysicBallHammerHitRadius;

		if(dot(Delta, Delta) < CombinedRadius * CombinedRadius)
		{
			const vec2 PlayerToBall = pBall->m_Pos - m_HammerPlayerPos;
			const vec2 Direction = dot(PlayerToBall, PlayerToBall) > 0.0f ? normalize(PlayerToBall) : vec2(0.0f, -1.0f);
			vec2 Temp = pBall->m_Vel + normalize(Direction + vec2(0.f, -1.1f)) * 10.0f;
			Temp = ClampVel(0, Temp);
			Temp -= pBall->m_Vel;
			const vec2 Force = (vec2(0.f, -1.0f) + Temp) * m_HammerStrength;
			pBall->m_Vel = ClampVel(0, pBall->m_Vel + Force);
		}
	}
}

void CPhysicBalls::WakeForWeaponFire(CBall &Ball) const
{
	if(m_Weapon == WEAPON_GUN && m_FireHeld)
	{
		// Gun force currently reaches every ball, including sleeping ones.
		Ball.WakeUp();
	}
	else if(m_Weapon == WEAPON_HAMMER && m_HammerHitValid)
	{
		const vec2 Delta = Ball.m_Pos - m_HammerHitPos;
		const float CombinedRadius = Ball.Radius() + CCharacterCore::PhysicalSize() * PhysicBallHammerHitRadius;
		if(dot(Delta, Delta) < CombinedRadius * CombinedRadius)
			Ball.WakeUp();
	}
}

bool CPhysicBalls::KillBall(CBall &Ball)
{
	if(Ball.m_Dead)
		return false;
	Ball.m_Dead = true;

	for(int i = 0; i < 16; i++)
	{
		const ColorRGBA Color = ColorRGBA(random_float(1.0f), random_float(1.0f), random_float(1.0f), 1.0f);

		CParticle Particle;
		Particle.SetDefault();
		Particle.m_Spr = SPRITE_PART_SPLAT01 + (rand() % 3);
		Particle.m_Pos = Ball.m_Pos;
		Particle.m_Vel = random_direction() * (random_float(0.1f, 1.1f) * 900.0f);
		Particle.m_LifeSpan = random_float(0.3f, 0.6f);
		Particle.m_StartSize = random_float(24.0f, 40.0f);
		Particle.m_EndSize = 0.0f;
		Particle.m_Rot = random_angle();
		Particle.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
		Particle.m_Gravity = 800.0f;
		Particle.m_Friction = 0.8f;
		Particle.m_Color = Color.Multiply(random_float(0.75f, 1.0f));
		Particle.m_StartAlpha = 1.0f;
		GameClient()->m_Particles.Add(CParticles::GROUP_GENERAL, &Particle);
	}

	return true;
}

void CPhysicBalls::PruneDeadBalls()
{
	const auto It = std::remove_if(m_vBalls.begin(), m_vBalls.end(), [](const CBall &Ball) { return Ball.m_Dead; });
	if(It == m_vBalls.end())
		return;

	m_vBalls.erase(It, m_vBalls.end());
	WakeAll();
}

void CPhysicBalls::WakeAll()
{
	for(CBall &Ball : m_vBalls)
		Ball.WakeUp();
}

void CPhysicBalls::UpdateSleepState(CBall *pBall, float Dt) const
{
	if(Dt <= 0.0f)
		return;

	if(!pBall->m_Grounded && !pBall->m_Supported)
	{
		pBall->m_RestTime = 0.0f;
		return;
	}

	if(length(pBall->m_Pos - pBall->m_PrevPos) > PhysicBallRestSpeed * Dt)
	{
		pBall->m_RestTime = 0.0f;
		return;
	}

	pBall->m_RestTime += Dt;
	if(pBall->m_RestTime < PhysicBallRestDuration)
		return;

	pBall->m_Asleep = true;
	pBall->m_Vel = vec2(0.0f, 0.0f);
	pBall->m_AngularVelocity = 0.0f;
}

void CPhysicBalls::DoBallPhysics(CBall *pBall, float Dt, float Elasticity)
{
	const int CurrentIndex = Collision()->GetMapIndex(pBall->m_Pos);
	pBall->m_TuneZone = Collision()->IsTune(CurrentIndex);

	const float DtTicks = Dt * (float)SERVER_TICK_SPEED;

	DoWeaponFireEffects(pBall, DtTicks);
	DoMapCollisions(pBall, DtTicks, Elasticity);

	pBall->m_Vel.y += m_Gravity * DtTicks;

	pBall->m_Vel.x *= pBall->m_Grounded ? m_GroundFriction : m_AirFriction;

	if(dot(pBall->m_MapContactNormal, pBall->m_MapContactNormal) > 0.0f && DtTicks > 0.0f)
	{
		// The tangent turns ground, ceiling, and wall travel into rolling spin.
		const vec2 Normal = pBall->m_MapContactNormal;
		const vec2 Tangent(-Normal.y, Normal.x);
		const float Travel = dot(pBall->m_Pos - pBall->m_PrevPos, Tangent);
		pBall->m_AngularVelocity = std::clamp(Travel / (pBall->RenderRadius() * DtTicks), -PhysicBallMaxAngularSpeed, PhysicBallMaxAngularSpeed);
	}
	else
	{
		pBall->m_AngularVelocity *= m_AirAngularDamping;
	}
	pBall->m_Rotation += pBall->m_AngularVelocity * DtTicks;

	if(pBall->m_Rotation > 2.0f * pi || pBall->m_Rotation < -2.0f * pi)
		pBall->m_Rotation = std::fmod(pBall->m_Rotation, 2.0f * pi);
}

void CPhysicBalls::Update(float Dt)
{
	if(m_vBalls.empty())
	{
		m_LastObservedDummy = g_Config.m_ClDummy;
		m_LastObservedFire = GameClient()->m_Controls.m_aInputData[m_LastObservedDummy].m_Fire;
		m_FireCounterInitialized = true;
		return;
	}

	const float Elasticity = 0.66f;

	UpdateStepState();

	const float DtTicks = Dt * (float)SERVER_TICK_SPEED;
	m_GroundFriction = std::pow(0.96f, DtTicks);
	m_AirFriction = std::pow(0.98f, DtTicks);
	m_AirAngularDamping = std::pow(0.99f, DtTicks);

	for(CBall &Ball : m_vBalls)
	{
		Ball.m_PrevPos = Ball.m_Pos;
		Ball.m_Supported = false;
		WakeForWeaponFire(Ball);
	}

	bool AnyAwake = false;
	for(CBall &Ball : m_vBalls)
	{
		DoPlayerCollisions(&Ball, Elasticity);

		if(!Ball.m_Asleep)
		{
			DoBallPhysics(&Ball, Dt, Elasticity);
			AnyAwake = true;
		}
	}

	// Resolve the positions produced by this step before they are rendered.
	if(AnyAwake)
		DoBallCollisions(Elasticity);

	if(g_Config.m_ClPhysicBallsKillBorder)
	{
		const float MaxX = (float)Collision()->GetWidth() * 32.0f + PhysicBallKillMargin;
		const float MaxY = (float)Collision()->GetHeight() * 32.0f + PhysicBallKillMargin;

		for(CBall &Ball : m_vBalls)
		{
			if(!Ball.m_Asleep)
				UpdateSleepState(&Ball, Dt);

			// Written inverted so that a non finite position is dropped as well.
			if(!(Ball.m_Pos.x > -PhysicBallKillMargin && Ball.m_Pos.x < MaxX &&
				   Ball.m_Pos.y > -PhysicBallKillMargin && Ball.m_Pos.y < MaxY))
				KillBall(Ball);
		}
	}

	PruneDeadBalls();
}

void CPhysicBalls::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const int64_t Now = time();
	if(m_LastPhysicsTime == 0)
	{
		m_LastPhysicsTime = Now;
	}
	else if(Now > m_LastPhysicsTime)
	{
		const int64_t Delta = Now - m_LastPhysicsTime;
		m_LastPhysicsTime = Now;

		const float Dt = std::clamp((float)Delta / (float)time_freq(), 0.0f, 1.0f / 20.0f); // max 50 ms
		Update(Dt);
	}

	RenderBalls();
}

bool CPhysicBalls::GetNearestAirPos(vec2 Pos, vec2 *pOutPos, float BallSize) const
{
	const float Radius = BallSize * PhysicBallRadiusScale;

	if(!TestBox(Pos, Radius))
	{
		*pOutPos = Pos;
		return true;
	}

	static constexpr int SearchRadius = 12;
	static constexpr float Step = 16.0f;

	float BestDistSq = std::numeric_limits<float>::max();
	vec2 BestPos = Pos;

	for(int Ring = 1; Ring <= SearchRadius; Ring++)
	{
		for(int y = -Ring; y <= Ring; y++)
		{
			for(int x = -Ring; x <= Ring; x++)
			{
				if(std::max(std::abs(x), std::abs(y)) != Ring)
					continue;

				const vec2 Candidate = Pos + vec2(x * Step, y * Step);
				if(TestBox(Candidate, Radius))
					continue;

				const vec2 Delta = Candidate - Pos;
				const float DistSq = dot(Delta, Delta);
				if(DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestPos = Candidate;
				}
			}
		}

		if(BestDistSq < std::numeric_limits<float>::max())
		{
			*pOutPos = BestPos;
			return true;
		}
	}

	return false;
}

bool CPhysicBalls::HoldingHook() const
{
	return GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Hook == 1;
}

bool CPhysicBalls::HoldingFire() const
{
	return GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire % 2 != 0;
}

int CPhysicBalls::CurrentWeapon() const
{
	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId == -1)
		return -1;
	return GameClient()->m_aClients[LocalId].m_Predicted.m_ActiveWeapon;
}

bool CPhysicBalls::TestBox(vec2 Pos, float HalfExtent) const
{
	const CCollision *pCollision = Collision();
	const int Width = pCollision->GetWidth();
	const int MaxX = Width - 1;
	const int MaxY = pCollision->GetHeight() - 1;
	const CTile *pTiles = pCollision->GameLayer();
	if(MaxX < 0 || MaxY < 0 || !pTiles)
		return false;

	const int TileX0 = std::clamp(round_to_int(Pos.x - HalfExtent) / 32, 0, MaxX);
	const int TileX1 = std::clamp(round_to_int(Pos.x + HalfExtent) / 32, 0, MaxX);
	const int TileY0 = std::clamp(round_to_int(Pos.y - HalfExtent) / 32, 0, MaxY);
	const int TileY1 = std::clamp(round_to_int(Pos.y + HalfExtent) / 32, 0, MaxY);

	for(int y = TileY0; y <= TileY1; y++)
	{
		const CTile *pRow = pTiles + y * Width;
		for(int x = TileX0; x <= TileX1; x++)
		{
			const int Tile = pRow[x].m_Index;
			if(Tile == TILE_SOLID || Tile == TILE_NOHOOK)
				return true;
		}
	}

	return false;
}

void CPhysicBalls::OnExplosion(vec2 Pos, bool SameTeam)
{
	if(!SameTeam)
		return;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	constexpr float Radius = 200.0f;
	constexpr float InnerRadius = 64.0f;
	constexpr float Strength = 12.0f;

	for(CBall &Ball : m_vBalls)
	{
		const vec2 Diff = Ball.m_Pos - Pos;
		const float DistSq = dot(Diff, Diff);
		if(DistSq <= 0.0001f * 0.0001f || DistSq > Radius * Radius)
			continue;

		const float Dist = std::sqrt(DistSq);
		const vec2 Dir = Diff / Dist;
		const float Falloff = 1.0f - std::clamp((Dist - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);

		Ball.m_Vel += Dir * (Strength * Falloff);
		Ball.WakeUp();
	}
}
