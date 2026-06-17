#include "local_practice.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/players.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/projectile.h>
#include <game/gamecore.h>

void CLocalPractice::ConToggleLocalPractice(IConsole::IResult *pResult, void *pUserData)
{
	CLocalPractice *pSelf = (CLocalPractice *)pUserData;
	pSelf->m_Active = !pSelf->m_Active;
	pSelf->m_Initialized = false;
}

bool CLocalPractice::ShouldPredictDummy() const
{
	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	const int DummyId = GameClient()->m_aLocalIds[!g_Config.m_ClDummy];

	return m_Active &&
	       !GameClient()->m_Snap.m_SpecInfo.m_Active &&
	       Client()->DummyConnected() &&
	       LocalId >= 0 &&
	       DummyId >= 0 &&
	       !GameClient()->m_aClients[DummyId].m_Paused &&
	       GameClient()->m_Teams.CanCollide(LocalId, DummyId);
}

bool CLocalPractice::IsPracticeParticipant(int ClientId) const
{
	if(!m_Active || ClientId < 0)
		return false;

	const int ControlledId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(ClientId == ControlledId)
		return true;

	const int OtherId = GameClient()->m_aLocalIds[!g_Config.m_ClDummy];
	return ShouldPredictDummy() && ClientId == OtherId;
}

void CLocalPractice::ApplyTeams(CTeamsCore &Teams) const
{
	if(!m_Active)
		return;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	const int PracticeTeam = LocalId >= 0 ? GameClient()->m_Teams.Team(LocalId) : TEAM_FLOCK;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		Teams.Team(ClientId, TEAM_FLOCK);
	}

	if(LocalId >= 0)
		Teams.Team(LocalId, PracticeTeam);

	if(ShouldPredictDummy())
		Teams.Team(GameClient()->m_aLocalIds[!g_Config.m_ClDummy], PracticeTeam);
}

void CLocalPractice::SyncParticipants(CGameWorld &World) const
{
	for(int Tee = 0; Tee < NUM_DUMMIES; Tee++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Tee];
		if(ClientId < 0 || !IsPracticeParticipant(ClientId))
			continue;
		if(World.GetCharacterById(ClientId))
			continue;

		CCharacter *pSrc = GameClient()->m_GameWorld.GetCharacterById(ClientId);
		if(!pSrc)
			continue;

		CCharacter *pCopy = new CCharacter(*pSrc);
		pCopy->m_pParent = nullptr;
		pCopy->m_pChild = nullptr;
		pCopy->SetCoreWorld(&World);
		pCopy->SetCore(GameClient()->m_aClients[ClientId].m_Predicted);
		pCopy->m_Pos = pCopy->m_PrevPos = pCopy->m_PrevPrevPos = pCopy->Core()->m_Pos;
		World.InsertEntity(pCopy);
	}
}

void CLocalPractice::PrepareWorld(CGameWorld &World)
{
	if(!m_Active)
		return;

	ApplyTeams(World.m_Teams);

	World.m_WorldConfig.m_PredictEvents = true;
	World.m_WorldConfig.m_ForcePredictEvents = true;
	World.m_WorldConfig.m_PredictWeapons = true;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(CCharacter *pChar = World.GetCharacterById(ClientId))
		{
			if(!IsPracticeParticipant(ClientId))
			{
				GameClient()->m_aClients[ClientId].m_Predicted = pChar->GetCore();
				pChar->Destroy();
			}
		}
	}

	CProjectile *pProjNext = nullptr;
	for(CProjectile *pProj = (CProjectile *)World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = pProjNext)
	{
		pProjNext = (CProjectile *)pProj->TypeNext();
		if(!IsPracticeParticipant(pProj->GetOwner()))
			pProj->Destroy();
	}

	CLaser *pLaserNext = nullptr;
	for(CLaser *pLaser = (CLaser *)World.FindFirst(CGameWorld::ENTTYPE_LASER); pLaser; pLaser = pLaserNext)
	{
		pLaserNext = (CLaser *)pLaser->TypeNext();
		if(!IsPracticeParticipant(pLaser->GetOwner()))
			pLaser->Destroy();
	}

	SyncParticipants(World);
}

void CLocalPractice::ClampSendInput(CNetObj_PlayerInput &Input) const
{
	if(!m_Active)
		return;

	Input.m_Direction = 0;
	Input.m_Jump = 0;
	Input.m_Hook = 0;
	Input.m_Fire &= ~1;
	Input.m_WantedWeapon = 0;
	Input.m_NextWeapon = 0;
	Input.m_PrevWeapon = 0;
	Input.m_TargetX = 1;
	Input.m_TargetY = 0;
}

