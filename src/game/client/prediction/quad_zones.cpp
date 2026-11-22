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
#include <limits>

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
	for(bool &Valid : m_aNeighbourCacheValid)
		Valid = false;
	for(std::vector<int> &vStart : m_avNeighbourStart)
		vStart.clear();
	for(std::vector<int> &vIndices : m_avNeighbourIndices)
		vIndices.clear();
	for(std::vector<CQuadBound> &vBounds : m_avBounds)
		vBounds.clear();
	m_vEnvKeys.clear();
	m_vTransforms.clear();
	for(std::vector<CQuadTransform> &vCached : m_avTickCache)
		vCached.clear();
	m_pEnvelopePoints = nullptr;
	m_pMap = nullptr;
	m_Active = false;
	m_StopAGivesDj = false;
	m_QuadStartTick = 0;
	m_AnyRotation = false;
	DropCachedTicks();
}

void CQuadZones::DropCachedTicks()
{
	m_CachedTick = -1;
	for(int Slot = 0; Slot < TICK_CACHE_SIZE; Slot++)
		m_aTickCacheTick[Slot] = std::numeric_limits<int>::min();
	for(int &MaterializedTick : m_aMaterializedTick)
		MaterializedTick = std::numeric_limits<int>::min();
}

void CQuadZones::SetActive(bool Active)
{
	if(Active == m_Active)
		return;

	m_Active = Active;
	DropCachedTicks();
}

void CQuadZones::SetQuadStartTick(int QuadStartTick)
{
	if(QuadStartTick == m_QuadStartTick)
		return;

	// the quads are somewhere else entirely now, the cached ticks say nothing about them anymore
	m_QuadStartTick = QuadStartTick;
	DropCachedTicks();
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

			// Only the push needs to know how a quad moved, see UpdateTo
			const bool NeedsMotion = Type == (int)EPredictedZone::StopA;

			std::vector<CQuadData> &vQuads = m_avQuads[Type];
			std::vector<CQuadBound> &vBounds = m_avBounds[Type];
			vQuads.reserve(vQuads.size() + pQuadsLayer->m_NumQuads);
			vBounds.reserve(vBounds.size() + pQuadsLayer->m_NumQuads);

			for(int QuadIndex = 0; QuadIndex < pQuadsLayer->m_NumQuads; QuadIndex++)
			{
				CQuadData ZoneQuad;
				ZoneQuad.Init(pQuads[QuadIndex]);

				CQuadBound Bound;
				Bound.m_Min = ZoneQuad.m_LocalAabbMin;
				Bound.m_Max = ZoneQuad.m_LocalAabbMax;
				Bound.m_EnvKey = AddEnvKey(ZoneQuad.m_PosEnv, ZoneQuad.m_PosEnvOffset, NeedsMotion);

				vQuads.push_back(ZoneQuad);
				vBounds.push_back(Bound);
				AnyQuads = true;
			}
		}
	}

	if(!AnyQuads)
		return;

	m_pEnvelopePoints = std::make_unique<CMapBasedEnvelopePointAccess>(m_pMap);

	// Walking the map for an envelope is the same answer every time, so it is looked up once here
	const std::vector<const CMapItemEnvelope *> &vpEnvelopes = m_pEnvelopePoints->Envelopes();
	for(CQuadEnvKey &Key : m_vEnvKeys)
	{
		if(Key.m_PosEnv < 0 || Key.m_PosEnv >= (int)vpEnvelopes.size())
			continue;

		const CMapItemEnvelope *pEnvelope = vpEnvelopes[Key.m_PosEnv];
		if(!pEnvelope || pEnvelope->m_Channels <= 0)
			continue;

		Key.m_pEnvelope = pEnvelope;
	}

	m_vTransforms.resize(m_vEnvKeys.size());
	for(std::vector<CQuadTransform> &vCached : m_avTickCache)
		vCached.resize(m_vEnvKeys.size());
}

int CQuadZones::AddEnvKey(int PosEnv, int PosEnvOffset, bool NeedsMotion)
{
	if(PosEnv < 0)
		return -1;

	for(size_t Index = 0; Index < m_vEnvKeys.size(); Index++)
	{
		CQuadEnvKey &Key = m_vEnvKeys[Index];
		if(Key.m_PosEnv != PosEnv || Key.m_PosEnvOffset != PosEnvOffset)
			continue;

		Key.m_NeedsMotion = Key.m_NeedsMotion || NeedsMotion;
		return (int)Index;
	}

	CQuadEnvKey Key;
	Key.m_PosEnv = PosEnv;
	Key.m_PosEnvOffset = PosEnvOffset;
	Key.m_NeedsMotion = NeedsMotion;
	m_vEnvKeys.push_back(Key);
	return (int)m_vEnvKeys.size() - 1;
}

