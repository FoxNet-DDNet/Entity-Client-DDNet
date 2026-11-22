#ifndef GAME_QUAD_DATA_H
#define GAME_QUAD_DATA_H

#include <base/math.h>
#include <base/vmath.h>

#include <game/mapitems.h>

#include <algorithm>
#include <limits>

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

	/*
	 * The narrowest way through this quad: the shortest of the two triangle halves' heights. It
	 * bounds how far apart the collision may sample a path before this quad could be stepped
	 * over, see CCharacter::QuadZoneSweep. Rigid motion does not change it, so it is settled
	 * once at load even for a quad that moves. Kept identical to the server, see CQuadData::Init.
	 */
	float m_Waist = 0.0f;

	// The same bounds over the local corners, so a quad that only shifts can skip rebuilding them
	vec2 m_LocalAabbMin = vec2(0, 0);
	vec2 m_LocalAabbMax = vec2(0, 0);

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

		m_Waist = NarrowestWaist();

		m_LocalAabbMin = m_aLocalPoints[0];
		m_LocalAabbMax = m_aLocalPoints[0];
		for(int i = 1; i < 4; i++)
		{
			m_LocalAabbMin.x = std::min(m_LocalAabbMin.x, m_aLocalPoints[i].x);
			m_LocalAabbMin.y = std::min(m_LocalAabbMin.y, m_aLocalPoints[i].y);
			m_LocalAabbMax.x = std::max(m_LocalAabbMax.x, m_aLocalPoints[i].x);
			m_LocalAabbMax.y = std::max(m_LocalAabbMax.y, m_aLocalPoints[i].y);
		}

		UpdateAabb();
	}

	// Shortest height of either triangle half, which is the least a crossing path has to cover.
	// Split in two because an editor quad can be concave and a triangle never is.
	float NarrowestWaist() const
	{
		static const int s_aaTriangles[2][3] = {{0, 1, 2}, {0, 2, 3}};

		float Waist = std::numeric_limits<float>::max();
		for(const auto &aTriangle : s_aaTriangles)
		{
			const vec2 &A = m_aLocalPoints[aTriangle[0]];
			const vec2 &B = m_aLocalPoints[aTriangle[1]];
			const vec2 &C = m_aLocalPoints[aTriangle[2]];

			const float TwiceArea = absolute((B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x));
			const float Longest = std::max(std::max(distance(A, B), distance(B, C)), distance(C, A));
			if(TwiceArea <= 1e-3f || Longest <= 1e-3f)
				continue; // degenerate, the other half speaks for the quad

			Waist = std::min(Waist, TwiceArea / Longest);
		}

		return Waist == std::numeric_limits<float>::max() ? 0.0f : Waist;
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

	/*
	 * Puts the quad at Offset without turning it. Shifting every corner by the same amount shifts
	 * the bounds by it too, so the min and max come straight off the local ones instead of out of
	 * a pass over the corners, which is what this runs for thousands of quads a tick to avoid.
	 */
	void MoveTo(vec2 Offset)
	{
		for(int i = 0; i < 4; i++)
			m_aPoints[i] = m_aLocalPoints[i] + Offset;

		m_AabbMin = m_LocalAabbMin + Offset;
		m_AabbMax = m_LocalAabbMax + Offset;
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
