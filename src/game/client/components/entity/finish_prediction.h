// EClient
#ifndef GAME_CLIENT_COMPONENTS_ENTITY_FINISH_PREDICTION_H
#define GAME_CLIENT_COMPONENTS_ENTITY_FINISH_PREDICTION_H

#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * Works out how far through the map the tee has got, and from that how long the whole run is
 * going to take.
 *
 * There is no way to ask the server how long a map is, so the route has to be found on the
 * client. On map load a snapshot of the tile layers is taken and handed to a worker thread,
 * which walks a route from the start tiles to the finish tiles. Progress is then the arc length
 * of the point on that route nearest the tee, over the route's total length, and the predicted
 * time is the elapsed race time divided by that fraction.
 *
 * The route is found in two passes, because a single pass that treats freeze as impassable fails
 * on every map that requires a freeze crossing, and a single pass that ignores freeze walks
 * straight through walls of it:
 *
 *   1. The coarse pass ignores freeze entirely and only respects what a tee genuinely cannot
 *      pass: solid tiles, death tiles and the stoppers. It answers "is the finish reachable at
 *      all, and roughly which way", and its distance field is what the overlay draws as the
 *      unrefined route.
 *   2. The refined pass searches the same grid with a second state bit for whether the tee is
 *      frozen. Entering freeze flips that bit, every tile travelled while it is set is heavily
 *      surcharged, and the only cheap way to clear it is to land on an unfreeze tile. Waiting
 *      the freeze out is possible but costs about a hundred tiles of detour. A shortest path
 *      through that graph therefore avoids freeze where there is any way round, crosses it at
 *      its narrowest and onto an unfreeze tile where there is not, and still finds something on
 *      a map where freeze is unavoidable.
 *
 * Teleporters are followed as edges. The checkpoint ones cannot be, because where they send the
 * tee depends on the last checkpoint it touched rather than on the tile, so the search carries
 * that checkpoint as a third coordinate of its state and they become ordinary edges within a
 * layer. Maps built on the mechanic join their sections with nothing else, and without this have
 * no route at all.
 *
 * None of this simulates a tee. It cannot tell a reachable ledge from an unreachable one, so it
 * is a progress estimate and not a solution to the map.
 */
class CFinishPrediction : public CComponent
{
public:
	enum class EState
	{
		IDLE, // Not in a race, or the map has no start and finish to run between
		SEARCHING, // The worker still has the map
		READY,
		FAILED, // The finish is not reachable from the start on the tile grid alone
	};

	int Sizeof() const override { return sizeof(*this); }
	~CFinishPrediction() override;

	void OnMapLoad() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnRender() override;
	void OnSelfDeath(bool Dummy) override;
	void OnConsoleInit() override;

	/**
	 * Draws the readout. Called from CHud::OnRender, late enough that the layout has been set up
	 * for the frame. It scopes itself, because it only takes up room on the frames it has
	 * something to say. Preview is the HUD editor asking for something to aim at.
	 */
	void RenderHud(bool Preview);

	EState State() const { return m_State; }
	/** Why there is no route, for the status line. Empty unless the state is FAILED. */
	const char *FailureReason() const { return m_pFailureReason; }
	/** How far along the route the tee is, 0 to 1, valid while HasProgress(). */
	float Progress() const { return m_Progress; }
	bool HasProgress() const { return m_State == EState::READY && m_HasProgress; }
	/** The whole route's length in tiles, for the settings page and the console. */
	float RouteTiles() const;
	const char *StatusText() const;

private:
	// Tile properties the search cares about, one bitfield per tile.
	enum
	{
		TILEINFO_SOLID = 1 << 0,
		TILEINFO_DEATH = 1 << 1,
		TILEINFO_FREEZE = 1 << 2,
		TILEINFO_DEEP = 1 << 3, // Deep freeze, which only a deep unfreeze tile clears
		TILEINFO_UNFREEZE = 1 << 4,
		TILEINFO_TELEIN = 1 << 5,
		TILEINFO_FINISH = 1 << 6,
		TILEINFO_START = 1 << 7,
		// A checkpoint teleporter entrance. Where it sends the tee depends on the last
		// checkpoint the tee touched and not on the tile, so it is only an edge of the search
		// once the checkpoint is part of the state; without that it is a tile to stay off.
		TILEINFO_TRAP = 1 << 8,
		// A tele checkpoint, which sets where a later trap sends the tee
		TILEINFO_CHECK = 1 << 9,
		// Where a trap sends a tee holding the matching checkpoint
		TILEINFO_CHECKOUT = 1 << 10,
	};