void CQuadZones::EvalPosEnvelope(const CQuadEnvKey &Key, std::chrono::nanoseconds Time, vec2 &Offset, float &Rotation) const
{
	const CMapItemEnvelope *pEnvelope = Key.m_pEnvelope;
	if(!pEnvelope)
		return;

	m_pEnvelopePoints->SetPointsRange(pEnvelope->m_StartPoint, pEnvelope->m_NumPoints);
	if(m_pEnvelopePoints->NumPoints() == 0)
		return;

	ColorRGBA Position(0.0f, 0.0f, 0.0f, 0.0f);
	const size_t Channels = std::min({(size_t)3, (size_t)pEnvelope->m_Channels, (size_t)CEnvPoint::MAX_CHANNELS});
	CRenderMap::RenderEvalEnvelope(m_pEnvelopePoints.get(), Time + std::chrono::milliseconds(Key.m_PosEnvOffset), Position, Channels);

	Offset = vec2(Position.r, Position.g);
	Rotation = Position.b / 180.0f * pi;
}

bool CQuadZones::QuadJumped(const CQuadEnvKey &Key, std::chrono::nanoseconds Time, std::chrono::nanoseconds PrevTime) const
{
	const CMapItemEnvelope *pEnvelope = Key.m_pEnvelope;
	if(!pEnvelope)
		return false;

	m_pEnvelopePoints->SetPointsRange(pEnvelope->m_StartPoint, pEnvelope->m_NumPoints);
	const int NumPoints = m_pEnvelopePoints->NumPoints();
	if(NumPoints < 2)
		return false;

	const CEnvPoint *pFirst = m_pEnvelopePoints->GetPoint(0);
	const CEnvPoint *pLast = m_pEnvelopePoints->GetPoint(NumPoints - 1);
	const int64_t Loop = pLast->m_Time.GetInternal();
	if(Loop <= 0)
		return false;

	const auto Millis = [&](std::chrono::nanoseconds At) {
		int64_t Value = (std::chrono::duration_cast<std::chrono::milliseconds>(At).count() + Key.m_PosEnvOffset) % Loop;
		if(Value < 0)
			Value += Loop;
		return Value;
	};
	const auto Segment = [&](int64_t At) {
		for(int i = 0; i < NumPoints - 1; i++)
		{
			if(At >= m_pEnvelopePoints->GetPoint(i)->m_Time.GetInternal() &&
				At <= m_pEnvelopePoints->GetPoint(i + 1)->m_Time.GetInternal())
				return i;
		}
		return NumPoints - 1;
	};
	const auto Differs = [](const CEnvPoint *pA, const CEnvPoint *pB) {
		return pA->m_aValues[0] != pB->m_aValues[0] ||
		       pA->m_aValues[1] != pB->m_aValues[1] ||
		       pA->m_aValues[2] != pB->m_aValues[2];
	};

	const int64_t Now = Millis(Time);
	const int64_t Before = Millis(PrevTime);

	// Running off the end and back to the start only moves the quad if the two ends differ
	if(Now < Before && Differs(pFirst, pLast))
		return true;

	// A step holds one point's value and drops onto the next one's when the segment ends
	const int Was = Segment(Before);
	if(Was < NumPoints - 1 && Segment(Now) != Was)
	{
		const CEnvPoint *pCur = m_pEnvelopePoints->GetPoint(Was);
		const CEnvPoint *pNext = m_pEnvelopePoints->GetPoint(Was + 1);
		if(pCur->m_Curvetype == CURVETYPE_STEP && Differs(pCur, pNext))
			return true;
	}

	return false;
}

