// EClient
#include "finish_prediction.h"

#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/entity/hud/hud_layout.h>
#include <game/client/components/hud.h>
#include <game/client/components/multiview.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <queue>

// A straight step, in the hundredths-of-a-tile units everything below is costed in. The diagonal
// is that step scaled by sqrt(2), so a staircase costs what the diagonal does and the route does
// not prefer one of them for being counted cheaper than it is.
static constexpr int gs_StepCost = 100;
static constexpr int gs_DiagonalCost = 141;
// A teleporter is nearly free to take, but not free, so that a chain of them cannot tie with a
// shorter walk and leave the trace wandering between exits.
static constexpr int gs_TeleCost = 10;
// Deep freeze costs this much more than ordinary freeze, because time alone never clears it. A
// multiple of ec_finish_prediction_freeze_penalty, which is the real knob: the number of tiles of
// detour the route will take rather than enter one tile of freeze.
static constexpr int gs_DeepMultiplier = 2;
// Travelling one tile while frozen, on top of the step. This is the cost of being out of control
// rather than of the freeze itself, so it is a flat couple of tiles and not a multiple of the
// penalty.
//
// It has to be separate from the cost of entering freeze, and the two were one and the same
// until this comment was written. A single surcharge on "the tee ends up frozen" is paid by the
// freeze tile and by the dry tile next to it alike, so once the bit was set, freeze cost exactly
// what air cost and the route had no reason left to leave it. It would avoid freeze beautifully
// right up to the first crossing and then plough through every patch after it, which is precisely
// what that looks like from inside the game.
static constexpr int gs_FrozenDriftCost = 200;
// Waiting a freeze out where no unfreeze tile is in reach. Flat, and not large: a tee thaws by
// itself in a few seconds, so this is the time that costs, which is worth some tens of tiles
// however hard the route is trying to stay dry.
static constexpr int gs_ThawCost = 4000;
// An edge skip costs this many freeze crossings per hazardous side. Two, because threading the
// exact point where two tiles meet is harder than walking into freeze that at least has an
// unfreeze tile somewhere after it, and a squeeze with freeze on both sides pays it twice.
static constexpr int gs_EdgeSkipMultiplier = 2;
// What one tile of freeze is worth to the progress field, as a multiple of a step. Nothing like
// the routing penalty, and that is the point: the route is being drawn, so it should go a long
// way round rather than touch freeze, but a tee that is already standing on a path with freeze
// ahead of it is going to push through, not walk two thirds of the map backwards. Weighting its
// way out the way the route is weighted makes the readout claim it has further to go than the
// start line did.
static constexpr int gs_TravelFreezeMultiplier = 8;
// And what an edge skip is worth to it, per hazardous side. The travel field is naive on purpose,
// but not about this: it is the field the branch back to the route is drawn from, and a branch
// that threads the corner between two freeze tiles is no more walkable than a route that does.
// Charged on the cost, which picks the way through, and not on the travel that comes out.
static constexpr int gs_TravelEdgeSkipMultiplier = 32;
// Stepping onto a checkpoint teleporter entrance on a map where the search could not afford the
// checkpoint dimension. There is then no destination to follow, so the tile is one to stay off,
// which is usually right: these carpet the places a run goes wrong. Where the dimension is on
// the entrance is a portal like any other and this is not charged at all.
static constexpr int gs_TrapMultiplier = 100;
// How far a tee may walk, on dry ground, from a checkpoint tile to a checkpoint teleporter
// entrance before the search stops treating that entrance as the way onward.
//
// Not a tuning knob so much as the difference between the two things these tiles are used for. A
// mapper who means one as the way onward puts the checkpoint right beside it, so that touching
// one and stepping into the other is a single move. The rest of a map's entrances are kill tiles,
// and that they send a tee holding checkpoint N to exit N is a punishment, not a route.
//
// Without this the difference is invisible to a shortest path, and the consequence is severe
// rather than untidy. Purple Panic 4 carries four hundred and ninety-seven of these scattered
// over the whole map, so from anywhere at all, holding checkpoint N, the exit of checkpoint N was
// a short walk and a few tiles of cost away. The route walked back across the map after every
// checkpoint to find a kill tile, and did it again at the next one.
//
// Dry, and walked rather than measured, because proximity alone is not the test. On BG13 every
// one of the seventeen designed pairs is a dry walk and fifteen are a single tile. On Purple
// Panic 4 not one of the eighteen is: its checkpoints sit in freeze, where a tee cannot reach the
// entrance beside it under its own power at all and the pairing is a coincidence of layout. A box
// drawn round the checkpoint keeps those; asking whether a tee could walk it does not.
static constexpr int gs_TrapReach = 8;
// And how much of that reach may be freeze. A tee cannot walk through freeze, but on a DDRace map
// it does not have to: it gets hooked. So the first tiles out of a checkpoint are allowed to be
// freeze, which covers the pair a tee is carried across by momentum or by a teammate, while a
// crossing long enough to need real dragging is not a step the search gets to assume.
//
// This is where the two maps that drive the rule disagree, so it is worth being plain about what
// it costs. BG13 is unaffected: every one of its seventeen pairs is a dry walk. Purple Panic 4
// has no route at all without the long freeze-bound jumps, because that is how the map is joined
// -- two players, one hooking the other through freeze. Allowing those back gives it a route
// built on a move no single tee can make, which is worse than saying there is none.
static constexpr int gs_TrapCarry = 2;
// What taking a checkpoint teleporter costs, counted in freeze crossings, because that is the
// scale the rest of the search is priced on and a death is not a cheap move.
//
// It had been costed as a teleporter, about twenty tiles against the two hundred a single freeze
// tile costs, which made dying one of the cheapest things a route could do. Where a map allows
// the loose reading below, that is the whole game: the route would walk a long way back across
// the map to reach a kill tile, because arriving at the exit was worth far more than the walk.
// Measured on Purple Panic 4 as the gap, in route nodes, between the checkpoint driving a jump
// and the entrance the route took: a hundred and seventy-four and sixty-eight nodes at the old
// price, seven and three once a death costs this, which is the mapper's own touch-and-go and
// nothing else. It saturates well below this figure, so it is not a delicate one.
static constexpr int gs_TrapDeathFreezes = 20;
// Beyond this the search would cost more memory than the rest of the client. Nothing in DDNet
// comes near it; the limit is only here so a corrupt or hostile map cannot ask for an unbounded
// allocation.
static constexpr int gs_MaxCells = 4 * 1024 * 1024;
// The refined search holds two ints per state, and the checkpoint dimension multiplies the state
// count by the number of groups, so this ceiling is a hundred and twenty-eight megabytes held by
// the worker while it runs and freed as soon as it is done. High, deliberately, because the maps
// that need the dimension are the big ones: BG13 at four hundred by three hundred and fifty with
// its seventeen checkpoints comes to five million states, and a four-hundred-by-five-hundred map
// with thirty of them to fourteen million. Past the ceiling the dimension is dropped and
// checkpoint teleporters go back to being tiles to stay off, which loses the route on a map built
// around them but is better than an allocation nobody asked for.
static constexpr int gs_MaxStates = 16 * 1024 * 1024;
// Guards the packing of a distance into the top half of the queue key, and any map pathological
// enough to reach it is not one this is going to say anything useful about anyway.
static constexpr int gs_MaxDistance = 1 << 30;

// How far back and forward along the route the tee is looked for, in nodes, before the search
// gives up and reacquires from anywhere on the map. A window is needed because a route that
// doubles back through its own corridor has two nodes equally near the tee, and only the one it
// came from is the right answer.
static constexpr int gs_TrackBack = 96;
static constexpr int gs_TrackForward = 384;
// How far off the route the tee may be and still count as on it, in world units
static constexpr float gs_TrackSnap = 480.0f;
// How quickly the shown percentage closes the gap to the real one, as a proportion per second,
// and the least it will move per second regardless.
//
// The rate alone is an exponential approach, which never actually arrives: coming down from a
// long run it would crawl across the last percent for a third of a second after everything else
// had settled. The floor gives the tail a constant speed so it lands.
static constexpr float gs_ProgressBlendRate = 8.0f;
static constexpr float gs_ProgressMinRate = 0.25f;
// And the pair used to bring a finished run down, which is a good deal brisker. Measured against
// the drop that prompted it, sixty-four per cent to nothing: 0.52s on the ordinary pair, 0.23s on
// this one. Fast enough not to sit there being wrong, slow enough to read as the run ending
// rather than as the figure blinking out, and the same either side of a frame rate change --
// 0.233s at sixty frames a second against 0.229s at a hundred and forty-four.
// The pace window: how often it takes a reading, how many it wants before it will answer, and
// how long a stretch those have to cover. Half-second readings over forty-five seconds, and it
// will not speak until it has eight of them.
//
// The window has to be long because progress on a DDRace map is not made at anything like a
// steady rate. A tee flows for three seconds, freezes, climbs back, waits for a teammate, and
// flows again; over twelve seconds the answer depended almost entirely on which part of that
// cycle the window happened to be sitting in, and the flowing part of the cycle is the part that
// looks like a record pace. Forty-five seconds is long enough for a few of those cycles to be in
// it at once.
//
// Eight seconds before it will speak for the same reason. Two and a half seconds is one burst,
// and one burst is exactly the thing that should not be extrapolated over the rest of a map.
static constexpr int gs_PaceIntervalDivisor = 2;
static constexpr int gs_PaceMinSamples = 10;
static constexpr float gs_PaceMinSpan = 8.0f;
// And how much of that span has to have been spent actually travelling before the speed term is
// taken from it, so that the speed is not read off a sliver of movement in an otherwise still
// window. Separate from the span above, which is wall clock: a tee that stalls a great deal
// should still get a figure, just a slow one.
static constexpr float gs_PaceMinMoving = 2.5f;
// Over what span the duty cycle -- the fraction of the run spent travelling at all -- is taken.
//
// Long, because it is the character of the whole run and not a recent event. Three minutes is
// enough that one wait at a drag is worth a fraction of it rather than all of it, and short
// enough that a run which settles down after a bad opening is not judged by that opening
// forever.
static constexpr float gs_DutyHorizon = 180.0f;
// The least the window may have gained before its rate is used. Below this the tee has spent the
// best part of a minute going nowhere, so there is no speed left to measure at all; the previous
// estimate stands instead, which is the honest thing to say about a tee that is not moving.
static constexpr float gs_PaceMinGain = 0.002f;
// How readily the figure may come down, as a fraction of how readily it goes up.
//
// A burst of speed is the normal texture of a run rather than news about the rest of it, so it is
// not allowed to drag the predicted finish earlier as quickly as a genuine slowdown pushes it
// later. The window above decides what the run is doing; this decides how much benefit of the
// doubt a sudden improvement gets, and the answer is: less than a sudden deterioration.
//
// The easing approaches at 4 * this per second, so the figure goes up with a time constant of a
// quarter of a second and comes down with one of about one and two thirds. Worth stating because
// the obvious-looking values do nothing: at 0.35 the two are 0.25s and 0.71s, a difference
// nobody can see. This is the knob to turn if the readout still flatters a burst.
static constexpr float gs_PredictionFallBias = 0.15f;
// How much of the start of a run is left out of the pace altogether.
//
// The window clears an unrepresentative opening on its own, but only once it has slid past it,
// and until then it answers with the opening still in it. Since the race tick says exactly when
// the run began, the opening can be left out from the first reading instead of waited out. On a
// map opening with a three second swing this is the difference between a first honest answer six
// seconds in and sixteen, with nothing but nonsense in between -- better than half a minute of
// the figure the whole-run average gave, which was still eight per cent fast a minute in.
static constexpr float gs_PaceSkipOpening = 4.0f;
static constexpr float gs_ProgressRushRate = 14.0f;
static constexpr float gs_ProgressRushMinRate = 1.0f;
// How many of the nodes in range a cold start will trace a line to before giving up. Only ever
// paid on a frame that has no previous node to work from, so a handful is plenty.
static constexpr size_t gs_ColdStartTraces = 16;
// How far back of where the tee left the route an off-route reading may be and still count as a
// branch rather than a fall back, as a fraction of the whole route.
static constexpr float gs_BranchSlack = 0.25f;
// Moving further than this between two frames was not moving, it was being moved: a map
// teleporter, a practice teleport, a spawn. Scaled by the frame time so that a low frame rate
// does not read a fast tee as having been teleported, with a floor for a high one.
static constexpr float gs_TeleportSpeed = 8000.0f;
static constexpr float gs_TeleportFloor = 320.0f;
// How many tiles of branch the walk will follow before giving up. A branch is normally tens of
// tiles; this is only here so that a field pointing somewhere strange cannot cost a frame.
static constexpr int gs_BranchSteps = 4096;

// The refined pass's extra state bit, as an offset into the same arrays rather than a second set
static constexpr int gs_StateMobile = 0;
static constexpr int gs_StateFrozen = 1;

// The four orthogonal steps, as (dx, dy, the CANTMOVE_* that forbids them)
static constexpr int gs_aaSteps[4][3] = {
	{1, 0, CANTMOVE_RIGHT},
	{0, 1, CANTMOVE_DOWN},
	{-1, 0, CANTMOVE_LEFT},
	{0, -1, CANTMOVE_UP},
};

/**
 * The stoppers, as CCollision::GetMoveRestrictions reads them.
 *
 * Reimplemented rather than called, because the search runs off its own snapshot of the tile
 * layers and so has no CCollision to ask. Doors are left out: they need a switch state, which is
 * not a property of the map.
 */
static int FinishPredictionRawRestrictions(int Tile, int Flags)
{
	Flags = Flags & (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
	switch(Tile)
	{
	case TILE_STOP:
		switch(Flags)
		{
		case ROTATION_0: return CANTMOVE_DOWN;
		case ROTATION_90: return CANTMOVE_LEFT;
		case ROTATION_180: return CANTMOVE_UP;
		case ROTATION_270: return CANTMOVE_RIGHT;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_0): return CANTMOVE_UP;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_90): return CANTMOVE_RIGHT;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_180): return CANTMOVE_DOWN;
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_270): return CANTMOVE_LEFT;
		}
		break;
	case TILE_STOPS:
		switch(Flags)
		{
		case ROTATION_0:
		case ROTATION_180:
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_0):
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_180):
			return CANTMOVE_DOWN | CANTMOVE_UP;
		case ROTATION_90:
		case ROTATION_270:
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_90):
		case static_cast<int>(TILEFLAG_YFLIP) ^ static_cast<int>(ROTATION_270):
			return CANTMOVE_LEFT | CANTMOVE_RIGHT;
		}
		break;
	case TILE_STOPA:
		return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
	}
	return 0;
}

//////////////////////////////////////////////////
// Map snapshot
//////////////////////////////////////////////////

