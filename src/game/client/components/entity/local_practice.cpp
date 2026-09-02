#include "local_practice.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/components/effects.h>
#include <game/client/components/hud.h>
#include <game/client/components/players.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/client/render.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iterator>

// The furthest the real tee may drift before it counts as having been moved rather than as having
// settled. An idle tee is stable to the pixel, so this only has to clear rounding.
static const float gs_MovedThreshold = 48.0f;

// Practice tees closer together than this hide the ghost entirely, so that the first moment of
// practice does not look like the tee has been drawn twice by mistake.
// How many snapshots a tee has to be missing before it counts as gone rather than as mid swap
static const int gs_MissingSnapsForDeath = 10;

static const float gs_GhostFadeNear = 28.0f;
static const float gs_GhostFadeFar = 96.0f;

const CLocalPractice::CCommand CLocalPractice::ms_aCommands[] = {
	// Deliberately not "/practice": that is a real DDNet server command, and swallowing it would
	// take away the ability to turn server practice on at all. Everything below it is only ever
	// matched while the local world is running, where taking it over is the whole point.
	{"exit", "", "Toggle the local practice world", &CLocalPractice::CmdPractice},
	// Both spellings, because a server takes both and nobody should have to remember which one
	// this is
	{"tp", "<name>", "Teleport to a player", &CLocalPractice::CmdTp},
	{"tele", "<name>", "Teleport to a player", &CLocalPractice::CmdTp},
	{"tpreal", "", "Teleport to your real tee on the server", &CLocalPractice::CmdTpReal},
	{"telereal", "", "Teleport to your real tee on the server", &CLocalPractice::CmdTpReal},
	{"tpxy", "<x> <y>", "Teleport to a position in tiles", &CLocalPractice::CmdTpXy},
	{"telexy", "<x> <y>", "Teleport to a position in tiles", &CLocalPractice::CmdTpXy},
	{"tpcursor", "", "Teleport to your cursor", &CLocalPractice::CmdTpCursor},
	{"telecursor", "", "Teleport to your cursor", &CLocalPractice::CmdTpCursor},
	{"lasttp", "", "Teleport back to where you last teleported from", &CLocalPractice::CmdLastTp},
	{"rescue", "", "Teleport back to the last safe position", &CLocalPractice::CmdRescue},
	{"r", "", "Teleport back to the last safe position", &CLocalPractice::CmdRescue},
	{"kill", "", "Respawn at a spawn tile", &CLocalPractice::CmdKill},
	// Taken over so they act on the practice tee. Left alone they reach the server and move the
	// real tee into spectators, which is the one thing local practice exists to avoid.
	{"pause", "", "Stop controlling the practice tee, leaving it standing there", &CLocalPractice::CmdPause},
	{"spec", "", "Stop controlling the practice tee and take it out of the world once it rests", &CLocalPractice::CmdSpec},
	{"unpause", "", "Take control of the practice tee again", &CLocalPractice::CmdUnpause},
	{"unfreeze", "", "Remove freeze", &CLocalPractice::CmdUnfreeze},
	{"deep", "", "Deep freeze", &CLocalPractice::CmdDeep},
	{"undeep", "", "Remove deep freeze", &CLocalPractice::CmdUnDeep},
	{"livefreeze", "", "Live freeze", &CLocalPractice::CmdLiveFreeze},
	{"unlivefreeze", "", "Remove live freeze", &CLocalPractice::CmdUnLiveFreeze},
	{"solo", "", "Turn solo on", &CLocalPractice::CmdSolo},
	{"unsolo", "", "Turn solo off", &CLocalPractice::CmdUnSolo},
	{"invincible", "[0|1]", "Toggle invincibility", &CLocalPractice::CmdInvincible},
	{"weapons", "", "Give all weapons", &CLocalPractice::CmdWeapons},
	{"unweapons", "", "Take all weapons away", &CLocalPractice::CmdUnWeapons},
	{"ninja", "", "Give ninja", &CLocalPractice::CmdNinja},
	{"unninja", "", "Take ninja away", &CLocalPractice::CmdUnNinja},
	{"jetpack", "[0|1]", "Toggle jetpack", &CLocalPractice::CmdJetpack},
	{"endless", "[0|1]", "Toggle endless hook", &CLocalPractice::CmdEndlessHook},
	{"endlessjump", "[0|1]", "Toggle endless jumps", &CLocalPractice::CmdEndlessJump},
	{"setjumps", "<amount>", "Set how many jumps you have", &CLocalPractice::CmdSetJumps},
	{"addweapon", "<id>", "Give one weapon (0 hammer, 1 gun, 2 shotgun, 3 grenade, 4 laser)", &CLocalPractice::CmdAddWeapon},
	{"removeweapon", "<id>", "Take one weapon away", &CLocalPractice::CmdRemoveWeapon},
	{"practicecmdlist", "", "List the practice commands", &CLocalPractice::CmdHelp},
	{"help", "", "List the practice commands", &CLocalPractice::CmdHelp},
};

void CLocalPractice::OnConsoleInit()
{
	Console()->Register("local_practice", "", CFGFLAG_CLIENT, ConPractice, this, "Toggle the local practice world");
	Console()->Register("local_practice_cmd", "r[command]", CFGFLAG_CLIENT, ConPracticeCommand, this, "Run a local practice command, as if typed into chat without the slash");
}

void CLocalPractice::OnReset()
{
	Stop();
	mem_zero(m_aHasServerPos, sizeof(m_aHasServerPos));
	mem_zero(m_aServerAlive, sizeof(m_aServerAlive));
	m_AlertReason = EMoveReason::NONE;
}

void CLocalPractice::OnStateChange(int NewState, int OldState)
{
	if(NewState != IClient::STATE_ONLINE)
		OnReset();
}

void CLocalPractice::ConPractice(IConsole::IResult *pResult, void *pUserData)
{
	((CLocalPractice *)pUserData)->Toggle();
}

void CLocalPractice::ConPracticeCommand(IConsole::IResult *pResult, void *pUserData)
{
	CLocalPractice *pSelf = (CLocalPractice *)pUserData;
	char aLine[256];
	str_format(aLine, sizeof(aLine), "/%s", pResult->GetString(0));
	if(!pSelf->OnChatCommand(aLine))
		pSelf->Print("unknown local practice command, try /help");
}

void CLocalPractice::Print(const char *pFormat, ...) const
{
	char aBuf[512];
	va_list Args;
	va_start(Args, pFormat);
	str_format_v(aBuf, sizeof(aBuf), pFormat, Args);
	va_end(Args);

	char aLine[544];
	str_format(aLine, sizeof(aLine), "[local practice] %s", aBuf);
	GameClient()->m_Chat.AddLine(CChat::ECLIENT_MSG, TEAM_ALL, aLine);
}

int CLocalPractice::LocalConn(int ClientId) const
{
	if(ClientId < 0)
		return -1;
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		if(GameClient()->m_aLocalIds[Conn] == ClientId)
			return Conn;
	return -1;
}

bool CLocalPractice::HasServerPresence(int ClientId) const
{
	// Paused and spectating still leave a tee behind on the server, as a spectator character. That
	// is just as good a thing to practice away from as a live one.
	if(ClientId < 0)
		return false;
	return GameClient()->m_Snap.m_aCharacters[ClientId].m_Active || GameClient()->m_aClients[ClientId].m_SpecCharPresent;
}

bool CLocalPractice::ServerPos(int ClientId, vec2 &Out) const
{
	if(ClientId < 0)
		return false;

	const CGameClient::CSnapState::CCharacterInfo &Info = GameClient()->m_Snap.m_aCharacters[ClientId];
	if(Info.m_Active)
	{
		Out = mix(vec2(Info.m_Prev.m_X, Info.m_Prev.m_Y), vec2(Info.m_Cur.m_X, Info.m_Cur.m_Y),
			Client()->IntraGameTick(g_Config.m_ClDummy));
		return true;
	}
	if(GameClient()->m_aClients[ClientId].m_SpecCharPresent)
	{
		Out = GameClient()->m_aClients[ClientId].m_SpecChar;
		return true;
	}
	return false;
}

bool CLocalPractice::IsLocalId(int ClientId) const
{
	return LocalConn(ClientId) >= 0;
}

CCharacter *CLocalPractice::PracticeChar(int Conn) const
{
	if(!m_Active || Conn < 0 || Conn >= NUM_DUMMIES)
		return nullptr;
	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId < 0)
		return nullptr;
	return const_cast<CGameWorld &>(m_World).GetCharacterById(ClientId);
}

CCharacter *CLocalPractice::AnyPracticeChar() const
{
	// Commands act on whichever tee is being played right now, falling back to the other one so
	// that a command typed straight after a swap still lands somewhere sensible.
	if(CCharacter *pChar = PracticeChar(g_Config.m_ClDummy))
		return pChar;
	return PracticeChar(!g_Config.m_ClDummy);
}

bool CLocalPractice::IsAvailableHere(const char **ppReason) const
{
	const CGameInfo &Info = GameClient()->m_GameInfo;

	// Stepping out of the round to practice beside it is not something a mode that scores players
	// against each other should have to account for, so none of them get it. m_Pvp covers every
	// non-race gametype; fastcap is a race mode but is still players against players.
	if(Info.m_Pvp || Info.m_FlagStartsRace)
	{
		if(ppReason)
			*ppReason = "not available in competitive game modes";
		return false;
	}

	// Servers that mean their map to be discovered a screen at a time turn zoom off. A practice
	// world hands back exactly what that is holding on to, since its camera follows a tee that /tp
	// will put anywhere on the map, so it has to follow the same switch.
	if(!Info.m_AllowZoom)
	{
		if(ppReason)
			*ppReason = "not available while the server has zoom disabled";
		return false;
	}

	return true;
}

void CLocalPractice::Toggle()
{
	if(m_Active)
		Stop();
	else
		Start();
}

