#ifndef GAME_CLIENT_PREDICTION_QUAD_ZONES_H
#define GAME_CLIENT_PREDICTION_QUAD_ZONES_H

#include <base/vmath.h>

#include <game/map/render_map.h>
#include <game/quad_data.h>

#include <memory>
#include <vector>

class IMap;

/*
 * The zones the client acts on itself, in the order the server runs them, see
 * CZoneManager::OnTick. QHook and QUnHook are not here because they are not acted on: they turn
 * into solid and unhookable tiles, so they go into the collision map and everything from the
 * hook to MoveBox picks them up on its own, see CQuadZones::OnMapLoad.
 *
 * The rest are left to the server, since neither has an outcome the client could know: QDeath
 * kills and QCfrm teleports to a random tele checkout or to spawn.
 */
enum class EPredictedZone
{
	Freeze,
	Unfreeze,
	StopA,
	Num,
};

/*
 * One position envelope read at one offset, which is all a quad's placement depends on. A zone
 * layer points whole runs of quads at the same pair, so the envelope is walked once per pair per
 * tick and every quad behind it just reads the answer, see CQuadZones::UpdateTransforms.
 */
class CQuadEnvKey
{
public:
	int m_PosEnv = -1;
	int m_PosEnvOffset = 0;

	// Resolved once at map load, null when no envelope of the map backs the index
	const CMapItemEnvelope *m_pEnvelope = nullptr;
	// Whether any quad using this pair asks how it moved, which costs a second walk
	bool m_NeedsMotion = false;
};

// Where one key's envelope stands on one tick, which is the whole of what a tick changes
class CQuadTransform
{
public:
	vec2 m_Offset = vec2(0.0f, 0.0f);
	float m_Rotation = 0.0f;
	vec2 m_PrevOffset = vec2(0.0f, 0.0f);
	float m_PrevRotation = 0.0f;
	bool m_Teleported = false;
};

/*
 * A quad's bounds before any envelope moves it, kept apart from the quad itself so that the
 * overlap scan walks a tight array instead of striding over whole quads.
 */
class CQuadBound
{
public:
	vec2 m_Min = vec2(0.0f, 0.0f);
	vec2 m_Max = vec2(0.0f, 0.0f);
	int m_EnvKey = -1;
};

/*
 * The moving zones of FoxNet servers, and the one thing on the client that knows where any of
 * their quads is. The server acts on whoever stands in one of them once per tick from its own
 * component tick, see CFreezeZone::OnTick, CUnfreezeZone::OnTick and CCollidableZone::OnTick,
 * so without this every one of them costs a full round trip.
 *
 * It holds the ones the client has to act on, "QFr", "QUnFr" and "QStopa". "QHook" and "QUnHook"
 * are left to the server: they are solid geometry rather than something to act on, so predicting
 * them means teaching the whole collision about moving quads, and carrying the attachment state
 * that costs across every rebuild of the predicted world.
 *
 * They follow their position envelope, evaluated against the tick the server started
 * animating them at. FoxNet snaps that tick as GameInfo::m_RoundStartTick precisely so the
 * client can place the quads where the server had them, which makes them predictable.
 *
 * Quad layers of a minigame group belong to that minigame whatever they are named, so those
 * groups are skipped here the same way, see CZoneManager::OnMapLoad.
 *
 * The quads themselves are kept where the map put them and are never walked per tick. A tick
 * only moves the handful of envelope keys behind them, and a question about a point is answered
 * by asking it of the key's own frame instead, see Inside. Only QStopa, which hands the push its
 * corners and its speed, is placed for real, and only once something asks for it.
 */
class CQuadZones
{
public:
	CQuadZones() { Reset(); }

	void OnMapLoad(IMap *pMap);
	void Reset();

	// The prediction has no way back to the client, so these are pushed in once per snapshot
	void SetActive(bool Active);
	void SetStopAGivesDj(bool GivesDj) { m_StopAGivesDj = GivesDj; }
	// Whether QFr and QUnFr act on the whole path of a tick, see sv_quad_freeze_path
	void SetFreezePath(bool Path) { m_FreezePath = Path; }
	void SetQuadStartTick(int QuadStartTick);

	bool Active() const { return m_Active; }
	bool StopAGivesDj() const { return m_StopAGivesDj; }
	bool FreezePath() const { return m_FreezePath; }

	/*
	 * Settles where the envelopes stand on this tick, so that everything acting during it agrees
	 * on where the quads are, the way the server settles them before any zone runs, see
	 * CZoneManager::OnTick. Several predicted worlds replay overlapping ranges of ticks every
	 * frame, see CGameClient::OnPredict, so the answer is kept per tick rather than only for the
	 * last one asked for.
	 */
	void UpdateTo(int Tick);