void CQuadZones::UpdateQuad(CQuadData &Quad, const CQuadTransform *pTransform, bool WithMotion) const
{
	const vec2 Offset = pTransform ? pTransform->m_Offset : vec2(0.0f, 0.0f);
	const float Rotation = pTransform ? pTransform->m_Rotation : 0.0f;

	Quad.m_Pivot = Quad.m_aLocalPoints[4] + Offset;
	Quad.m_Angle = Rotation;

	if(WithMotion && pTransform)
	{
		Quad.m_PrevPivot = Quad.m_aLocalPoints[4] + pTransform->m_PrevOffset;
		Quad.m_PrevAngle = pTransform->m_PrevRotation;
		Quad.m_Teleported = pTransform->m_Teleported;
	}
	else
	{
		// Nothing asked how it moved, or it cannot have moved at all. Saying it stands where it
		// stood is the honest answer either way, and it is the one MotionAt reads.
		Quad.m_PrevPivot = Quad.m_Pivot;
		Quad.m_PrevAngle = Quad.m_Angle;
		Quad.m_Teleported = true;
	}

	if(Rotation == 0.0f)
	{
		Quad.MoveTo(Offset);
		return;
	}

	for(int i = 0; i < 4; i++)
	{
		Quad.m_aPoints[i] = Quad.m_aLocalPoints[i];
		Rotate(Quad.m_aLocalPoints[4], &Quad.m_aPoints[i], Rotation);
		Quad.m_aPoints[i] += Offset;
	}

	Quad.UpdateAabb();
}

void CQuadZones::UpdateTransforms(int Tick)
{
	// the time the server animates the quads with, see CGameControllerDDRace::GetTime
	static const std::chrono::nanoseconds s_NanosPerTick = std::chrono::nanoseconds(1s) / SERVER_TICK_SPEED;
	const std::chrono::nanoseconds Time = (int64_t)(Tick - m_QuadStartTick) * s_NanosPerTick;
	const std::chrono::nanoseconds PrevTime = Time - s_NanosPerTick;

	for(size_t Index = 0; Index < m_vEnvKeys.size(); Index++)
	{
		const CQuadEnvKey &Key = m_vEnvKeys[Index];
		CQuadTransform &Transform = m_vTransforms[Index];

		Transform.m_Offset = vec2(0.0f, 0.0f);
		Transform.m_Rotation = 0.0f;
		EvalPosEnvelope(Key, Time, Transform.m_Offset, Transform.m_Rotation);

		/*
		 * Only the push needs the quad's velocity, and asking costs a second walk of the
		 * envelope. The prediction re-simulates a whole range of ticks every frame, so the
		 * tick before this one cannot just be whatever the last update happened to see.
		 */
		if(!Key.m_NeedsMotion)
			continue;

		Transform.m_PrevOffset = vec2(0.0f, 0.0f);
		Transform.m_PrevRotation = 0.0f;
		EvalPosEnvelope(Key, PrevTime, Transform.m_PrevOffset, Transform.m_PrevRotation);
		Transform.m_Teleported = QuadJumped(Key, Time, PrevTime);
	}
}

void CQuadZones::UpdateTo(int Tick)
{
	if(!m_Active || Tick == m_CachedTick)
		return;
	m_CachedTick = Tick;

	/*
	 * Every predicted world of a frame runs through this, and their tick ranges overlap, so a
	 * tick settled for one of them is handed to the next instead of walking the envelopes again,
	 * see CGameClient::OnPredict.
	 */
	const size_t Slot = (size_t)((unsigned)Tick % (unsigned)TICK_CACHE_SIZE);
	if(m_aTickCacheTick[Slot] == Tick)
	{
		m_vTransforms = m_avTickCache[Slot];
	}
	else
	{
		UpdateTransforms(Tick);
		m_aTickCacheTick[Slot] = Tick;
		m_avTickCache[Slot] = m_vTransforms;
	}

	m_AnyRotation = false;
	for(const CQuadTransform &Transform : m_vTransforms)
	{
		if(Transform.m_Rotation != 0.0f)
		{
			m_AnyRotation = true;
			break;
		}
	}
}

void CQuadZones::Materialize(EPredictedZone Type)
{
	const int TypeIndex = (int)Type;
	if(m_aMaterializedTick[TypeIndex] == m_CachedTick)
		return;
	m_aMaterializedTick[TypeIndex] = m_CachedTick;

	const bool WithMotion = TypeIndex == (int)EPredictedZone::StopA;

	std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];
	for(size_t Index = 0; Index < vQuads.size(); Index++)
	{
		const int KeyIndex = vBounds[Index].m_EnvKey;
		UpdateQuad(vQuads[Index], KeyIndex < 0 ? nullptr : &m_vTransforms[KeyIndex], WithMotion);
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
	Materialize(Type);
	return vQuads;
}