bool CFinishPrediction::BuildGrid(CGrid *pGrid) const
{
	const CCollision *pCollision = GameClient()->Collision();
	if(!pCollision || !pCollision->GameLayer())
		return false;

	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	if(Width < 3 || Height < 3 || (int64_t)Width * (int64_t)Height > (int64_t)gs_MaxCells)
		return false;

	const int Cells = Width * Height;
	pGrid->m_Width = Width;
	pGrid->m_Height = Height;
	pGrid->m_FreezePenalty = std::max(1, g_Config.m_ClFinishPredictionFreezePenalty);
	pGrid->m_TeleporterBias = std::max(0, g_Config.m_ClFinishPredictionTeleporterBias);
	pGrid->m_vInfo.assign(Cells, 0);
	pGrid->m_vBlocked.assign(Cells, 0);

	const CTile *pGame = pCollision->GameLayer();
	const CTile *pFront = pCollision->FrontLayer();
	const CTeleTile *pTele = pCollision->TeleLayer();

	// Teleporter exits, gathered by number so the entrances can be linked to them below. The
	// layer is read directly rather than through CCollision::TeleOuts, which inserts into its own
	// lookup tables as a side effect of being asked about a number that is not in them.
	std::unordered_map<int, std::vector<int>> Exits;
	// Checkout cells by their raw number, and every checkpoint tile's number, so the groups can
	// be worked out once the whole layer has been read
	std::map<int, std::vector<int>> CheckOuts;
	std::vector<std::pair<int, int>> Checks;
	// Time checkpoint cells by their number, which is the tile index above the first of them
	std::map<int, std::vector<int>> TimeCheckpoints;

	std::vector<unsigned char> vRaw(Cells, 0);
	for(int Cell = 0; Cell < Cells; Cell++)
	{
		const int Game = pGame[Cell].m_Index;
		const int Front = pFront ? pFront[Cell].m_Index : 0;
		int Info = 0;

		if(Game == TILE_SOLID || Game == TILE_NOHOOK)
			Info |= TILEINFO_SOLID;
		if(Game == TILE_DEATH || Front == TILE_DEATH)
			Info |= TILEINFO_DEATH;
		if(Game == TILE_FREEZE || Front == TILE_FREEZE || Game == TILE_LFREEZE || Front == TILE_LFREEZE)
			Info |= TILEINFO_FREEZE;
		if(Game == TILE_DFREEZE || Front == TILE_DFREEZE)
			Info |= TILEINFO_FREEZE | TILEINFO_DEEP;
		if(Game == TILE_UNFREEZE || Front == TILE_UNFREEZE ||
			Game == TILE_DUNFREEZE || Front == TILE_DUNFREEZE ||
			Game == TILE_LUNFREEZE || Front == TILE_LUNFREEZE)
			Info |= TILEINFO_UNFREEZE;
		if(Game == TILE_START || Front == TILE_START)
			Info |= TILEINFO_START;
		if(Game == TILE_FINISH || Front == TILE_FINISH)
			Info |= TILEINFO_FINISH;

		// A tile that both freezes and unfreezes is contradictory, and reading it as an unfreeze
		// is what would make the route walk into it, so freeze wins.
		if(Info & TILEINFO_FREEZE)
			Info &= ~TILEINFO_UNFREEZE;

		if(pTele)
		{
			const int Type = pTele[Cell].m_Type;
			const int Number = pTele[Cell].m_Number;
			if(Type == TILE_TELEIN || Type == TILE_TELEINEVIL)
				Info |= TILEINFO_TELEIN;
			else if(Type == TILE_TELECHECKIN || Type == TILE_TELECHECKINEVIL)
				Info |= TILEINFO_TRAP;
			else if(Type == TILE_TELECHECK)
			{
				Info |= TILEINFO_CHECK;
				Checks.emplace_back(Cell, Number);
			}
			else if(Type == TILE_TELECHECKOUT)
			{
				Info |= TILEINFO_CHECKOUT;
				CheckOuts[Number].push_back(Cell);
			}
			else if(Type == TILE_TELEOUT)
				Exits[Number].push_back(Cell);
		}

		pGrid->m_vInfo[Cell] = (unsigned short)Info;
		vRaw[Cell] = (unsigned char)(FinishPredictionRawRestrictions(Game, pGame[Cell].m_Flags) |
					     (pFront ? FinishPredictionRawRestrictions(Front, pFront[Cell].m_Flags) : 0));

		if(Info & TILEINFO_START)
			pGrid->m_vStarts.push_back(Cell);
		if(Info & TILEINFO_FINISH)
			pGrid->m_vFinishes.push_back(Cell);

		for(const int Index : {Game, Front})
		{
			if(Index >= TILE_TIME_CHECKPOINT_FIRST && Index <= TILE_TIME_CHECKPOINT_LAST)
				TimeCheckpoints[Index - TILE_TIME_CHECKPOINT_FIRST].push_back(Cell);
		}
	}

	for(int Cell = 0; Cell < Cells; Cell++)
	{
		// A one-way blocker also holds a tee that is standing on it, which is the one case where
		// a tile's own restriction applies rather than its neighbour's.
		int Blocked = 0;
		if(pGame[Cell].m_Index == TILE_STOP)
			Blocked |= FinishPredictionRawRestrictions(TILE_STOP, pGame[Cell].m_Flags);
		if(pFront && pFront[Cell].m_Index == TILE_STOP)
			Blocked |= FinishPredictionRawRestrictions(TILE_STOP, pFront[Cell].m_Flags);

		const int x = Cell % Width;
		const int y = Cell / Width;
		for(const auto &Step : gs_aaSteps)
		{
			const int Nx = x + Step[0];
			const int Ny = y + Step[1];
			if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Height)
			{
				Blocked |= Step[2];
				continue;
			}
			Blocked |= vRaw[Ny * Width + Nx] & Step[2];
		}
		pGrid->m_vBlocked[Cell] = (unsigned char)Blocked;
	}

	// Link every entrance to its exits, in both directions. The search runs backwards from the
	// finish, so it needs the entrances of a given exit rather than the other way round, and the
	// trace that follows it needs them the usual way round.
	if(pTele)
	{
		for(int Cell = 0; Cell < Cells; Cell++)
		{
			if(!pGrid->Has(Cell, TILEINFO_TELEIN))
				continue;
			const int Number = pTele[Cell].m_Number;
			const auto Found = Exits.find(Number);
			if(Found != Exits.end())
			{
				for(int Exit : Found->second)
				{
					if(!pGrid->Passable(Exit))
						continue;
					pGrid->m_TeleTargets[Cell].push_back(Exit);
					pGrid->m_TeleSources[Exit].push_back(Cell);
				}
			}
			if(pGrid->m_TeleTargets.find(Cell) == pGrid->m_TeleTargets.end())
			{
				// An entrance whose number has no exit is ordinary ground. The server looks its
				// number up, finds the exit list empty and simply carries on, so the tee walks
				// over the tile and nothing happens to it.
				//
				// This used to be marked solid, on the reasoning that an entrance with nowhere to
				// go swallows the tee. It does not, and a map that carpets a corridor with them
				// would have had that corridor walled off.
				pGrid->m_vInfo[Cell] &= (unsigned short)~TILEINFO_TELEIN;
			}
		}
	}

	// The checkpoint teleporters, which are an edge of the search only once the search carries
	// the tee's checkpoint. Everything that needs working out about them is worked out here.
	for(int Cell = 0; Cell < Cells; Cell++)
		if(pGrid->Has(Cell, TILEINFO_TRAP))
			pGrid->m_vTraps.push_back(Cell);

	if(CheckOuts.empty())
	{
		// A map with traps and no exits for them at all. The server's last resort then always
		// applies and every one of them puts the tee back on the start line, so they are
		// run-enders rather than ground, and the same is already done just above to a teleporter
		// entrance whose exit is missing. Left walkable, they were worth crossing on one of the
		// test maps eighty-nine times over, and a route that restarts the run eighty-nine times
		// reports a percentage for ground the tee never covers.
		for(int Cell : pGrid->m_vTraps)
		{
			pGrid->m_vInfo[Cell] |= TILEINFO_SOLID;
			pGrid->m_vInfo[Cell] &= (unsigned short)~TILEINFO_TRAP;
		}
		pGrid->m_vTraps.clear();
	}
	else if(!pGrid->m_vTraps.empty())
	{
		// One group per distinct checkout number, plus group 0 for a tee that has not touched a
		// checkpoint yet, whose traps only ever send it back to the start and so are dead ends.
		//
		// A checkpoint resolves to the highest checkout number at or below its own, which is
		// what the server does when it walks its checkpoint down looking for an exit that exists.
		std::vector<int> vNumbers;
		for(const auto &[Number, vCells] : CheckOuts)
			vNumbers.push_back(Number);
		const int Layers = (int)vNumbers.size() + 1;
		if((int64_t)Cells * 2 * Layers <= (int64_t)gs_MaxStates)
		{
			pGrid->m_CheckpointLayers = Layers;
			for(size_t Index = 0; Index < vNumbers.size(); Index++)
				for(int Cell : CheckOuts[vNumbers[Index]])
					pGrid->m_CheckoutGroupByCell[Cell] = (int)Index + 1;
			for(const auto &[Cell, Number] : Checks)
			{
				// The highest checkout number at or below this checkpoint's
				int Group = 0;
				for(size_t Index = 0; Index < vNumbers.size(); Index++)
					if(vNumbers[Index] <= Number)
						Group = (int)Index + 1;
				pGrid->m_CheckGroupByCell[Cell] = Group;
			}

			// And which entrances a tee could walk to from each group's own checkpoint tiles,
			// which are the only ones that group's layer may use
			std::vector<int> vQueue;
			std::vector<int> vSeenAt(Cells, -1);
			int Sweep = 0;
			for(const auto &[Start, Group] : pGrid->m_CheckGroupByCell)
			{
				// A tee standing on a checkpoint laid in freeze is frozen the moment it arrives
				// and walks nowhere at all
				if(Group <= 0 || !pGrid->Passable(Start) || pGrid->Has(Start, TILEINFO_FREEZE))
					continue;
				std::vector<int> &vReachable = pGrid->m_TrapsByGroup[Group];
				const int Mark = Sweep++;
				vQueue.assign(1, Start);
				vSeenAt[Start] = Mark;
				for(int Step = 0; Step < gs_TrapReach && !vQueue.empty(); Step++)
				{
					std::vector<int> vNext;
					for(int Cell : vQueue)
					{
						const int x = Cell % Width;
						const int y = Cell / Width;
						const auto Visit = [&](int Nx, int Ny) {
							if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Height)
								return;
							const int To = Ny * Width + Nx;
							if(vSeenAt[To] == Mark || !pGrid->Passable(To))
								return;
							vSeenAt[To] = Mark;
							// An entrance is the destination, not a tile to walk on: standing on
							// one takes the tee away before it can step off.
							if(pGrid->Has(To, TILEINFO_TRAP))
								vReachable.push_back(To);
							else if(!pGrid->Has(To, TILEINFO_TELEIN) &&
								(!pGrid->Has(To, TILEINFO_FREEZE) || Step < gs_TrapCarry))
								vNext.push_back(To);
						};
						for(const auto &Step2 : gs_aaSteps)
						{
							if(!(pGrid->m_vBlocked[Cell] & Step2[2]))
								Visit(x + Step2[0], y + Step2[1]);
						}
						for(int Dx = -1; Dx <= 1; Dx += 2)
						{
							for(int Dy = -1; Dy <= 1; Dy += 2)
							{
								const int Block = (Dx > 0 ? CANTMOVE_RIGHT : CANTMOVE_LEFT) |
										  (Dy > 0 ? CANTMOVE_DOWN : CANTMOVE_UP);
								if(pGrid->m_vBlocked[Cell] & Block)
									continue;
								if(x + Dx < 0 || x + Dx >= Width || y + Dy < 0 || y + Dy >= Height)
									continue;
								if(!pGrid->Passable(y * Width + x + Dx) || !pGrid->Passable((y + Dy) * Width + x))
									continue;
								Visit(x + Dx, y + Dy);
							}
						}
					}
					vQueue = std::move(vNext);
				}
			}

			// Which exits each of those entrances means, for the passes that carry no checkpoint
			std::vector<std::vector<int>> vvCheckoutsByGroup(Layers);
			for(const auto &[Cell, Group] : pGrid->m_CheckoutGroupByCell)
				vvCheckoutsByGroup[Group].push_back(Cell);
			for(auto &[Group, vReachable] : pGrid->m_TrapsByGroup)
			{
				std::sort(vReachable.begin(), vReachable.end());
				vReachable.erase(std::unique(vReachable.begin(), vReachable.end()), vReachable.end());
				for(int Exit : vvCheckoutsByGroup[Group])
					for(int Trap : vReachable)
						pGrid->m_TrapSources[Exit].push_back(Trap);
			}
			for(auto &[Exit, vTraps] : pGrid->m_TrapSources)
			{
				std::sort(vTraps.begin(), vTraps.end());
				vTraps.erase(std::unique(vTraps.begin(), vTraps.end()), vTraps.end());
			}
		}
	}

	// In number order, and only the ones a tee could stand on: a checkpoint buried in a wall
	// would make its leg unroutable and cost the whole route.
	for(const auto &[Number, vCells] : TimeCheckpoints)
	{
		std::vector<int> vUsable;
		for(int Cell : vCells)
			if(pGrid->Passable(Cell))
				vUsable.push_back(Cell);
		if(!vUsable.empty())
			pGrid->m_vvTimeCheckpoints.push_back(std::move(vUsable));
	}

	// Without start tiles the race has no beginning to measure from, so the spawns stand in for
	// them. They are where the tee is before it has started, which is near enough.
	if(pGrid->m_vStarts.empty())
	{
		for(int Cell = 0; Cell < Cells; Cell++)
		{
			const int Game = pGame[Cell].m_Index;
			if(Game == ENTITY_SPAWN + ENTITY_OFFSET || Game == ENTITY_SPAWN_RED + ENTITY_OFFSET || Game == ENTITY_SPAWN_BLUE + ENTITY_OFFSET)
				pGrid->m_vStarts.push_back(Cell);
		}
	}

	return !pGrid->m_vStarts.empty() && !pGrid->m_vFinishes.empty();
}

//////////////////////////////////////////////////
// Search
//////////////////////////////////////////////////

/**
 * How many of a diagonal step's two side tiles are something it should not be threading between.
 *
 * Freeze and teleporter entrances. Solid and death tiles are not listed because they stop the
 * diagonal outright, and nothing else on a tile grid catches a tee that clips its corner.
 */
int CFinishPrediction::HazardSides(const CGrid &Grid, int SideA, int SideB)
{
	const auto Threaded = [&](int Side) {
		return Grid.Has(Side, TILEINFO_FREEZE | TILEINFO_TELEIN | TILEINFO_TRAP);
	};
	return (Threaded(SideA) ? 1 : 0) + (Threaded(SideB) ? 1 : 0);
}

/**
 * The coarse pass: a reverse Dijkstra from every finish tile that ignores freeze.
 *
 * Reverse, because what is wanted is the cost from every tile to the finish rather than from one
 * tile to everywhere, so popping a cell relaxes the cells that can step *into* it.
 */