void CLocalPractice::Start()
{
	if(m_Active)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		Print("you have to be on a server");
		return;
	}

	const char *pReason;
	if(!IsAvailableHere(&pReason))
	{
		Print("%s", pReason);
		return;
	}

	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || !HasServerPresence(LocalId))
	{
		// A tee to practice away from is all this needs, and a paused one is still a tee
		Print("you need a tee in the game, alive or paused");
		return;
	}

	m_ClockConn = g_Config.m_ClDummy;

	// Everything the world needs comes across with this: the collision, the tuning, the map bugs,
	// the team state and a copy of the switchers, which is what lets practice switch state fork
	// from the server's instead of tracking it.
	m_World.CopyWorldClean(&GameClient()->m_PredictedWorld);
	m_World.m_LocalClientId = LocalId;

	// Nothing in here is ever second-guessed against a server, so every "how much do we dare
	// predict" switch goes on. They exist to keep mispredictions down, and a world with no server
	// behind it has no mispredictions to keep down. Left at the user's settings, cl_antiping_weapons
	// alone would decide whether a practice weapon fires at all, and by default it does not.
	m_World.m_WorldConfig.m_PredictWeapons = true;
	m_World.m_WorldConfig.m_PredictEvents = true;
	if(m_World.m_WorldConfig.m_IsDDRace)
	{
		m_World.m_WorldConfig.m_PredictDDRace = true;
		m_World.m_WorldConfig.m_PredictTiles = true;
	}
	// Freeze is physics here rather than a display choice, so it cannot be left switched off
	if(m_World.m_WorldConfig.m_PredictFreeze == 0)
		m_World.m_WorldConfig.m_PredictFreeze = 1;
	m_Tick = m_World.m_GameTick;

	// Predicted events belong to the world that produced them. Starting empty keeps practice
	// sounds and explosions out of the real world's dedup list.
	m_World.m_PredictedEvents.clear();

	RemoveForeignEntities();
	CollectSpawns();

	m_Active = true;

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;
		if(!m_World.GetCharacterById(ClientId))
			SeedCharacter(ClientId);
		if(m_World.GetCharacterById(ClientId))
			BeginConn(Conn);
	}

	m_PrevWorld.CopyWorldClean(&m_World);
	m_AlertReason = EMoveReason::NONE;
	mem_zero(m_aPauseState, sizeof(m_aPauseState));
	m_TeleSeed = (unsigned int)Client()->GameTick(g_Config.m_ClDummy);

	// Registered with the chat so they autocomplete alongside the server's own, and taken back out
	// again on the way out so they do not linger as commands that would go nowhere
	for(const CCommand &Command : ms_aCommands)
		GameClient()->m_Chat.RegisterCommand(Command.m_pName, Command.m_pArgs, Command.m_pHelp);

	Print("practice world started, /exit again to leave, /help for commands");
}

void CLocalPractice::Stop()
{
	if(!m_Active)
		return;
	// Before the world goes, so the outgoing counters are put back to what the server has been
	// seeing rather than jumping by however many clicks practice absorbed
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		if(m_aSeeded[Conn])
			ResyncCounters(Conn);
	mem_zero(m_aSeeded, sizeof(m_aSeeded));
	mem_zero(m_aWatchArmed, sizeof(m_aWatchArmed));
	for(CPracticeTee &Tee : m_aPracticeTee)
		Tee.m_Valid = false;
	m_WasControlling = false;

	for(const CCommand &Command : ms_aCommands)
		GameClient()->m_Chat.UnregisterCommand(Command.m_pName);

	m_Active = false;
	m_pRenderCurWorld = nullptr;
	m_pRenderPrevWorld = nullptr;
	m_World.Clear();
	m_PrevWorld.Clear();
	if(Client()->State() == IClient::STATE_ONLINE)
		Print("left local practice");
}

void CLocalPractice::RemoveForeignEntities()
{
	// Only our own tees are simulated. Everyone else's would be running on inputs we do not have,
	// so they would drift immediately and be worse than not being there.
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsLocalId(i))
			continue;
		if(CCharacter *pChar = m_World.GetCharacterById(i))
			pChar->Destroy();
	}

	CEntity *pNext = nullptr;
	for(CEntity *pEnt = m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pEnt; pEnt = pNext)
	{
		pNext = pEnt->TypeNext();
		if(!IsLocalId(((CProjectile *)pEnt)->GetOwner()))
			pEnt->Destroy();
	}
	for(CEntity *pEnt = m_World.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pNext)
	{
		pNext = pEnt->TypeNext();
		if(!IsLocalId(((CLaser *)pEnt)->GetOwner()))
			pEnt->Destroy();
	}
}

void CLocalPractice::SeedCharacter(int ClientId)
{
	const CGameClient::CSnapState::CCharacterInfo &Info = GameClient()->m_Snap.m_aCharacters[ClientId];
	if(m_World.GetCharacterById(ClientId))
		return;

	if(!Info.m_Active)
	{
		// Paused: there is no character on the wire to copy, only somewhere the tee is standing, so
		// one is built there from scratch the way the server would hand one back.
		vec2 Pos;
		if(!GameClient()->m_aClients[ClientId].m_SpecCharPresent || !ServerPos(ClientId, Pos))
			return;

		CNetObj_Character Spawned = {};
		Spawned.m_X = (int)Pos.x;
		Spawned.m_Y = (int)Pos.y;
		Spawned.m_Tick = m_Tick;
		Spawned.m_Weapon = WEAPON_GUN;
		Spawned.m_AmmoCount = -1;

		m_World.m_GameTick = m_Tick;
		CCharacter *pSpec = new CCharacter(&m_World, ClientId, &Spawned, nullptr);
		m_World.InsertEntity(pSpec);
		pSpec->m_IsLocal = true;
		pSpec->GiveWeapon(WEAPON_HAMMER);
		pSpec->GiveWeapon(WEAPON_GUN);
		pSpec->SetActiveWeapon(WEAPON_GUN);
		ClearFreeze(pSpec);
		return;
	}

	// CCharacter::Read works freeze durations out against the world's own tick, and the snapshot
	// it is reading was stamped on the server's. So build the character on that clock and shift it
	// onto ours afterwards, rather than handing it a tick it has no way to interpret.
	const int RefTick = GameClient()->m_PredictedWorld.m_GameTick;
	const int Delta = m_Tick - RefTick;

	CNetObj_Character Cur = Info.m_Cur;
	CNetObj_DDNetCharacter Extended = Info.m_ExtendedData;

	m_World.m_GameTick = RefTick;
	CCharacter *pChar = new CCharacter(&m_World, ClientId, &Cur, Info.m_HasExtendedData ? &Extended : nullptr);
	m_World.InsertEntity(pChar);
	m_World.m_GameTick = m_Tick;
	pChar->RebaseTicks(Delta);
	pChar->m_IsLocal = true;
}

void CLocalPractice::OnNewSnapshot()
{
	if(!m_Active)
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
	{
		Stop();
		return;
	}

	// The game info this rests on is rebuilt from every server info, so a mode change or the server
	// turning zoom off has to end a practice world that is already running, not just refuse the next
	const char *pReason;
	if(!IsAvailableHere(&pReason))
	{
		Print("local practice ended, %s", pReason);
		Stop();
		return;
	}

	// Pausing and spectating are fine to practice through; having no tee left at all is not
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	const int LocalConnIdx = LocalConn(LocalId);
	if(LocalId < 0 || (!HasServerPresence(LocalId) && LocalConnIdx >= 0 && m_aMissingSnaps[LocalConnIdx] > gs_MissingSnapsForDeath))
	{
		Print("local practice ended, your real tee is no longer in the game");
		Stop();
		return;
	}

	// A dummy that connects mid practice materializes here, at the position the server actually
	// spawned it at, rather than being conjured next to the tee already practicing.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0 || m_aSeeded[Conn])
			continue;
		if(!HasServerPresence(ClientId))
			continue;

		SeedCharacter(ClientId);
		if(m_World.GetCharacterById(ClientId))
		{
			BeginConn(Conn);
			Print("dummy joined local practice");
		}
	}

	// Only a connection going away takes a practice tee with it. Tying this to the real tee being
	// alive instead, as it first did, destroyed and re-seeded the tee on any snapshot where the
	// server had it dead -- which pinned it to its spawn and stopped it interacting with anything.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(GameClient()->m_aLocalIds[Conn] >= 0)
			continue;
		if(!m_aSeeded[Conn])
			continue;
		EndConn(Conn);
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsLocalId(i))
			continue;
		if(CCharacter *pChar = m_World.GetCharacterById(i))
			pChar->Destroy();
	}

	CheckServerMoved();

	// m_SpecInfo has just been rebuilt from the snapshot, so the override goes back on straight
	// away. A frame without it puts CControls::ClampMousePos back on the radius around the tee,
	// which is what dragged the free view back whenever it got far enough out.
	OverrideSpectatorView();
}

