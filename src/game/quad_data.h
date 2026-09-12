#ifndef GAME_QUAD_DATA_H
#define GAME_QUAD_DATA_H

#include <base/math.h>
#include <base/vmath.h>

#include <game/mapitems.h>

#include <algorithm>

class CQuadData
{
public:
	vec2 m_aPoints[4] = {vec2(0, 0)}; // the corners in outline order, where they stand on the current tick
	vec2 m_aLocalPoints[5] = {vec2(0, 0)}; // 4 corners in outline order + the pivot, as the map defines them

	vec2 m_Pivot = vec2(0, 0);
	vec2 m_PrevPivot = vec2(0, 0);
	float m_Angle = 0.0f;
	float m_PrevAngle = 0.0f;

	vec2 m_AabbMin = vec2(0, 0);
	vec2 m_AabbMax = vec2(0, 0);

	int m_PosEnv = -1;
	int m_PosEnvOffset = 0;

	/*
	 * Whether the quad was put somewhere else this tick rather than having travelled there. An
	 * envelope that loops back to its start, or steps from one point to the next, moves it
	 * without any speed behind it, and the tick's delta is then the whole jump. Kept identical to
	 * the server, see CQuadData::UpdatePositionEnvelope.
	 */
	bool m_Teleported = true;

	void Init(const CQuad &Quad)
	{
		for(int i = 0; i < 5; i++)
			m_aLocalPoints[i] = vec2(fx2f(Quad.m_aPoints[i].x), fx2f(Quad.m_aPoints[i].y));
		// the map stores the corners row by row, walking the outline needs them in order
		std::swap(m_aLocalPoints[2], m_aLocalPoints[3]);

		// A mirrored quad winds the other way, which flips the edge normals QuadZonePush pushes
		// along, so put the ring back the way that code assumes. Shoelace over the corners:
		// positive means vec2(-Edge.y, Edge.x) points into the quad.
		float Area = 0.0f;
		for(int i = 0, j = 3; i < 4; j = i++)
			Area += m_aLocalPoints[j].x * m_aLocalPoints[i].y - m_aLocalPoints[i].x * m_aLocalPoints[j].y;
		if(Area < 0.0f)
			std::swap(m_aLocalPoints[1], m_aLocalPoints[3]);

		for(int i = 0; i < 4; i++)
			m_aPoints[i] = m_aLocalPoints[i];
		m_Pivot = m_aLocalPoints[4];
		m_PrevPivot = m_Pivot;

		m_PosEnv = Quad.m_PosEnv;
		m_PosEnvOffset = Quad.m_PosEnvOffset;

		UpdateAabb();
	}

	void UpdateAabb()
	{
		m_AabbMin = m_aPoints[0];
		m_AabbMax = m_aPoints[0];
		for(int i = 1; i < 4; i++)
		{
			m_AabbMin.x = std::min(m_AabbMin.x, m_aPoints[i].x);
			m_AabbMin.y = std::min(m_AabbMin.y, m_aPoints[i].y);
			m_AabbMax.x = std::max(m_AabbMax.x, m_aPoints[i].x);
			m_AabbMax.y = std::max(m_AabbMax.y, m_aPoints[i].y);
		}
	}

	vec2 MotionAt(vec2 Pos) const
	{
		// A jump is not motion, so there is no speed to hand over
		if(m_Teleported)
			return vec2(0.0f, 0.0f);

		// A quad that does not turn moves every point of itself by the same amount, and saying
		// so here keeps four trig calls out of a probe that runs per quad per collision step
		if(m_Angle == 0.0f && m_PrevAngle == 0.0f)
			return m_Pivot - m_PrevPivot;

		const vec2 Local = RotateVec(Pos - m_PrevPivot, -m_PrevAngle);
		return (m_Pivot + RotateVec(Local, m_Angle)) - Pos;
	}

	bool AabbContains(vec2 Pos) const
	{
		return Pos.x >= m_AabbMin.x && Pos.x <= m_AabbMax.x && Pos.y >= m_AabbMin.y && Pos.y <= m_AabbMax.y;
	}
	bool AabbIntersects(vec2 Pos, vec2 Size) const
	{
		return Pos.x + Size.x >= m_AabbMin.x && Pos.x - Size.x <= m_AabbMax.x &&
		       Pos.y + Size.y >= m_AabbMin.y && Pos.y - Size.y <= m_AabbMax.y;
	}

	bool Contains(vec2 Pos) const
	{
		return AabbContains(Pos) && InsideQuadrilateral(Pos, m_aPoints);
	}
	// A box counts as inside once any of its corners is, with a zero size all four are Pos
	bool Overlaps(vec2 Pos, vec2 Size) const
	{
		return AabbIntersects(Pos, Size) && InsideQuadrilateral(Pos, m_aPoints, Size);
	}
};

#endif // GAME_QUAD_DATA_H