bool CFinishPrediction::Coarse(const CGrid &Grid, const std::atomic<bool> &Abort, std::vector<int> *pvDistance, std::vector<int> *pvNext)
{
	const int Cells = Grid.Cells();
	const int Width = Grid.m_Width;
	// Checkpoint teleporters, as permissively as possible: every entrance on the map leads to
	// every exit. Deliberately looser than anything the refined pass will do, and loose on
	// purpose, because this field's only job is to rank the start tiles and PickStart throws out
	// any start it cannot see. A pass that vetoes a start the refined search could have routed
	// from costs the whole route, which is exactly what happened on every map built on the
	// mechanic until this followed them at all. Over-reaching here costs nothing but a rank.
	const bool Checkpoints = Grid.m_CheckpointLayers > 1;
	const int NoWalkIn = TILEINFO_TELEIN | (Checkpoints ? TILEINFO_TRAP : 0);
	pvDistance->assign(Cells, -1);
	pvNext->assign(Cells, -1);

	// Packed as the distance in the high word and the cell in the low one, so the default
	// comparison orders by distance and the queue needs no comparator of its own.
	std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>> Queue;
	for(int Finish : Grid.m_vFinishes)
	{
		if(!Grid.Passable(Finish) || (*pvDistance)[Finish] == 0)
			continue;
		(*pvDistance)[Finish] = 0;
		Queue.push((uint64_t)Finish);
	}
	if(Queue.empty())
		return false;

	int Guard = 0;
	while(!Queue.empty())
	{
		if((++Guard & 0xffff) == 0 && Abort.load())
			return false;

		const uint64_t Top = Queue.top();
		Queue.pop();
		const int Distance = (int)(Top >> 32);
		const int Cell = (int)(Top & 0xffffffffu);
		if((*pvDistance)[Cell] != Distance)
			continue;

		const auto Relax = [&](int From, int Cost) {
			if(!Grid.Passable(From))
				return;
			const int Value = Distance + Cost;
			if(Value > gs_MaxDistance)
				return;
			if((*pvDistance)[From] >= 0 && (*pvDistance)[From] <= Value)
				return;
			(*pvDistance)[From] = Value;
			(*pvNext)[From] = Cell;
			Queue.push(((uint64_t)Value << 32) | (uint64_t)From);
		};

		// Whoever arrives here by teleporter. The lookup is skipped outright on the majority of
		// maps that have none, since it would otherwise be one hash probe per popped cell.
		if(!Grid.m_TeleSources.empty())
		{
			if(const auto Found = Grid.m_TeleSources.find(Cell); Found != Grid.m_TeleSources.end())
			{
				for(int Source : Found->second)
					Relax(Source, gs_TeleCost);
			}
		}

		if(Checkpoints && Grid.Has(Cell, TILEINFO_CHECKOUT))
		{
			for(int Trap : Grid.m_vTraps)
				Relax(Trap, gs_TeleCost);
		}

		const int x = Cell % Width;
		const int y = Cell / Width;
		for(const auto &Step : gs_aaSteps)
		{
			const int Nx = x - Step[0];
			const int Ny = y - Step[1];
			if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
				continue;
			const int From = Ny * Width + Nx;
			// A tee standing on an entrance is taken away before it can walk off, so an entrance
			// only ever leads to its own exits.
			if(Grid.Has(From, NoWalkIn) || (Grid.m_vBlocked[From] & Step[2]))
				continue;
			Relax(From, gs_StepCost);
		}

		// Diagonals, only where both of their straight halves are open, so the route cannot slip
		// through the seam between two solid tiles meeting at a corner.
		for(int Dx = -1; Dx <= 1; Dx += 2)
		{
			for(int Dy = -1; Dy <= 1; Dy += 2)
			{
				const int Nx = x - Dx;
				const int Ny = y - Dy;
				if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
					continue;
				const int From = Ny * Width + Nx;
				if(Grid.Has(From, NoWalkIn))
					continue;
				if(!Grid.Passable(Ny * Width + x) || !Grid.Passable(y * Width + Nx))
					continue;
				const int Block = (Dx > 0 ? CANTMOVE_RIGHT : CANTMOVE_LEFT) | (Dy > 0 ? CANTMOVE_DOWN : CANTMOVE_UP);
				if(Grid.m_vBlocked[From] & Block)
					continue;
				Relax(From, gs_DiagonalCost);
			}
		}
	}

	return true;
}

/**
 * The refined pass: the same reverse Dijkstra over twice the states, the extra bit being whether
 * the tee is frozen.
 *
 * Read forwards, a state is (cell, frozen) and the edges are
 *
 *     (X, f) -> (Y, 0)   Y unfreezes, one step
 *     (X, f) -> (Y, 1)   Y freezes, one step plus the cost of entering freeze, plus drift
 *     (X, f) -> (Y, f)   anything else, plus drift while f is set
 *     (X, 1) -> (X, 0)   X does not freeze, gs_ThawCost
 *
 * and what follows relaxes their predecessors instead. Because the surcharge is charged on the
 * tile being entered and an unfreeze tile is the one cheap way out of the frozen state, the
 * shortest path through this graph goes round freeze wherever there is a way round, crosses it at
 * its narrowest and onto an unfreeze tile where there is not, and still finds something on a map
 * where neither is on offer.
 */
bool CFinishPrediction::Refine(const CGrid &Grid, const std::atomic<bool> &Abort, const std::vector<int> &vSeeds,
	const std::vector<unsigned char> *pStopAt, int *pStopState,
	std::vector<int> *pvDistance, std::vector<int> *pvNext)
{
	if(pStopState)
		*pStopState = -1;
	const int Cells = Grid.Cells();
	const int Width = Grid.m_Width;
	// One tile of freeze is worth this many tiles of walking, so the route will go that far out of
	// its way rather than touch it. Finite, so a map with no dry way round still gets a route.
	// What it costs to set foot on freeze, which is the figure the penalty names
	const int FreezeEnterCost = Grid.m_FreezePenalty * gs_StepCost;
	const int DeepEnterCost = FreezeEnterCost * gs_DeepMultiplier;
	const int TeleEnterCost = gs_TeleCost + Grid.m_TeleporterBias * gs_StepCost;
	// The checkpoint dimension: one layer per distinct checkout number, plus layer 0 for a tee
	// that has touched no checkpoint yet. A map without the mechanic has one layer and every
	// state index below collapses back to what it was, at no cost to anything.
	const int Layers = std::max(1, Grid.m_CheckpointLayers);
	const bool Checkpoints = Layers > 1;
	// With the dimension on, a checkpoint teleporter entrance is a portal whose destination the
	// search knows, so it is priced like any other teleporter. Without it there is no destination
	// to follow and the only honest thing left is to stay off the tile.
	const int TrapCost = Checkpoints ? 0 : gs_TrapMultiplier * gs_StepCost;
	// Relative to freeze, so it follows the penalty the player set rather than drifting away
	// from it
	const int TrapEnterCost = TeleEnterCost + gs_TrapDeathFreezes * FreezeEnterCost;
	// Walking onto one is taking it, so with the dimension on nothing may leave a trap on foot
	const int NoWalkIn = TILEINFO_TELEIN | (Checkpoints ? TILEINFO_TRAP : 0);
	const auto EdgeSkipCost = [&](int SideA, int SideB) {
		return HazardSides(Grid, SideA, SideB) * gs_EdgeSkipMultiplier * FreezeEnterCost;
	};
	const size_t States = (size_t)Cells * 2 * Layers;
	pvDistance->assign(States, -1);
	pvNext->assign(States, -1);

	std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>> Queue;
	for(int Finish : vSeeds)
	{
		if(!Grid.Passable(Finish))
			continue;
		// Seeded in whichever state the tile actually allows, and that is the whole point of the
		// loop. A finish line laid on freeze is a standard way to build one, since the freeze is
		// what stops the tee, and such a tile has no mobile state at all.
		//
		// Seeding only the mobile state left this pass with nowhere to go from its own goal. On a
		// map whose every finish tile is freeze it reached twenty-seven cells out of six hundred
		// thousand, the route fell back to the coarse one, and the coarse one walks through
		// freeze wherever it likes. From inside the game that looks like a refinement that gives
		// up after the first crossing, which is nothing like the truth.
		//
		// Both states for an ordinary tile: arriving frozen still finishes.
		const bool Freezes = Grid.Has(Finish, TILEINFO_FREEZE);
		const bool Unfreezes = Grid.Has(Finish, TILEINFO_UNFREEZE);
		// And in every layer, since a tee crosses the line holding whatever checkpoint it last
		// touched and which one that was is no business of the line's.
		const int Group = Grid.Has(Finish, TILEINFO_CHECK) ? Grid.CheckGroup(Finish) : -1;
		for(int Layer = 0; Layer < Layers; Layer++)
		{
			if(Group >= 0 && Group != Layer)
				continue;
			for(int Frozen = 0; Frozen < 2; Frozen++)
			{
				if(Frozen == gs_StateMobile ? Freezes : Unfreezes)
					continue;
				const int State = ((Layer * 2) + Frozen) * Cells + Finish;
				if((*pvDistance)[State] == 0)
					continue;
				(*pvDistance)[State] = 0;
				Queue.push((uint64_t)State);
			}
		}
	}
	if(Queue.empty())
		return false;

	int Guard = 0;
	while(!Queue.empty())
	{
		if((++Guard & 0xffff) == 0 && Abort.load())
			return false;

		const uint64_t Top = Queue.top();
		Queue.pop();
		const int Distance = (int)(Top >> 32);
		const int State = (int)(Top & 0xffffffffu);
		if((*pvDistance)[State] != Distance)
			continue;

		const int Cell = State % Cells;
		const int Frozen = (State / Cells) % 2;
		const int Layer = State / (Cells * 2);

		// Arrived where this leg was headed, by the cheapest way there is
		if(pStopAt && (*pStopAt)[Cell])
		{
			if(pStopState)
				*pStopState = State;
			return true;
		}

		// Which checkpoint this cell forces on a tee standing on it, if it is one at all
		const int CheckHere = Grid.Has(Cell, TILEINFO_CHECK) ? Grid.CheckGroup(Cell) : -1;

		const auto Relax = [&](int From, int FromFrozen, int FromLayer, int Cost) {
			if(!Grid.Passable(From))
				return;
			// A tee standing on a checkpoint is holding that checkpoint, whatever it held on the
			// way in, so that cell has no state in any other layer.
			if(Grid.Has(From, TILEINFO_CHECK))
			{
				const int Group = Grid.CheckGroup(From);
				if(Group >= 0 && Group != FromLayer)
					return;
			}
			// A tee on a freeze tile is frozen and a tee on an unfreeze tile is not, so the other
			// half of those cells' states describes nothing and is left out. Reaching one is a
			// dead end rather than a wrong answer, since nothing relaxes back out of it, but it
			// is still half the queue on a map with a lot of freeze.
			if(Grid.Has(From, FromFrozen == gs_StateMobile ? TILEINFO_FREEZE : TILEINFO_UNFREEZE))
				return;
			const int Value = Distance + Cost;
			if(Value > gs_MaxDistance)
				return;
			const int FromState = ((FromLayer * 2) + FromFrozen) * Cells + From;
			if((*pvDistance)[FromState] >= 0 && (*pvDistance)[FromState] <= Value)
				return;
			(*pvDistance)[FromState] = Value;
			(*pvNext)[FromState] = State;
			Queue.push(((uint64_t)Value << 32) | (uint64_t)FromState);
		};

		const bool Freezes = Grid.Has(Cell, TILEINFO_FREEZE);
		const bool Unfreezes = Grid.Has(Cell, TILEINFO_UNFREEZE);

		// Thawing in place, for the crossing that has no unfreeze tile to land on
		if(Frozen == gs_StateMobile && !Freezes && !Unfreezes)
			Relax(Cell, gs_StateFrozen, Layer, gs_ThawCost);

		// Which states this cell can be stepped into from, and what the step costs. Freeze is
		// charged on arrival, so a crossing pays for its own width.
		// Two separate charges, and keeping them separate is the whole point. Setting foot on
		// freeze costs what the penalty says, every tile of it. Travelling while frozen costs a
		// little whatever is underfoot. A freeze tile pays both, so even a tee that is already
		// frozen is strongly pushed out of the freeze rather than along it.
		int Surcharge = 0;
		bool FromMobile = false;
		bool FromFrozen = false;
		if(Freezes)
		{
			// Reachable only as frozen, out of either state
			FromMobile = FromFrozen = Frozen == gs_StateFrozen;
			Surcharge = (Grid.Has(Cell, TILEINFO_DEEP) ? DeepEnterCost : FreezeEnterCost) + gs_FrozenDriftCost;
			if(Grid.Has(Cell, TILEINFO_TRAP))
				Surcharge += TrapCost;
		}
		else if(Unfreezes)
		{
			// Reachable only as thawed, out of either state. A deep unfreeze clears ordinary
			// freeze too, and telling the two apart is not worth a third state.
			FromMobile = FromFrozen = Frozen == gs_StateMobile;
		}
		else
		{
			// Carries the state through
			FromMobile = Frozen == gs_StateMobile;
			FromFrozen = Frozen == gs_StateFrozen;
			Surcharge = Frozen == gs_StateFrozen ? gs_FrozenDriftCost : 0;
			if(Grid.Has(Cell, TILEINFO_TRAP))
				Surcharge += TrapCost;
		}
		if(!FromMobile && !FromFrozen)
			continue;

		// Every step keeps the tee's checkpoint, except the one onto a checkpoint tile: that
		// sets it, so a tee out of any layer could have been the one that took it.
		const int FirstLayer = CheckHere >= 0 ? 0 : Layer;
		const int LastLayer = CheckHere >= 0 ? Layers - 1 : Layer;
		const auto RelaxBoth = [&](int From, int Cost) {
			for(int FromLayer = FirstLayer; FromLayer <= LastLayer; FromLayer++)
			{
				if(FromMobile)
					Relax(From, gs_StateMobile, FromLayer, Cost);
				if(FromFrozen)
					Relax(From, gs_StateFrozen, FromLayer, Cost);
			}
		};

		// The checkpoint teleporters. A trap sends the tee to the exit of the checkpoint it is
		// holding, so in this layer it is this cell that every trap leads to -- but only the
		// traps this group's own checkpoint tiles stand next to are taken, which is what keeps
		// the whole map from collapsing into "walk to the nearest kill tile". See gs_TrapReach.
		if(Checkpoints && Grid.Has(Cell, TILEINFO_CHECKOUT) && Grid.CheckoutGroup(Cell) == Layer)
		{
			if(const auto Found = Grid.m_TrapsByGroup.find(Layer); Found != Grid.m_TrapsByGroup.end())
			{
				for(int Trap : Found->second)
					RelaxBoth(Trap, TrapEnterCost + Surcharge);
			}
		}

		if(!Grid.m_TeleSources.empty())
		{
			if(const auto Found = Grid.m_TeleSources.find(Cell); Found != Grid.m_TeleSources.end())
			{
				for(int Source : Found->second)
					RelaxBoth(Source, TeleEnterCost + Surcharge);
			}
		}

		const int x = Cell % Width;
		const int y = Cell / Width;
		for(const auto &Step : gs_aaSteps)
		{
			const int Nx = x - Step[0];
			const int Ny = y - Step[1];
			if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
				continue;
			const int From = Ny * Width + Nx;
			if(Grid.Has(From, NoWalkIn) || (Grid.m_vBlocked[From] & Step[2]))
				continue;
			RelaxBoth(From, gs_StepCost + Surcharge);
		}

		for(int Dx = -1; Dx <= 1; Dx += 2)
		{
			for(int Dy = -1; Dy <= 1; Dy += 2)
			{
				const int Nx = x - Dx;
				const int Ny = y - Dy;
				if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
					continue;
				const int From = Ny * Width + Nx;
				if(Grid.Has(From, NoWalkIn))
					continue;
				// The two tiles the diagonal passes between
				const int SideX = Ny * Width + x;
				const int SideY = y * Width + Nx;
				if(!Grid.Passable(SideX) || !Grid.Passable(SideY))
					continue;
				const int Block = (Dx > 0 ? CANTMOVE_RIGHT : CANTMOVE_LEFT) | (Dy > 0 ? CANTMOVE_DOWN : CANTMOVE_UP);
				if(Grid.m_vBlocked[From] & Block)
					continue;
				// An edge skip. A diagonal passes through the single point where its two side
				// tiles meet, and where a side tile is freeze or a teleporter entrance the tee
				// has to thread that point exactly or be caught by it. That is a trick, not a
				// route, so each such side is charged a freeze crossing over again.
				//
				// Note what this costs if it is left out: neither side tile is solid, so the
				// step is legal and cheap, and because the route never enters either tile it is
				// recorded as crossing no freeze at all. A free shortcut that also reports
				// itself as clean.
				//
				// Charged rather than forbidden, so the rare map that genuinely demands the skip
				// still gets a route, after paying a long detour's worth for it.
				RelaxBoth(From, gs_DiagonalCost + Surcharge + EdgeSkipCost(SideX, SideY));
			}
		}
	}

	return true;
}