void CLocalPractice::UpdatePracticeCores()
{
	if(!m_Active)
		return;

	const float Intra = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	for(int Tee = 0; Tee < NUM_DUMMIES; Tee++)
	{
		const int ClientId = GameClient()->m_aLocalIds[Tee];
		if(ClientId < 0 || !IsPracticeParticipant(ClientId))
			continue;

		m_PracticeCore[Tee] = GameClient()->m_aClients[ClientId].m_Predicted;
		m_PracticeCore[Tee].m_Pos = mix(
			GameClient()->m_aClients[ClientId].m_PrevPredicted.m_Pos,
			GameClient()->m_aClients[ClientId].m_Predicted.m_Pos,
			Intra);
	}
}

void CLocalPractice::ResetPosition(bool Dummy)
{
	if(!m_Active)
		return;

	const int ClientId = GameClient()->m_aLocalIds[Dummy];
	if(ClientId < 0 || !IsPracticeParticipant(ClientId))
		return;
	const float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);
	const CNetObj_Character &Prev = GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
	const CNetObj_Character &Cur = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;
	const vec2 Pos = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Cur.m_X, Cur.m_Y), Intra);

	m_PracticeCore[Dummy].m_Pos = Pos;
}

bool CLocalPractice::GetInput(int Tee, CNetObj_PlayerInput &Input, bool Fast) const
{
	if(!m_Active)
		return false;

	if(Tee == g_Config.m_ClDummy)
	{
		Input = Fast ? GameClient()->m_Controls.m_aFastInput[Tee] : GameClient()->m_Controls.m_aInputData[Tee];
		return true;
	}

	if(!ShouldPredictDummy())
		return false;

	if(g_Config.m_ClDummyHammer)
	{
		Input = GameClient()->m_HammerInput;
		return true;
	}

	Input = GameClient()->m_DummyInput;
	if(Fast)
	{
		if(g_Config.m_ClDummyCopyMoves)
		{
			Input = GameClient()->m_Controls.m_aFastInput[g_Config.m_ClDummy];
			Input.m_Fire = GameClient()->m_Controls.m_aFastInput[Tee].m_Fire;
			Input.m_WantedWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_WantedWeapon;
			Input.m_NextWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_NextWeapon;
			Input.m_PrevWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_PrevWeapon;
			if(g_Config.m_ClDummyControl)
			{
				Input.m_Jump = GameClient()->m_DummyInput.m_Jump;
				Input.m_Fire = GameClient()->m_DummyInput.m_Fire;
				Input.m_Hook = GameClient()->m_DummyInput.m_Hook;
			}
		}
		else if(g_Config.m_ClDummyControl)
		{
			Input.m_Direction = GameClient()->m_Controls.m_aFastInput[Tee].m_Direction;
			Input.m_PlayerFlags = GameClient()->m_Controls.m_aFastInput[Tee].m_PlayerFlags;
			Input.m_TargetX = GameClient()->m_Controls.m_aFastInput[Tee].m_TargetX;
			Input.m_TargetY = GameClient()->m_Controls.m_aFastInput[Tee].m_TargetY;
			Input.m_WantedWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_WantedWeapon;
			Input.m_NextWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_NextWeapon;
			Input.m_PrevWeapon = GameClient()->m_Controls.m_aFastInput[Tee].m_PrevWeapon;
		}
	}

	return true;
}

void CLocalPractice::OnConsoleInit()
{
	Console()->Register("local_practice", "", CFGFLAG_CLIENT, ConToggleLocalPractice, this, "Toggle Local Passive");
}

void CLocalPractice::OnNewSnapshot()
{
	const bool DummyConnected = Client()->DummyConnected();
	if(m_Active && DummyConnected && !m_WasDummyConnected)
	{
		m_Active = false;
		m_Initialized = false;
	}
	m_WasDummyConnected = DummyConnected;
}

void CLocalPractice::OnReset()
{
	m_Active = false;
	m_Initialized = false;
	m_WasDummyConnected = false;
}

void CLocalPractice::OnRender()
{
	if(!m_Active || GameClient()->m_Snap.m_SpecInfo.m_Active)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
	if(Alpha <= 0.0f)
		return;

	const CAnimState *pIdleState = CAnimState::GetIdle();
	const float Intra = Client()->IntraGameTick(g_Config.m_ClDummy);

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!IsPracticeParticipant(ClientId))
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		const CNetObj_Character &Prev = GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev;
		const CNetObj_Character &Cur = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur;

		CTeeRenderInfo RenderInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;
		RenderInfo.m_Size = 64.0f;

		const vec2 Pos = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Cur.m_X, Cur.m_Y), Intra);
		const float Angle = GameClient()->m_Players.GetPlayerTargetAngle(&Prev, &Cur, ClientId, Intra);
		const vec2 Direction = direction(Angle);

		RenderTools()->RenderTee(pIdleState, &RenderInfo, Cur.m_Emote, Direction, Pos, Alpha);
	}
}