void CLocalPractice::CollectSpawns()
{
	m_vSpawns.clear();

	const CCollision *pCollision = GameClient()->Collision();
	const CTile *pTiles = pCollision->GameLayer();
	if(!pTiles)
		return;

	// The server picks a spawn out of the game layer's spawn entities; the same tiles are sitting
	// in the collision the practice world already borrows, so they can just be read off it.
	const int Width = pCollision->GetWidth();
	const int Height = pCollision->GetHeight();
	for(int y = 0; y < Height; y++)
	{
		for(int x = 0; x < Width; x++)
		{
			if(pTiles[y * Width + x].m_Index != ENTITY_OFFSET + ENTITY_SPAWN)
				continue;
			m_vSpawns.emplace_back(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
		}
	}
}

bool CLocalPractice::OnKill()
{
	if(!m_Active)
		return false;

	const int Conn = g_Config.m_ClDummy;
	if(!PracticeChar(Conn))
		return false;

	Respawn(Conn);
	// The dummy follows along when it is set to copy, the same way a real kill would reach it
	if(g_Config.m_ClDummyCopyMoves && PracticeChar(!Conn))
		Respawn(!Conn);
	return true;
}

void CLocalPractice::Respawn(int Conn)
{
	if(!PracticeChar(Conn) && !IsPausedOut(Conn))
		return;

	vec2 Pos = m_aStartPos[Conn];
	if(!m_vSpawns.empty())
	{
		// Round robin rather than random, so repeatedly killing walks the spawns instead of
		// sometimes handing back the one just left
		Pos = m_vSpawns[m_NextSpawn % m_vSpawns.size()];
		m_NextSpawn++;
	}

	TeleportTo(Conn, Pos);
	// A kill is a fresh tee, not a teleport, so nothing carries over
	m_aHasLastTp[Conn] = false;
	m_aRescuePos[Conn] = Pos;
	m_aHasRescuePos[Conn] = true;

	CCharacter *pChar = PracticeChar(Conn);
	if(!pChar)
		return; // paused out: the position is all there is to reset

	pChar->Core()->m_Jumped = 0;
	pChar->Core()->m_JumpedTotal = 0;
	pChar->Core()->m_Jumps = 2;
	for(int Weapon = WEAPON_SHOTGUN; Weapon < NUM_WEAPONS; Weapon++)
		pChar->GiveWeapon(Weapon, true);
	pChar->GiveWeapon(WEAPON_HAMMER);
	pChar->GiveWeapon(WEAPON_GUN);
	pChar->SetActiveWeapon(WEAPON_GUN);
}

void CLocalPractice::SyncFireCounter(int Conn)
{
	CCharacter *pChar = PracticeChar(Conn);
	if(!pChar)
		return;

	// A character carries whatever fire counter it was built with: zero for one made from scratch,
	// the real prediction's for one copied out of it. Handing it a counter that starts anywhere
	// else is a jump, and a jump in a press counter is a burst of presses -- which is what fired
	// the moment practice started, and again on either side of a pause.
	m_aFireCounter[Conn] = pChar->LatestInput()->m_Fire;
	m_aLastSourceFire[Conn] = 0;
	m_aLastSourceId[Conn] = -1;
}

void CLocalPractice::BeginConn(int Conn)
{
	CCharacter *pChar = m_World.GetCharacterById(GameClient()->m_aLocalIds[Conn]);
	const vec2 Pos = pChar ? pChar->Core()->m_Pos : vec2(0.0f, 0.0f);

	m_aSeeded[Conn] = true;
	m_aStartPos[Conn] = Pos;
	m_aRescuePos[Conn] = Pos;
	m_aHasRescuePos[Conn] = pChar != nullptr;
	m_aHasLastTp[Conn] = false;

	// Latched rather than zeroed: fire, next weapon and previous weapon are all press counters, so
	// a zero on the way out reads as a burst of real actions rather than as nothing happening.
	m_aLatchedInput[Conn] = GameClient()->m_Controls.m_aInputData[Conn];
	m_aLatchedInput[Conn].m_Direction = 0;
	m_aLatchedInput[Conn].m_Jump = 0;
	m_aLatchedInput[Conn].m_Hook = 0;
	// An odd counter means the button is being held. Left that way, a full auto weapon on the real
	// tee fires for as long as practice lasts, so one release edge is sent and then nothing more.
	if(m_aLatchedInput[Conn].m_Fire & 1)
		m_aLatchedInput[Conn].m_Fire++;

	// Whatever this tee did on the server before practice started is not something practice should
	// report as having happened to it
	SyncFireCounter(Conn);
	m_aWatchArmed[Conn] = false;
	m_aWasHooked[Conn] = false;
	m_aMissingSnaps[Conn] = 0;
	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId >= 0 && GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		m_aAnchorPos[Conn] = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
	else
		m_aAnchorPos[Conn] = Pos;
}

void CLocalPractice::EndConn(int Conn)
{
	if(!m_aSeeded[Conn])
		return;
	m_aSeeded[Conn] = false;
	m_aWatchArmed[Conn] = false;

	if(CCharacter *pChar = m_World.GetCharacterById(GameClient()->m_aLocalIds[Conn]))
		pChar->Destroy();

	ResyncCounters(Conn);
}

void CLocalPractice::ResyncCounters(int Conn)
{
	// The server has been receiving the latched counters all along, while the client's own kept
	// climbing with every click. Handing it the climbed values would land as a burst of presses --
	// which is what made the real tee fire the moment practice was switched off. Bring them back
	// down, keeping whichever parity the key is physically in so a held button stays held.
	const auto Resync = [](int &Live, int Latched) {
		Live = Latched + ((Live ^ Latched) & 1);
	};

	CNetObj_PlayerInput *apInputs[] = {
		&GameClient()->m_Controls.m_aInputData[Conn],
		Conn == g_Config.m_ClDummy ? nullptr : &GameClient()->m_DummyInput,
	};
	for(CNetObj_PlayerInput *pInput : apInputs)
	{
		if(!pInput)
			continue;
		Resync(pInput->m_Fire, m_aLatchedInput[Conn].m_Fire);
		Resync(pInput->m_NextWeapon, m_aLatchedInput[Conn].m_NextWeapon);
		Resync(pInput->m_PrevWeapon, m_aLatchedInput[Conn].m_PrevWeapon);
	}
}

void CLocalPractice::CheckServerMoved()
{
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
		{
			continue;
		}

		const CGameClient::CSnapState::CCharacterInfo &Info = GameClient()->m_Snap.m_aCharacters[ClientId];
		const bool Alive = HasServerPresence(ClientId);
		vec2 Pos = vec2(Info.m_Cur.m_X, Info.m_Cur.m_Y);
		ServerPos(ClientId, Pos);
		const int FreezeEnd = Info.m_HasExtendedData ? Info.m_ExtendedData.m_FreezeEnd : 0;

		const bool HadPos = m_aHasServerPos[ClientId];
		const bool WasAlive = m_aServerAlive[ClientId];
		const int WasFreezeEnd = m_aServerFreezeEnd[ClientId];
		const bool Armed = m_aWatchArmed[Conn];

		if(GameClient()->m_IsDummySwapping)
			continue;

		m_aHasServerPos[ClientId] = Alive;
		m_aServerAlive[ClientId] = Alive;
		m_aServerFreezeEnd[ClientId] = FreezeEnd;

		// A connection is only worth watching once its real tee has been alive for a snapshot under
		// this connection's own id. Without that, a dummy joining reports itself as having died,
		// because it genuinely was dead for the snapshots before it spawned.
		if(!m_aSeeded[Conn])
			continue;
		m_aWatchArmed[Conn] = m_aWatchArmed[Conn] || Alive;
		if(!Armed)
			continue;

		// Swapping connections takes the tee out of a snapshot or two on the way through, and a
		// pause takes the character away and hands back a spectator one. Neither is a death, so a
		// death has to be absent for long enough that it cannot be either of those.
		if(!Alive)
		{
			m_aMissingSnaps[Conn]++;
			if(WasAlive && m_aMissingSnaps[Conn] == gs_MissingSnapsForDeath)
				TriggerAlert(ClientId, EMoveReason::DIED);
			continue;
		}
		m_aMissingSnaps[Conn] = 0;

		if(!HadPos || !WasAlive)
			continue;

		// Someone else's hook being on us, rather than ours being on them. m_HookedPlayer on our own
		// character says who we hooked, which is why watching it caught nothing that mattered.
		const bool HookedByOther = IsHookedByOther(ClientId);
		if(HookedByOther && !m_aWasHooked[Conn])
		{
			m_aWasHooked[Conn] = true;
			TriggerAlert(ClientId, EMoveReason::DRAGGED);
			continue;
		}
		m_aWasHooked[Conn] = HookedByOther;

		// Measured against where the tee was left, not against the previous snapshot. A tee being
		// dragged moves a few pixels per snapshot and would never trip a per-snapshot threshold,
		// yet it ends up somewhere else entirely.
		if(distance(Pos, m_aAnchorPos[Conn]) > gs_MovedThreshold)
		{
			TriggerAlert(ClientId, EMoveReason::TELEPORTED);
			continue;
		}
		// Edge triggered, so being frozen, thawed and frozen again is caught each time
		if(FreezeEnd != 0 && WasFreezeEnd == 0)
		{
			TriggerAlert(ClientId, EMoveReason::FROZEN);
			continue;
		}
	}
}

bool CLocalPractice::IsHookedByOther(int ClientId) const
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == ClientId || IsSimulated(i))
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		if(GameClient()->m_Snap.m_aCharacters[i].m_Cur.m_HookedPlayer == ClientId)
			return true;
	}
	return false;
}

void CLocalPractice::TriggerAlert(int ClientId, EMoveReason Reason)
{
	// Re-anchor, so one move is reported once instead of every snapshot the tee spends away from
	// where it started
	const int Conn = LocalConn(ClientId);
	if(Conn >= 0 && GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		m_aAnchorPos[Conn] = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);

	m_AlertReason = Reason;
	m_AlertSince = Client()->LocalTime();

	if(g_Config.m_ClLocalPracticeExitOnMove)
	{
		Print("your real tee was moved");
		Stop();
	}
}

void CLocalPractice::OnUpdatePractice()
{
	if(!m_Active)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		Stop();
		return;
	}

	// The practice world runs on the prediction clock rather than on a wall clock of its own.
	// A free-running clock drifts out of phase with PredIntraGameTick, and everything that reads a
	// tick stamp back -- the weapon animation above all -- measures it against that fraction, which
	// is what made the hammer stutter. Sharing the clock also means fast input composes here the
	// same way it does everywhere else, instead of having to be reinvented.
	// Only the active connection's prediction time is kept up to date, see CClient::ProcessServerPacket
	// -- the other one's simply stops. So the clock follows whichever is active and is rebased when
	// that changes, rather than being pinned to one and left standing still after a swap.
	const int Conn = g_Config.m_ClDummy;
	const int Target = Client()->PredGameTick(Conn);
	if(Target <= 0)
		return; // that connection has no prediction time yet, as just after a dummy connects

	// A swap hands over a tick belonging to a different connection, and a fresh connection starts
	// its own at zero. Neither is a backlog to work through: stepping the difference races the world
	// forward or drags it back, so the new time is taken as it is.
	if(Conn != m_ClockConn || Target < m_Tick || Target - m_Tick > SERVER_TICK_SPEED)
	{
		m_ClockConn = Conn;
		m_Tick = Target;
		m_World.m_GameTick = m_Tick;
		m_PrevWorld.CopyWorldClean(&m_World);
	}

	const int Steps = std::min(Target - m_Tick, 10);
	for(int i = 0; i < Steps; i++)
		TickPracticeWorld();

	UpdateLookahead();
}