bool CQuadZones::Inside(EPredictedZone Type, vec2 Pos, int Tick)
{
	if(!m_Active)
		return false;

	const int TypeIndex = (int)Type;
	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];
	if(vBounds.empty())
		return false;

	UpdateTo(Tick);

	/*
	 * A quad that only shifts holds the same points relative to its own envelope all the way
	 * through, so instead of carrying thousands of quads to where they stand, the asking point
	 * is carried back into the frame each one sits in. A turning quad is not a shifted copy of
	 * itself, and its corners turn around its own pivot rather than the envelope's, so when any
	 * key turns this tick the quads are placed for real first.
	 */
	if(m_AnyRotation)
	{
		Materialize(Type);
		for(const CQuadData &Quad : m_avQuads[TypeIndex])
		{
			if(Quad.Overlaps(Pos, vec2(0.0f, 0.0f)))
				return true;
		}
		return false;
	}

	const std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
	for(size_t Index = 0; Index < vBounds.size(); Index++)
	{
		const CQuadBound &Bound = vBounds[Index];
		const vec2 LocalPos = Bound.m_EnvKey < 0 ? Pos : Pos - m_vTransforms[Bound.m_EnvKey].m_Offset;

		if(LocalPos.x < Bound.m_Min.x || LocalPos.x > Bound.m_Max.x ||
			LocalPos.y < Bound.m_Min.y || LocalPos.y > Bound.m_Max.y)
			continue;

		if(InsideQuadrilateral(LocalPos, vQuads[Index].m_aLocalPoints))
			return true;
	}
	return false;
}

bool CQuadZones::SegmentInside(EPredictedZone Type, vec2 From, vec2 To, int Tick)
{
	if(!m_Active)
		return false;

	const int TypeIndex = (int)Type;
	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];
	if(vBounds.empty())
		return false;

	UpdateTo(Tick);

	// Same trick as Inside: a quad that only shifts is asked in its own frame, so both ends of
	// the path are carried back into it rather than the quad being walked to where it stands
	if(m_AnyRotation)
	{
		Materialize(Type);
		for(const CQuadData &Quad : m_avQuads[TypeIndex])
		{
			if(std::max(From.x, To.x) < Quad.m_AabbMin.x || std::min(From.x, To.x) > Quad.m_AabbMax.x)
				continue;
			if(std::max(From.y, To.y) < Quad.m_AabbMin.y || std::min(From.y, To.y) > Quad.m_AabbMax.y)
				continue;
			if(SegmentHitsQuadrilateral(From, To, Quad.m_aPoints))
				return true;
		}
		return false;
	}

	const std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
	for(size_t Index = 0; Index < vBounds.size(); Index++)
	{
		const CQuadBound &Bound = vBounds[Index];
		const vec2 Offset = Bound.m_EnvKey < 0 ? vec2(0.0f, 0.0f) : m_vTransforms[Bound.m_EnvKey].m_Offset;
		const vec2 LocalFrom = From - Offset;
		const vec2 LocalTo = To - Offset;

		if(std::max(LocalFrom.x, LocalTo.x) < Bound.m_Min.x || std::min(LocalFrom.x, LocalTo.x) > Bound.m_Max.x)
			continue;
		if(std::max(LocalFrom.y, LocalTo.y) < Bound.m_Min.y || std::min(LocalFrom.y, LocalTo.y) > Bound.m_Max.y)
			continue;

		if(SegmentHitsQuadrilateral(LocalFrom, LocalTo, vQuads[Index].m_aLocalPoints))
			return true;
	}
	return false;
}

void CQuadZones::BuildNeighbourCache(EPredictedZone Type)
{
	const int TypeIndex = (int)Type;
	std::vector<int> &vStart = m_avNeighbourStart[TypeIndex];
	std::vector<int> &vIndices = m_avNeighbourIndices[TypeIndex];
	const std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];

	m_aNeighbourCacheValid[TypeIndex] = false;
	vStart.assign(vQuads.size() + 1, 0);
	vIndices.clear();

	if(vQuads.empty())
		return;

	for(size_t Index = 0; Index < vQuads.size(); Index++)
	{
		vStart[Index] = (int)vIndices.size();

		// A quad an envelope moves stands somewhere else every tick, so nothing is settled here
		if(vBounds[Index].m_EnvKey >= 0)
			continue;

		const CQuadData &Quad = vQuads[Index];
		for(size_t OtherIndex = 0; OtherIndex < vQuads.size(); OtherIndex++)
		{
			if(OtherIndex == Index || vBounds[OtherIndex].m_EnvKey >= 0)
				continue;
			const CQuadData &Other = vQuads[OtherIndex];
			if(Other.m_AabbMax.x < Quad.m_AabbMin.x || Other.m_AabbMin.x > Quad.m_AabbMax.x)
				continue;
			if(Other.m_AabbMax.y < Quad.m_AabbMin.y || Other.m_AabbMin.y > Quad.m_AabbMax.y)
				continue;
			vIndices.push_back((int)OtherIndex);
		}
	}
	vStart[vQuads.size()] = (int)vIndices.size();

	m_aNeighbourCacheValid[TypeIndex] = true;
}