	// The quads of one zone as they stand on this tick, empty while the prediction is off
	const std::vector<CQuadData> &Quads(EPredictedZone Type, int Tick);
	bool Inside(EPredictedZone Type, vec2 Pos, int Tick);
	/*
	 * Whether the path From->To touches any quad of this zone along it, rather than only where it
	 * ends. A quad thinner than one tick of travel is otherwise passed straight through at speed.
	 * Nothing is resolved here, so unlike the push this needs no sampling -- the question is only
	 * whether the path crossed the quad. Mirrors CQuadZone::SegmentInsideQuad.
	 */
	bool SegmentInside(EPredictedZone Type, vec2 From, vec2 To, int Tick);
	/*
	 * Indices into Quads of those whose bounds reach this box, replacing whatever vOut held. The
	 * sweep asks several times a tick and the push once per contact, which is too often to walk
	 * a layer of thousands, see CCharacter::QuadZoneSweep.
	 */
	void QueryQuads(EPredictedZone Type, int Tick, vec2 Min, vec2 Max, std::vector<int> &vOut);
	/*
	 * The quads of one zone overlapping the one at Index, itself excluded. Asking QueryQuads for
	 * them reads that quad's own bounds, and one long diagonal quad reaches across a great deal of
	 * the layer, so the scan came back with a dozen answers after looking at all of them. Which
	 * quads that never move overlap which never changes, so it is settled once; a quad that moves
	 * is asked for as it stands. Mirrors CQuadZone::QueryNeighbours.
	 */
	void QueryNeighbours(EPredictedZone Type, int Tick, int Index, std::vector<int> &vOut);

private:
	/*
	 * How many ticks' worth of envelope positions are kept at once. The worlds of one frame span
	 * the ticks between the last snapshot and the prediction's end plus what fast input and the
	 * smoothing add, which stays far inside this.
	 */
	static constexpr int TICK_CACHE_SIZE = 64;

	// Forgets every tick settled so far, for when the quads are somewhere else entirely
	void DropCachedTicks();
	void BuildNeighbourCache(EPredictedZone Type);
	// The pair this quad reads its placement from, adding it to the table when it is a new one
	int AddEnvKey(int PosEnv, int PosEnvOffset, bool NeedsMotion);
	void UpdateTransforms(int Tick);
	// Places the quads of one zone for the settled tick, for the callers that need their corners
	void Materialize(EPredictedZone Type);
	void UpdateQuad(CQuadData &Quad, const CQuadTransform *pTransform, bool WithMotion) const;
	void EvalPosEnvelope(const CQuadEnvKey &Key, std::chrono::nanoseconds Time, vec2 &Offset, float &Rotation) const;
	// Whether the envelope put the quad somewhere else between these two times instead of moving it there
	bool QuadJumped(const CQuadEnvKey &Key, std::chrono::nanoseconds Time, std::chrono::nanoseconds PrevTime) const;

	std::vector<CQuadData> m_avQuads[(int)EPredictedZone::Num];
	// Parallel to m_avQuads, what the overlap scan reads instead of the quads
	std::vector<CQuadBound> m_avBounds[(int)EPredictedZone::Num];

	// Parallel to m_avQuads: each never-moving quad's never-moving neighbours, settled once
	std::vector<int> m_avNeighbourStart[(int)EPredictedZone::Num];
	std::vector<int> m_avNeighbourIndices[(int)EPredictedZone::Num];
	bool m_aNeighbourCacheValid[(int)EPredictedZone::Num] = {};
	std::vector<int> m_vNeighbourScratch;

	std::vector<CQuadEnvKey> m_vEnvKeys;
	std::vector<CQuadTransform> m_vTransforms; // where those keys stand on m_CachedTick
	std::vector<CQuadTransform> m_avTickCache[TICK_CACHE_SIZE];
	int m_aTickCacheTick[TICK_CACHE_SIZE];

	std::unique_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	IMap *m_pMap = nullptr;
	bool m_Active = false;
	bool m_StopAGivesDj = false;
	bool m_FreezePath = true;
	int m_QuadStartTick = 0;
	int m_CachedTick = -1;
	// The tick each zone's quads were last placed for, see Materialize
	int m_aMaterializedTick[(int)EPredictedZone::Num];
	// Whether any key turns its quads on this tick, which no shift of the asking point can undo
	bool m_AnyRotation = false;
};

#endif // GAME_CLIENT_PREDICTION_QUAD_ZONES_H