	/**
	 * A copy of everything the search reads out of the map.
	 *
	 * The worker never touches CCollision, because the next map load frees it while the worker
	 * may still be running. Copying is cheap next to the search itself.
	 */
	class CGrid
	{
	public:
		int m_Width = 0;
		int m_Height = 0;
		std::vector<unsigned short> m_vInfo;
		// The CANTMOVE_* directions a tee standing on this tile cannot leave in
		std::vector<unsigned char> m_vBlocked;
		// Teleporter links, as cell indices. Both directions are kept because the search runs
		// backwards from the finish and so needs the entrances of a given exit.
		std::unordered_map<int, std::vector<int>> m_TeleTargets;
		std::unordered_map<int, std::vector<int>> m_TeleSources;
		std::vector<int> m_vStarts;
		std::vector<int> m_vFinishes;
		// The map's own time checkpoints, grouped by number, in ascending order of number.
		//
		// A DDRace mapper lays these along the way the map is meant to be run, which makes them
		// the only statement of route order a map offers outright. Everything else here has to
		// infer the way from costs, and a cost model can always be talked into a shortcut the
		// mapper never intended: on Shortcut the cheapest path to the finish climbed out of the
		// start area and ran the perimeter, while the map's own checkpoints say to go right and
		// down. Routed leg by leg through these, it goes where it is told.
		//
		// Numbers are frequently not contiguous -- Motus has one to three and then six to nine
		// -- so these are the groups that exist, not a run from one.
		std::vector<std::vector<int>> m_vvTimeCheckpoints;

		// Checkpoint teleporters, as a dimension of the search.
		//
		// A trap tile sends the tee to the exit of the last checkpoint it touched, so where it
		// goes is a property of the tee and not of the tile. The search carries that as a third
		// coordinate: a group is one distinct checkout number, group 0 is "no checkpoint yet",
		// and touching a checkpoint moves the search from any group into that checkpoint's.
		//
		// Without it a map built on the mechanic has no route at all, because its sections are
		// joined only by these teleporters. With it the ordinary case costs nothing: a map with
		// no checkpoint teleporters has one group and the dimension collapses away.
		int m_CheckpointLayers = 1;
		std::unordered_map<int, int> m_CheckGroupByCell;
		std::unordered_map<int, int> m_CheckoutGroupByCell;
		std::vector<int> m_vTraps;
		// Per group, the entrances a tee can walk to from one of that group's own checkpoint
		// tiles, and the only ones the search will take in that group's layer. See gs_TrapReach.
		std::unordered_map<int, std::vector<int>> m_TrapsByGroup;
		// The same links the other way round, as exit cell to the entrances that lead there, for
		// the two passes that have no checkpoint of their own to go on. Once an entrance has to
		// be a walk from its checkpoint it is very nearly a fixed portal anyway: the tee touches
		// the checkpoint and steps straight into it, so there is only one exit it can mean.
		std::unordered_map<int, std::vector<int>> m_TrapSources;
		// Read off the config when the snapshot is taken, because the worker must not touch it
		int m_FreezePenalty = 0;
		int m_TeleporterBias = 0;

		int Cells() const { return m_Width * m_Height; }
		bool Has(int Cell, int Info) const { return (m_vInfo[Cell] & Info) != 0; }
		// The checkpoint a tee standing here is left holding, or -1 for a tile that is not one
		int CheckGroup(int Cell) const
		{
			const auto Found = m_CheckGroupByCell.find(Cell);
			return Found == m_CheckGroupByCell.end() ? -1 : Found->second;
		}
		// The checkpoint whose traps send a tee here, or -1 for a tile that is not an exit
		int CheckoutGroup(int Cell) const
		{
			const auto Found = m_CheckoutGroupByCell.find(Cell);
			return Found == m_CheckoutGroupByCell.end() ? -1 : Found->second;
		}
		bool Passable(int Cell) const { return (m_vInfo[Cell] & (TILEINFO_SOLID | TILEINFO_DEATH)) == 0; }
		vec2 Center(int Cell) const { return vec2((Cell % m_Width) * 32.0f + 16.0f, (Cell / m_Width) * 32.0f + 16.0f); }
	};

