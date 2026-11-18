#ifndef GAME_CLIENT_COMPONENTS_ENTITY_LOCAL_PRACTICE_H
#define GAME_CLIENT_COMPONENTS_ENTITY_LOCAL_PRACTICE_H

#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/console.h>

#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/components/players.h>
#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

#include <cstddef>
#include <vector>

class CCharacter;
class CCharacterCore;
class CScreenRect;
class CTeeRenderInfo;

/**
 * A local practice world, simulated entirely on this client while the real tee stands still.
 *
 * The world here never takes part in prediction. It is seeded once from CGameClient::m_PredictedWorld,
 * borrows that world's collision, tuning and switcher state, and from then on runs on a clock of its
 * own that owes nothing to the server. Nothing reconciles it against a snapshot, so no prediction
 * error can be attributed to it and no existing antiping, fast input or smoothing path has to know
 * it exists.
 *
 * What reaches the server in the meantime is the input the real tee had at the moment practice
 * started, replayed unchanged. That is what keeps the real tee still, and it is why the fire counter
 * and wanted weapon are latched rather than zeroed: both are edge triggered, and a zero would read
 * as a real action on the way out.
 */
class CLocalPractice : public CComponent
{
public:
	/**
	 * Why the real tee stopped being where it was left.
	 */
	enum class EMoveReason
	{
		NONE = 0,
		TELEPORTED,
		DIED,
		FROZEN,
		DRAGGED,
	};

	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnNewSnapshot() override;

	bool IsActive() const { return m_Active; }

	/**
	 * Whether the server currently being played on is one local practice is allowed on at all.
	 * Answers off m_GameInfo, so it can change under a running practice world, which is why
	 * OnNewSnapshot rechecks it rather than only Start doing so.
	 *
	 * @param ppReason Set to a reason phrase when this returns false, if given.
	 */
	bool IsAvailableHere(const char **ppReason = nullptr) const;

	void Toggle();
	void Start();
	void Stop();

	/**
	 * Advances the practice world. Called from CGameClient::OnRender before UpdatePositions, so that
	 * the camera and everything laying out against the local tee see this frame's result rather than
	 * the previous one's.
	 */
	void OnUpdatePractice();

	/**
	 * Overwrites an outgoing input with the latched one. Only ever touches the buffer being sent;
	 * the client's own input state is left alone, or the practice world would be fed the same
	 * neutralized input it is meant to be spared.
	 */
	void NeutralizeInput(CNetObj_PlayerInput *pInput, int Conn) const;

	/**
	 * Puts the practice tees into the client data for as long as it is alive, and takes them back
	 * out when it goes out of scope.
	 *
	 * A client id has one set of render state and, while practicing, two tees behind it. The real
	 * one owns that state and is never written to, so it renders exactly as it always did and
	 * cannot be corrupted by anything here; the practice tee is swapped in only for its own draw
	 * and for the few things that should follow it rather than the real one. Anything not wrapped
	 * therefore shows the real tee, which is wrong but harmless, rather than a mixture of the two.
	 * CHud::OnRender does the same trick for the HUD editor's preview.
	 */
	class CScope
	{
	public:
		explicit CScope(CLocalPractice *pPractice);
		~CScope();

		CScope(const CScope &Other) = delete;
		CScope &operator=(const CScope &Other) = delete;

	private:
		CLocalPractice *m_pPractice;
		bool m_Applied;
	};

	/**
	 * How solid a tee should be drawn given which of the two is being drawn right now: the practice
	 * tee is solid, and the real tee behind it is the ghost.
	 */
	/**
	 * Whether this tee's opacity is the practice world's to decide rather than the client's.
	 */
	bool OwnsRenderAlpha(int ClientId) const;

	float DrawAlpha(int ClientId) const;

	/**
	 * Whether the tee currently being drawn for this id is the one the local input is aiming --
	 * the mouse for your own tee, the dummy input for the dummy. While practicing that is the
	 * practice tee, and the real tee behind it keeps whatever aim the server has for it.
	 */
	bool AimsWithLocalInput(int ClientId) const;

