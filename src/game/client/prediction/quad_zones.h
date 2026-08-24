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
 */
class CQuadZones
{
public:
	void OnMapLoad(IMap *pMap);
	void Reset();

	// The prediction has no way back to the client, so these are pushed in once per snapshot
	void SetActive(bool Active);
	void SetStopAGivesDj(bool GivesDj) { m_StopAGivesDj = GivesDj; }
	void SetQuadStartTick(int QuadStartTick);

	bool Active() const { return m_Active; }
	bool StopAGivesDj() const { return m_StopAGivesDj; }

	/*
	 * Moves every quad to where it stands on this tick. Called once at the top of the world tick
	 * so that everything acting during that tick
	 * agrees on where the quads are, the way the server settles them before any zone runs, see
	 * CZoneManager::OnTick. The prediction replays a whole range of ticks every frame, so this
	 * caches the tick it last ran for and does nothing when asked for it twice.
	 */
	void UpdateTo(int Tick);

	// The quads of one zone as they stand on this tick, empty while the prediction is off
	const std::vector<CQuadData> &Quads(EPredictedZone Type, int Tick);
	bool Inside(EPredictedZone Type, vec2 Pos, int Tick);

private:
	void UpdateQuad(CQuadData &Quad, std::chrono::nanoseconds Time, std::chrono::nanoseconds PrevTime, bool WithMotion) const;
	void EvalPosEnvelope(const CQuadData &Quad, std::chrono::nanoseconds Time, vec2 &Offset, float &Rotation) const;

	std::vector<CQuadData> m_avQuads[(int)EPredictedZone::Num];
	std::unique_ptr<CMapBasedEnvelopePointAccess> m_pEnvelopePoints;
	IMap *m_pMap = nullptr;
	bool m_Active = false;
	bool m_StopAGivesDj = false;
	int m_QuadStartTick = 0;
	int m_CachedTick = -1;
};

#endif // GAME_CLIENT_PREDICTION_QUAD_ZONES_H