	/** What the worker hands back. Read-only once the job reports done. */
	class CRoute
	{
	public:
		int m_Width = 0;
		int m_Height = 0;
		std::vector<vec2> m_vPath; // Tile centres, start first
		std::vector<float> m_vArc; // Cumulative length along m_vPath, same size
		// How far there is left to travel from every tile to the finish, in hundredths of a tile.
		// Its own pass, not the refined search's: see TravelField. This is what places a tee
		// that is nowhere near the traced route but is still running the map: a team map's drag
		// part sends the two tees down different paths, and the one off the traced route is not
		// lost, it is running a path this does not know about.
		//
		// Travel, emphatically not cost. The search's cost is mostly freeze penalty wherever
		// there is freeze, so reading progress off it put a tee with three freeze tiles ahead of
		// it further from the finish than the start line was, and the readout collapsed to 0%
		// for the whole first half of the path. Cost picks the route; travel measures it.
		std::vector<int> m_vTravelLeft;
		// The same figure at each route node. It descends by construction, each step subtracting
		// its own length, which is what lets a tee's figure be looked up as an arc position.
		std::vector<int> m_vNodeTravelLeft;
		// Per tile, the next tile towards the finish on the travel field's own path. Following it
		// from anywhere gives the way out of there, which is how the branch a tee is actually on
		// gets found: walk it until it meets the route.
		std::vector<int> m_vTravelNext;
		// The route's own cells, mapped to the earliest node that uses them, so the walk above can
		// tell when it has arrived. A map rather than an array per tile: a route is a few thousand
		// cells against a map's hundreds of thousands.
		std::unordered_map<int, int> m_RouteNodeByCell;
		float m_Length = 0.0f;
		int m_FreezeTiles = 0; // Freeze the refined route still crosses
		// Hazard corners the route still threads between, which should be zero on all but a
		// handful of maps. Counted so that it cannot go wrong invisibly the way it used to.
		int m_EdgeSkips = 0;
		// How many checkpoint teleporter groups the search carried, one more than the number of
		// distinct checkout numbers. One means the dimension was off, either because the map has
		// no such teleporters or because it was too big to afford them.
		int m_CheckpointLayers = 1;
		// How many of the map's time checkpoints this route was threaded through, or 0 when it
		// was found in one sweep because the map has none
		int m_TimeCheckpoints = 0;
		// Set when the refined pass produced nothing usable and this is the coarse route, which
		// ignores freeze. Reported rather than hidden: a route that silently stops being the
		// refined one looks exactly like a refinement that is not working.
		bool m_CoarseFallback = false;
		// What the search had to work with, for ec_finish_prediction_status. Usable starts being
		// zero while the total is not is the shape of the bug above.
		int m_UsableStarts = 0;
		int m_TotalStarts = 0;
		int m_TotalFinishes = 0;
	};

	class CJob
	{
	public:
		std::atomic<bool> m_Abort{false};
		std::atomic<bool> m_Done{false};
		CGrid m_Grid;
		std::unique_ptr<CRoute> m_pRoute;
		// Set instead of a route where the worker decided none was worth offering. A literal,
		// never owned.
		const char *m_pFailureReason = "";
	};

	std::shared_ptr<CJob> m_pJob;
	std::thread m_Worker;
	std::shared_ptr<const CRoute> m_pRoute;

	EState m_State = EState::IDLE;
	bool m_HasProgress = false;
	float m_Progress = 0.0f;
	float m_ShownProgress = 0.0f;
	// Set where the figure has no business easing from what it just said: a map load, or the tee
	// turning up somewhere else mid-run. Smoothing is for a readout that twitches, not for
	// crossing a discontinuity.
	bool m_SnapProgress = true;
	// Set where a run has ended and the figure should come down deliberately rather than simply
	// cease to be true. Cleared the moment it arrives.
	bool m_RushProgress = false;
	// Which of the three ways of placing the tee answered this frame, for the status line
	enum class ESource
	{
		NONE,
		SIGHT, // Visible from the tee, near where it was last placed
		BRANCH, // On a path of its own, placed between where it left the route and where it rejoins
		REMAINING, // Off the route, placed by how far it has left to travel
	};
	ESource m_Source = ESource::NONE;

	// Why the last search produced nothing, for the console. A literal, never owned.
	const char *m_pFailureReason = "";
	int m_TrackNode = -1;
	// Scratch for NodeAt's cold start, kept here only so that the frames which use it are not
	// the frames that allocate it
	std::vector<int> m_vColdStart;
	// The last node the tee was placed at by sight, which is where it left the route. Taking a
	// branch is not the same as falling back, so off-route placement is not allowed to read as
	// less progress than this.
	int m_BranchNode = -1;
	// Where the tee was last frame, to tell being moved from going somewhere. Both kinds of
	// tracking above are claims about a tee that walked, and neither survives a teleport.
	vec2 m_LastPos = vec2(0.0f, 0.0f);
	bool m_HasLastPos = false;
	// Where along the route the tee was placed, in the arc's own units. Normally just the arc of
	// the node it was placed at, but a branch places it between two nodes, which no single node
	// can express.
	float m_PlacedArc = 0.0f;
	// The node the branch the tee is on comes back to, and the furthest it has been from that
	// point while on this branch, which is what the interpolation measures against.
	int m_BranchRejoin = -1;
	int m_BranchTravel = 0;
	std::vector<vec2> m_vBranchPath;
	int m_TrackedRaceTick = -1;
	// The freeze penalty the live route was searched with, and the one the config last said,
	// which are compared every frame so that dragging the setting re-searches. A conchain cannot
	// do this: the settings page writes g_Config directly and never goes through the console.
	int m_RoutePenalty = 0;
	int m_SeenPenalty = 0;
	float m_PenaltyChangedAt = 0.0f;
	int m_PredictedTicks = -1;
	float m_PredictionBlend = 0.0f;