	/**
	 * The direction this tee is aiming in, where the practice world works it out rather than taking
	 * it from the mouse.
	 *
	 * Only the hammering dummy does: CGameClient::OnSnapInput aims m_HammerInput from one real tee
	 * at the other, and both of those are standing still while practice runs, so the aim drawn from
	 * it has nothing to do with where the practice dummy is hammering. The fire direction is right
	 * because BuildInput works that one out again from the tees in here; this is the same thing for
	 * the places that draw an aim.
	 *
	 * @return false for a tee whose aim is the mouse's, which the caller already has.
	 */
	bool AimDirection(int ClientId, vec2 &Out) const;

	/**
	 * Fills in the practice tees' render state for this frame. Called after the client has filled
	 * in the real ones, since it copies what it does not override.
	 */
	void BuildRenderState();

	/**
	 * Draws the practice tees, over the real ones.
	 */
	void RenderPracticeTees(const CScreenRect &ScreenRect);

	/**
	 * Draws a spectator character for every tee that is paused out of the world.
	 */
	void RenderPausedTees();

	/**
	 * Whether this client id is one the practice world is simulating, i.e. one of our own tees.
	 */
	bool IsSimulated(int ClientId) const;

	/**
	 * Whether this tee is a bystander: on the server, not in the practice world, and so drawn at
	 * the show-others opacity while practice runs.
	 */
	bool IsBystander(int ClientId) const;

	/**
	 * Whether a practice tee is being played right now, as opposed to practice merely running.
	 */
	bool IsControlling() const;

	/**
	 * Whether this client id's practice tee has been paused by /pause or /spec in here.
	 */

	/**
	 * Whether solo inside the practice world means this tee cannot be interacted with. The server
	 * never hears about solo set in here, so nothing else can work it out.
	 */
	bool IsPracticeSolo(int ClientId) const;

	/**
	 * How solid anything belonging to the real tee should be drawn, the tee itself included.
	 *
	 * Returns 1 for any tee the practice world is not simulating, so callers can multiply it in
	 * unconditionally.
	 */
	float ServerEntityAlpha(int ClientId) const;

	/**
	 * The worlds CItems draws locally simulated projectiles, lasers and pickups out of.
	 */
	CGameWorld *PrevRenderWorld() { return m_pRenderPrevWorld ? m_pRenderPrevWorld : &m_PrevWorld; }

	/**
	 * The world that owns the doors, which is the only one that has any.
	 *
	 * A door's whole state is the stamp it leaves in the collision, which every copy of this world
	 * shares, so the copies need no door entities of their own -- and must not have any, since
	 * destroying a copy's door would clear collision the original still wants. Doors never move, so
	 * there is nothing for a previous world to interpolate out of either.
	 */
	CGameWorld *DoorWorld() { return &m_World; }

	/**
	 * Runs a practice command typed into chat. Returns true if it was one of ours and was handled,
	 * in which case the line must not reach the server.
	 */
	bool OnChatCommand(const char *pInput);

	/**
	 * The moved notice. Called from CHud so that it draws inside the base HUD screen.
	 */
	void RenderMovedAlert(bool Preview);

	/**
	 * Points the spectator camera and cursor at the practice tee while paused or spectating.
	 */
	void OverrideSpectatorView();

	/**
	 * Answers the kill command locally while practicing. Returns true if it was handled here, in
	 * which case nothing may be sent to the server.
	 */
	bool OnKill();

private:
	class CCommand
	{
	public:
		const char *m_pName;
		const char *m_pArgs;
		const char *m_pHelp;
		void (CLocalPractice::*m_pfnHandler)(const char *pArgs);
	};

	static const CCommand ms_aCommands[];

	bool m_Active = false;

	/**
	 * What CPlayer::m_Paused holds on the server.
	 *
	 * Both states stop the tee being controlled, so it comes to rest and blinks. Only SPEC goes on
	 * to take it out of the world, and only once CPlayer::CanSpec allows -- which is why /spec in
	 * mid air does nothing until the tee has landed and stopped.
	 */
	enum
	{
		PAUSE_NONE = 0,
		PAUSE_PAUSED,
		PAUSE_SPEC,
	};
	int m_aPauseState[NUM_DUMMIES] = {PAUSE_NONE, PAUSE_NONE};