/**
 * How far there is left to travel to the finish from every tile.
 *
 * A third pass, and a deliberately naive one. It has no frozen state and no edge-skip rule, and
 * it prices freeze at a few steps rather than a few hundred, so its idea of the way out of
 * anywhere is roughly what a player's would be: push through a little freeze, walk round a lot of
 * it. The route is not drawn from this; it only answers how far from done a tee is.
 *
 * Two values are carried. Cost picks the tree, mildly avoiding freeze. Travel accumulates the
 * actual length of each step taken, which is what comes out, because a surcharge is distance the
 * tee never moves and a teleporter moves it without travelling. Reading the cost out instead
 * would put a route over a few hundred freeze tiles thousands of tiles from the finish.
 */
bool CFinishPrediction::TravelField(const CGrid &Grid, const std::atomic<bool> &Abort, std::vector<int> *pvTravel,
	std::vector<int> *pvNext)
{
	const int Cells = Grid.Cells();
	const int Width = Grid.m_Width;
	// Only the checkpoint links a tee can take on purpose, never the loose reading the route may
	// have needed, because what this field measures is how much travel is left and arriving
	// somewhere by dying is not travel.
	//
	// Following them anyway does not merely overstate things, it destroys the measure. A
	// teleport is zero distance by construction, so a trap reached from an exit takes that
	// exit's travel, and every cell behind the trap inherits it. Purple Panic 4 carpets the map
	// with four hundred and ninety-seven of them, so standing almost anywhere read as nearly
	// finished -- eighty per cent, a few tiles off the route. Charging the death does not help:
	// cost decides which way the field points, not the figure it stores.
	//
	// A route that needed the loose reading therefore has no honest off-route measure at all,
	// and NodeAt refuses to give one rather than inventing it.
	const bool Checkpoints = Grid.m_CheckpointLayers > 1;
	const int NoWalkIn = TILEINFO_TELEIN | (Checkpoints ? TILEINFO_TRAP : 0);
	std::vector<int> vCost(Cells, -1);
	pvTravel->assign(Cells, -1);
	pvNext->assign(Cells, -1);

	std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>> Queue;
	for(int Finish : Grid.m_vFinishes)
	{
		if(!Grid.Passable(Finish) || vCost[Finish] == 0)
			continue;
		vCost[Finish] = 0;
		(*pvTravel)[Finish] = 0;
		Queue.push((uint64_t)Finish);
	}
	if(Queue.empty())
		return false;

	int Guard = 0;
	while(!Queue.empty())
	{
		if((++Guard & 0xffff) == 0 && Abort.load())
			return false;

		const uint64_t Top = Queue.top();
		Queue.pop();
		const int Cost = (int)(Top >> 32);
		const int Cell = (int)(Top & 0xffffffffu);
		if(vCost[Cell] != Cost)
			continue;
		const int TravelHere = (*pvTravel)[Cell];
		int Surcharge = Grid.Has(Cell, TILEINFO_FREEZE) ? gs_StepCost * gs_TravelFreezeMultiplier : 0;
		if(Grid.Has(Cell, TILEINFO_TRAP))
			Surcharge += gs_TrapMultiplier * gs_StepCost;

		// Step is the distance moved and comes out as travel. Extra is paid for choosing this way
		// rather than another and does not, since the tee does not move further for it.
		const auto Relax = [&](int From, int Step, int Extra) {
			if(!Grid.Passable(From))
				return;
			const int Value = Cost + Step + Surcharge + Extra;
			if(Value > gs_MaxDistance)
				return;
			if(vCost[From] >= 0 && vCost[From] <= Value)
				return;
			vCost[From] = Value;
			(*pvTravel)[From] = TravelHere + Step;
			(*pvNext)[From] = Cell;
			Queue.push(((uint64_t)Value << 32) | (uint64_t)From);
		};

		if(!Grid.m_TeleSources.empty())
		{
			if(const auto Found = Grid.m_TeleSources.find(Cell); Found != Grid.m_TeleSources.end())
			{
				for(int Source : Found->second)
				{
					// A teleporter is no distance travelled, which is also how the route's own
					// arc counts it
					if(Grid.Passable(Source))
						Relax(Source, 0, Grid.m_TeleporterBias * gs_StepCost);
				}
			}
		}

		if(Checkpoints && !Grid.m_TrapSources.empty())
		{
			if(const auto Found = Grid.m_TrapSources.find(Cell); Found != Grid.m_TrapSources.end())
			{
				// Charged as cost and not as distance, so that discouraging a death does not
				// also pretend the tee walked a long way to take it. Priced in this pass's own
				// milder units, where a freeze tile is eight steps rather than two hundred.
				for(int Trap : Found->second)
					Relax(Trap, 0, gs_TrapDeathFreezes * gs_StepCost * gs_TravelFreezeMultiplier);
			}
		}

		const int x = Cell % Width;
		const int y = Cell / Width;
		for(const auto &Step : gs_aaSteps)
		{
			const int Nx = x - Step[0];
			const int Ny = y - Step[1];
			if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
				continue;
			const int From = Ny * Width + Nx;
			if(Grid.Has(From, NoWalkIn) || (Grid.m_vBlocked[From] & Step[2]))
				continue;
			Relax(From, gs_StepCost, 0);
		}

		for(int Dx = -1; Dx <= 1; Dx += 2)
		{
			for(int Dy = -1; Dy <= 1; Dy += 2)
			{
				const int Nx = x - Dx;
				const int Ny = y - Dy;
				if(Nx < 0 || Nx >= Width || Ny < 0 || Ny >= Grid.m_Height)
					continue;
				const int From = Ny * Width + Nx;
				if(Grid.Has(From, TILEINFO_TELEIN))
					continue;
				const int SideX = Ny * Width + x;
				const int SideY = y * Width + Nx;
				if(!Grid.Passable(SideX) || !Grid.Passable(SideY))
					continue;
				const int Block = (Dx > 0 ? CANTMOVE_RIGHT : CANTMOVE_LEFT) | (Dy > 0 ? CANTMOVE_DOWN : CANTMOVE_UP);
				if(Grid.m_vBlocked[From] & Block)
					continue;
				Relax(From, gs_DiagonalCost,
					HazardSides(Grid, SideX, SideY) * gs_TravelEdgeSkipMultiplier * gs_StepCost);
			}
		}
	}

	return true;
}

/**
 * Walks a successor field from one state to the finish.
 *
 * Stride is the number of cells per state, so the same code reads the coarse field, which has one
 * state per cell, and the refined one, which has two. Repeated cells are dropped, because the
 * thaw edge's successor is on the cell it started from.
 */
std::vector<int> CFinishPrediction::Trace(const CGrid &Grid, const std::vector<int> &vNext, int Start, int Stride,
	const std::vector<unsigned char> *pStopAt)
{
	std::vector<int> vPath;
	// The distance strictly decreases along the trace, so no state repeats and this bound cannot
	// be reached. Bailing out rather than asserting keeps a broken map from taking the client
	// down with it.
	const size_t Limit = vNext.size() + 2;
	int State = Start;
	while(State >= 0 && (size_t)State < vNext.size())
	{
		const int Cell = State % Stride;
		if(vPath.empty() || vPath.back() != Cell)
			vPath.push_back(Cell);
		if(pStopAt ? (*pStopAt)[Cell] != 0 : Grid.Has(Cell, TILEINFO_FINISH))
			break;
		if(vPath.size() > Limit)
		{
			vPath.clear();
			break;
		}
		State = vNext[State];
	}
	return vPath;
}

/**
 * Picks the start tile the route is measured from.
 *
 * The one with the most map ahead of it, so that a map whose start tiles are spread out does not
 * get a quietly shortened route, and with it every percentage read off that route. "Most" is
 * judged on vRank, which is the coarse field: ranking by the refined cost instead would pick
 * whichever start tile has the *worst* route rather than the one furthest from the finish, since
 * the refined cost is mostly freeze penalty wherever there is freeze.
 *
 * The candidate also has to be somewhere the search that will be traced can actually begin,
 * which is what vReachable is for. Both fields put the mobile state first, so a cell indexes
 * either of them directly.
 *
 * Checking that is not a formality. A start line capped with a freeze tile at each end is a
 * normal mapping idiom, and such a tile has a perfectly good coarse distance and no mobile state
 * at all. Choosing it on the coarse field alone threw away the entire refined route and fell
 * back to the one that ignores freeze, which is exactly what it looked like: a refinement that
 * had stopped working.
 */
int CFinishPrediction::StartState(const CGrid &Grid, int Cell)
{
	// Mobile unless the tile freezes on contact, and holding whatever checkpoint the tile sets.
	//
	// Laying the start line on a tele checkpoint is an ordinary thing for a mapper to do, and
	// such a tile exists only in that checkpoint's layer -- the search refuses it in any other,
	// because a tee standing there is holding that checkpoint whatever it held before. Reading
	// the field at the bare cell index asks about layer zero, which on those maps is the one
	// layer the start does not have.
	const int Frozen = Grid.Has(Cell, TILEINFO_FREEZE) ? gs_StateFrozen : gs_StateMobile;
	int Layer = 0;
	if(Grid.Has(Cell, TILEINFO_CHECK))
		Layer = std::max(0, Grid.CheckGroup(Cell));
	return ((Layer * 2) + Frozen) * Grid.Cells() + Cell;
}

int CFinishPrediction::PickStart(const CGrid &Grid, const std::vector<int> &vRank, const std::vector<int> &vReachable,
	bool Layered)
{
	int Start = -1;
	int Best = -1;
	for(int Candidate : Grid.m_vStarts)
	{
		if(!Grid.Passable(Candidate) || vRank[Candidate] < 0)
			continue;
		const int State = Layered ? StartState(Grid, Candidate) : Candidate;
		if(State < 0 || (size_t)State >= vReachable.size() || vReachable[State] < 0)
			continue;
		if(vRank[Candidate] > Best)
		{
			Best = vRank[Candidate];
			Start = State;
		}
	}
	return Start;
}