	/**
	 * Where the tee was along the route, and when, kept for the last little while.
	 *
	 * The prediction used to be the whole run's average pace: elapsed over the fraction done.
	 * That is wrong at the start of nearly every DDRace map, because nearly every DDRace map
	 * opens with a swing, and a swing covers a great deal of route for very little time. The
	 * average says the tee is travelling at swinging speed and predicts a finish to match, and
	 * because an average only ever dilutes what is already in it, the claim stays too fast for
	 * most of the run and merely becomes less wrong as it goes.
	 *
	 * Taking the pace from a window of the recent past instead means the opening leaves the
	 * reckoning entirely once the window has moved past it, rather than being divided down.
	 */
	/**
	 * One slice of the window: how long it lasted and how much ground was made up during it.
	 *
	 * Slices rather than positions, because the rate is wanted per unit of time spent moving and
	 * not per unit of wall clock. A tee that stops contributes slices of no gain, and those are
	 * left out of the reckoning rather than diluting it.
	 */
	class CPaceSample
	{
	public:
		int m_End = -1; // the tick it ran to
		int m_Ticks = 0;
		float m_Gained = 0.0f;
	};
	enum
	{
		PACE_SAMPLES = 24
	};
	CPaceSample m_aPace[PACE_SAMPLES];
	int m_PaceCount = 0;
	int m_PaceNext = 0;
	int m_PaceLastTick = -1;
	float m_PaceLastProgress = 0.0f;

	void ResetPace();
	void SamplePace(int Tick, float Progress);
	/** Progress per tick over the window, or -1 where there is not enough to go on. */
	float PaceRate(int Tick, float Progress, int RaceTick) const;

	void StopWorker();
	void StartSearch();
	void PollSettings();
	bool BuildGrid(CGrid *pGrid) const;
	void CollectJob();
	void UpdateProgress();
	void RenderRoute();
	int NodeAt(vec2 Pos);
	int NodeForTravelLeft(int TravelLeft) const;
	int NodeForArc(float Arc) const;
	int PlaceOnBranch(int Cell);

	static void Search(const std::shared_ptr<CJob> &pJob);
	static bool Coarse(const CGrid &Grid, const std::atomic<bool> &Abort, std::vector<int> *pvDistance, std::vector<int> *pvNext);
	/**
	 * The refined pass, from vSeeds outwards.
	 *
	 * pStopAt, where given, is a per-cell flag of where the search is looking for: the moment one
	 * is settled the search stops and leaves that state in pStopState. Dijkstra settles in
	 * increasing cost, so the first one reached is the cheapest, and a leg costs only what it
	 * takes to get that far rather than a sweep of the whole map.
	 */
	static bool Refine(const CGrid &Grid, const std::atomic<bool> &Abort, const std::vector<int> &vSeeds,
		const std::vector<unsigned char> *pStopAt, int *pStopState,
		std::vector<int> *pvDistance, std::vector<int> *pvNext);
	static bool TravelField(const CGrid &Grid, const std::atomic<bool> &Abort, std::vector<int> *pvTravel,
		std::vector<int> *pvNext);
	static std::vector<int> Trace(const CGrid &Grid, const std::vector<int> &vNext, int Start, int Stride,
		const std::vector<unsigned char> *pStopAt = nullptr);
	/** The state a tee standing on this start tile is in: its freeze state and its checkpoint. */
	static int StartState(const CGrid &Grid, int Cell);
	/**
	 * Picks the start tile, and returns the state to trace from rather than the cell.
	 *
	 * Layered says whether vReachable is the refined field, which has a state per cell per freeze
	 * state per checkpoint, or a flat one with a single state per cell.
	 */
	static int PickStart(const CGrid &Grid, const std::vector<int> &vRank, const std::vector<int> &vReachable,
		bool Layered);
	static int HazardSides(const CGrid &Grid, int SideA, int SideB);

	static void ConFinishPredictionStatus(IConsole::IResult *pResult, void *pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_FINISH_PREDICTION_H