void CLocalPractice::StepWorld(CGameWorld &World, int Tick)
{
	CNetObj_PlayerInput aInput[NUM_DUMMIES];
	CCharacter *apChar[NUM_DUMMIES] = {nullptr, nullptr};

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		apChar[Conn] = ClientId >= 0 ? World.GetCharacterById(ClientId) : nullptr;
		if(apChar[Conn])
			aInput[Conn] = BuildInput(World, Conn, Tick);
	}

	// Lower client id first, the same order the server applies input in, so hook strength between
	// our own two tees comes out the way it would on a server.
	int aOrder[NUM_DUMMIES] = {0, 1};
	if(apChar[0] && apChar[1] && apChar[0]->GetCid() > apChar[1]->GetCid())
	{
		aOrder[0] = 1;
		aOrder[1] = 0;
	}

	for(int i = 0; i < NUM_DUMMIES; i++)
		if(apChar[aOrder[i]])
			apChar[aOrder[i]]->OnDirectInput(&aInput[aOrder[i]]);

	World.m_GameTick = Tick;

	for(int i = 0; i < NUM_DUMMIES; i++)
		if(apChar[aOrder[i]])
			apChar[aOrder[i]]->OnPredictedInput(&aInput[aOrder[i]]);

	World.Tick();
	HandleTeleporters(World, Tick);
}

void CLocalPractice::UpdateLookahead()
{
	// Where along the timeline the tee should be drawn, counted in ticks from the previous one.
	// Fast input asks for it to be drawn further along than the simulation has reached, so that far
	// is simulated speculatively -- the same trade the real prediction makes for the same reason.
	float Ahead = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	if(g_Config.m_TcFastInput)
		Ahead += g_Config.m_TcFastInputAmount / (float)(1000 / SERVER_TICK_SPEED);

	const int Whole = std::min((int)Ahead, 16);
	m_RenderIntra = std::clamp(Ahead - Whole, 0.0f, 1.0f);

	if(Whole <= 0)
	{
		m_pRenderPrevWorld = &m_PrevWorld;
		m_pRenderCurWorld = &m_World;
		return;
	}

	// Speculative ticks must not be heard: their sounds and explosions are re-run every frame and
	// may never happen at all
	m_aLookWorld[0].CopyWorldClean(&m_World);
	m_aLookWorld[0].m_WorldConfig.m_PredictEvents = false;

	int Tick = m_Tick;
	for(int i = 1; i <= Whole; i++)
	{
		if(i == Whole)
		{
			m_aLookWorld[1].CopyWorldClean(&m_aLookWorld[0]);
			m_aLookWorld[1].m_WorldConfig.m_PredictEvents = false;
		}
		StepWorld(m_aLookWorld[0], ++Tick);
	}

	m_pRenderPrevWorld = &m_aLookWorld[1];
	m_pRenderCurWorld = &m_aLookWorld[0];
}

void CLocalPractice::TickPracticeWorld()
{
	m_PrevWorld.CopyWorldClean(&m_World);

	// Only on the ticks that really happen: the speculative ones re-run every frame and would turn
	// one press into as many as the look-ahead is deep
	AdvanceInputCounters();
	ProcessPause();

	StepWorld(m_World, ++m_Tick);

	// A tee that jumped across the map this tick must not be drawn sliding there. Same rule the
	// prediction applies to a misprediction in CCharacter::Read, and it covers the teleport
	// commands as much as the teleporters.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;
		CCharacter *pCur = m_World.GetCharacterById(ClientId);
		CCharacter *pPrev = m_PrevWorld.GetCharacterById(ClientId);
		if(!pCur || !pPrev)
			continue;
		if(distance(pPrev->Core()->m_Pos, pCur->Core()->m_Pos) > 10.0f * 32.0f)
			pPrev->Core()->m_Pos = pCur->Core()->m_Pos;
	}

	// Air jumps, jump and hook sounds are not predicted events at all: the core raises them as
	// flags that live for exactly one tick, and OnPredict reads them inline. So they have to be
	// read here, straight after the tick that raised them, or they are gone.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		if(CCharacter *pChar = PracticeChar(Conn))
			PlayCoreEvents(pChar);

	// Sounds, explosions and hit markers from the practice world, played out of its own event list
	// so they never touch the one the real world dedups against
	GameClient()->HandlePredictedEvents(m_Tick, &m_World);

	RecordRescuePos();
}

CNetObj_PlayerInput CLocalPractice::BuildInput(CGameWorld &World, int Conn, int Tick) const
{
	const CControls &Controls = GameClient()->m_Controls;
	const int Active = g_Config.m_ClDummy;

	// m_aInputData and m_DummyInput only become fully current when the engine snaps input for the
	// network, which is both slower than the frame rate and gated on there being something worth
	// sending. A world with no network under it has no reason to wait for the network's schedule,
	// so everything that goes stale in between is taken from the live source instead. That is the
	// whole reason the dummy features misbehaved in here: their outputs are written by the snap.
	if(IsPaused(Conn))
	{
		// Paused, the tee simply stops receiving input, which is what settles it into its resting
		// animation. It is still there and still solid, exactly as on the server.
		CNetObj_PlayerInput Input = {};
		Input.m_TargetX = 1;
		Input.m_Fire = m_aFireCounter[Conn];
		return Input;
	}

	if(Conn == Active)
	{
		CNetObj_PlayerInput Input = Controls.m_aInputData[Conn];
		Input.m_Fire = m_aFireCounter[Conn];

		Input.m_Direction = 0;
		if(Controls.m_aInputDirectionLeft[Conn] && !Controls.m_aInputDirectionRight[Conn])
			Input.m_Direction = -1;
		if(!Controls.m_aInputDirectionLeft[Conn] && Controls.m_aInputDirectionRight[Conn])
			Input.m_Direction = 1;

		Input.m_TargetX = (int)Controls.m_aMousePos[Conn].x;
		Input.m_TargetY = (int)Controls.m_aMousePos[Conn].y;
		if(!Input.m_TargetX && !Input.m_TargetY)
			Input.m_TargetY = -1;

		return Input;
	}

	CNetObj_PlayerInput Input = GameClient()->m_DummyInput;
	Input.m_Fire = m_aFireCounter[Conn];

	if(g_Config.m_ClDummyHammer)
	{
		// The same input the real dummy hammers with, so it hammers at the same rate. Only the aim
		// is worked out here, from the tees in this world rather than the ones on the server.
		Input = GameClient()->m_HammerInput;
		Input.m_Fire = m_aFireCounter[Conn];
		Input.m_Direction = 0;
		Input.m_Jump = 0;
		Input.m_Hook = 0;
		Input.m_WantedWeapon = WEAPON_HAMMER + 1;

		const int DummyId = GameClient()->m_aLocalIds[Conn];
		const int MainId = GameClient()->m_aLocalIds[Active];
		CCharacter *pDummy = DummyId >= 0 ? World.GetCharacterById(DummyId) : nullptr;
		CCharacter *pMain = MainId >= 0 ? World.GetCharacterById(MainId) : nullptr;
		if(pDummy && pMain)
		{
			const vec2 Dir = pMain->Core()->m_Pos - pDummy->Core()->m_Pos;
			Input.m_TargetX = (int)Dir.x;
			Input.m_TargetY = (int)Dir.y;
			if(!Input.m_TargetX && !Input.m_TargetY)
				Input.m_TargetY = -1;
		}
		return Input;
	}

	if(g_Config.m_ClDummyControl)
	{
		Input.m_Jump = g_Config.m_ClDummyJump;
		Input.m_Hook = g_Config.m_ClDummyHook;
		return Input;
	}

	if(g_Config.m_ClDummyCopyMoves)
	{
		const CNetObj_PlayerInput Main = BuildInput(World, Active, Tick);
		Input.m_Direction = Main.m_Direction;
		Input.m_Jump = Main.m_Jump;
		Input.m_Hook = Main.m_Hook;
		Input.m_TargetX = Main.m_TargetX;
		Input.m_TargetY = Main.m_TargetY;
		Input.m_WantedWeapon = Main.m_WantedWeapon;
		Input.m_NextWeapon = Main.m_NextWeapon;
		Input.m_PrevWeapon = Main.m_PrevWeapon;
	}

	return Input;
}

int CLocalPractice::FireSource(int Conn, int &SourceId) const
{
	// The counter the client is really driving this tee's fire with, and an id for which one it is.
	// A change of id is a change of meaning, not a press.
	const int Active = g_Config.m_ClDummy;
	if(Conn == Active)
	{
		SourceId = 0;
		return GameClient()->m_Controls.m_aInputData[Conn].m_Fire;
	}
	if(g_Config.m_ClDummyHammer)
	{
		// m_HammerInput is what CGameClient::OnSnapInput hands the server, cadence and all: one
		// press every 25 input snaps, the counter kept odd throughout. Reading it rather than
		// reinventing the timing is the only way for this to hammer at the rate the real one does.
		SourceId = 1;
		return GameClient()->m_HammerInput.m_Fire;
	}
	if(g_Config.m_ClDummyControl)
	{
		SourceId = 2;
		return 0; // held state rather than a counter, handled by the caller
	}
	if(g_Config.m_ClDummyCopyMoves)
	{
		SourceId = 3;
		return GameClient()->m_Controls.m_aInputData[Active].m_Fire;
	}
	SourceId = 4;
	return GameClient()->m_DummyInput.m_Fire;
}

void CLocalPractice::AdvanceInputCounters()
{
	const int Active = g_Config.m_ClDummy;
	const bool Swapped = Active != m_LastActiveConn;
	m_LastActiveConn = Active;

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		int SourceId = -1;
		const int Source = FireSource(Conn, SourceId);
		const int LastSource = m_aLastSourceFire[Conn];
		const bool SameSource = !Swapped && SourceId == m_aLastSourceId[Conn];
		m_aLastSourceFire[Conn] = Source;
		m_aLastSourceId[Conn] = SourceId;

		// A swap exchanges the two connections' counters and a feature change swaps the source out
		// from under us. Both look like a jump and neither is anything the player did.
		if(!SameSource)
			continue;

		if(SourceId == 1)
		{
			// Dummy hammer. OnSnapInput steps its counter by two per hammer and keeps it odd
			// throughout, so each step is one press; mirrored here the way it is written there.
			if(Source != LastSource)
				m_aFireCounter[Conn] = (m_aFireCounter[Conn] + 1) | 1;
			continue;
		}

		if(SourceId == 2)
		{
			// cl_dummy_control holds the button rather than counting presses
			if((g_Config.m_ClDummyFire != 0) != ((m_aFireCounter[Conn] & 1) != 0))
				m_aFireCounter[Conn]++;
			continue;
		}

		// Everything else is a real button, where odd means held. Following its parity rather than
		// adding on its difference is what puts the shot on the press: the two counters start from
		// unrelated numbers, and while a difference carries across either parity, an offset between
		// them turns every press into a release and every release into a press.
		if(((Source & 1) != 0) != ((m_aFireCounter[Conn] & 1) != 0))
			m_aFireCounter[Conn]++;
	}
}

