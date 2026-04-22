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

constexpr float PhysicBallSize = 60.0f;

void CPhysicBalls::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
		Reset();
}

void CPhysicBalls::Reset()
{
	m_vBalls.clear();
	m_LastPhysicsTime = 0;
}

void CPhysicBalls::OnConsoleInit()
{
	// Collision breaks if size is changed
	Console()->Register("physic_ball_new", "?f[size]", CFGFLAG_CLIENT, ConNewPhysicBall, this, "Summon a new physic ball");
	Console()->Register("physic_ball_new_cursor", "?f[size]", CFGFLAG_CLIENT, ConNewPhysicBallAtCursor, this, "Summon a new physic ball at the cursor");
	Console()->Register("physic_balls_remove_cursor", "?f[radius]", CFGFLAG_CLIENT, ConRemovePhysicBallsAtCursor, this, "Removes ball at cursor");
	Console()->Register("physic_balls_reset", "", CFGFLAG_CLIENT, ConResetPhysicBalls, this, "Reset all physic balls");
}

void CPhysicBalls::NewBallPlayer(float Size)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	vec2 Pos = PlayerPos(Size);

	m_vBalls.emplace_back(Pos, vec2(), Size);
}

void CPhysicBalls::NewBallCursor(float Size)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	vec2 Pos = GameClient()->GetCursorWorldPos();
	vec2 OutPos;
	if(GetNearestAirPos(Pos, Pos, &OutPos, Size))
		Pos = OutPos;

	m_vBalls.emplace_back(Pos, vec2(), Size);
}

void CPhysicBalls::ConNewPhysicBall(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);
	float Size = pResult->NumArguments() > 0 ? pResult->GetFloat(0) : PhysicBallSize;

	pSelf->NewBallPlayer(Size);
}

void CPhysicBalls::ConNewPhysicBallAtCursor(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);

	float Size = pResult->NumArguments() > 0 ? pResult->GetFloat(0) : PhysicBallSize;

	pSelf->NewBallCursor(Size);
}

void CPhysicBalls::ConRemovePhysicBallsAtCursor(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);

	if(pSelf->Client()->State() != IClient::STATE_ONLINE)
		return;

	const float Radius = pResult->NumArguments() > 0 ? pResult->GetFloat(0) : 20.0f;
	const vec2 CursorPos = pSelf->GameClient()->GetCursorWorldPos();

	for(int i = (int)pSelf->m_vBalls.size() - 1; i >= 0; --i)
	{
		const CBall &Ball = pSelf->m_vBalls[i];
		const vec2 Delta = Ball.m_Pos - CursorPos;
		const float Distance = length(Delta);
		const float BallRadius = Ball.m_Size * 0.5f;

		if(Distance < Radius + BallRadius)
			pSelf->KillBall(&pSelf->m_vBalls[i]);
	}
}

void CPhysicBalls::ConResetPhysicBalls(IConsole::IResult *pResult, void *pUserData)
{
	CPhysicBalls *pSelf = static_cast<CPhysicBalls *>(pUserData);
	pSelf->Reset();
}

vec2 CPhysicBalls::PlayerPos(float BallSize) const
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

	vec2 Pos = GetPos() - vec2(0, 2.0f);
	vec2 OutPos;
	if(GetNearestAirPos(Pos, Pos, &OutPos, BallSize))
		return OutPos;
	return Pos;
}

bool CPhysicBalls::IsBallVisible(const CBall *pBall)
{
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	const float HalfSize = pBall->m_Size * 0.75f;

	return pBall->m_Pos.x + HalfSize >= ScreenX0 &&
	       pBall->m_Pos.x - HalfSize <= ScreenX1 &&
	       pBall->m_Pos.y + HalfSize >= ScreenY0 &&
	       pBall->m_Pos.y - HalfSize <= ScreenY1;
}

void CPhysicBalls::RenderBall(const CBall *pBall)
{
	const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_ClPhysicBallsSkin);
	if(!pSkin)
		return;

	// Render at current position (variable timestep physics updates per frame).
	vec2 Position = pBall->m_Pos;

	float Alpha = 1.0f;

	const float Size = pBall->m_Size;
	Graphics()->TextureSet(pSkin->m_OriginalSkin.m_BodyOutline);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	IEngineGraphics::CQuadItem QuadOutline{Position.x, Position.y, Size, Size};
	Graphics()->QuadsSetRotation(pBall->m_Rotation);
	Graphics()->QuadsDraw(&QuadOutline, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureSet(pSkin->m_OriginalSkin.m_Body);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	Graphics()->QuadsSetRotation(pBall->m_Rotation);
	IEngineGraphics::CQuadItem Quad{Position.x, Position.y, Size, Size};
	Graphics()->QuadsDraw(&Quad, 1);
	Graphics()->QuadsEnd();
}