void CFinishPrediction::Search(const std::shared_ptr<CJob> &pJob)
{
	const CGrid &Grid = pJob->m_Grid;
	const std::atomic<bool> &Abort = pJob->m_Abort;
	const int Cells = Grid.Cells();

	std::vector<int> vCoarseDistance;
	std::vector<int> vCoarseNext;
	if(!Coarse(Grid, Abort, &vCoarseDistance, &vCoarseNext) || Abort.load())
	{
		pJob->m_Done.store(true);
		return;
	}

	std::vector<int> vPath;
	std::vector<int> vTravelLeft;
	std::vector<int> vTravelNext;
	bool Fallback = false;
	int UsableStarts = 0;
	int Legs = 0;
	{
		std::vector<int> vDistance;
		std::vector<int> vNext;
		int Start = -1;

		// Through the map's own time checkpoints first, where it has any. Each leg is the same
		// refined search run between two consecutive groups, so the route obeys the order the
		// mapper laid down instead of whatever turns out cheapest across the whole map.
		//
		// This is the difference between following a map and outsmarting it. On Shortcut the one
		// sweep climbed out of the start area and ran the perimeter -- 2010 tiles the wrong way,
		// past a freeze climb no tee can make -- while the seven checkpoints say to go right and
		// down, and routed through them it does, in 5562. On Sunlit Peaks the sweep found nothing
		// at all and the legs find a route, because a forced waypoint leaves no room for the
		// shortcut that was swallowing it.
		if(!Grid.m_vvTimeCheckpoints.empty())
		{
			std::vector<unsigned char> vFromFlags(Cells, 0);
			std::vector<unsigned char> vToFlags(Cells, 0);
			std::vector<int> vLegPath;
			const size_t Groups = Grid.m_vvTimeCheckpoints.size() + 2;
			bool Ok = true;
			for(size_t Index = 0; Index + 1 < Groups && Ok; Index++)
			{
				const std::vector<int> &vFrom = Index == 0 ? Grid.m_vStarts : Grid.m_vvTimeCheckpoints[Index - 1];
				const std::vector<int> &vTo = Index + 2 == Groups ? Grid.m_vFinishes : Grid.m_vvTimeCheckpoints[Index];
				std::fill(vFromFlags.begin(), vFromFlags.end(), (unsigned char)0);
				std::fill(vToFlags.begin(), vToFlags.end(), (unsigned char)0);
				for(int Cell : vFrom)
					vFromFlags[Cell] = 1;
				for(int Cell : vTo)
					vToFlags[Cell] = 1;

				int StopState = -1;
				if(!Refine(Grid, Abort, vTo, &vFromFlags, &StopState, &vDistance, &vNext) || StopState < 0)
				{
					Ok = false;
					break;
				}
				if(Index == 0)
				{
					// Each start tile asked about in the state it would actually be in, which on
					// a map whose start line is laid on a checkpoint is not the one layer zero
					// describes. See StartState.
					for(int Candidate : Grid.m_vStarts)
					{
						const int State = StartState(Grid, Candidate);
						if(Grid.Passable(Candidate) && (size_t)State < vDistance.size() && vDistance[State] >= 0)
							UsableStarts++;
					}
				}
				const std::vector<int> vStep = Trace(Grid, vNext, StopState, Cells, &vToFlags);
				if(vStep.size() < 2)
				{
					Ok = false;
					break;
				}
				// The legs meet on a shared cell, which belongs to the route once
				vLegPath.insert(vLegPath.end(),
					vStep.begin() + (vLegPath.empty() || vLegPath.back() != vStep.front() ? 0 : 1), vStep.end());
			}
			if(Ok && vLegPath.size() >= 2)
			{
				vPath = std::move(vLegPath);
				Legs = (int)Grid.m_vvTimeCheckpoints.size();
			}
			else
			{
				// A checkpoint there is no way to or from leaves the whole chain unusable, so
				// the one sweep below gets its turn after all.
				Start = -1;
				UsableStarts = 0;
			}
		}

		if(vPath.size() < 2 && Refine(Grid, Abort, Grid.m_vFinishes, nullptr, nullptr, &vDistance, &vNext))
		{
			Start = PickStart(Grid, vCoarseDistance, vDistance, true);
			for(int Candidate : Grid.m_vStarts)
			{
				const int State = StartState(Grid, Candidate);
				if(Grid.Passable(Candidate) && vCoarseDistance[Candidate] >= 0 &&
					(size_t)State < vDistance.size() && vDistance[State] >= 0)
					UsableStarts++;
			}
			if(Start >= 0)
				vPath = Trace(Grid, vNext, Start, Cells);
		}
		if(vPath.size() < 2)
		{
			// Nothing usable came back, so the coarse route stands in. Worse, since it walks
			// through freeze and takes every checkpoint teleporter to be joined to every exit,
			// but a route is more use than none.
			Fallback = true;
			Start = PickStart(Grid, vCoarseDistance, vCoarseDistance, false);
			if(Start >= 0)
				vPath = Trace(Grid, vCoarseNext, Start % Cells, Cells);
		}
		if(!TravelField(Grid, Abort, &vTravelLeft, &vTravelNext))
		{
			vTravelLeft.clear();
			vTravelNext.clear();
		}
	}
	vCoarseDistance = std::vector<int>();
	vCoarseNext = std::vector<int>();

	if(vPath.size() < 2 || Abort.load())
	{
		pJob->m_Done.store(true);
		return;
	}

	auto pRoute = std::make_unique<CRoute>();
	pRoute->m_Width = Grid.m_Width;
	pRoute->m_Height = Grid.m_Height;
	pRoute->m_CoarseFallback = Fallback;
	pRoute->m_UsableStarts = UsableStarts;
	pRoute->m_TotalStarts = (int)Grid.m_vStarts.size();
	pRoute->m_TotalFinishes = (int)Grid.m_vFinishes.size();
	pRoute->m_CheckpointLayers = Grid.m_CheckpointLayers;
	pRoute->m_TimeCheckpoints = Legs;
	pRoute->m_vPath.reserve(vPath.size());
	pRoute->m_vArc.reserve(vPath.size());
	pRoute->m_vNodeTravelLeft.reserve(vPath.size());
	// The travel field is its own pass and does not follow checkpoint teleporters, so on a map
	// built on those it reaches only the last section and has nothing to say about the rest of
	// the route. Carrying the previous node's figure over the gap would not be a worse estimate,
	// it would be a wrong one: every node before the first covered one would read as standing at
	// the finish. All of the route or none of it.
	bool TravelFieldCovers = true;
	for(size_t Node = 0; Node < vPath.size(); Node++)
	{
		const vec2 Pos = Grid.Center(vPath[Node]);
		if(Node)
		{
			// A teleporter is a jump between two tiles nowhere near each other, and counting the
			// gap as distance travelled would put most of the map's length inside it. Only
			// neighbouring tiles add to the arc.
			const int Previous = vPath[Node - 1];
			const int Dx = std::abs(vPath[Node] % Grid.m_Width - Previous % Grid.m_Width);
			const int Dy = std::abs(vPath[Node] / Grid.m_Width - Previous / Grid.m_Width);
			if(Dx <= 1 && Dy <= 1)
				pRoute->m_Length += distance(pRoute->m_vPath.back(), Pos);
		}
		if(Grid.Has(vPath[Node], TILEINFO_FREEZE))
			pRoute->m_FreezeTiles++;
		if(Node)
		{
			const int Previous = vPath[Node - 1];
			const int Px = Previous % Grid.m_Width, Py = Previous / Grid.m_Width;
			const int Cx = vPath[Node] % Grid.m_Width, Cy = vPath[Node] / Grid.m_Width;
			if(std::abs(Cx - Px) == 1 && std::abs(Cy - Py) == 1)
				pRoute->m_EdgeSkips += HazardSides(Grid, Cy * Grid.m_Width + Px, Py * Grid.m_Width + Cx);
		}
		pRoute->m_vPath.push_back(Pos);
		pRoute->m_vArc.push_back(pRoute->m_Length);

		// Clamped to descend. It does anyway along the route it was traced from, each step
		// subtracting its own length, but a binary search is only correct on a sequence that is
		// actually ordered and a coarse fallback route is not traced from this field.
		int TravelLeft = vTravelLeft.empty() ? -1 : vTravelLeft[vPath[Node]];
		if(TravelLeft < 0)
		{
			TravelFieldCovers = false;
			TravelLeft = pRoute->m_vNodeTravelLeft.empty() ? 0 : pRoute->m_vNodeTravelLeft.back();
		}
		if(!pRoute->m_vNodeTravelLeft.empty())
			TravelLeft = std::min(TravelLeft, pRoute->m_vNodeTravelLeft.back());
		pRoute->m_vNodeTravelLeft.push_back(TravelLeft);
	}
	if(vTravelLeft.empty() || !TravelFieldCovers)
	{
		// No route at all, rather than one that can only be read while the tee sits on it.
		//
		// Covered means every node of the route has a figure for the travel left from it, and
		// that is the condition the off-route readings need: the branch is walked out of this
		// field, and so is the placement of a tee the branch cannot reach. Without it the only
		// thing left is line of sight to the drawn route.
		//
		// Which is not a route worth drawing. These are two-player maps, the players swap
		// positions constantly, and a readout that only answers while the tee is on one
		// particular line answers almost never. Nothing is clearer than that.
		pJob->m_pFailureReason = "the route cannot be measured away from itself, so none is offered";
		pJob->m_Done.store(true);
		return;
	}
	pRoute->m_vTravelLeft = std::move(vTravelLeft);
	pRoute->m_vTravelNext = std::move(vTravelNext);
	// Latest node wins. Where the route uses a cell twice, a branch arriving there is rejoining
	// at the furthest point along the route that the cell stands for, which is the one worth
	// coming back to: the earlier visit is ground the tee has already covered.
	for(size_t Node = 0; Node < vPath.size(); Node++)
		pRoute->m_RouteNodeByCell[vPath[Node]] = (int)Node;

	if(pRoute->m_Length <= 0.0f || Abort.load())
	{
		pJob->m_Done.store(true);
		return;
	}

	pJob->m_pRoute = std::move(pRoute);
	pJob->m_Done.store(true);
}

//////////////////////////////////////////////////
// Component
//////////////////////////////////////////////////

CFinishPrediction::~CFinishPrediction()
{
	// OnShutdown normally gets there first, but a joinable std::thread left in the member would
	// call std::terminate from its own destructor, so this is not left to chance.
	StopWorker();
}

void CFinishPrediction::StopWorker()
{
	if(m_pJob)
		m_pJob->m_Abort.store(true);
	if(m_Worker.joinable())
		m_Worker.join();
	m_pJob = nullptr;
}

/**
 * Drops the route and everything tracked against it, and stops any search in flight.
 *
 * A map change and the readout being switched off want the same thing: nothing of the old route
 * left behind, and no worker still holding a grid.
 */
void CFinishPrediction::Discard()
{
	StopWorker();
	m_pRoute = nullptr;
	m_State = EState::IDLE;
	m_pFailureReason = "";
	m_HasProgress = false;
	m_Progress = 0.0f;
	m_ShownProgress = 0.0f;
	m_SnapProgress = true;
	m_RushProgress = false;
	for(CTrack &Track : m_aTracks)
		Track.Forget();
	m_TrackedRaceTick = -1;
	m_TrackedClientId = -1;
	m_MemberCount = 0;
	m_SeenRoster = 0;
	m_LaggardId = -1;
	m_PredictedTicks = -1;
	m_PredictionBlend = 0.0f;
	m_HeldRemaining = -1;
	ResetPace();
}

void CFinishPrediction::OnMapLoad()
{
	Discard();

	m_SeenEnabled = g_Config.m_ClShowhudFinishPrediction != 0;
	if(m_SeenEnabled)
		StartSearch();
}

void CFinishPrediction::StartSearch()
{
	StopWorker();
	m_State = EState::IDLE;

	auto pJob = std::make_shared<CJob>();
	const bool Built = BuildGrid(&pJob->m_Grid);
	// Remembered either way, so that a map with nothing to race between does not get a fresh
	// attempt every time the setting moves
	m_RoutePenalty = std::max(1, g_Config.m_ClFinishPredictionFreezePenalty);
	m_SeenPenalty = m_RoutePenalty;
	if(!Built)
		return;

	m_pJob = pJob;
	m_State = EState::SEARCHING;
	m_Worker = std::thread([pJob]() {
		CFinishPrediction::Search(pJob);
	});
}

/**
 * Picks up the two settings that a route cannot be adjusted to after the fact: whether the
 * readout is on at all, and the freeze penalty.
 *
 * Both are watched here rather than through a conchain, because the settings page writes
 * g_Config directly and never goes through the console.
 *
 * The penalty is baked into the route, so it cannot be applied to one already found, and it gets
 * tuned by eye. Waiting out the drag matters: without it, every frame of a slider drag would
 * start and abandon a search of the whole map.
 */
void CFinishPrediction::PollSettings()
{
	// Switching the readout on is what starts the first search on a map loaded with it off, and
	// switching it off hands the route and the worker back rather than leaving them to idle.
	const bool Enabled = g_Config.m_ClShowhudFinishPrediction != 0;
	if(Enabled != m_SeenEnabled)
	{
		m_SeenEnabled = Enabled;
		if(Enabled)
			StartSearch();
		else
			Discard();
		return;
	}
	if(!Enabled)
		return;

	const int Penalty = std::max(1, g_Config.m_ClFinishPredictionFreezePenalty);
	if(Penalty != m_SeenPenalty)
	{
		m_SeenPenalty = Penalty;
		m_PenaltyChangedAt = Client()->LocalTime();
	}
	else if(Penalty != m_RoutePenalty && Client()->LocalTime() - m_PenaltyChangedAt > 0.4f)
	{
		for(CTrack &Track : m_aTracks)
			Track.Forget();
		m_PredictedTicks = -1;
		m_PredictionBlend = 0.0f;
		m_HeldRemaining = -1;
		m_pRoute = nullptr;
		StartSearch();
	}
}

void CFinishPrediction::OnReset()
{
	for(CTrack &Track : m_aTracks)
		Track.Forget();
	m_TrackedClientId = -1;
	m_MemberCount = 0;
	m_LaggardId = -1;
	m_HasProgress = false;
	m_PredictedTicks = -1;
	m_PredictionBlend = 0.0f;
	m_HeldRemaining = -1;
}

void CFinishPrediction::OnShutdown()
{
	StopWorker();
	m_pRoute = nullptr;
}

void CFinishPrediction::OnSelfDeath(bool Dummy)
{
	// Only where the readout is about our own tee. While someone else is being spectated our own
	// death is not the run on screen ending, and the spectated tee's own death arrives as the
	// race tick it is tracked by going away.
	if(m_TrackedClientId >= 0 && m_TrackedClientId != GameClient()->m_Snap.m_LocalClientId)
		return;
	if(Dummy == (g_Config.m_ClDummy != 0))
	{
		for(CTrack &Track : m_aTracks)
			Track.Forget();
		// Eased down rather than snapped, and the snap is withdrawn in case something else has
		// already asked for one: being put back on the start line is part of dying, not a
		// separate teleport to be treated on its own.
		m_RushProgress = true;
		m_SnapProgress = false;
		ResetPace();
	}
}

void CFinishPrediction::CollectJob()
{
	if(!m_pJob || !m_pJob->m_Done.load())
		return;

	if(m_Worker.joinable())
		m_Worker.join();

	if(m_pJob->m_pRoute)
	{
		std::shared_ptr<const CRoute> pRoute(m_pJob->m_pRoute.release());
		m_State = EState::READY;
		m_pFailureReason = "";
		log_info("finishpred", "route found: %.0f tiles over %d nodes, %d freeze tiles crossed, %d edge skips%s",
			pRoute->m_Length / 32.0f, (int)pRoute->m_vPath.size(), pRoute->m_FreezeTiles, pRoute->m_EdgeSkips,
			pRoute->m_CoarseFallback ? ", UNREFINED (the refined pass found nothing from any start tile)" : "");
		m_pRoute = std::move(pRoute);
	}
	else
	{
		m_State = EState::FAILED;
		m_pFailureReason = m_pJob->m_pFailureReason;
		log_info("finishpred", "no route: %s",
			m_pFailureReason[0] ? m_pFailureReason : "the finish is not reachable from the start tiles");
	}

	m_pJob = nullptr;
}

/**
 * Which node of the route the tee counts as being on, or -1 for none.
 *
 * "None" is a real answer and the readout shows it as --%. The alternative, naming whichever node
 * happens to be nearest, reports progress along a stretch of route the tee has no way of being
 * on: standing in a side room with the route running past behind its wall used to read as several
 * percent of a run that had not started.
 *
 * So a node only counts if the tee can see it, or could have walked to it. Those are the two
 * things that make a position and a piece of route the same place.
 */
/**
 * The route node with as much travel left as the figure given.
 *
 * This is how a tee that is not on the traced route at all gets an arc position. Distance left to
 * travel is the one thing that compares directly across two different paths to the same place, so
 * the answer is the point of the route that has as far still to go.
 */
int CFinishPrediction::NodeForTravelLeft(int TravelLeft) const
{
	const std::vector<int> &vNode = m_pRoute->m_vNodeTravelLeft;
	const int Last = (int)vNode.size() - 1;
	// Descending, so this finds the first node at or past the tee
	const auto It = std::lower_bound(vNode.begin(), vNode.end(), TravelLeft, std::greater<int>());
	int Node = (int)(It - vNode.begin());
	if(Node > Last)
		return Last;
	if(Node > 0 && std::abs(vNode[Node - 1] - TravelLeft) < std::abs(vNode[Node] - TravelLeft))
		Node--;
	return Node;
}

/**
 * Which node of the route the tee counts as being on, or -1 for none.
 *
 * Two readings, in order of precision.
 *
 * Sight is positional and continuous: the nearest node to the tee, near where it was last
 * placed, and only if nothing solid is in the way. It is the one that gets a route doubling back
 * through its own corridor right, because it remembers which pass the tee came in on.
 *
 * Travel answers from anywhere: the point of the route with as far left to go as the tee has.
 * Distance left is the only figure comparable across two different paths to the same place, which
 * is what a team map's drag part needs - the tee that went the other way is not lost and is not
 * at --%, it is running a path this does not know about.
 *
 * Travel, not the search's cost. Cost is mostly freeze penalty wherever there is freeze, so
 * reading progress off it put a tee with three freeze tiles ahead of it further from the finish
 * than the start line was, and the readout collapsed to 0% for half the path. Cost picks the
 * route; travel measures it.
 *
 * What used to sit between them, the nearest route node by walking, is gone. In a corridor that
 * runs parallel to the route it answers with the junction at one end or the other, so a tee
 * three quarters of the way along a drag read as either 0% or 100%. Cost gets the same positions
 * to within about three percent. Walking distance measures how far off the route the tee is,
 * which turns out not to be the same question as how far along it is.
 *
 * -1 means the finish cannot be reached from this tile at all, and the readout shows --%. That is
 * a real answer: it is what stops a tee somewhere sealed off from claiming a percentage.
 */
/**
 * The route node nearest a position along the arc.
 *
 * The arc ascends, so this is a plain search. Only needed because a branch places the tee between
 * two nodes rather than at one, and the window and the overlay still want a node to work from.
 */
int CFinishPrediction::NodeForArc(float Arc) const
{
	const std::vector<float> &vArc = m_pRoute->m_vArc;
	const int Last = (int)vArc.size() - 1;
	const auto It = std::lower_bound(vArc.begin(), vArc.end(), Arc);
	int Node = (int)(It - vArc.begin());
	if(Node > Last)
		return Last;
	if(Node > 0 && Arc - vArc[Node - 1] < vArc[Node] - Arc)
		Node--;
	return Node;
}

