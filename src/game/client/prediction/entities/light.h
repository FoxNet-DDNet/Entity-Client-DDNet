/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H
#define GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H

#include <game/client/prediction/entity.h>

/**
 * A rotating freeze laser, the client's side of CLight.
 *
 * Like CGun this is built out of the map rather than read off the wire: the laser object the server
 * snaps carries where the beam currently ends but none of the rotation, length or sweep state the
 * beam has to be swung by, so there is nothing in a snapshot to simulate from. Everything the beam
 * needs comes off the tiles around it, the way the server reads it at map load.
 */
class CLight : public CEntity
{
	float m_Rotation;
	vec2 m_To;
	vec2 m_Core;

	int m_EvalTick;

	// How far the beam is swung on each of the ticks that move it, which is every 0.15s rather than
	// every tick, so the step is that whole stretch at once
	int m_Tick;

	bool HitCharacter();
	void Move();
	void Step();

public:
	int m_CurveLength;
	int m_LengthL;
	float m_AngularSpeed;
	int m_Speed;
	int m_Length;

	CLight(CGameWorld *pGameWorld, int Id, vec2 Pos, float Rotation, int Length, int Layer, int Number,
		float AngularSpeed, int Speed, int CurveLength);

	/**
	 * Where the beam currently ends, which is what the server puts on the wire and so what gets
	 * drawn.
	 */
	vec2 To() const { return m_To; }
	int GetEvalTick() const { return m_EvalTick; }

	void Tick() override;
};

#endif // GAME_CLIENT_PREDICTION_ENTITIES_LIGHT_H
