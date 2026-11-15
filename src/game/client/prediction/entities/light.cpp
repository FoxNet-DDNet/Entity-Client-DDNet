/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "light.h"

#include "character.h"

#include <base/math.h>

#include <game/collision.h>
#include <game/mapitems.h>

#include <cmath>
#include <vector>

CLight::CLight(CGameWorld *pGameWorld, int Id, vec2 Pos, float Rotation, int Length, int Layer, int Number,
	float AngularSpeed, int Speed, int CurveLength) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHT)
{
	m_Id = Id;
	m_To = vec2(0.0f, 0.0f);
	m_Core = vec2(0.0f, 0.0f);
	m_Layer = Layer;
	m_Number = Number;
	m_Tick = (int)(GameWorld()->GameTickSpeed() * 0.15f);
	m_Pos = Pos;
	m_Rotation = Rotation;
	m_Length = Length;
	m_EvalTick = GameWorld()->GameTick();

	// The server leaves these for whoever built the light to fill in afterwards and steps it before
	// they are set, so its first step reads them uninitialized. They are taken up front here
	// instead, which is both defined and one step closer to where the beam really is.
	m_AngularSpeed = AngularSpeed;
	m_Speed = Speed;
	m_CurveLength = CurveLength;
	m_LengthL = 0;

	Step();
}

bool CLight::HitCharacter()
{
	std::vector<CCharacter *> vpHitCharacters = GameWorld()->IntersectedCharacters(m_Pos, m_To, 0.0f, nullptr);
	if(vpHitCharacters.empty())
		return false;
	for(CCharacter *pChar : vpHitCharacters)
	{
		if(m_Layer == LAYER_SWITCH &&
			m_Number > 0 &&
			m_Number < (int)Switchers().size() &&
			!Switchers()[m_Number].m_aStatus[pChar->Team()])
			continue;
		pChar->Freeze();
	}
	return true;
}

void CLight::Move()
{
	if(m_Speed != 0)
	{
		if((m_CurveLength >= m_Length && m_Speed > 0) || (m_CurveLength <= 0 && m_Speed < 0))
			m_Speed = -m_Speed;
		m_CurveLength += m_Speed * m_Tick + m_LengthL;
		m_LengthL = 0;
		if(m_CurveLength > m_Length)
		{
			m_LengthL = m_CurveLength - m_Length;
			m_CurveLength = m_Length;
		}
		else if(m_CurveLength < 0)
		{
			m_LengthL = 0 + m_CurveLength;
			m_CurveLength = 0;
		}
	}

	m_Rotation += m_AngularSpeed * m_Tick;
	if(m_Rotation > pi * 2)
		m_Rotation -= pi * 2;
	else if(m_Rotation < 0)
		m_Rotation += pi * 2;
}

void CLight::Step()
{
	Move();
	const vec2 Direction = vec2(std::sin(m_Rotation), std::cos(m_Rotation));
	const vec2 NextPosition = m_Pos + normalize(Direction) * m_CurveLength;
	Collision()->IntersectNoLaser(m_Pos, NextPosition, &m_To, nullptr);
}

void CLight::Tick()
{
	if(GameWorld()->GameTick() % (int)(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		m_EvalTick = GameWorld()->GameTick();
		Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
		Step();
	}

	HitCharacter();
}