void CLocalPractice::PlayCoreEvents(CCharacter *pChar)
{
	if(GameClient()->m_SuppressEvents)
		return;

	const vec2 Pos = pChar->Core()->m_Pos;
	const int Events = pChar->Core()->m_TriggeredEvents;

	// Deliberately not gated on cl_predict: this world is only ever predicted, so switching
	// prediction off would leave it silent rather than leaving it unpredicted.
	if(Events & COREEVENT_AIR_JUMP)
		GameClient()->m_Effects.AirJump(Pos, 1.0f, 1.0f);

	if(!g_Config.m_SndGame)
		return;

	if(Events & COREEVENT_GROUND_JUMP)
		GameClient()->m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, Pos);
	if(Events & COREEVENT_HOOK_ATTACH_GROUND)
		GameClient()->m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_ATTACH_GROUND, 1.0f, Pos);
	if(Events & COREEVENT_HOOK_HIT_NOHOOK)
		GameClient()->m_Sounds.PlayAndRecord(CSounds::CHN_WORLD, SOUND_HOOK_NOATTACH, 1.0f, Pos);
	if(Events & COREEVENT_HOOK_ATTACH_PLAYER)
		m_World.CreatePredictedSound(Pos, SOUND_HOOK_ATTACH_PLAYER, pChar->GetCid());
}

int CLocalPractice::PickTeleOut(int Tick, int Size) const
{
	// A pure function of this world's seed and the tick, deliberately: the speculative fast input
	// ticks run this too, and anything that consumed randomness as it went would hand them a
	// different exit every frame and make the tee flicker between them.
	if(Size <= 1)
		return 0;
	unsigned int Hash = m_TeleSeed ^ ((unsigned int)Tick * 2654435761u);
	Hash ^= Hash >> 13;
	Hash *= 1274126177u;
	Hash ^= Hash >> 16;
	return (int)(Hash % (unsigned int)Size);
}

void CLocalPractice::TeleportChar(CCharacter *pChar, vec2 Pos, bool ResetVel, bool ReleaseHooked, CGameWorld &World)
{
	pChar->Core()->m_Pos = Pos;
	if(ResetVel)
		pChar->Core()->m_Vel = vec2(0.0f, 0.0f);

	if(!g_Config.m_SvTeleportHoldHook)
	{
		pChar->ResetHook();
		if(ReleaseHooked)
			World.ReleaseHooked(pChar->GetCid());
	}

	// The entity position is only synced from the core during the tick, and this runs after it
	pChar->m_Pos = Pos;
	pChar->m_PrevPos = Pos;
	pChar->m_PrevPrevPos = Pos;
}

void CLocalPractice::LoseWeapons(CCharacter *pChar)
{
	for(int Weapon = WEAPON_SHOTGUN; Weapon < NUM_WEAPONS; Weapon++)
		pChar->GiveWeapon(Weapon, true);
}

void CLocalPractice::HandleTeleporters(CGameWorld &World, int Tick)
{
	CCollision *pCollision = World.Collision();
	if(!pCollision || !pCollision->TeleLayer())
		return;

	// Mirrors CCharacter::HandleTiles on the server, in the same order and with the same outcomes.
	// The prediction leaves teleporters alone because it cannot know which exit was picked; nothing
	// here is reconciled against a server, so it picks its own and is only ever answerable to
	// itself. The sv_ settings below are CFGFLAG_GAME, so the server has already sent its own.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;
		CCharacter *pChar = World.GetCharacterById(ClientId);
		if(!pChar)
			continue;
		// Super and invincible pass straight through every one of them
		if(pChar->Core()->m_Super || pChar->Core()->m_Invincible)
			continue;

		const int Index = pCollision->GetPureMapIndex(pChar->Core()->m_Pos);

		const int Tele = pCollision->IsTeleport(Index);
		if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && Tele && !pCollision->TeleOuts(Tele - 1).empty())
		{
			const std::vector<vec2> &vOuts = pCollision->TeleOuts(Tele - 1);
			// A blue teleporter keeps the speed it was entered with; only the red one takes it away
			TeleportChar(pChar, vOuts[PickTeleOut(Tick, vOuts.size())], false, false, World);
			if(g_Config.m_SvTeleportLoseWeapons)
				LoseWeapons(pChar);
			continue;
		}

		const int Evil = pCollision->IsEvilTeleport(Index);
		if(Evil && !pCollision->TeleOuts(Evil - 1).empty())
		{
			const std::vector<vec2> &vOuts = pCollision->TeleOuts(Evil - 1);
			const vec2 Out = vOuts[PickTeleOut(Tick, vOuts.size())];
			if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
			{
				TeleportChar(pChar, Out, true, true, World);
				if(g_Config.m_SvTeleportLoseWeapons)
					LoseWeapons(pChar);
			}
			else
			{
				// The old behaviour moves the tee and touches nothing else
				pChar->Core()->m_Pos = Out;
				pChar->m_Pos = Out;
				pChar->m_PrevPos = Out;
				pChar->m_PrevPrevPos = Out;
			}
			continue;
		}

		const bool CheckEvil = pCollision->IsCheckEvilTeleport(Index);
		if(CheckEvil || pCollision->IsCheckTeleport(Index))
		{
			// The checkpoint the tee last ran over, then every earlier one, then the spawn. The
			// checkpoint itself is already tracked by the prediction, in CCharacter::HandleTiles.
			bool Teleported = false;
			for(int k = pChar->m_TeleCheckpoint - 1; k >= 0 && !Teleported; k--)
			{
				const std::vector<vec2> &vOuts = pCollision->TeleCheckOuts(k);
				if(vOuts.empty())
					continue;
				TeleportChar(pChar, vOuts[PickTeleOut(Tick, vOuts.size())], CheckEvil, CheckEvil, World);
				Teleported = true;
			}

			if(!Teleported && !m_vSpawns.empty())
			{
				const vec2 Spawn = m_vSpawns[PickTeleOut(Tick, m_vSpawns.size())];
				TeleportChar(pChar, Spawn, CheckEvil, CheckEvil, World);
			}
			continue;
		}
	}
}

void CLocalPractice::RecordRescuePos()
{
	// What /rescue goes back to: the last place the tee stood on the ground, unfrozen. That is
	// close enough to what a server means by it to be useful, without keeping a position history.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		CCharacter *pChar = PracticeChar(Conn);
		if(!pChar)
			continue;
		if(pChar->m_FreezeTime != 0 || pChar->Core()->m_DeepFrozen || pChar->Core()->m_LiveFrozen)
			continue;
		if(!pChar->IsGrounded())
			continue;
		m_aRescuePos[Conn] = pChar->Core()->m_Pos;
		m_aHasRescuePos[Conn] = true;
	}
}

void CLocalPractice::NeutralizeInput(CNetObj_PlayerInput *pInput, int Conn) const
{
	if(!m_Active || Conn < 0 || Conn >= NUM_DUMMIES)
		return;
	// Keyed on the connection being in practice at all, not on it having a character right now.
	// /spec takes the practice tee out of the world, and keying on the character meant the real tee
	// started receiving live input again the moment it did -- including the fire that had it
	// throwing a projectile of its own.
	if(!m_aSeeded[Conn])
		return;

	// Player flags stay live so the server still sees chatting, the scoreboard and the rest.
	const int PlayerFlags = pInput->m_PlayerFlags;
	const int TargetX = pInput->m_TargetX;
	const int TargetY = pInput->m_TargetY;

	*pInput = m_aLatchedInput[Conn];
	pInput->m_PlayerFlags = PlayerFlags;

	if(!g_Config.m_ClLocalPracticeFreezeAim)
	{
		pInput->m_TargetX = TargetX;
		pInput->m_TargetY = TargetY;
	}
	if(!pInput->m_TargetX && !pInput->m_TargetY)
		pInput->m_TargetX = 1;
}