/**
 * Places a tee that is on a path of its own, by working out where that path rejoins the route.
 *
 * The travel field already holds a way out of every tile, so the branch does not have to be
 * searched for: following the field from the tee is the branch, and it is followed until it meets
 * a cell the route uses. That cell is where the tee is heading back to.
 *
 * With both ends known - where it left the route and where it will rejoin - the tee is placed
 * between them, by how much of the branch it has put behind it. That is what makes the reading
 * move while the tee is off the route. Measuring distance to the finish instead cannot: a second
 * path is usually longer than the stretch it replaces, so distance alone reads a branch as a
 * regression, and the honest answer to "how far is left" turns out not to be the answer to "how
 * far along is it".
 *
 * Returns -1 when there is nothing to interpolate between: no known branch point, or a field that
 * leads off without ever meeting the route again. The caller falls back to distance left.
 */
int CFinishPrediction::PlaceOnBranch(CTrack *pTrack, int Cell)
{
	const CRoute *pRoute = m_pRoute.get();
	pTrack->m_vBranchPath.clear();
	if(pTrack->m_BranchNode < 0 || pTrack->m_BranchNode >= (int)pRoute->m_vPath.size() ||
		pRoute->m_vTravelNext.empty() || pRoute->m_RouteNodeByCell.empty())
		return -1;

	const int Width = pRoute->m_Width;
	int Walk = Cell;
	int Travel = 0;
	int Rejoin = -1;
	for(int Step = 0; Step < gs_BranchSteps; Step++)
	{
		if(Walk < 0 || Walk >= (int)pRoute->m_vTravelNext.size())
			break;
		if(const auto Found = pRoute->m_RouteNodeByCell.find(Walk); Found != pRoute->m_RouteNodeByCell.end())
		{
			Rejoin = Found->second;
			break;
		}
		pTrack->m_vBranchPath.push_back(vec2((Walk % Width) * 32.0f + 16.0f, (Walk / Width) * 32.0f + 16.0f));
		const int Next = pRoute->m_vTravelNext[Walk];
		if(Next < 0)
			break;
		// A teleporter is no distance travelled, which is how the route's own arc counts it too
		const int Dx = std::abs(Next % Width - Walk % Width);
		const int Dy = std::abs(Next / Width - Walk / Width);
		if(Dx <= 1 && Dy <= 1)
			Travel += (Dx && Dy) ? gs_DiagonalCost : gs_StepCost;
		Walk = Next;
	}

	// A rejoin behind where the tee left is not a way onward, it is the way it came
	if(Rejoin <= pTrack->m_BranchNode)
	{
		pTrack->m_vBranchPath.clear();
		return -1;
	}
	pTrack->m_vBranchPath.push_back(pRoute->m_vPath[Rejoin]);

	// The furthest this tee has been from its rejoin on this branch, which is the length the
	// fraction is taken against. Kept as a maximum so that wandering further out reads as no
	// progress rather than as a jump backwards.
	pTrack->m_BranchTravel = std::max(pTrack->m_BranchTravel, Travel);
	const float Fraction = pTrack->m_BranchTravel > 0 ?
				       std::clamp(1.0f - (float)Travel / (float)pTrack->m_BranchTravel, 0.0f, 1.0f) :
				       1.0f;
	pTrack->m_BranchRejoin = Rejoin;
	pTrack->m_Source = ESource::BRANCH;
	pTrack->m_PlacedArc = mix(pRoute->m_vArc[pTrack->m_BranchNode], pRoute->m_vArc[Rejoin], Fraction);
	return NodeForArc(pTrack->m_PlacedArc);
}

int CFinishPrediction::NodeAt(CTrack *pTrack, vec2 Pos)
{
	const CRoute *pRoute = m_pRoute.get();
	const int x = round_to_int(Pos.x) / 32;
	const int y = round_to_int(Pos.y) / 32;
	if(x < 0 || x >= pRoute->m_Width || y < 0 || y >= pRoute->m_Height)
		return -1;
	const int Last = (int)pRoute->m_vPath.size() - 1;
	const int Cell = y * pRoute->m_Width + x;

	// With a node from last frame, the search is a window around it, and that window is what
	// stops a route which comes back past itself from snapping to the wrong visit. With no node
	// to work from there is no window to take, so the whole route is searched instead.
	//
	// That cold start is not a corner case. It is how every run begins and how every teleport
	// continues, and until it was written the only thing that ever gave tracking its first node
	// was the off-route reading further down. On a map where that reading is not available the
	// readout then sat at nothing for an entire run with a perfectly good route drawn over it.
	const bool Cold = pTrack->m_Node < 0 || pTrack->m_Node > Last;
	{
		int Best = -1;
		if(!Cold)
		{
			const int First = std::max(0, pTrack->m_Node - gs_TrackBack);
			const int End = std::min(Last, pTrack->m_Node + gs_TrackForward);
			float BestDistance = gs_TrackSnap * gs_TrackSnap;
			for(int Node = First; Node <= End; Node++)
			{
				const float Candidate = distance_squared(pRoute->m_vPath[Node], Pos);
				if(Candidate < BestDistance)
				{
					BestDistance = Candidate;
					Best = Node;
				}
			}
			// Only the one candidate is traced. Testing every node in the window would be
			// hundreds of line walks a frame, and where the nearest is behind a wall the
			// off-route reading below is the better answer anyway.
			if(Best >= 0 && GameClient()->Collision()->IntersectLine(Pos, pRoute->m_vPath[Best], nullptr, nullptr))
				Best = -1;
		}
		// A cold start walks the whole route and traces a line to each of the nearest few, so it
		// is much the most expensive thing here, and a team run would otherwise pay for it once
		// per member per frame. One a frame is enough: a tee that misses its turn falls through
		// to the off-route reading below exactly as if the scan had found nothing, and gets its
		// turn on the next frame. The watched tee is placed first, so it is never the one waiting.
		else if(m_ColdBudget > 0)
		{
			m_ColdBudget--;
			// Cold, so the nearest few are all tried rather than only the nearest: there is no
			// previous position to sanity-check the pick against, and across a whole route the
			// nearest node is quite often the one on the other side of a wall.
			m_vColdStart.clear();
			for(int Node = 0; Node <= Last; Node++)
			{
				if(distance_squared(pRoute->m_vPath[Node], Pos) < gs_TrackSnap * gs_TrackSnap)
					m_vColdStart.push_back(Node);
			}
			const size_t Tried = std::min(gs_ColdStartTraces, m_vColdStart.size());
			std::partial_sort(m_vColdStart.begin(), m_vColdStart.begin() + Tried, m_vColdStart.end(),
				[&](int A, int B) {
					return distance_squared(pRoute->m_vPath[A], Pos) < distance_squared(pRoute->m_vPath[B], Pos);
				});
			for(size_t Index = 0; Index < Tried; Index++)
			{
				const int Node = m_vColdStart[Index];
				if(!GameClient()->Collision()->IntersectLine(Pos, pRoute->m_vPath[Node], nullptr, nullptr))
				{
					Best = Node;
					break;
				}
			}
		}

		if(Best >= 0)
		{
			pTrack->m_Source = ESource::SIGHT;
			pTrack->m_BranchNode = Best;
			pTrack->m_BranchTravel = 0;
			pTrack->m_BranchRejoin = -1;
			pTrack->m_vBranchPath.clear();
			pTrack->m_PlacedArc = pRoute->m_vArc[Best];
			return Best;
		}
	}

	if(g_Config.m_ClFinishPredictionOffPath && !pRoute->m_vTravelLeft.empty())
	{
		// The branch walks the travel field until it meets the route and measures itself against
		// where the tee left, so it needs the field and the route's own cells and nothing else.
		// It used to be gated on the node table below as well, which it never reads, and that
		// table is dropped whenever the field does not cover the whole route -- so on the maps
		// where the field is patchy the branch was switched off for no reason at all.
		if(const int Branch = PlaceOnBranch(pTrack, Cell); Branch >= 0)
			return Branch;

		const int TravelLeft = pRoute->m_vTravelLeft[Cell];
		// Reading a node off the table, on the other hand, means searching it, and that is only
		// correct where every node in it came from the field rather than carried over a gap.
		if(TravelLeft >= 0 && pRoute->m_vNodeTravelLeft.size() == pRoute->m_vPath.size())
		{
			pTrack->m_Source = ESource::REMAINING;
			int Node = NodeForTravelLeft(TravelLeft);
			// A second path to the same place is not a step backwards, however far round it goes.
			// Distance left cannot say so on its own: from the near end of a branch with anything
			// awkward on it, going back the way the tee came and round the other side is often
			// genuinely shorter, so the honest answer to "how far is left" is "further than at
			// the start line" and the readout collapses. What the tee has actually done is leave
			// the route at a known point, so it is not read as being behind that point.
			//
			// Only within a margin, because something really can go backwards: a long fall onto
			// an earlier part of the map is a regression and should read as one.
			if(pTrack->m_BranchNode >= 0 && pTrack->m_BranchNode <= Last && Node < pTrack->m_BranchNode &&
				pRoute->m_vArc[pTrack->m_BranchNode] - pRoute->m_vArc[Node] < pRoute->m_Length * gs_BranchSlack)
				Node = pTrack->m_BranchNode;
			pTrack->m_PlacedArc = pRoute->m_vArc[Node];
			return Node;
		}
	}

	return -1;
}

void CFinishPrediction::ResetPace()
{
	m_PaceCount = 0;
	m_PaceNext = 0;
	m_PaceLastTick = -1;
	m_PaceLastProgress = 0.0f;
	m_DutyTicks = 0.0f;
	m_DutyMovingTicks = 0.0f;
}

void CFinishPrediction::SamplePace(int Tick, float Progress, int RaceTick)
{
	const int Interval = std::max(1, Client()->GameTickSpeed() / gs_PaceIntervalDivisor);
	// A slice every half second rather than every frame, so the window covers a span measured in
	// seconds whatever the frame rate, and so no rate is ever taken across one frame of noise
	if(m_PaceLastTick < 0)
	{
		m_PaceLastTick = Tick;
		m_PaceLastProgress = Progress;
		return;
	}
	if(Tick - m_PaceLastTick < Interval)
		return;
	const int Ticks = Tick - m_PaceLastTick;
	const float Gained = Progress - m_PaceLastProgress;
	m_aPace[m_PaceNext].m_End = Tick;
	m_aPace[m_PaceNext].m_Ticks = Ticks;
	m_aPace[m_PaceNext].m_Gained = Gained;
	m_PaceNext = (m_PaceNext + 1) % (int)PACE_SAMPLES;
	m_PaceCount = std::min(m_PaceCount + 1, (int)PACE_SAMPLES);
	m_PaceLastTick = Tick;
	m_PaceLastProgress = Progress;

	// The duty cycle runs over a far longer span than the window, so it is accumulated here
	// rather than summed out of the ring. Slices inside the opening are left out of it for the
	// same reason they are left out of the window.
	const int TickSpeed = std::max(1, Client()->GameTickSpeed());
	if(Tick - Ticks < RaceTick + (int)(gs_PaceSkipOpening * (float)TickSpeed))
		return;
	m_DutyTicks += (float)Ticks;
	if(Gained > 0.0f)
		m_DutyMovingTicks += (float)Ticks;
	// Bounded so that it is a long memory and not a perfect one. Scaling both totals keeps the
	// ratio while capping the span they represent, which makes this an exponential average over
	// about the horizon: the first minute of a twenty minute run should not still be deciding
	// what the run looks like.
	const float Horizon = gs_DutyHorizon * (float)TickSpeed;
	if(m_DutyTicks > Horizon)
	{
		const float Scale = Horizon / m_DutyTicks;
		m_DutyTicks *= Scale;
		m_DutyMovingTicks *= Scale;
	}
}

float CFinishPrediction::PaceRate(int Tick, float Progress, int RaceTick) const
{
	const int TickSpeed = std::max(1, Client()->GameTickSpeed());
	// Slices wholly inside the opening are left out, since the opening is not the pace the rest
	// of the map will be run at
	const int Earliest = RaceTick + (int)(gs_PaceSkipOpening * (float)TickSpeed);

	float Gained = 0.0f;
	int Ticks = 0;
	int MovingTicks = 0;
	int Slices = 0;
	for(int i = 0; i < m_PaceCount; i++)
	{
		const CPaceSample &Slice = m_aPace[(m_PaceNext - m_PaceCount + i + (int)PACE_SAMPLES) % (int)PACE_SAMPLES];
		if(Slice.m_Ticks <= 0 || Slice.m_End - Slice.m_Ticks < Earliest)
			continue;
		Slices++;
		Ticks += Slice.m_Ticks;
		// A slice that went backwards is not evidence of a speed: a tee that fell has not started
		// finishing in the other direction. It counts as time not spent travelling, which is what
		// the duty cycle below is for.
		if(Slice.m_Gained > 0.0f)
		{
			Gained += Slice.m_Gained;
			MovingTicks += Slice.m_Ticks;
		}
	}
	if(Slices < gs_PaceMinSamples || Gained < gs_PaceMinGain)
		return -1.0f;
	if((float)Ticks < gs_PaceMinSpan * (float)TickSpeed)
		return -1.0f;
	if((float)MovingTicks < gs_PaceMinMoving * (float)TickSpeed)
		return -1.0f;

	// Two separate questions, asked over two separate spans.
	//
	// How fast the tee covers ground while it is covering ground is a recent matter and comes
	// from the window: it changes with the terrain, and the last three quarters of a minute is
	// the right amount of evidence about the next stretch.
	//
	// What fraction of the time it is covering ground at all is not a recent matter. It is the
	// character of the run -- how much of it is freeze, falls and waiting -- and it belongs over
	// the longest span there is. Reading it off the window instead, which is what this did at
	// first, meant one stall of twenty seconds was extrapolated as though the rest of the map
	// would be stalled in the same proportion, and the predicted finish leapt.
	//
	// The product of the two is progress per tick of wall clock, which is what the caller wants.
	// A stall still pushes the finish out -- it lowers the duty cycle, and it reaches the figure
	// through elapsed as well -- but by what it is worth against the whole run rather than
	// against the last forty-five seconds of it.
	const float Speed = Gained / (float)MovingTicks;
	const float Duty = m_DutyTicks > 0.0f ? std::clamp(m_DutyMovingTicks / m_DutyTicks, 0.0f, 1.0f) :
						(float)MovingTicks / (float)Ticks;
	if(Duty <= 0.0f)
		return -1.0f;
	return Speed * Duty;
}

bool CFinishPrediction::TeePos(int ClientId, vec2 *pPos) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;

	// Not named Client, which is the method that reaches the engine client
	const CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
	if(GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
	{
		*pPos = Data.m_RenderPos;
		return true;
	}
	// Paused or in spectators: no character of its own in the snapshot, but a spectator character
	// standing where it left off, which is the position to measure. Not interpolated, because
	// there is nothing to interpolate -- a tee that is not in the world is not going anywhere.
	if(Data.m_SpecCharPresent)
	{
		*pPos = Data.m_SpecChar;
		return true;
	}
	return false;
}