void CPhysicBalls::DoPlayerCollisions(CBall *pBall, float Dt, float Elasticity) const
{
	const float BallRadius = pBall->m_Size * 0.5f;
	const float SeparationPadding = 0.5f;

	int LocalId = GameClient()->m_Snap.m_LocalClientId;
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

		vec2 PlayerPos = Client.m_RenderPos;
		vec2 Delta = pBall->m_Pos - PlayerPos;
		float Distance = length(Delta);
		const float CombinedRadius = (BallRadius + CCharacterCore::PhysicalSize() * 0.5f) * 0.75f;

		if(Distance >= CombinedRadius)
			continue;

		vec2 Normal = Distance > 0.0001f ? Delta / Distance : vec2(0.0f, -1.0f);
		const float Penetration = CombinedRadius - Distance + SeparationPadding;

		vec2 NewPos = pBall->m_Pos + Normal * Penetration;

		if(TestBox(vec2(NewPos.x, pBall->m_Pos.y), BallRadius))
			NewPos.x = pBall->m_Pos.x;
		if(TestBox(vec2(pBall->m_Pos.x, NewPos.y), BallRadius))
			NewPos.y = pBall->m_Pos.y;

		const vec2 RelativeVel = pBall->m_Vel - Client.m_Predicted.m_Vel;
		const float RelativeNormalVel = dot(RelativeVel, Normal);
		if(RelativeNormalVel < 0.0f)
			pBall->m_Vel -= Normal * ((1.0f + Elasticity) * RelativeNormalVel);

		pBall->m_Pos = NewPos;
	}
}