void CLocalPractice::BuildRenderState()
{
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
		m_aPracticeTee[Conn].m_Valid = false;

	if(!m_Active || m_InScope)
		return;

	CGameWorld *pCurWorld = m_pRenderCurWorld ? m_pRenderCurWorld : &m_World;
	CGameWorld *pPrevWorld = m_pRenderPrevWorld ? m_pRenderPrevWorld : &m_PrevWorld;

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;
		CCharacter *pChar = pCurWorld->GetCharacterById(ClientId);
		if(!pChar)
			continue;
		CCharacter *pPrev = pPrevWorld->GetCharacterById(ClientId);

		// Everything the practice tee does not have an opinion about is copied from the real tee,
		// which is where the client, name and skin live. Nothing is written back.
		const CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
		CPracticeTee &Tee = m_aPracticeTee[Conn];
		Tee.m_RenderPrev = Data.m_RenderPrev;
		Tee.m_RenderCur = Data.m_RenderCur;

		const CCharacterCore Cur = pChar->GetCore();
		const CCharacterCore Prev = pPrev ? pPrev->GetCore() : Cur;
		Tee.m_Predicted = Cur;
		Tee.m_PrevPredicted = Prev;

		Cur.Write(&Tee.m_RenderCur);
		Prev.Write(&Tee.m_RenderPrev);

		Tee.m_RenderCur.m_Weapon = pChar->GetActiveWeapon();
		Tee.m_RenderPrev.m_Weapon = Tee.m_RenderCur.m_Weapon;

		// On the prediction clock the attack tick already means what every reader thinks it means
		Tee.m_RenderCur.m_AttackTick = pChar->GetAttackTick();
		Tee.m_RenderPrev.m_AttackTick = Tee.m_RenderCur.m_AttackTick;

		// The core's freeze stamps are ticks on whichever clock froze the tee, and the remaining
		// freeze time is the one clock independent fact, so they are rebuilt from it on the clock
		// the freeze bar and the frozen skin actually read.
		const int ServerTick = Client()->GameTick(g_Config.m_ClDummy);
		const int TotalFreeze = std::max(1, g_Config.m_SvFreezeDelay * SERVER_TICK_SPEED);
		if(pChar->Core()->m_DeepFrozen)
		{
			Tee.m_Predicted.m_FreezeStart = ServerTick - TotalFreeze;
			Tee.m_Predicted.m_FreezeEnd = -1;
		}
		else if(pChar->m_FreezeTime > 0)
		{
			Tee.m_Predicted.m_FreezeEnd = ServerTick + pChar->m_FreezeTime;
			Tee.m_Predicted.m_FreezeStart = Tee.m_Predicted.m_FreezeEnd - std::max(TotalFreeze, pChar->m_FreezeTime + 1);
		}
		else
		{
			Tee.m_Predicted.m_FreezeStart = 0;
			Tee.m_Predicted.m_FreezeEnd = 0;
		}
		Tee.m_PrevPredicted.m_FreezeStart = Tee.m_Predicted.m_FreezeStart;
		Tee.m_PrevPredicted.m_FreezeEnd = Tee.m_Predicted.m_FreezeEnd;
		// The frozen skin and the freeze bar read this one rather than m_Predicted
		Tee.m_RegularPredicted = Tee.m_Predicted;

		// Derived from this world alone, so the real tee's eyes cannot drive the practice tee's.
		// The cost is that /emote does not reach it, since nothing on the wire tells a chosen emote
		// apart from a reflex one.
		const bool Frozen = pChar->m_FreezeTime > 0 || pChar->Core()->m_DeepFrozen || pChar->Core()->m_LiveFrozen;
		if(IsPaused(Conn))
			Tee.m_RenderCur.m_Emote = Frozen ? EMOTE_NORMAL : EMOTE_BLINK; // CCharacter::DetermineEyeEmote
		else if(Frozen)
			Tee.m_RenderCur.m_Emote = EMOTE_BLINK;
		else
			Tee.m_RenderCur.m_Emote = EMOTE_NORMAL;
		Tee.m_RenderPrev.m_Emote = Tee.m_RenderCur.m_Emote;

		Tee.m_RenderPos = mix(Prev.m_Pos, Cur.m_Pos, m_RenderIntra);
		Tee.m_IsPredicted = true;
		Tee.m_IsPredictedLocal = true;
		// Its own pause, so the sitting animation answers to this world rather than to the server
		Tee.m_Paused = IsPaused(Conn);
		Tee.m_Afk = false;
		Tee.m_Valid = true;
	}
}

void CLocalPractice::SwapPracticeTees()
{
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(!m_aPracticeTee[Conn].m_Valid)
			continue;
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;

		CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
		CPracticeTee &Tee = m_aPracticeTee[Conn];
		std::swap(Data.m_Predicted, Tee.m_Predicted);
		std::swap(Data.m_PrevPredicted, Tee.m_PrevPredicted);
		std::swap(Data.m_RegularPredicted, Tee.m_RegularPredicted);
		std::swap(Data.m_RenderPrev, Tee.m_RenderPrev);
		std::swap(Data.m_RenderCur, Tee.m_RenderCur);
		std::swap(Data.m_RenderPos, Tee.m_RenderPos);
		std::swap(Data.m_IsPredicted, Tee.m_IsPredicted);
		std::swap(Data.m_IsPredictedLocal, Tee.m_IsPredictedLocal);
		std::swap(Data.m_Paused, Tee.m_Paused);
		std::swap(Data.m_Afk, Tee.m_Afk);
	}
}

CLocalPractice::CScope::CScope(CLocalPractice *pPractice) :
	m_pPractice(pPractice), m_Applied(pPractice->m_Active && !pPractice->m_InScope)
{
	if(!m_Applied)
		return;
	m_pPractice->SwapPracticeTees();
	m_pPractice->m_InScope = true;
}

CLocalPractice::CScope::~CScope()
{
	if(!m_Applied)
		return;
	m_pPractice->SwapPracticeTees();
	m_pPractice->m_InScope = false;
}

bool CLocalPractice::AimsWithLocalInput(int ClientId) const
{
	// Inside the scope this is the practice tee, which is the one being aimed. Outside it, while
	// practicing, it is the real tee left standing, and it keeps the aim the server has for it.
	return !m_Active || m_InScope || !IsSimulated(ClientId);
}

bool CLocalPractice::OwnsRenderAlpha(int ClientId) const
{
	return m_Active && IsSimulated(ClientId);
}

float CLocalPractice::DrawAlpha(int ClientId) const
{
	if(!OwnsRenderAlpha(ClientId))
		return 1.0f;

	// Outside the scope this is the real tee still standing on the server, which is the ghost
	if(!m_InScope)
		return ServerEntityAlpha(ClientId);

	// Inside it, it is the practice tee: solid, unless this world's own solo says otherwise
	if(IsPracticeSolo(ClientId))
		return g_Config.m_ClShowOthersAlpha / 100.0f;
	return 1.0f;
}

void CLocalPractice::RenderPracticeTees(const CScreenRect &ScreenRect)
{
	if(!m_Active)
		return;

	CScope Scope(this);
	const CPlayers::CFrameSkins Skins = GameClient()->m_Players.LookupFrameSkins();

	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(!m_aPracticeTee[Conn].m_Valid)
			continue;
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;

		const CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
		if(!ScreenRect.Inside(Data.m_RenderPos))
			continue;

		CTeeRenderInfo RenderInfo;
		GameClient()->m_Players.BuildTeeRenderInfo(ClientId, Skins, RenderInfo);
		GameClient()->m_Players.RenderHook(ScreenRect, &Data.m_RenderPrev, &Data.m_RenderCur, &RenderInfo, ClientId);
	}

	// Every hook first and every tee after, the order CPlayers::OnRender draws them in. Drawing
	// each tee with its own hook instead puts the second tee's hook over the first one.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(!m_aPracticeTee[Conn].m_Valid)
			continue;
		const int ClientId = GameClient()->m_aLocalIds[Conn];
		if(ClientId < 0)
			continue;

		const CGameClient::CClientData &Data = GameClient()->m_aClients[ClientId];
		if(!ScreenRect.Inside(Data.m_RenderPos))
			continue;

		CTeeRenderInfo RenderInfo;
		GameClient()->m_Players.BuildTeeRenderInfo(ClientId, Skins, RenderInfo);
		GameClient()->m_Players.RenderHookCollLine(ScreenRect, &Data.m_RenderPrev, &Data.m_RenderCur, ClientId);
		GameClient()->m_Players.RenderPlayer(ScreenRect, &Data.m_RenderPrev, &Data.m_RenderCur, &RenderInfo, ClientId);
	}
}

void CLocalPractice::OverrideSpectatorView()
{
	// Paused or spectating, the camera and the cursor both come off the spectator position rather
	// than off the local character. Pointing that at the practice tee is what lets the whole thing
	// keep working from in there, instead of leaving you watching from wherever you paused.
	if(!m_Active)
		return;

	const int Conn = g_Config.m_ClDummy;
	const int ClientId = GameClient()->m_aLocalIds[Conn];

	if(IsPaused(Conn))
	{
		m_WasControlling = false;
		// A paused player is a free flying spectator, and free view is only reachable through that
		// state -- the camera picks it out of m_SpecInfo, see CCamera::UpdateCamera, and so does the
		// mouse clamp. So the state the server would have put us in is put in place here instead.
		GameClient()->m_Snap.m_SpecInfo.m_Active = true;
		GameClient()->m_Snap.m_SpecInfo.m_SpectatorId = SPEC_FREEVIEW;
		GameClient()->m_Snap.m_SpecInfo.m_UsePosition = false;
		return;
	}

	// Not paused: the practice tee is being played, so the client is put back into the state it
	// would be in if it were playing -- whatever the server thinks. Pinning the spectator position
	// at the tee instead was not enough: spectating changes what m_aMousePos means. Free view holds
	// an absolute world position in it, see CControls::ClampMousePos, where playing holds an offset
	// from the tee. Aim, and with it firing and hooking, is built from that offset.
	if(!IsSimulated(ClientId))
		return;

	// Free view keeps an absolute world position in m_aMousePos, playing keeps an offset from the
	// tee. Taking control turns one into the other, and nothing re-clamps it until the mouse is
	// next moved -- so a cursor left over from spectating aimed, and teleported, half a map away.
	if(!m_WasControlling)
	{
		m_WasControlling = true;
		GameClient()->m_Controls.ClampMousePos();
	}

	GameClient()->m_Snap.m_SpecInfo.m_Active = false;
	GameClient()->m_Snap.m_SpecInfo.m_UsePosition = false;

	// The camera and the cursor both hang off this, and the client fills it in from the real tee --
	// which is the one this world is practicing away from. It is the practice tee that is being
	// played, so it is the practice tee's position that goes in.
	if(m_aPracticeTee[Conn].m_Valid)
		GameClient()->m_LocalCharacterPos = m_aPracticeTee[Conn].m_RenderPos;
}

bool CLocalPractice::IsSimulated(int ClientId) const
{
	const int Conn = LocalConn(ClientId);
	return Conn >= 0 && PracticeChar(Conn) != nullptr;
}

bool CLocalPractice::IsControlling() const
{
	// Playing a practice tee right now: one exists for the connection being played and it is not
	// paused out of our hands.
	const int Conn = g_Config.m_ClDummy;
	return m_Active && !IsPaused(Conn) && PracticeChar(Conn) != nullptr;
}

bool CLocalPractice::IsPracticeSolo(int ClientId) const
{
	// Solo set in here never reaches m_aClients -- that comes off the snapshot -- so IsOtherTeam
	// cannot see it, and the tee it applies to went on being drawn solid.
	if(!m_Active)
		return false;

	const int Conn = LocalConn(ClientId);
	const int Active = g_Config.m_ClDummy;
	if(Conn < 0 || Conn == Active)
		return false; // the tee being played is never the one that fades

	CCharacter *pPlayed = PracticeChar(Active);
	CCharacter *pOther = PracticeChar(Conn);
	if(!pPlayed || !pOther)
		return false;
	return pPlayed->Core()->m_Solo || pOther->Core()->m_Solo;
}