	/**
	 * A tee that has been paused out of the world, the way CCharacter::Pause does it on the server:
	 * taken out of the simulation entirely while everything else carries on, with its state kept so
	 * it can be put back exactly where it was.
	 */
	class CPausedTee
	{
	public:
		bool m_Valid = false;
		CCharacterCore m_Core;
		int m_FreezeTime = 0;
		int m_TeleCheckpoint = 0;
		int m_PausedTick = 0;
	};
	CPausedTee m_aPausedTee[NUM_DUMMIES];

	// Fire counters owned here rather than copied from whatever is driving each tee. A counter is
	// read as presses by their difference, so copying one means every change of what drives it --
	// a dummy swap exchanges the two, see CGameClient::OnDummySwap -- lands as a burst of presses.
	// Only genuine presses from the current source are passed on.
	int m_aFireCounter[NUM_DUMMIES] = {0, 0};
	int m_aLastSourceFire[NUM_DUMMIES] = {0, 0};
	int m_aLastSourceId[NUM_DUMMIES] = {-1, -1};
	int m_LastActiveConn = -1;

	// Whether we were already driving a practice tee last frame, so that taking control can
	// re-clamp the cursor exactly once
	bool m_WasControlling = false;

	CGameWorld m_World;
	CGameWorld m_PrevWorld;

	// A second view of the same map, sharing every one of its layers and owning only the door
	// layer, which is the one thing a running game writes into. Doors here can therefore stand open
	// while the server has them shut, and the doors this world builds for the far side of the map
	// are not wiped the moment the server stops sending any. All of this world's copies share it,
	// so a door only ever has to be stamped into it once, by the one world that owns the entities.
	CCollision m_Collision;

	// Speculative ticks for fast input: the pair the tee is actually drawn between when it is asked
	// to be drawn further along than the simulation has reached.
	CGameWorld m_aLookWorld[2];
	CGameWorld *m_pRenderCurWorld = nullptr;
	CGameWorld *m_pRenderPrevWorld = nullptr;
	float m_RenderIntra = 0.0f;

	// This world's own teleporter randomness. Self consistency is all it owes anyone.
	unsigned int m_TeleSeed = 0;

	// Shared with the prediction rather than kept separately, so that everything reading a tick
	// stamp back off this world measures it against the fraction it was written with
	int m_Tick = 0;

	CNetObj_PlayerInput m_aLatchedInput[NUM_DUMMIES];

	// Whether this connection has a practice tee of its own, and whether its real tee has settled
	// enough for a move to mean anything. A tee that has only just spawned has not.
	bool m_aSeeded[NUM_DUMMIES];
	bool m_aWatchArmed[NUM_DUMMIES];
	// Where the real tee was left, which is what "moved" is measured against
	vec2 m_aAnchorPos[NUM_DUMMIES];
	bool m_aWasHooked[NUM_DUMMIES];
	int m_aMissingSnaps[NUM_DUMMIES];

	// The connection the clock follows, fixed for as long as practice runs
	int m_ClockConn = -1;

	// Practice command state, per local connection
	vec2 m_aStartPos[NUM_DUMMIES];
	vec2 m_aLastTp[NUM_DUMMIES];
	bool m_aHasLastTp[NUM_DUMMIES];
	vec2 m_aRescuePos[NUM_DUMMIES];
	bool m_aHasRescuePos[NUM_DUMMIES];
	// Stands in for CPlayer::m_DieTick, which is what rate limits the death effect on a kill tile
	int m_aDieTick[NUM_DUMMIES];

	// What the real tees were doing last snapshot, to notice them being moved
	bool m_aHasServerPos[MAX_CLIENTS];
	int m_aServerFreezeEnd[MAX_CLIENTS];
	bool m_aServerAlive[MAX_CLIENTS];

	EMoveReason m_AlertReason = EMoveReason::NONE;
	float m_AlertSince = 0.0f;

