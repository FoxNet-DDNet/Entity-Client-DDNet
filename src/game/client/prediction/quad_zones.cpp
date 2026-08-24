#include "quad_zones.h"

#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/map.h>
#include <engine/shared/protocol.h>

#include <game/gamecore.h>
#include <game/mapitems.h>

#include <algorithm>
#include <chrono>
#include <iterator>

using namespace std::chrono_literals;

// The layer name the server builds each zone from, see CZoneManager::OnMapLoad
static const char *const gs_apZoneLayerNames[(int)EPredictedZone::Num] = {"QFr", "QUnFr", "QStopa"};

/*
 * Quad layers inside one of these groups belong to that minigame instead of to a zone,
 * whatever the layer is called, see CZoneManager::OnMapLoad. What they do is part of the
 * minigame and is left to the server.
 */
static const char *const gs_apMinigameGroups[] = {"#Roulette", "#MoneyWheel", "#HideNSeek"};

void CQuadZones::Reset()
{
	for(std::vector<CQuadData> &vQuads : m_avQuads)
		vQuads.clear();
	m_pEnvelopePoints = nullptr;
	m_pMap = nullptr;
	m_Active = false;
	m_StopAGivesDj = false;
	m_QuadStartTick = 0;
	m_CachedTick = -1;
}

void CQuadZones::SetActive(bool Active)
{
	if(Active == m_Active)
		return;

	m_Active = Active;
	m_CachedTick = -1;
}

void CQuadZones::SetQuadStartTick(int QuadStartTick)
{
	if(QuadStartTick == m_QuadStartTick)
		return;

	// the quads are somewhere else entirely now, the cached tick says nothing about them anymore
	m_QuadStartTick = QuadStartTick;
	m_CachedTick = -1;
}

void CQuadZones::OnMapLoad(IMap *pMap)
{
	Reset();

	m_pMap = pMap;
	if(!m_pMap)
		return;

	int GroupsStart, GroupsNum, LayersStart, LayersNum;
	m_pMap->GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	m_pMap->GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

	bool AnyQuads = false;
	for(int GroupIndex = 0; GroupIndex < GroupsNum; GroupIndex++)
	{
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(m_pMap->GetItem(GroupsStart + GroupIndex));
		if(!pGroup)
			continue;

		if(pGroup->m_Version >= 3)
		{
			char aGroupName[sizeof(pGroup->m_aName)];
			IntsToStr(pGroup->m_aName, std::size(pGroup->m_aName), aGroupName, std::size(aGroupName));
			const bool Minigame = std::any_of(std::begin(gs_apMinigameGroups), std::end(gs_apMinigameGroups),
				[&](const char *pMinigame) { return !str_comp(pMinigame, aGroupName); });
			if(Minigame)
				continue;
		}

		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			const CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(m_pMap->GetItem(LayersStart + pGroup->m_StartLayer + LayerIndex));
			if(!pLayer || pLayer->m_Type != LAYERTYPE_QUADS)
				continue;

			const CMapItemLayerQuads *pQuadsLayer = reinterpret_cast<const CMapItemLayerQuads *>(pLayer);
			if(pQuadsLayer->m_Version < 2)
				continue;

			char aLayerName[sizeof(pQuadsLayer->m_aName)];
			IntsToStr(pQuadsLayer->m_aName, std::size(pQuadsLayer->m_aName), aLayerName, std::size(aLayerName));

			int Type = 0;
			for(; Type < (int)EPredictedZone::Num; Type++)
			{
				if(!str_comp(gs_apZoneLayerNames[Type], aLayerName))
					break;
			}

			if(Type == (int)EPredictedZone::Num)
				continue;

			const CQuad *pQuads = static_cast<CQuad *>(m_pMap->GetDataSwapped(pQuadsLayer->m_Data));
			if(!pQuads)
				continue;

			std::vector<CQuadData> &vQuads = m_avQuads[Type];
			vQuads.reserve(vQuads.size() + pQuadsLayer->m_NumQuads);

			for(int QuadIndex = 0; QuadIndex < pQuadsLayer->m_NumQuads; QuadIndex++)
			{
				CQuadData ZoneQuad;
				ZoneQuad.Init(pQuads[QuadIndex]);
				vQuads.push_back(ZoneQuad);
				AnyQuads = true;
			}
		}
	}

	if(AnyQuads)
		m_pEnvelopePoints = std::make_unique<CMapBasedEnvelopePointAccess>(m_pMap);
}