bool CLocalPractice::IsBystander(int ClientId) const
{
	// Everyone the practice world is not simulating is scenery while it runs: they are still on the
	// server doing whatever they were doing, and none of it can touch the tee being practiced with.
	if(!m_Active || ClientId < 0)
		return false;
	return !IsSimulated(ClientId);
}

float CLocalPractice::ServerEntityAlpha(int ClientId) const
{
	// How solid anything belonging to the real tee is: the tee itself, and everything the server
	// hung off it. Without this the tee went see-through while its cosmetics stayed painted on at
	// full strength, which looks like the cosmetics belong to the practice tee.
	if(!m_Active || !IsSimulated(ClientId) || !g_Config.m_ClLocalPracticeGhost)
		return m_Active && IsSimulated(ClientId) ? 0.0f : 1.0f;

	float Alpha = g_Config.m_ClLocalPracticeGhostAlpha / 100.0f;
	if(g_Config.m_ClLocalPracticeGhostFade)
	{
		// Two tees on top of each other read as a rendering fault rather than as information, so
		// the ghost is only there once it has something to say.
		const int Conn = LocalConn(ClientId);
		vec2 ServerAt;
		if(Conn >= 0 && m_aPracticeTee[Conn].m_Valid && ServerPos(ClientId, ServerAt))
		{
			const float Dist = distance(ServerAt, m_aPracticeTee[Conn].m_RenderPos);
			Alpha *= std::clamp((Dist - gs_GhostFadeNear) / (gs_GhostFadeFar - gs_GhostFadeNear), 0.0f, 1.0f);
		}
	}
	return Alpha;
}

void CLocalPractice::RenderPausedTees()
{
	// What everyone else would see of a paused player: a spectator character standing where the tee
	// was left. Drawn the same way CPlayers draws the real ones.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(!IsPausedOut(Conn))
			continue;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &GameClient()->m_Players.SpectatorTeeRenderInfo()->TeeRenderInfo(),
			EMOTE_BLINK, vec2(1.0f, 0.0f), m_aPausedTee[Conn].m_Core.m_Pos, 1.0f);
	}
}

void CLocalPractice::RenderMovedAlert(bool Preview)
{
	CHudLayout &Layout = GameClient()->m_Hud.HudLayout();
	const float FontSize = 10.0f;
	const float RestY = 30.0f;

	const float Age = Client()->LocalTime() - m_AlertSince;
	const float Life = (float)g_Config.m_ClLocalPracticeAlertTime;
	if(m_AlertReason != EMoveReason::NONE && Age > Life)
		m_AlertReason = EMoveReason::NONE;

	const float CenterX = 150.0f * Graphics()->ScreenAspect();

	// Being moved out of practice is not something the editor can arrange, so the preview says so
	// on its behalf rather than leaving an empty slot to place
	const bool ShowPreview = Preview && m_AlertReason == EMoveReason::NONE;
	if(!g_Config.m_ClLocalPracticeAlert || (m_AlertReason == EMoveReason::NONE && !ShowPreview))
	{
		// While practicing, or while the editor is open to place it, it has somewhere it would go.
		// Outside of that it is withdrawn rather than merely idle, so that a notice that can never
		// appear does not sit in the layout shoving the real HUD around.
		if(m_Active || Preview)
			Layout.ReportNominalRect(EHudElement::PRACTICE_ALERT, vec2(CenterX - 45.0f, RestY), vec2(90.0f, FontSize));
		else
			Layout.ClearNominalRect(EHudElement::PRACTICE_ALERT);
		return;
	}

	const char *pText = "Your tee was moved";
	if(!ShowPreview)
	{
		switch(m_AlertReason)
		{
		case EMoveReason::DIED: pText = "Your tee died"; break;
		case EMoveReason::FROZEN: pText = "Your tee was frozen"; break;
		case EMoveReason::DRAGGED: pText = "Your tee was hooked"; break;
		default: break;
		}
	}

	const float Width = TextRender()->TextWidth(FontSize, pText, -1, -1.0f);
	const float PosX = CenterX - Width / 2.0f;
	const float PosY = RestY;

	Layout.ReportNaturalRect(EHudElement::PRACTICE_ALERT, vec2(PosX, PosY), vec2(Width, FontSize));
	if(Layout.IsOccluded(EHudElement::PRACTICE_ALERT))
		return;

	// The last second is spent fading, so it leaves rather than blinking out
	float Alpha = 1.0f;
	if(!ShowPreview && Age > Life - 1.0f)
		Alpha = std::clamp(Life - Age, 0.0f, 1.0f);

	CHudLayout::CScope Scope(&Layout, EHudElement::PRACTICE_ALERT);
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClLocalPracticeAlertColor));
	Color.a = Alpha;
	TextRender()->TextColor(Color);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha));
	TextRender()->Text(PosX, PosY, FontSize, pText, -1.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
}

//////////////////////////////////////////////////
// Commands
//////////////////////////////////////////////////

bool CLocalPractice::OnChatCommand(const char *pInput)
{
	if(!pInput || pInput[0] != '/')
		return false;

	const char *pRest = pInput + 1;
	for(const CCommand &Command : ms_aCommands)
	{
		const int Len = str_length(Command.m_pName);
		if(str_comp_nocase_num(pRest, Command.m_pName, Len) != 0)
			continue;
		if(pRest[Len] != '\0' && pRest[Len] != ' ')
			continue;

		// Only /practice works from outside; the rest would silently do nothing and look broken
		const bool IsEntry = Command.m_pfnHandler == &CLocalPractice::CmdPractice;
		if(!m_Active && !IsEntry)
			return false;

		const char *pArgs = pRest + Len;
		while(*pArgs == ' ')
			pArgs++;

		(this->*Command.m_pfnHandler)(pArgs);
		return true;
	}
	return false;
}

void CLocalPractice::CmdHelp(const char *pArgs)
{
	char aBuf[512];
	aBuf[0] = '\0';
	for(const CCommand &Command : ms_aCommands)
	{
		char aOne[64];
		str_format(aOne, sizeof(aOne), "%s/%s", aBuf[0] ? " " : "", Command.m_pName);
		str_append(aBuf, aOne);
	}
	Print("%s", aBuf);
}

void CLocalPractice::CmdPractice(const char *pArgs)
{
	Toggle();
}

bool CLocalPractice::TeleportTo(int Conn, vec2 Pos)
{
	// A tee paused out of the world still has somewhere it is standing, and moving that is exactly
	// what a teleport means for it
	if(IsPausedOut(Conn))
	{
		m_aLastTp[Conn] = m_aPausedTee[Conn].m_Core.m_Pos;
		m_aHasLastTp[Conn] = true;

		m_aPausedTee[Conn].m_Core.m_Pos = Pos;
		m_aPausedTee[Conn].m_Core.m_Vel = vec2(0.0f, 0.0f);
		m_aPausedTee[Conn].m_FreezeTime = 0;
		m_aPausedTee[Conn].m_Core.m_FreezeStart = 0;
		m_aPausedTee[Conn].m_Core.m_FreezeEnd = 0;
		m_aPausedTee[Conn].m_Core.m_DeepFrozen = false;
		m_aPausedTee[Conn].m_Core.m_LiveFrozen = false;
		m_aPausedTee[Conn].m_Core.m_IsInFreeze = false;

		// Look where it went, since nothing else would show that it moved
		GameClient()->m_Camera.SetViewPos(Pos);
		return true;
	}

	CCharacter *pChar = PracticeChar(Conn);
	if(!pChar)
		return false;

	m_aLastTp[Conn] = pChar->Core()->m_Pos;
	m_aHasLastTp[Conn] = true;

	CCharacterCore Core = pChar->GetCore();
	Core.m_Pos = Pos;
	Core.m_Vel = vec2(0.0f, 0.0f);
	pChar->SetCore(Core);
	pChar->ResetHook();
	pChar->m_Pos = Pos;
	pChar->m_PrevPos = Pos;
	pChar->m_PrevPrevPos = Pos;

	// Landing somewhere frozen with no way out is never what a teleport was for
	ClearFreeze(pChar);
	return true;
}

bool CLocalPractice::ResolveTarget(const char *pName, vec2 &Pos) const
{
	if(!pName || !pName[0])
		return false;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_aClients[i].m_Active)
			continue;
		if(str_comp_nocase(GameClient()->m_aClients[i].m_aName, pName) != 0)
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
			return false;
		Pos = vec2(GameClient()->m_Snap.m_aCharacters[i].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[i].m_Cur.m_Y);
		return true;
	}
	return false;
}

void CLocalPractice::CmdTp(const char *pArgs)
{
	if(!pArgs[0])
	{
		// Deliberately not "teleport to your real tee": that is what /tpreal is for, and having the
		// bare command do it reads as the teleport having gone wrong
		Print("usage: /tp <name>, or /tpreal, /tpcursor, /tpxy <x> <y>");
		return;
	}

	vec2 Pos;
	if(!ResolveTarget(pArgs, Pos))
	{
		Print("no player called '%s' is alive here", pArgs);
		return;
	}
	if(!TeleportTo(g_Config.m_ClDummy, Pos))
		Print("nothing to teleport");
}

void CLocalPractice::CmdTpReal(const char *pArgs)
{
	const int Conn = g_Config.m_ClDummy;
	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
	{
		Print("your real tee is not in the game");
		return;
	}
	const CNetObj_Character &Cur = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
	if(!TeleportTo(Conn, vec2(Cur.m_X, Cur.m_Y)))
		Print("nothing to teleport");
}

void CLocalPractice::CmdTpXy(const char *pArgs)
{
	float x, y;
	if(sscanf(pArgs, "%f %f", &x, &y) != 2)
	{
		Print("usage: /tpxy <x> <y> (in tiles)");
		return;
	}
	if(!TeleportTo(g_Config.m_ClDummy, vec2(x * 32.0f, y * 32.0f)))
		Print("nothing to teleport");
}

void CLocalPractice::CmdTpCursor(const char *pArgs)
{
	if(!TeleportTo(g_Config.m_ClDummy, GameClient()->GetCursorWorldPos()))
		Print("nothing to teleport");
}

void CLocalPractice::CmdLastTp(const char *pArgs)
{
	const int Conn = g_Config.m_ClDummy;
	if(!m_aHasLastTp[Conn])
	{
		Print("you have not teleported yet");
		return;
	}
	TeleportTo(Conn, m_aLastTp[Conn]);
}