	/**
	 * A practice tee's render state, in the shape the client data holds it, ready to be swapped in.
	 */
	class CPracticeTee
	{
	public:
		bool m_Valid = false;
		CCharacterCore m_Predicted;
		CCharacterCore m_PrevPredicted;
		CCharacterCore m_RegularPredicted;
		CNetObj_Character m_RenderPrev = {};
		CNetObj_Character m_RenderCur = {};
		vec2 m_RenderPos = vec2(0.0f, 0.0f);
		bool m_IsPredicted = false;
		bool m_IsPredictedLocal = false;
		bool m_Paused = false;
		bool m_Afk = false;
		bool m_InsideQuadFreeze = false;

		// The DDNet status flags the client keeps beside every tee, which it fills in from that
		// tee's extended snapshot data. A practice tee has no snapshot, so they are taken off its
		// core instead, and everything that reads them off the client data rather than out of the
		// core -- the frozen and sparkle looks, the freeze effects, the HUD's frozen-teammate rows
		// -- then describes the tee being played.
		bool m_Solo = false;
		bool m_Jetpack = false;
		bool m_CollisionDisabled = false;
		bool m_EndlessHook = false;
		bool m_EndlessJump = false;
		bool m_HammerHitDisabled = false;
		bool m_GrenadeHitDisabled = false;
		bool m_LaserHitDisabled = false;
		bool m_ShotgunHitDisabled = false;
		bool m_HookHitDisabled = false;
		bool m_Super = false;
		bool m_Invincible = false;
		bool m_HasTelegunGun = false;
		bool m_HasTelegunGrenade = false;
		bool m_HasTelegunLaser = false;
		bool m_DeepFrozen = false;
		bool m_LiveFrozen = false;
		int m_FreezeEnd = 0;
	};
	CPracticeTee m_aPracticeTee[NUM_DUMMIES];
	// Whether the client data currently holds the practice tees rather than the real ones
	bool m_InScope = false;

	void SwapPracticeTees();

	int LocalConn(int ClientId) const;
	bool HasServerPresence(int ClientId) const;
	bool ServerPos(int ClientId, vec2 &Out) const;
	bool IsLocalId(int ClientId) const;
	CCharacter *PracticeChar(int Conn) const;
	CCharacter *AnyPracticeChar() const;

	std::vector<vec2> m_vSpawns;
	size_t m_NextSpawn = 0;

	// Where each practice tee stood when the map's own pickups and draggers were last built around
	// it. Those are built per area rather than map-wide and rebuilt once a tee has walked far
	// enough out of the area it was given, see SeedStaticEntities.
	vec2 m_aStaticCenter[NUM_DUMMIES];
	bool m_aHasStaticCenter[NUM_DUMMIES] = {false, false};

	void CollectSpawns();
	void Respawn(int Conn);
	void SeedCharacter(int ClientId);
	void SeedSnapshotEntities();

	/**
	 * Running ids for the map entities built into the practice world. Nothing ever matches these
	 * against a snapshot, so they only have to be unique within their own entity type, which is
	 * what lets the previous world's copy of an entity be found again while drawing.
	 */
	class CStaticIds
	{
	public:
		int m_Pickup = 0;
		int m_Dragger = 0;
		int m_Gun = 0;
		int m_Light = 0;
		int m_Door = 0;
	};

	void SeedStaticEntities();
	void UpdateStaticEntities();
	void AddStaticEntity(int Index, int x, int y, int Layer, int Flags, int Number, CStaticIds &Ids);
	void BeginConn(int Conn);
	void EndConn(int Conn);
	void ResyncCounters(int Conn);
	void RemoveForeignEntities();
	void TickPracticeWorld();
	void StepWorld(CGameWorld &World, int Tick);
	void UpdateLookahead();
	void HandleTeleporters(CGameWorld &World, int Tick);
	void HandleDeathTiles(CGameWorld &World, int Tick);
	bool IsDeathTileAt(vec2 Pos, float ProximityRadius) const;
	int PickTeleOut(int Tick, int Size) const;
	/**
	 * Puts a character at a position, and the copy of it in the world holding the previous tick
	 * with it.
	 *
	 * A tee is drawn between where it was on the previous tick and where it is on this one, so a
	 * jump left in that pair is drawn as a slide across everything between the two positions. The
	 * distance test the prediction makes do with in CCharacter::Read cannot help here: it cannot
	 * tell a two tile teleporter from running.
	 */
	void PlaceChar(CGameWorld &World, CCharacter *pChar, vec2 Pos);
	void TeleportChar(CCharacter *pChar, vec2 Pos, bool ResetVel, bool ReleaseHooked, CGameWorld &World);
	static void LoseWeapons(CCharacter *pChar);
	CNetObj_PlayerInput BuildInput(CGameWorld &World, int Conn, int Tick) const;
	void PlayCoreEvents(CCharacter *pChar);
	void RecordRescuePos();
	void CheckServerMoved();
	bool IsHookedByOther(int ClientId) const;
	void TriggerAlert(int ClientId, EMoveReason Reason);