void CPhysicBalls::DoBallCollisions(CBall *pBall, float Dt, float Elasticity)
{
	const float BallRadius = pBall->m_Size * 0.55f;
	const float SeparationPadding = 0.5f;

	for(auto &OtherBall : m_vBalls)
	{
		if(&OtherBall == pBall)
			continue;

		// Process each pair once and update both balls.
		if(&OtherBall < pBall)
			continue;

		const float OtherRadius = OtherBall.m_Size * 0.55f;
		vec2 Delta = pBall->m_Pos - OtherBall.m_Pos;
		float Distance = length(Delta);
		const float CombinedRadius = (BallRadius + OtherRadius) * 0.5f;

		if(Distance >= CombinedRadius)
			continue;

		vec2 Normal = Distance > 0.0001f ? Delta / Distance : vec2(0.0f, -1.0f);
		const float Penetration = CombinedRadius - Distance + SeparationPadding;

		const float BallMass = pBall->m_Size;
		const float OtherMass = OtherBall.m_Size;
		const float MassSum = BallMass + OtherMass;

		const vec2 Correction = Normal * Penetration;
		vec2 NewPos = pBall->m_Pos + Correction * (OtherMass / MassSum);
		vec2 NewOtherPos = OtherBall.m_Pos - Correction * (BallMass / MassSum);

		if(TestBox(vec2(NewPos.x, pBall->m_Pos.y), BallRadius))
			NewPos.x = pBall->m_Pos.x;
		if(TestBox(vec2(pBall->m_Pos.x, NewPos.y), BallRadius))
			NewPos.y = pBall->m_Pos.y;

		if(TestBox(vec2(NewOtherPos.x, OtherBall.m_Pos.y), OtherRadius))
			NewOtherPos.x = OtherBall.m_Pos.x;
		if(TestBox(vec2(OtherBall.m_Pos.x, NewOtherPos.y), OtherRadius))
			NewOtherPos.y = OtherBall.m_Pos.y;

		pBall->m_Pos = NewPos;
		OtherBall.m_Pos = NewOtherPos;

		const vec2 RelativeVel = pBall->m_Vel - OtherBall.m_Vel;
		const float RelativeNormalVel = dot(RelativeVel, Normal);
		if(RelativeNormalVel < 0.0f)
		{
			const vec2 Impulse = Normal * ((1.0f + Elasticity) * RelativeNormalVel);
			pBall->m_Vel -= Impulse * (OtherMass / MassSum);
			OtherBall.m_Vel += Impulse * (BallMass / MassSum);
		}
	}
}
void CPhysicBalls::DoMapCollisions(CBall *pBall, float Dt, float Elasticity) const
{
	const vec2 StartPos = pBall->m_PrevPos;
	const vec2 TargetPos = pBall->m_Pos + pBall->m_Vel * Dt;
	const vec2 FullDelta = TargetPos - StartPos;
	const float Distance = length(FullDelta);
	const int Steps = Distance > 0.0f ? std::clamp((int)Distance, 1, 64) : 1; // cap substeps to avoid huge loops

	bool Grounded = false;

	vec2 Pos = StartPos;
	vec2 Vel = pBall->m_Vel;

	const float BallSize = pBall->m_Size * 0.5f;

	for(int i = 0; i < Steps; i++)
	{
		vec2 NextPos = StartPos + FullDelta * ((float)(i + 1) / (float)Steps);

		if(NextPos == Pos)
			break;

		if(TestBox(vec2(NextPos.x, NextPos.y), BallSize))
		{
			int Hits = 0;

			// Y axis
			if(TestBox(vec2(Pos.x, NextPos.y), BallSize))
			{
				if(Vel.y > 0)
					Grounded = true;
				NextPos.y = Pos.y;
				Vel.y *= -Elasticity;
				Hits++;
			}

			// X axis
			if(TestBox(vec2(NextPos.x, Pos.y), BallSize))
			{
				NextPos.x = Pos.x;
				Vel.x *= -Elasticity;
				Hits++;
			}

			// Corner hit
			if(Hits == 0)
			{
				if(Vel.y > 0)
					Grounded = true;
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
}
void CPhysicBalls::DoWeaponFireEffects(CBall *pBall, float Dt) const
{
	bool Holding = HoldingFire();

	int Weapon = CurrentWeapon();
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		Weapon = WEAPON_GUN;
	}
	if(!Holding)
		return;
	if(Weapon == -1)
		return;

	if(Weapon == WEAPON_GUN)
	{
		vec2 CursorPos = GameClient()->GetCursorWorldPos();
		vec2 ToCursor = CursorPos - pBall->m_Pos;
		vec2 ForceDir = normalize(ToCursor);

		float ForceStrength = 2.0f;
		pBall->m_Vel += (ForceDir * (ForceStrength * Dt));
	}
	else if(Weapon == WEAPON_HAMMER && PressedFire())
	{
		const int LocalId = GameClient()->m_Snap.m_LocalClientId;
		if(LocalId < 0 || !GameClient()->m_Snap.m_apPlayerInfos[LocalId])
			return;

		const vec2 InputDir = normalize(vec2(
			(float)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x,
			(float)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y));

		const vec2 LocalPos = GameClient()->m_aClients[LocalId].m_Predicted.m_Pos;
		const vec2 HitPos = LocalPos + InputDir * CCharacterCore::PhysicalSize();
		const float Strength = GameClient()->m_aClients[LocalId].m_Predicted.m_Tuning.m_HammerStrength;

		vec2 Delta = pBall->m_Pos - HitPos;
		float Distance = length(Delta);
		const float CombinedRadius = (pBall->m_Size * 0.5f + CCharacterCore::PhysicalSize() * 0.5f) * 0.75f;

		if(Distance < CombinedRadius)
		{
			vec2 Temp = pBall->m_Vel + normalize(InputDir + vec2(0.f, -0.3f)) * 8.0f;
			Temp = ClampVel(0, Temp);
			Temp -= pBall->m_Vel;
			Temp += (vec2(0.f, -1.0f) + Temp) * Strength;
			pBall->m_Vel = ClampVel(0, Temp);
		}
	}
}

bool CPhysicBalls::KillBall(const CBall *pBall)
{
	if(!pBall)
		return false;

	auto It = std::find_if(m_vBalls.begin(), m_vBalls.end(), [&](const CBall &b) { return &b == pBall; });
	if(It != m_vBalls.end())
	{
		for(int i = 0; i < 16; i++)
		{
			const ColorRGBA Color = ColorRGBA(random_float(1.0f), random_float(1.0f), random_float(1.0f), 1.0f);

			CParticle Particle;
			Particle.SetDefault();
			Particle.m_Spr = SPRITE_PART_SPLAT01 + (rand() % 3);
			Particle.m_Pos = pBall->m_Pos;
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

		m_vBalls.erase(It);
		return true;
	}
	return false;
}

void CPhysicBalls::DoBallPhysics(CBall *pBall, float DtTicks)
{
	pBall->m_PrevPos = pBall->m_Pos;

	const int LocalId = GameClient()->m_Snap.m_LocalClientId;

	int CurrentIndex = Collision()->GetMapIndex(pBall->m_Pos);
	pBall->m_TuneZone = Collision()->IsTune(CurrentIndex);

	float Elasticity = 0.66f;

	DoBallCollisions(pBall, DtTicks, Elasticity);
	DoPlayerCollisions(pBall, DtTicks, Elasticity);

	DtTicks *= (float)SERVER_TICK_SPEED;

	DoWeaponFireEffects(pBall, DtTicks);
	DoMapCollisions(pBall, DtTicks, Elasticity);

	pBall->m_Vel.y += GameClient()->m_aClients[LocalId].m_Predicted.m_Tuning.m_Gravity * DtTicks;

	float GroundFriction = 0.96f;
	float AirFriction = 0.98f;

	float Friction = pBall->m_Grounded ? GroundFriction : AirFriction;

	pBall->m_Vel.x *= powf(Friction, DtTicks);

	if(pBall->m_Grounded)
	{
		const float Radius = pBall->m_Size * 0.3f;
		if(Radius > 0.0001f)
			pBall->m_Rotation += (pBall->m_Vel.x / Radius) * DtTicks;
	}
	else if(Collision()->CheckPoint(pBall->m_Pos + vec2(0, pBall->m_Size / 2.2f)) && pBall->m_Vel.y < 6.0f)
	{
		const float Radius = pBall->m_Size * 0.5f;
		if(Radius > 0.0001f)
			pBall->m_Rotation += (pBall->m_Vel.x / Radius) * DtTicks;
	}
	else
	{
		const float Radius = pBall->m_Size * 0.7f;
		if(Radius > 0.0001f)
			pBall->m_Rotation += (pBall->m_Vel.x / Radius) * DtTicks;
	}
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
	else
	{
		int64_t Delta = Now - m_LastPhysicsTime;
		if(Delta > 0)
		{
			m_LastPhysicsTime = Now;

			// Convert to seconds, clamp to avoid giant jumps if a frame stalls
			const float Dt = std::clamp((float)Delta / (float)time_freq(), 0.0f, 1.0f / 20.0f); // max 50 ms

			for(auto &Ball : m_vBalls)
			{
				DoBallPhysics(&Ball, Dt);
				if(round_to_int(Ball.m_Pos.y) / 32 > Collision()->GetHeight() + 200)
					KillBall(&Ball);
			}
		}
	}

	for(auto &Ball : m_vBalls)
	{
		// Dont render if ball is outside of view
		if(!IsBallVisible(&Ball))
			continue;

		RenderBall(&Ball);
	}
}

bool CPhysicBalls::GetNearestAirPos(vec2 Pos, vec2 PrevPos, vec2 *pOutPos, float BallSize) const
{
	const float Size = BallSize * 0.5f;

	if(!TestBox(Pos, Size))
	{
		*pOutPos = Pos;
		return true;
	}

	static constexpr int SearchRadius = 12;
	static constexpr float Step = 16.0f;

	float BestDist = std::numeric_limits<float>::max();
	vec2 BestPos = Pos;

	for(int y = -SearchRadius; y <= SearchRadius; y++)
	{
		for(int x = -SearchRadius; x <= SearchRadius; x++)
		{
			vec2 Candidate = Pos + vec2(x * Step, y * Step);
			if(TestBox(Candidate, Size))
				continue;

			float Dist = distance(Pos, Candidate);
			if(Dist < BestDist)
			{
				BestDist = Dist;
				BestPos = Candidate;
			}
		}
	}

	if(BestDist < std::numeric_limits<float>::max())
	{
		*pOutPos = BestPos;
		return true;
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

bool CPhysicBalls::PressedFire() const
{
	int LastFire = GameClient()->m_Controls.m_aLastData[g_Config.m_ClDummy].m_Fire;
	int CurrentFire = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire;
	return (CurrentFire % 2 != 0) && (LastFire % 2 == 0);
}

int CPhysicBalls::CurrentWeapon() const
{
	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId == -1)
		return -1;
	return GameClient()->m_aClients[LocalId].m_Predicted.m_ActiveWeapon;
}

bool CPhysicBalls::TestBox(vec2 Pos, float Size) const
{
	const bool TestBox = Collision()->TestBox(Pos, vec2(Size, Size));

	if(TestBox)
		return true;

	if(Size <= 32.0f)
		return false;

	// For larger sizes, test multiple points in the box to avoid tunneling through thin walls.
	const int NumPoints = 4;
	for(int y = 0; y < NumPoints; y++)
	{
		for(int x = 0; x < NumPoints; x++)
		{
			vec2 Offset = vec2(
				((float)x / (float)(NumPoints - 1) - 0.5f) * Size,
				((float)y / (float)(NumPoints - 1) - 0.5f) * Size);
			if(Collision()->CheckPoint(Pos + Offset))
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

	for(auto &Ball : m_vBalls)
	{
		vec2 Diff = Ball.m_Pos - Pos;
		float Dist = length(Diff);
		if(Dist <= 0.0001f || Dist > Radius)
			continue;

		vec2 Dir = normalize(Diff);
		float Falloff = 1.0f - std::clamp((Dist - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);

		Ball.m_Vel += Dir * (Strength * Falloff);
	}
}
