#include "map_overview.h"

#include <base/color.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

constexpr float TileSize = 32.0f;
constexpr int MaxSolidSearchDist = 55;
constexpr int IgnoreNewestPoints = 6;

void CMapOverview::CCoverage::Reset(bool Dirty)
{
	m_Dirty = Dirty;

	m_MinX = 0;
	m_MinY = 0;
	m_MaxX = -1;
	m_MaxY = -1;
	m_Width = 0;
	m_Height = 0;

	m_vMask.clear();
}

void CMapOverview::CCoverage::Rebuild(const std::vector<CPoint> &vPoints)
{
	if(!m_Dirty)
		return;

	if(vPoints.empty())
	{
		Reset(false);
		return;
	}

	const CPoint &First = vPoints.front();
	int MinX = First.m_Pos.x - First.m_TileDist;
	int MinY = First.m_Pos.y - First.m_TileDist;
	int MaxX = First.m_Pos.x + First.m_TileDist;
	int MaxY = First.m_Pos.y + First.m_TileDist;

	for(const CPoint &P : vPoints)
	{
		MinX = minimum(MinX, P.m_Pos.x - P.m_TileDist);
		MinY = minimum(MinY, P.m_Pos.y - P.m_TileDist);
		MaxX = maximum(MaxX, P.m_Pos.x + P.m_TileDist);
		MaxY = maximum(MaxY, P.m_Pos.y + P.m_TileDist);
	}

	m_MinX = MinX;
	m_MinY = MinY;
	m_MaxX = MaxX;
	m_MaxY = MaxY;

	m_Width = m_MaxX - m_MinX + 1;
	m_Height = m_MaxY - m_MinY + 1;

	if(m_Width <= 0 || m_Height <= 0)
	{
		m_Width = 0;
		m_Height = 0;
		m_Dirty = false;
		return;
	}

	m_vMask.assign((size_t)m_Width * (size_t)m_Height, 0);

	for(const CPoint &P : vPoints)
	{
		const int Dist = maximum(P.m_TileDist, 0);

		const int X0 = P.m_Pos.x - Dist - m_MinX;
		const int Y0 = P.m_Pos.y - Dist - m_MinY;
		const int X1 = P.m_Pos.x + Dist - m_MinX;
		const int Y1 = P.m_Pos.y + Dist - m_MinY;

		for(int y = Y0; y <= Y1; ++y)
		{
			uint8_t *pRow = &m_vMask[(size_t)y * (size_t)m_Width];
			for(int x = X0; x <= X1; ++x)
			{
				pRow[x] = 1;
			}
		}
	}

	m_Dirty = false;
}

void CMapOverview::OnRender()
{
	if(!g_Config.m_ClMapOverview)
		return;

	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId == -1)
		return;

	if(!GameClient()->StartedRace())
	{
		Reset();
		return;
	}

	const vec2 Pos = mix(vec2(GameClient()->m_aClients[LocalId].m_RenderPrev.m_X, GameClient()->m_aClients[LocalId].m_RenderPrev.m_Y), vec2(GameClient()->m_aClients[LocalId].m_RenderCur.m_X, GameClient()->m_aClients[LocalId].m_RenderCur.m_Y), 0);

	bool AddedPoint = AddPoint(Pos);

	if(AddedPoint && m_vPoints.size() > (size_t)IgnoreNewestPoints)
		m_Coverage.m_Dirty = true;

	std::vector<CPoint> vRenderPoints;
	if(m_vPoints.size() > (size_t)IgnoreNewestPoints)
		vRenderPoints.assign(m_vPoints.begin(), m_vPoints.end() - IgnoreNewestPoints);

	m_Coverage.Rebuild(vRenderPoints);

	if(g_Config.m_ClMapOverviewSpectatingOnly && !GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	Graphics()->TextureClear();

	const ColorRGBA Color(0.0f, 0.0f, 0.0f, g_Config.m_ClMapOverviewOpacity / 100.0f);

	if(!m_Coverage.m_vMask.empty())
	{
		Graphics()->QuadsBegin();
		Graphics()->SetColor(Color);

		for(int y = 0; y < m_Coverage.m_Height; ++y)
		{
			const uint8_t *pRow = m_Coverage.RowPtr(y);

			int x = 0;
			while(x < m_Coverage.m_Width)
			{
				while(x < m_Coverage.m_Width && pRow[x] == 0)
					++x;
				if(x >= m_Coverage.m_Width)
					break;

				const int StartX = x;
				while(x < m_Coverage.m_Width && pRow[x] != 0)
					++x;

				const int EndX = x;

				const float WorldX = (m_Coverage.m_MinX + StartX) * TileSize;
				const float WorldY = (m_Coverage.m_MinY + y) * TileSize;
				const float WorldW = (EndX - StartX) * TileSize;
				const float WorldH = TileSize;

				const IGraphics::CQuadItem Span(WorldX, WorldY, WorldW, WorldH);
				Graphics()->QuadsDrawTL(&Span, 1);
			}
		}

		Graphics()->QuadsEnd();
	}
}

void CMapOverview::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState)
		Reset();
}

void CMapOverview::Reset()
{
	m_vPoints.clear();
	m_Coverage.Reset(false);
}

vec2 CMapOverview::GetCloseSolidTile(vec2 Pos, int MaxDistance) const
{
	const CCollision *pCollision = GameClient()->Collision();
	ivec2 Center = ivec2(Pos.x / 32, Pos.y / 32);
	for(int r = 0; r <= MaxDistance; r++)
	{
		for(int y = -r; y <= r; y++)
		{
			for(int x = -r; x <= r; x++)
			{
				if(abs(x) != r && abs(y) != r)
					continue;
				ivec2 CheckPos = Center + ivec2(x, y);

				if(pCollision->CheckPoint(CheckPos.x * 32 + 16, CheckPos.y * 32 + 16))
					return vec2(CheckPos.x * 32 + 16, CheckPos.y * 32 + 16);
			}
		}
	}
	return vec2(-1, -1);
}

bool CMapOverview::AddPoint(vec2 Pos)
{
	const float Size = 32.0f;
	ivec2 IdxPos = ivec2((Pos.x + 16) / Size, (Pos.y + 16) / Size);

	vec2 CloseSolid = (Pos - GetCloseSolidTile(Pos, MaxSolidSearchDist));
	CloseSolid = vec2(std::abs(CloseSolid.x), std::abs(CloseSolid.y));

	int TileDistance = CloseSolid.x > CloseSolid.y ? int(CloseSolid.x / Size) : int(CloseSolid.y / Size);
	TileDistance = std::clamp(TileDistance, 4, MaxSolidSearchDist) + 4; // Paddiing

	for(CPoint &p : m_vPoints)
	{
		if(IdxPos.x + p.m_TileDist > p.m_Pos.x &&
			IdxPos.x - p.m_TileDist < p.m_Pos.x &&
			IdxPos.y + p.m_TileDist > p.m_Pos.y &&
			IdxPos.y - p.m_TileDist < p.m_Pos.y)
			return false;
	}

	CPoint NewPoint;
	NewPoint.m_Pos = IdxPos;
	NewPoint.m_TileDist = TileDistance;

	m_vPoints.push_back(NewPoint);
	return true;
}
