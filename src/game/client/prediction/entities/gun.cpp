/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gun.h"

#include "character.h"
#include "plasma.h"

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/laser_data.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <iterator>

CGun::CGun(CGameWorld *pGameWorld, int Id, vec2 Pos, bool Freeze, bool Explosive, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_GUN)
{
	m_Id = Id;
	m_Core = vec2(0.0f, 0.0f);
	m_Pos = Pos;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_Layer = Layer;
	m_Number = Number;
	m_EvalTick = GameWorld()->GameTick();

	std::fill(std::begin(m_aLastFireTeam), std::end(m_aLastFireTeam), 0);
	std::fill(std::begin(m_aLastFireSolo), std::end(m_aLastFireSolo), 0);
}

void CGun::Tick()
{
	if(GameWorld()->GameTick() % (int)(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		m_EvalTick = GameWorld()->GameTick();
		Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
	}
	if(g_Config.m_SvPlasmaPerSec > 0)
	{
		Fire();
	}
}

void CGun::Fire()
{
	// Create a list of players who are in the range of the turret
	CEntity *apPlayersInRange[MAX_CLIENTS];
	std::fill(std::begin(apPlayersInRange), std::end(apPlayersInRange), nullptr);

	const int NumPlayersInRange = GameWorld()->FindEntities(m_Pos, g_Config.m_SvPlasmaRange,
		apPlayersInRange, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	// The closest player (within range) in a team is selected as the target
	int aTargetIdInTeam[MAX_CLIENTS];
	bool aIsTarget[MAX_CLIENTS];
	int aMinDistInTeam[MAX_CLIENTS];
	std::fill(std::begin(aMinDistInTeam), std::end(aMinDistInTeam), 0);
	std::fill(std::begin(aIsTarget), std::end(aIsTarget), false);
	std::fill(std::begin(aTargetIdInTeam), std::end(aTargetIdInTeam), -1);

	for(int i = 0; i < NumPlayersInRange; i++)
	{
		CCharacter *pTarget = static_cast<CCharacter *>(apPlayersInRange[i]);
		const int TargetTeam = pTarget->Team();
		// Do not fire at super players
		if(TargetTeam < 0 || TargetTeam >= MAX_CLIENTS || TargetTeam == pTarget->TeamsCore()->TeamSuper())
		{
			continue;
		}
		// If the turret is disabled for the target's team, the turret will not fire
		if(m_Layer == LAYER_SWITCH &&
			m_Number > 0 &&
			m_Number < (int)Switchers().size() &&
			!Switchers()[m_Number].m_aStatus[TargetTeam])
		{
			continue;
		}

		// Turrets can only shoot at a speed of sv_plasma_per_sec
		const int TargetClientId = pTarget->GetCid();
		if(TargetClientId < 0 || TargetClientId >= MAX_CLIENTS)
		{
			continue;
		}
		const bool TargetIsSolo = pTarget->TeamsCore()->GetSolo(TargetClientId);
		const int Wait = GameWorld()->GameTickSpeed() / g_Config.m_SvPlasmaPerSec;
		if((TargetIsSolo && m_aLastFireSolo[TargetClientId] + Wait > GameWorld()->GameTick()) ||
			(!TargetIsSolo && m_aLastFireTeam[TargetTeam] + Wait > GameWorld()->GameTick()))
		{
			continue;
		}

		// Turrets can shoot only at reachable players. The server also asks whether the player is
		// alive; a character that is not has already been taken out of this world.
		if(Collision()->IntersectLine(m_Pos, pTarget->m_Pos, nullptr, nullptr))
		{
			continue;
		}

		// Turrets fire on solo players regardless of the rest of the team
		if(TargetIsSolo)
		{
			aIsTarget[TargetClientId] = true;
			m_aLastFireSolo[TargetClientId] = GameWorld()->GameTick();
		}
		else
		{
			const int Distance = distance(pTarget->m_Pos, m_Pos);
			if(aMinDistInTeam[TargetTeam] == 0 || aMinDistInTeam[TargetTeam] > Distance)
			{
				aMinDistInTeam[TargetTeam] = Distance;
				aTargetIdInTeam[TargetTeam] = TargetClientId;
			}
		}
	}

	// Set the closest player for each team as a target
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(aTargetIdInTeam[i] != -1)
		{
			aIsTarget[aTargetIdInTeam[i]] = true;
			m_aLastFireTeam[i] = GameWorld()->GameTick();
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// Fire at each target
		if(!aIsTarget[i])
		{
			continue;
		}

		// The prediction only knows how to build a plasma bullet out of the laser object the server
		// snaps one as, so the turret writes one out the same way: CPlasma::Snap puts the bullet's
		// position in both ends, packs explosive and freeze into the subtype, and names the tee
		// being fired at as the owner, which is what the bullet takes its direction from.
		CLaserData Data = {};
		Data.m_From = m_Pos;
		Data.m_To = m_Pos;
		Data.m_StartTick = GameWorld()->GameTick();
		Data.m_ExtraInfo = true;
		Data.m_Owner = i;
		Data.m_Type = LASERTYPE_PLASMA;
		Data.m_SwitchNumber = m_Number;
		Data.m_Subtype = (m_Explosive ? 1 : 0) | (m_Freeze ? 2 : 0);
		Data.m_Predict = true;
		GameWorld()->InsertEntity(new CPlasma(GameWorld(), -1, &Data));
	}
}