void CLocalPractice::ClearFreeze(CCharacter *pChar)
{
	// Unfreeze only clears the end stamp when there was freeze time left to clear, so a tee that
	// was only ever deep or live frozen would go on reading as frozen to the freeze bar.
	pChar->Core()->m_DeepFrozen = false;
	pChar->Core()->m_LiveFrozen = false;
	pChar->Unfreeze();
	pChar->m_FreezeTime = 0;
	pChar->Core()->m_FreezeStart = 0;
	pChar->Core()->m_FreezeEnd = 0;
	pChar->Core()->m_IsInFreeze = false;
}

void CLocalPractice::CmdRescue(const char *pArgs)
{
	const int Conn = g_Config.m_ClDummy;
	if(!m_aHasRescuePos[Conn])
	{
		Print("no rescue position yet");
		return;
	}
	// TeleportTo covers a tee that has been paused out of the world, and thaws either way
	TeleportTo(Conn, m_aRescuePos[Conn]);
}

bool CLocalPractice::CanSpec(int Conn) const
{
	// CPlayer::CanSpec: grounded and not having moved this tick. This is why /spec in mid air seems
	// to do nothing -- it is waiting, not refusing.
	CCharacter *pChar = PracticeChar(Conn);
	return pChar && pChar->IsGrounded() && pChar->m_Pos == pChar->m_PrevPos;
}

void CLocalPractice::SetPauseState(int Conn, int State)
{
	if(Conn < 0 || Conn >= NUM_DUMMIES || State == m_aPauseState[Conn])
		return;

	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId < 0)
		return;

	m_aPauseState[Conn] = State;

	if(State == PAUSE_NONE)
	{
		RestoreToWorld(Conn);
		return;
	}

	// The view detaches straight away, where the tee is standing -- CPlayer::Tick stops following
	// the character the moment m_Paused is set, whatever the character does afterwards
	if(CCharacter *pChar = PracticeChar(Conn))
		GameClient()->m_Camera.SetViewPos(pChar->Core()->m_Pos);
}

void CLocalPractice::ProcessPause()
{
	// CPlayer::ProcessPause: only /spec takes the tee out of the world, and only once it has come
	// to rest. /pause leaves it standing there, still solid, simply not being told what to do.
	for(int Conn = 0; Conn < NUM_DUMMIES; Conn++)
	{
		if(m_aPauseState[Conn] != PAUSE_SPEC || m_aPausedTee[Conn].m_Valid)
			continue;
		if(!CanSpec(Conn))
			continue;
		PauseOutOfWorld(Conn);
	}
}

void CLocalPractice::PauseOutOfWorld(int Conn)
{
	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId < 0 || m_aPausedTee[Conn].m_Valid)
		return;

	{
		CCharacter *pChar = m_World.GetCharacterById(ClientId);
		if(!pChar)
			return;

		// Mirrors CCharacter::Pause(true): the tee leaves the simulation with everything about it
		// kept, and the hook goes both ways because keeping it would allow cheats.
		CPausedTee &Tee = m_aPausedTee[Conn];
		Tee.m_Core = pChar->GetCore();
		Tee.m_FreezeTime = pChar->m_FreezeTime;
		Tee.m_TeleCheckpoint = pChar->m_TeleCheckpoint;
		Tee.m_PausedTick = m_Tick;
		Tee.m_Valid = true;

		pChar->ResetHook();
		m_World.ReleaseHooked(ClientId);
		pChar->Destroy();
	}
}

void CLocalPractice::RestoreToWorld(int Conn)
{
	const int ClientId = GameClient()->m_aLocalIds[Conn];
	if(ClientId < 0 || !m_aPausedTee[Conn].m_Valid)
		return;

	// Mirrors CCharacter::Pause(false): back in at the same place, standing still, and the freeze
	// it was holding is pushed forward so none of it drained away while it was out.
	CPausedTee &Tee = m_aPausedTee[Conn];
	CNetObj_Character Obj = {};
	Obj.m_X = (int)Tee.m_Core.m_Pos.x;
	Obj.m_Y = (int)Tee.m_Core.m_Pos.y;
	Obj.m_Tick = m_Tick;
	Obj.m_Weapon = std::max(0, Tee.m_Core.m_ActiveWeapon);
	Obj.m_AmmoCount = -1;

	m_World.m_GameTick = m_Tick;
	CCharacter *pChar = new CCharacter(&m_World, ClientId, &Obj, nullptr);
	m_World.InsertEntity(pChar);

	CCharacterCore Core = Tee.m_Core;
	Core.m_Vel = vec2(0.0f, 0.0f);
	if(Core.m_FreezeStart > 0)
		Core.m_FreezeStart += m_Tick - Tee.m_PausedTick;
	if(Core.m_FreezeEnd > 0)
		Core.m_FreezeEnd += m_Tick - Tee.m_PausedTick;
	pChar->SetCore(Core);
	pChar->SetCoreWorld(&m_World);
	pChar->m_FreezeTime = Tee.m_FreezeTime;
	pChar->m_TeleCheckpoint = Tee.m_TeleCheckpoint;
	pChar->m_IsLocal = true;
	pChar->m_Pos = Core.m_Pos;
	pChar->m_PrevPos = Core.m_Pos;
	pChar->m_PrevPrevPos = Core.m_Pos;
	pChar->ResetHook();

	Tee.m_Valid = false;
	SyncFireCounter(Conn);
}

void CLocalPractice::CmdPause(const char *pArgs)
{
	const int Conn = g_Config.m_ClDummy;
	SetPauseState(Conn, m_aPauseState[Conn] == PAUSE_NONE ? PAUSE_PAUSED : PAUSE_NONE);
}

void CLocalPractice::CmdSpec(const char *pArgs)
{
	const int Conn = g_Config.m_ClDummy;
	SetPauseState(Conn, m_aPauseState[Conn] == PAUSE_NONE ? PAUSE_SPEC : PAUSE_NONE);
}

void CLocalPractice::CmdUnpause(const char *pArgs)
{
	SetPauseState(g_Config.m_ClDummy, PAUSE_NONE);
}

void CLocalPractice::CmdKill(const char *pArgs)
{
	Respawn(g_Config.m_ClDummy);
}

void CLocalPractice::CmdUnfreeze(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		ClearFreeze(pChar);
}

void CLocalPractice::CmdDeep(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
	{
		pChar->Freeze();
		pChar->Core()->m_DeepFrozen = true;
	}
}

void CLocalPractice::CmdUnDeep(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->Core()->m_DeepFrozen = false;
}

void CLocalPractice::CmdLiveFreeze(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->Core()->m_LiveFrozen = true;
}

void CLocalPractice::CmdUnLiveFreeze(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->Core()->m_LiveFrozen = false;
}

void CLocalPractice::CmdSolo(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->SetSolo(true);
}

void CLocalPractice::CmdUnSolo(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->SetSolo(false);
}

void CLocalPractice::CmdInvincible(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	pChar->Core()->m_Invincible = pArgs[0] ? str_toint(pArgs) != 0 : !pChar->Core()->m_Invincible;
}

void CLocalPractice::CmdWeapons(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->GiveAllWeapons();
}

void CLocalPractice::CmdUnWeapons(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	for(int Weapon = WEAPON_SHOTGUN; Weapon < NUM_WEAPONS - 1; Weapon++)
		pChar->GiveWeapon(Weapon, true);
	pChar->SetActiveWeapon(WEAPON_GUN);
}

void CLocalPractice::CmdNinja(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->GiveNinja();
}

void CLocalPractice::CmdUnNinja(const char *pArgs)
{
	if(CCharacter *pChar = AnyPracticeChar())
		pChar->RemoveNinja();
}

void CLocalPractice::CmdJetpack(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	pChar->Core()->m_Jetpack = pArgs[0] ? str_toint(pArgs) != 0 : !pChar->Core()->m_Jetpack;
	if(pChar->Core()->m_Jetpack)
		pChar->GiveWeapon(WEAPON_GUN);
}

void CLocalPractice::CmdEndlessHook(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	pChar->Core()->m_EndlessHook = pArgs[0] ? str_toint(pArgs) != 0 : !pChar->Core()->m_EndlessHook;
}

void CLocalPractice::CmdEndlessJump(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	pChar->Core()->m_EndlessJump = pArgs[0] ? str_toint(pArgs) != 0 : !pChar->Core()->m_EndlessJump;
}

static int ParseWeapon(const char *pArgs)
{
	if(!pArgs[0])
		return -1;
	if(str_isallnum(pArgs))
	{
		const int Weapon = str_toint(pArgs);
		return (Weapon >= 0 && Weapon < NUM_WEAPONS) ? Weapon : -1;
	}
	static const char *s_apNames[] = {"hammer", "gun", "shotgun", "grenade", "laser", "ninja"};
	for(int i = 0; i < (int)std::size(s_apNames); i++)
		if(str_comp_nocase(pArgs, s_apNames[i]) == 0)
			return i == 5 ? WEAPON_NINJA : i;
	// "rifle" is what a lot of people still call it
	if(str_comp_nocase(pArgs, "rifle") == 0)
		return WEAPON_LASER;
	return -1;
}

void CLocalPractice::CmdAddWeapon(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	const int Weapon = ParseWeapon(pArgs);
	if(Weapon < 0)
	{
		Print("usage: /addweapon <0-4|hammer|gun|shotgun|grenade|laser|ninja>");
		return;
	}
	pChar->GiveWeapon(Weapon);
}

void CLocalPractice::CmdRemoveWeapon(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	const int Weapon = ParseWeapon(pArgs);
	if(Weapon < 0)
	{
		Print("usage: /removeweapon <0-4|hammer|gun|shotgun|grenade|laser|ninja>");
		return;
	}
	pChar->GiveWeapon(Weapon, true);
}

void CLocalPractice::CmdSetJumps(const char *pArgs)
{
	CCharacter *pChar = AnyPracticeChar();
	if(!pChar)
		return;
	if(!pArgs[0])
	{
		Print("usage: /setjumps <amount>");
		return;
	}
	pChar->Core()->m_Jumps = std::clamp(str_toint(pArgs), 0, 255);
}
