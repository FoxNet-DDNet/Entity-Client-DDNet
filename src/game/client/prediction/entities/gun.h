/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_GUN_H
#define GAME_CLIENT_PREDICTION_ENTITIES_GUN_H

#include <game/client/prediction/entity.h>

/**
 * A turret, the client's side of CGun.
 *
 * The server never snaps a turret as anything a client could simulate: the laser object it sends is
 * a marker to draw, with no firing state in it. So this is built out of the map instead, by whoever
 * wants a world that fires plasma of its own, and from then on keeps its own firing clock. Nothing
 * reads it back off the wire, so it has neither Read nor Match.
 */
class CGun : public CEntity
{
	vec2 m_Core;
	bool m_Freeze;
	bool m_Explosive;
	int m_EvalTick;
	// Indexed by team and by client id respectively, the way the server rate limits a turret:
	// per team, and per solo tee regardless of its team
	int m_aLastFireTeam[MAX_CLIENTS];
	int m_aLastFireSolo[MAX_CLIENTS];

	void Fire();

public:
	CGun(CGameWorld *pGameWorld, int Id, vec2 Pos, bool Freeze, bool Explosive, int Layer, int Number);

	bool GetFreeze() const { return m_Freeze; }
	bool GetExplosive() const { return m_Explosive; }
	int GetEvalTick() const { return m_EvalTick; }

	void Tick() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_GUN_H