bool CFinishPrediction::TrackedTee(int *pClientId, vec2 *pPos) const
{
	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	// Which is what the server itself does for the race timer in the HUD: a spectated player if
	// there is one, and otherwise our own tee, which in free view is still ours whenever there is
	// one to have -- a tee that is merely paused is sitting on the route where it left off.
	const int ClientId = Snap.m_SpecInfo.m_Active && Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW ?
				     Snap.m_SpecInfo.m_SpectatorId :
				     Snap.m_LocalClientId;
	if(!TeePos(ClientId, pPos))
		return false;
	*pClientId = ClientId;
	return true;
}

int CFinishPrediction::TrackedRaceTick(int ClientId) const
{
	if(ClientId == GameClient()->m_Snap.m_LocalClientId)
		return GameClient()->LastRaceTick(g_Config.m_ClDummy);

	const CNetObj_GameInfo *pGameInfo = GameClient()->m_Snap.m_pGameInfoObj;
	if(!pGameInfo || !(pGameInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME))
		return -1;
	return -pGameInfo->m_WarmupTimer;
}

bool CFinishPrediction::CollectMembers(int TrackedId)
{
	m_MemberCount = 0;
	m_Roster = 0;

	if(!g_Config.m_ClFinishPredictionTeam)
	{
		m_aMembers[m_MemberCount++] = TrackedId;
		return false;
	}

	// Multi-view is asked first, because it is the more specific answer: a multi-view of a team
	// is that team, and a multi-view of a loose set of tees is a group the tees themselves know
	// nothing about. Either way the tees on screen are the ones the figure should be about, which
	// is why this does not go on to ask what they are to each other -- not even whether the team
	// is in team0mode. Putting tees in a multi-view is a deliberate act of watching them
	// together, and that is a plainer statement of intent than anything the server has said about
	// them. The one cost is that a set of tees running separate races has no shared start tick,
	// so the clock the prediction rests on is the spectated tee's; ec_finish_prediction_status
	// says so when that is the case.
	const bool MultiView = GameClient()->m_MultiView.IsActivated();
	int Group = 0;
	if(MultiView)
	{
		Group = 1;
	}
	else
	{
		const int Team = GameClient()->m_Teams.Team(TrackedId);
		// TEAM_FLOCK is "no team", and TEAM_SUPER is the one team whose members are not running
		// anything together at all
		if(Team == TEAM_FLOCK || Team == TEAM_SUPER || Team < 0 || Team >= MAX_CLIENTS)
		{
			m_aMembers[m_MemberCount++] = TrackedId;
			return false;
		}
		// team0mode makes a numbered team behave like team 0: the members still share the team,
		// but they start and finish one at a time, so the team is not a thing with a single
		// progress and each tee is measured on its own. The server says so per character.
		const CGameClient::CSnapState::CCharacterInfo &Info = GameClient()->m_Snap.m_aCharacters[TrackedId];
		if(Info.m_HasExtendedDisplayInfo && (Info.m_ExtendedData.m_Flags & CHARACTERFLAG_TEAM0_MODE) != 0)
		{
			m_aMembers[m_MemberCount++] = TrackedId;
			return false;
		}
		Group = Team + 2;
	}

	// The roster covers every member, the tracked one included, so that it identifies the group
	// and not the group-as-seen-from-one-of-its-members. That matters most in multi-view, where
	// the client really does spectate whichever viewed tee is nearest the camera and so the
	// tracked tee changes as the camera pans: a roster that left the tracked tee out changed with
	// every pan, and the reset below would have thrown the pace window away each time.
	//
	// It is also taken from membership rather than from who could be placed, because those are
	// not the same thing for a frame or two at a time. A member that dies has no character and no
	// spectator character until it respawns, and keying the reset on the placeable count made
	// every death in the group wipe the window.
	m_Roster = (unsigned)Group;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const bool Member = ClientId == TrackedId ||
				    (GameClient()->m_aClients[ClientId].m_Active &&
					    (MultiView ? GameClient()->m_MultiView.IsViewed(ClientId) :
							 GameClient()->m_Teams.Team(ClientId) == GameClient()->m_Teams.Team(TrackedId)));
		if(!Member)
			continue;
		m_Roster = m_Roster * 65599 + ClientId + 1;
		// A member the server is not showing us cannot be measured. It is still holding the group
		// up, but guessing where it is would be worse than leaving it out: the figure would be
		// wrong in a way nothing on screen explains.
		vec2 Pos;
		if(ClientId != TrackedId && !TeePos(ClientId, &Pos))
			continue;
		m_aMembers[m_MemberCount++] = ClientId;
	}
	// The tracked tee goes first, because it is placed first and the cold start budget in NodeAt
	// gives its one turn a frame to whoever comes first. The tee the camera is on is the one that
	// must never be the one waiting for a turn.
	for(int i = 1; i < m_MemberCount; i++)
	{
		if(m_aMembers[i] != TrackedId)
			continue;
		std::swap(m_aMembers[0], m_aMembers[i]);
		break;
	}
	// A multi-view of one tee is not a group, and should not be reported as one
	return m_MemberCount > 1;
}

void CFinishPrediction::UpdateTrack(int ClientId, bool Started)
{
	CTrack *pTrack = &m_aTracks[ClientId];
	pTrack->m_HasProgress = false;
	pTrack->m_Source = ESource::NONE;

	vec2 Pos;
	if(!TeePos(ClientId, &Pos))
	{
		pTrack->Forget();
		return;
	}

	// Being moved invalidates both kinds of tracking. The window would recover on its own, but
	// the branch point would not: it is the claim that the tee walked off the route at a known
	// place, and a tee that was picked up and put down somewhere else did no such thing. Left
	// alone it pins the readout to the furthest point reached and a teleport backwards never
	// shows, which is exactly what it did.
	const float Moved = pTrack->m_HasLastPos ? distance(pTrack->m_LastPos, Pos) : 0.0f;
	pTrack->m_LastPos = Pos;
	pTrack->m_HasLastPos = true;
	if(Moved > std::max(gs_TeleportFloor, gs_TeleportSpeed * Client()->RenderFrameTime()))
	{
		pTrack->m_Node = -1;
		pTrack->m_BranchNode = -1;
		pTrack->m_BranchTravel = 0;
		pTrack->m_BranchRejoin = -1;
		pTrack->m_vBranchPath.clear();
		// Unless a run has just ended, in which case this is the respawn and the figure is
		// already on its way down
		if(!m_RushProgress)
			m_SnapProgress = true;
	}

	const int Node = NodeAt(pTrack, Pos);
	if(Node < 0)
	{
		pTrack->m_Node = -1;
		return;
	}
	pTrack->m_Node = Node;
	// A tee that has not crossed the start line has made no progress, whatever node it happens to
	// be standing nearest to. Tracking still runs, so the figure is right the moment the race
	// does start; it is only the reported number that is held at nothing.
	//
	// Worth holding, because the nearest node is not zero in general. A start line thirty tiles
	// long has one tile the route was measured from and the tee may spawn at the other end of it,
	// and a route that passes back by the start later can be nearer still. Purple Panic 4 opened
	// at three per cent before the race had begun.
	pTrack->m_Progress = Started ? std::clamp(pTrack->m_PlacedArc / m_pRoute->m_Length, 0.0f, 1.0f) : 0.0f;
	pTrack->m_HasProgress = true;
}

void CFinishPrediction::UpdateProgress()
{
	m_HasProgress = false;
	if(m_State != EState::READY || !m_pRoute || m_pRoute->m_vPath.empty())
		return;

	const int Conn = g_Config.m_ClDummy;
	int TrackedId = -1;
	vec2 Pos = vec2(0.0f, 0.0f);
	if(!TrackedTee(&TrackedId, &Pos))
	{
		m_TrackedClientId = -1;
		m_MemberCount = 0;
		m_LaggardId = -1;
		for(CTrack &Track : m_aTracks)
			Track.Forget();
		return;
	}

	const bool Grouped = CollectMembers(TrackedId);

	// A fresh race starts the tracking over, so a restart cannot be read as having fallen all the
	// way back through the old one. So does the group changing: none of what is tracked below is
	// about a route, it is all about where some particular tees have been, and the figure is the
	// slowest of them.
	//
	// A different tracked tee only counts where there is no group. Within one, which tee the
	// camera happens to be on says nothing about what is being measured -- and in multi-view it
	// changes on its own as the camera pans.
	const int RaceTick = TrackedRaceTick(TrackedId);
	const bool Started = RaceTick != -1;
	const bool Moved = TrackedId != m_TrackedClientId;
	const bool Switched = m_Roster != m_SeenRoster || (!Grouped && Moved);
	// Members of a real team share a start tick, so a group whose tick changes under a camera
	// move is a group that does not -- a loose multi-view, where the tees are running separate
	// races and the server only ever tells us the time of the one we are spectating. Taking that
	// as a restart would throw the window away on every pan, so the new tick is adopted quietly.
	// It is only a baseline for elapsed; the pace itself is a delta and does not care.
	const bool Restarted = RaceTick != m_TrackedRaceTick && !(Grouped && Moved);
	if(Restarted || Switched)
	{
		m_TrackedRaceTick = RaceTick;
		m_TrackedClientId = TrackedId;
		m_SeenRoster = m_Roster;
		for(CTrack &Track : m_aTracks)
			Track.Forget();
		m_PredictedTicks = -1;
		m_PredictionBlend = 0.0f;
		m_HeldRemaining = -1;
		ResetPace();
		// Switching to another tee is not a run coming to an end, so there is nothing to bring
		// down: the figure is simply about somebody else now and should say so at once. Easing
		// across would read as the tee it was about having moved.
		m_SnapProgress = Switched;
		m_RushProgress = !Switched;
	}
	else
	{
		m_TrackedRaceTick = RaceTick;
		m_TrackedClientId = TrackedId;
	}

	// Every member is placed, because a team run is only as far along as whoever is furthest
	// behind -- the server finishes the team when the last of them reaches the finish, so the
	// laggard is the one whose progress the whole team's time will be decided by.
	float Slowest = 1.0f;
	bool Any = false;
	m_LaggardId = -1;
	m_ColdBudget = 1;
	for(int i = 0; i < m_MemberCount; i++)
	{
		const int ClientId = m_aMembers[i];
		UpdateTrack(ClientId, Started);
		const CTrack &Track = m_aTracks[ClientId];
		if(!Track.m_HasProgress)
			continue;
		if(!Any || Track.m_Progress < Slowest)
		{
			Slowest = Track.m_Progress;
			m_LaggardId = ClientId;
		}
		Any = true;
	}
	// Nobody could be placed at all. One member that could is enough once the figure is the
	// group's: requiring the tracked tee as well meant that watching a teammate who had wandered
	// off the route hid a figure the laggard was perfectly able to supply.
	if(!Any)
		return;

	m_Progress = Slowest;
	m_HasProgress = true;

	// The readout is smoothed because the nearest node hops a whole tile at a time, and a
	// percentage that twitches is harder to read than one a few frames behind. It is not smoothed
	// across a death or a restart: the figure from the run that just ended is not a place to
	// animate from, and easing down from it was the whole of why killing looked wrong.
	const float Dt = Client()->RenderFrameTime();
	const float Gap = m_Progress - m_ShownProgress;
	// The jump guard is skipped while a run is coming down, since that drop is the one long move
	// meant to be watched and it would otherwise trip the guard on its first frame
	if(m_SnapProgress || (!m_RushProgress && std::abs(Gap) > 0.1f))
	{
		m_ShownProgress = m_Progress;
		m_SnapProgress = false;
		m_RushProgress = false;
	}
	else
	{
		// Exponential in the gap so a long move starts fast, with a floor so the end of it
		// arrives rather than approaching forever. Expressed against elapsed time rather than
		// multiplied by it, so the speed is the same at thirty frames a second as at three
		// hundred instead of being a linear guess that falls apart when frames are slow.
		const float Rate = m_RushProgress ? gs_ProgressRushRate : gs_ProgressBlendRate;
		const float MinRate = m_RushProgress ? gs_ProgressRushMinRate : gs_ProgressMinRate;
		const float Blend = 1.0f - std::exp(-Rate * Dt);
		const float Step = std::max(std::abs(Gap) * Blend, MinRate * Dt);
		if(std::abs(Gap) <= Step)
		{
			m_ShownProgress = m_Progress;
			m_RushProgress = false;
		}
		else
		{
			m_ShownProgress += std::copysign(Step, Gap);
		}
	}

	// Elapsed over the fraction done. It is wild early in a run, where a second either way is a
	// large part of a small elapsed time, so it waits until there is something behind it and is
	// eased towards rather than replaced outright.
	const float TimeBlend = std::clamp(Dt * 8.0f, 0.0f, 1.0f);
	const int Tick = Client()->GameTick(Conn);
	const int Elapsed = Started ? Tick - RaceTick : 0;
	if(Elapsed > 0)
	{
		SamplePace(Tick, m_Progress, RaceTick);
		// Elapsed plus what is left at the pace of the last few seconds. The window answers only
		// once it has something to say, so the garbage the old reckoning showed over the opening
		// is not shown slightly better -- it is not shown at all until there is a figure worth
		// putting on screen.
		const float Rate = PaceRate(Tick, m_Progress, RaceTick);
		if(Rate > 0.0f)
		{
			const float Remaining = std::max(0.0f, 1.0f - m_Progress) / Rate;
			const int Estimate = Elapsed + (int)std::min(Remaining, (float)gs_MaxDistance);
			if(m_PredictedTicks < 0)
			{
				m_PredictedTicks = Estimate;
			}
			else
			{
				// Asymmetric: a later finish is approached at the full rate, an earlier one at a
				// fraction of it. The window is already slow to be talked into a fast pace by a
				// few good seconds, and this is the second half of the same argument -- between
				// them, flowing briefly on a map that is mostly not flowed can no longer show a
				// time the run was never going to make.
				const float Approach = TimeBlend * 0.5f * (Estimate < m_PredictedTicks ? gs_PredictionFallBias : 1.0f);
				m_PredictedTicks = (int)mix((float)m_PredictedTicks, (float)Estimate, Approach);
			}
			m_PredictedTicks = std::max(m_PredictedTicks, Elapsed);
			m_PredictionBlend = std::min(1.0f, m_PredictionBlend + TimeBlend);
			// What the figure on screen says is still to come, taken from the eased value and not
			// from the raw estimate, so that the held case below carries on from the same number
			// it was showing on the frame the window went quiet
			m_HeldRemaining = m_PredictedTicks - Elapsed;
		}
		else if(m_PredictedTicks > 0 && m_HeldRemaining >= 0)
		{
			// The window has nothing to say: the tee has got essentially nowhere for the best
			// part of a minute, so there is no rate to divide by. A shorter wait than that does
			// not come through here at all any more -- the window counts the still slices, so it
			// answers with a rate that has already been dragged down by them, which is the right
			// answer and a better one than this. This is only for a stall long enough that there
			// is no pace left to measure.
			//
			// What is left to run is held at the last thing the window did say, and the time
			// going by reaches the figure through elapsed.
			//
			// Holding the whole estimate instead stops it dead while the clock it is a time on
			// carries on, so a long enough wait predicted a finish earlier than the time already
			// on the timer.
			m_PredictedTicks = Elapsed + m_HeldRemaining;
		}
	}
	else
	{
		m_PredictedTicks = -1;
		m_PredictionBlend = 0.0f;
		m_HeldRemaining = -1;
	}
}

void CFinishPrediction::OnRender()
{
	CollectJob();

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;

	// Polled before the switch is consulted, because noticing that it has just been turned on is
	// what starts the search.
	PollSettings();
	if(!g_Config.m_ClShowhudFinishPrediction)
		return;

	UpdateProgress();
	RenderRoute();
}

