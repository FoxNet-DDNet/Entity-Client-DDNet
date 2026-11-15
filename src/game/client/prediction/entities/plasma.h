/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_PLASMA_H
#define GAME_CLIENT_PREDICTION_ENTITIES_PLASMA_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CPlasma : public CEntity
{
	vec2 m_Core;
	bool m_Freeze;
	bool m_Explosive;
	int m_ForClientId;
	int m_EvalTick;
	int m_LifeTime;

	void Move();
	bool HitCharacter(CCharacter *pTarget);
	bool HitObstacle(CCharacter *pTarget);

public:
	CPlasma(CGameWorld *pGameWorld, int Id, const CLaserData *pData);

	bool Match(const CPlasma *pPlasma) const;
	void Read(const CLaserData *pData);
	bool GetFreeze() const { return m_Freeze; }
	bool GetExplosive() const { return m_Explosive; }
	int GetEvalTick() const { return m_EvalTick; }
	int GetForClientId() const { return m_ForClientId; }

	void Reset();
	void Tick() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_PLASMA_H