	// The attribute is what lets the compiler see pFormat as a format string when it is handed on
	// to str_format_v, and check every call site here while it is at it
	[[gnu::format(printf, 2, 3)]] void Print(const char *pFormat, ...) const;
	bool TeleportTo(int Conn, vec2 Pos);
	static void ClearFreeze(CCharacter *pChar);
	bool ResolveTarget(const char *pName, vec2 &Pos) const;
	bool ViewPos(int Conn, vec2 &Pos) const;

	// Practice commands. Each takes the rest of the chat line, or an empty string.
	void CmdPractice(const char *pArgs);
	void CmdTp(const char *pArgs);
	void CmdTpReal(const char *pArgs);
	void CmdTpXy(const char *pArgs);
	void CmdTpCursor(const char *pArgs);
	void CmdLastTp(const char *pArgs);
	void CmdRescue(const char *pArgs);
	void CmdPause(const char *pArgs);
	void CmdSpec(const char *pArgs);
	void CmdUnpause(const char *pArgs);
	void SetPauseState(int Conn, int State);
	void ProcessPause();
	bool CanSpec(int Conn) const;
	void PauseOutOfWorld(int Conn);
	void RestoreToWorld(int Conn);
	// Not being controlled, whether or not it has left the world yet
	bool IsPaused(int Conn) const { return Conn >= 0 && Conn < NUM_DUMMIES && m_aPauseState[Conn] != PAUSE_NONE; }
	bool IsPausedOut(int Conn) const { return Conn >= 0 && Conn < NUM_DUMMIES && m_aPausedTee[Conn].m_Valid; }
	void AdvanceInputCounters();
	void SyncFireCounter(int Conn);
	int FireSource(int Conn, int &SourceId) const;
	void CmdKill(const char *pArgs);
	void CmdUnfreeze(const char *pArgs);
	void CmdDeep(const char *pArgs);
	void CmdUnDeep(const char *pArgs);
	void CmdLiveFreeze(const char *pArgs);
	void CmdUnLiveFreeze(const char *pArgs);
	void CmdSolo(const char *pArgs);
	void CmdUnSolo(const char *pArgs);
	void CmdInvincible(const char *pArgs);

	void CmdWeapons(const char *pArgs);
	void CmdUnWeapons(const char *pArgs);

	void CmdGrenade(const char *pArgs);
	void CmdUnGrenade(const char *pArgs);

	void CmdLaser(const char *pArgs);
	void CmdUnLaser(const char *pArgs);

	void CmdShotgun(const char *pArgs);
	void CmdUnShotgun(const char *pArgs);

	void CmdNinja(const char *pArgs);
	void CmdUnNinja(const char *pArgs);

	void CmdJetpack(const char *pArgs);
	void CmdEndlessHook(const char *pArgs);
	void CmdEndlessJump(const char *pArgs);
	void CmdSetJumps(const char *pArgs);
	void CmdAddWeapon(const char *pArgs);
	void CmdRemoveWeapon(const char *pArgs);
	void CmdHelp(const char *pArgs);

	/**
	 * Hands a weapon picked up in the practice world to the controls, which is what decides whether
	 * to switch to it, so that cl_autoswitch_weapons means the same thing in here as it does on a
	 * server. Only ever called from the one world that really ticks, never a speculative copy.
	 */
	static void OnPracticeWeaponPickup(int ClientId, int Weapon, void *pUser);

	static void ConPractice(IConsole::IResult *pResult, void *pUserData);
	static void ConPracticeCommand(IConsole::IResult *pResult, void *pUserData);
};

#endif