void CFinishPrediction::RenderRoute()
{
	if(!g_Config.m_ClFinishPredictionRoute || !m_pRoute || m_pRoute->m_vPath.size() < 2)
		return;

	Graphics()->TextureClear();
	Graphics()->LinesBegin();

	const ColorRGBA Behind = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFinishPredictionRouteDoneColor));
	const ColorRGBA Ahead = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFinishPredictionRouteColor));
	const float Alpha = (float)g_Config.m_ClFinishPredictionRouteAlpha / 100.0f;
	// The tee being watched, not the laggard: the covered stretch is drawn where the eye is, and
	// on a team run the figure may be about somebody else entirely.
	const bool Watching = m_TrackedClientId >= 0 && m_TrackedClientId < MAX_CLIENTS;
	const CTrack &Watched = m_aTracks[Watching ? m_TrackedClientId : 0];
	const int Here = m_HasProgress && Watching ? Watched.m_Node : -1;

	// Only what is in view is drawn. A long route is tens of thousands of segments, and the ones
	// off screen cost as much to hand the renderer as the ones on it.
	const CScreenRect Screen = Graphics()->GetScreen();
	const float Margin = 64.0f;

	const auto DrawPath = [&](const std::vector<vec2> &vPath) {
		for(size_t Node = 1; Node < vPath.size(); Node++)
		{
			const vec2 From = vPath[Node - 1];
			const vec2 To = vPath[Node];
			if(std::max(From.x, To.x) < Screen.m_TopLeft.x - Margin || std::min(From.x, To.x) > Screen.m_BottomRight.x + Margin ||
				std::max(From.y, To.y) < Screen.m_TopLeft.y - Margin || std::min(From.y, To.y) > Screen.m_BottomRight.y + Margin)
				continue;
			// A teleporter jump is not a stretch of route to draw a line along
			if(distance(From, To) > 64.0f)
				continue;
			ColorRGBA Color = Here >= 0 && (int)Node <= Here ? Behind : Ahead;
			Color.a *= Alpha;
			Graphics()->SetColor(Color);
			const IGraphics::CLineItem Line(From, To);
			Graphics()->LinesDraw(&Line, 1);
		}
	};

	DrawPath(m_pRoute->m_vPath);

	// The way back onto the route from wherever the tee has got to, which is the thing the
	// readout is interpolating along while it is off the route. Drawn on the "both" setting,
	// which is what that setting now means.
	if(g_Config.m_ClFinishPredictionRoute == 2 && Watching && !Watched.m_vBranchPath.empty())
	{
		ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFinishPredictionBranchColor));
		Color.a *= Alpha;
		Graphics()->SetColor(Color);
		for(size_t Node = 1; Node < Watched.m_vBranchPath.size(); Node++)
		{
			const vec2 From = Watched.m_vBranchPath[Node - 1];
			const vec2 To = Watched.m_vBranchPath[Node];
			if(distance(From, To) > 64.0f)
				continue;
			const IGraphics::CLineItem Line(From, To);
			Graphics()->LinesDraw(&Line, 1);
		}
	}

	Graphics()->LinesEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

float CFinishPrediction::RouteTiles() const
{
	return m_pRoute ? m_pRoute->m_Length / 32.0f : 0.0f;
}

const char *CFinishPrediction::StatusText() const
{
	switch(m_State)
	{
	case EState::SEARCHING: return "searching";
	case EState::READY: return "ready";
	case EState::FAILED: return "no route";
	default: return "no start or finish on this map";
	}
}

void CFinishPrediction::RenderHud(bool Preview)
{
	CHudLayout &Layout = GameClient()->m_Hud.HudLayout();

	if(!g_Config.m_ClShowhudFinishPrediction)
	{
		Layout.ClearNominalRect(EHudElement::FINISH_PREDICTION);
		return;
	}

	const bool ShowTime = g_Config.m_ClShowhudFinishPredictionTime != 0;
	const bool ShowPercent = g_Config.m_ClShowhudFinishPredictionPercent != 0;
	const bool ShowBar = g_Config.m_ClShowhudFinishPredictionBar != 0;
	if(!ShowTime && !ShowPercent && !ShowBar)
	{
		Layout.ClearNominalRect(EHudElement::FINISH_PREDICTION);
		return;
	}

	const float FontSize = 6.0f;
	// The row advance, kept one unit above the glyphs so the leading stays as it was
	const float LineHeight = 7.0f;
	const float Padding = 2.5f;
	const float BarHeight = 3.0f;
	const float BarGap = 1.5f;
	// How long the bar is drawn. The bar is the one part with a length of its own: a figure is as
	// wide as it reads, a bar has to be long enough to read as a bar.
	const float BarWidth = 76.0f;
	// Between the two figures when both are on, so they cannot meet in the middle
	const float ColumnGap = 4.0f;

	const float Left = 0.0f;
	const float Top = 125.0f;

	// The predicted time on the left and the percentage against the right edge, with the bar
	// running the full width under both. The time is the headline figure here rather than a
	// footnote under one, which is why there is no label: a time and a percentage over a progress
	// bar do not need to be told apart.
	char aValue[32];
	char aPrediction[64] = "";
	// How full the bar is drawn. Zero in every state that has no figure, which is why the bar can
	// keep its strip in those states without having to be told to stay empty.
	float Fraction = 0.0f;
	if(Preview && m_State != EState::READY)
	{
		// There is no route to show while placing it, and an empty slot is impossible to aim at
		str_copy(aValue, "42.0%");
		str_copy(aPrediction, "~1:23.45");
		Fraction = 0.42f;
	}
	else if(m_State == EState::IDLE)
	{
		Layout.ClearNominalRect(EHudElement::FINISH_PREDICTION);
		return;
	}
	else if(m_State == EState::SEARCHING)
	{
		str_copy(aValue, "...");
	}
	else if(m_State == EState::FAILED)
	{
		// Some maps have no route this can find, and on those the question mark is permanent. It
		// is still worth being able to see, since it is the difference between "not working" and
		// "nothing to say about this map", so it is a setting rather than a decision.
		if(g_Config.m_ClShowhudFinishPredictionHideNoRoute)
		{
			Layout.ClearNominalRect(EHudElement::FINISH_PREDICTION);
			return;
		}
		str_copy(aValue, "?");
	}
	else if(!m_HasProgress)
	{
		str_copy(aValue, "--%");
	}
	else
	{
		Fraction = std::clamp(m_ShownProgress, 0.0f, 1.0f);
		str_format(aValue, sizeof(aValue), "%.1f%%", Fraction * 100.0f);
		if(ShowTime && m_PredictedTicks > 0 && m_PredictionBlend > 0.5f)
		{
			char aTime[32];
			const int64_t Centiseconds = (int64_t)m_PredictedTicks * 100 / std::max(1, Client()->GameTickSpeed());
			str_time(Centiseconds, Centiseconds >= 360000 ? ETimeFormat::HOURS_CENTISECS : ETimeFormat::MINS_CENTISECS, aTime, sizeof(aTime));
			str_format(aPrediction, sizeof(aPrediction), "~%s", aTime);
		}
	}
	const bool HasPrediction = ShowTime && aPrediction[0] != '\0';
	const bool HasValue = ShowPercent;
	const bool HasRow = ShowTime || ShowPercent;
	const bool HasBar = ShowBar;
	if(!HasPrediction && !HasValue && !HasBar)
	{
		Layout.ClearNominalRect(EHudElement::FINISH_PREDICTION);
		return;
	}

	const float ValueWidth = HasValue ? TextRender()->TextWidth(FontSize, aValue, -1, -1.0f) : 0.0f;
	// Each column is measured from a reference and not from the figure standing in it, so the panel
	// keeps one width while the digits change underneath and so a column is already its full size
	// before its figure has arrived. A figure wider than its reference -- a prediction that has run
	// past the hour -- still gets the room it needs.
	float RowWidth = 0.0f;
	if(HasRow)
	{
		RowWidth = Padding * 2.0f;
		if(ShowTime)
		{
			RowWidth += std::max(TextRender()->TextWidth(FontSize, "~00:00.00", -1, -1.0f),
				HasPrediction ? TextRender()->TextWidth(FontSize, aPrediction, -1, -1.0f) : 0.0f);
		}
		if(ShowPercent)
			RowWidth += std::max(TextRender()->TextWidth(FontSize, "100.0%", -1, -1.0f), ValueWidth);
		if(ShowTime && ShowPercent)
			RowWidth += ColumnGap;
	}
	// The panel is the parts that are in it and nothing else. Switching the bar off takes its
	// length away with it rather than leaving it reserved behind the figures, and switching both
	// figures off leaves the bar's strip on its own.
	//
	// The floor is two of the shared corner radii, so that there is always a side left between the
	// arcs for them to be drawn against; nothing the font can measure comes anywhere near it, but a
	// panel that cannot be rounded is not worth the one comparison it costs to rule out.
	const float Width = std::max(10.0f, std::max(HasBar ? BarWidth : 0.0f, RowWidth));
	const float InnerWidth = Width - Padding * 2.0f;
	const float Height = Padding * 2.0f + (HasRow ? LineHeight : 0.0f) +
			     (HasBar ? (HasRow ? BarGap : 0.0f) + BarHeight : 0.0f);
	const float x = Left;

	Layout.ReportNaturalRect(EHudElement::FINISH_PREDICTION, vec2(x, Top), vec2(Width, Height));
	if(Layout.IsOccluded(EHudElement::FINISH_PREDICTION))
		return;

	CHudLayout::CScope Scope(&Layout, EHudElement::FINISH_PREDICTION);

	// Held to half the shorter side, because DrawRect does not check its corner arcs fit and a
	// radius over half the height folds the top and bottom corners into each other. The shared five
	// units is right for a panel with a row of figures in it and too much for one holding nothing
	// but the bar, which is eight units tall in all.
	const float PanelRounding = std::min(5.0f, std::min(Width, Height) * 0.5f);
	Graphics()->DrawRect(x, Top, Width, Height, CHudLayout::BackgroundColor(),
		Layout.CornerFlags(EHudElement::FINISH_PREDICTION), PanelRounding);

	float y = Top + Padding;
	if(HasRow)
	{
		if(HasPrediction)
			TextRender()->Text(x + Padding, y, FontSize, aPrediction, -1.0f);
		if(HasValue)
			TextRender()->Text(x + Width - Padding - ValueWidth, y, FontSize, aValue, -1.0f);
		y += LineHeight;
	}

	if(HasBar)
	{
		if(HasRow)
			y += BarGap;
		const float Rounding = BarHeight * 0.5f;
		Graphics()->DrawRect(x + Padding, y, InnerWidth, BarHeight,
			ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f), IGraphics::CORNER_ALL, Rounding);

		const ColorRGBA Fill = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFinishPredictionRouteColor));
		const float Filled = InnerWidth * Fraction;
		if(Filled > 0.0f)
		{
			Graphics()->DrawRect(x + Padding, y, Filled, BarHeight, Fill, IGraphics::CORNER_ALL,
				std::min(Rounding, Filled * 0.5f));
		}
		y += BarHeight;
	}
}

//////////////////////////////////////////////////
// Console
//////////////////////////////////////////////////

void CFinishPrediction::ConFinishPredictionStatus(IConsole::IResult *pResult, void *pUserData)
{
	CFinishPrediction *pThis = (CFinishPrediction *)pUserData;
	// Said outright, because with the readout off there is no route to report on and no search
	// to wait for, which otherwise reads the same as a map with nothing to race between.
	if(!g_Config.m_ClShowhudFinishPrediction)
	{
		pThis->GameClient()->Echo("Finish prediction: off, nothing searched for (ec_showhud_finish_prediction 1 to switch it on)");
		return;
	}
	const CRoute *pRoute = pThis->m_pRoute.get();
	char aBuf[256];
	if(!pRoute)
	{
		if(pThis->FailureReason()[0])
			str_format(aBuf, sizeof(aBuf), "Finish prediction: %s, %s", pThis->StatusText(), pThis->FailureReason());
		else
			str_format(aBuf, sizeof(aBuf), "Finish prediction: %s", pThis->StatusText());
		pThis->GameClient()->Echo(aBuf);
		return;
	}
	str_format(aBuf, sizeof(aBuf), "Finish prediction: %s, route %.0f tiles over %d nodes, %d freeze tiles crossed, %d edge skips, penalty %d%s",
		pThis->StatusText(), pThis->RouteTiles(), (int)pRoute->m_vPath.size(), pRoute->m_FreezeTiles,
		pRoute->m_EdgeSkips, pThis->m_RoutePenalty, pRoute->m_CoarseFallback ? ", UNREFINED FALLBACK" : "");
	pThis->GameClient()->Echo(aBuf);
	static const char *s_apSources[] = {"not tracking (the finish is not reachable from here)",
		"by line of sight", "on a branch back to the route", "by the distance left to travel"};
	const int WatchedId = std::clamp(pThis->m_TrackedClientId, 0, MAX_CLIENTS - 1);
	str_format(aBuf, sizeof(aBuf), "  placed %s, %d of %d start tiles and %d finish tiles usable",
		s_apSources[(int)pThis->m_aTracks[WatchedId].m_Source], pRoute->m_UsableStarts, pRoute->m_TotalStarts,
		pRoute->m_TotalFinishes);
	pThis->GameClient()->Echo(aBuf);
	// Which tees the figure came from, since on a group run it is somebody else's progress and
	// there is otherwise nothing on screen that says so
	if(pThis->m_MemberCount > 1)
	{
		const bool MultiView = pThis->GameClient()->m_MultiView.IsActivated();
		// A team shares one start tick, so its elapsed time is the team's. A loose multi-view
		// does not: the tees are running separate races and the server only ever sends the time
		// of the one being spectated, so the clock the prediction is built on is that tee's while
		// the percentage is the whole group's. Worth saying, since nothing on screen does.
		const int Team = pThis->GameClient()->m_Teams.Team(pThis->m_TrackedClientId);
		const bool Shared = Team != TEAM_FLOCK && Team != TEAM_SUPER;
		str_format(aBuf, sizeof(aBuf), "  %s of %d, figure taken from %s, who is furthest behind%s",
			MultiView ? "multi-view" : "team", pThis->m_MemberCount,
			pThis->m_LaggardId >= 0 ? pThis->GameClient()->m_aClients[pThis->m_LaggardId].m_aName : "nobody",
			Shared ? "" : " (no shared start tick, so the time is the spectated tee's)");
		pThis->GameClient()->Echo(aBuf);
	}
	// Both of these turn parts of this off without the route itself looking any different, which
	// is the sort of thing that is much easier to find written down than to deduce from in-game.
	str_format(aBuf, sizeof(aBuf), "  %d checkpoint teleporter group%s, %s, off-route tracking available",
		pRoute->m_CheckpointLayers - 1, pRoute->m_CheckpointLayers == 2 ? "" : "s",
		pRoute->m_TimeCheckpoints > 0 ? "threaded through the map's own time checkpoints" : "found in one sweep (this map states no checkpoint order)");
	pThis->GameClient()->Echo(aBuf);
}

void CFinishPrediction::OnConsoleInit()
{
	Console()->Register("ec_finish_prediction_status", "", CFGFLAG_CLIENT, ConFinishPredictionStatus, this,
		"Report what the finish prediction has worked out about this map");
}
