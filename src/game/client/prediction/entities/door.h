/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H
#define GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H

#include <game/client/prediction/entity.h>

class CLaserData;

class CDoor : public CEntity
{
	vec2 m_To;
	vec2 m_Direction;
	int m_Length;
	bool m_Active;

	int MapSide() const;
	int MapLength(int Side);

public:
	CDoor(CGameWorld *pGameWorld, int Id, const CLaserData *pData);
	void ResetCollision();
	bool Match(const CDoor *pDoor) const;
	void Read(const CLaserData *pData);
	// EClient: where the beam ends, which is what the server puts on the wire and what gets drawn.
	// Not the same as the span the collision is laid down along, see ResetCollision.
	vec2 To() const { return m_To; }

	void Destroy() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_DOOR_H