void CQuadZones::QueryNeighbours(EPredictedZone Type, int Tick, int Index, std::vector<int> &vOut)
{
	vOut.clear();

	if(!m_Active)
		return;

	const int TypeIndex = (int)Type;
	const std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
	if(Index < 0 || (size_t)Index >= vQuads.size())
		return;

	UpdateTo(Tick);
	Materialize(Type);

	if(!m_aNeighbourCacheValid[TypeIndex])
		BuildNeighbourCache(Type);

	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];
	const CQuadData &Quad = vQuads[Index];

	// Nothing is settled about a quad an envelope moves, so this one is asked for real
	if(!m_aNeighbourCacheValid[TypeIndex] || vBounds[Index].m_EnvKey >= 0)
	{
		QueryQuads(Type, Tick, Quad.m_AabbMin, Quad.m_AabbMax, vOut);
		vOut.erase(std::remove(vOut.begin(), vOut.end(), Index), vOut.end());
		return;
	}

	const std::vector<int> &vStart = m_avNeighbourStart[TypeIndex];
	for(int i = vStart[Index]; i < vStart[Index + 1]; i++)
		vOut.push_back(m_avNeighbourIndices[TypeIndex][i]);

	// The moving ones are few, so they are tested against as they stand now
	for(size_t OtherIndex = 0; OtherIndex < vQuads.size(); OtherIndex++)
	{
		if((int)OtherIndex == Index || vBounds[OtherIndex].m_EnvKey < 0)
			continue;
		const CQuadData &Other = vQuads[OtherIndex];
		if(Other.m_AabbMax.x < Quad.m_AabbMin.x || Other.m_AabbMin.x > Quad.m_AabbMax.x)
			continue;
		if(Other.m_AabbMax.y < Quad.m_AabbMin.y || Other.m_AabbMin.y > Quad.m_AabbMax.y)
			continue;
		vOut.push_back((int)OtherIndex);
	}
}

void CQuadZones::QueryQuads(EPredictedZone Type, int Tick, vec2 Min, vec2 Max, std::vector<int> &vOut)
{
	vOut.clear();

	if(!m_Active)
		return;

	const int TypeIndex = (int)Type;
	const std::vector<CQuadBound> &vBounds = m_avBounds[TypeIndex];
	if(vBounds.empty())
		return;

	UpdateTo(Tick);
	Materialize(Type);

	// A turning quad is not a shifted copy of itself, so there its placed bounds are the only
	// ones that answer; the shifting case is the box carried into the quad's frame, as in Inside
	if(m_AnyRotation)
	{
		const std::vector<CQuadData> &vQuads = m_avQuads[TypeIndex];
		for(size_t Index = 0; Index < vQuads.size(); Index++)
		{
			const CQuadData &Quad = vQuads[Index];
			if(Quad.m_AabbMax.x < Min.x || Quad.m_AabbMin.x > Max.x)
				continue;
			if(Quad.m_AabbMax.y < Min.y || Quad.m_AabbMin.y > Max.y)
				continue;
			vOut.push_back((int)Index);
		}
		return;
	}

	for(size_t Index = 0; Index < vBounds.size(); Index++)
	{
		const CQuadBound &Bound = vBounds[Index];
		const vec2 Offset = Bound.m_EnvKey < 0 ? vec2(0.0f, 0.0f) : m_vTransforms[Bound.m_EnvKey].m_Offset;

		if(Bound.m_Max.x + Offset.x < Min.x || Bound.m_Min.x + Offset.x > Max.x)
			continue;
		if(Bound.m_Max.y + Offset.y < Min.y || Bound.m_Min.y + Offset.y > Max.y)
			continue;

		vOut.push_back((int)Index);
	}
}