void CQuadZones::EvalPosEnvelope(const CQuadData &Quad, std::chrono::nanoseconds Time, vec2 &Offset, float &Rotation) const
{
	int EnvelopesStart, EnvelopesNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopesStart, &EnvelopesNum);
	if(Quad.m_PosEnv >= EnvelopesNum)
		return;

	const CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(m_pMap->GetItem(EnvelopesStart + Quad.m_PosEnv));
	if(!pEnvelope || pEnvelope->m_Channels <= 0)
		return;

	m_pEnvelopePoints->SetPointsRange(pEnvelope->m_StartPoint, pEnvelope->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
		return;

	ColorRGBA Position(0.0f, 0.0f, 0.0f, 0.0f);
	const size_t Channels = std::min({(size_t)3, (size_t)pEnvelope->m_Channels, (size_t)CEnvPoint::MAX_CHANNELS});
	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), Time + std::chrono::milliseconds(Quad.m_PosEnvOffset), Position, Channels);

	Offset = vec2(Position.r, Position.g);
	Rotation = Position.b / 180.0f * pi;
}

void CQuadZones::UpdateQuad(CQuadData &Quad, std::chrono::nanoseconds Time, std::chrono::nanoseconds PrevTime, bool WithMotion) const
{
	vec2 Offset = vec2(0.0f, 0.0f);
	float Rotation = 0.0f;
	if(Quad.m_PosEnv >= 0)
		EvalPosEnvelope(Quad, Time, Offset, Rotation);

	Quad.m_Pivot = Quad.m_aLocalPoints[4] + Offset;
	Quad.m_Angle = Rotation;

	if(WithMotion && Quad.m_PosEnv >= 0)
	{
		vec2 PrevOffset = vec2(0.0f, 0.0f);
		float PrevRotation = 0.0f;
		EvalPosEnvelope(Quad, PrevTime, PrevOffset, PrevRotation);

		Quad.m_PrevPivot = Quad.m_aLocalPoints[4] + PrevOffset;
		Quad.m_PrevAngle = PrevRotation;
	}
	else
	{
		// Nothing asked how it moved, or it cannot have moved at all. Saying it stands where it
		// stood is the honest answer either way, and it is the one MotionAt reads.
		Quad.m_PrevPivot = Quad.m_Pivot;
		Quad.m_PrevAngle = Quad.m_Angle;
	}

	for(int i = 0; i < 4; i++)
	{
		Quad.m_aPoints[i] = Quad.m_aLocalPoints[i];
		if(Rotation != 0.0f)
			Rotate(Quad.m_aLocalPoints[4], &Quad.m_aPoints[i], Rotation);
		Quad.m_aPoints[i] += Offset;
	}

	Quad.UpdateAabb();
}

void CQuadZones::UpdateTo(int Tick)
{
	if(!m_Active || Tick == m_CachedTick)
		return;
	m_CachedTick = Tick;

	// the time the server animates the quads with, see CGameControllerDDRace::GetTime
	static const std::chrono::nanoseconds s_NanosPerTick = std::chrono::nanoseconds(1s) / SERVER_TICK_SPEED;
	const std::chrono::nanoseconds Time = (int64_t)(Tick - m_QuadStartTick) * s_NanosPerTick;
	const std::chrono::nanoseconds PrevTime = Time - s_NanosPerTick;

	for(int Type = 0; Type < (int)EPredictedZone::Num; Type++)
	{
		/*
		 * Only the push needs the quad's velocity, and asking costs a second walk of the
		 * envelope. The prediction re-simulates a whole range of ticks every frame, so the
		 * tick before this one cannot just be whatever the last update happened to see.
		 */
		const bool WithMotion = Type == (int)EPredictedZone::StopA;

		for(CQuadData &Quad : m_avQuads[Type])
			UpdateQuad(Quad, Time, PrevTime, WithMotion);
	}
}

const std::vector<CQuadData> &CQuadZones::Quads(EPredictedZone Type, int Tick)
{
	static const std::vector<CQuadData> s_vNone;
	if(!m_Active)
		return s_vNone;

	std::vector<CQuadData> &vQuads = m_avQuads[(int)Type];
	if(vQuads.empty())
		return vQuads;

	UpdateTo(Tick);
	return vQuads;
}

bool CQuadZones::Inside(EPredictedZone Type, vec2 Pos, int Tick)
{
	for(const CQuadData &Quad : Quads(Type, Tick))
	{
		if(Quad.Overlaps(Pos, vec2(0.0f, 0.0f)))
			return true;
	}
	return false;
}
