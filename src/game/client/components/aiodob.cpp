#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/protocol.h>
#include <game/version.h>
#include <game/client/gameclient.h>
#include <engine/client.h>

#include <game/collision.h>
#include <game/generated/client_data.h>
#include <game/mapitems.h>

#include "aiodob.h"
#include <game/client/prediction/entities/character.h>

void CAiodob::OnRender()
{
	const int Local = m_pClient->m_Snap.m_LocalClientId;
	CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;
	CCollision *m_pCollision = GameClient()->Collision();

	if(g_Config.m_ClAutoKill)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			

			
		

			const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Prev;
			const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Cur;

			const CNetObj_Character *pPrevCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Prev;
			const CNetObj_Character *pCurCharO = &m_pClient->m_Snap.m_aCharacters[!Local].m_Cur;

			auto IsWar = 0;

			if(i != Local)
			{
				pPrevCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				pCurCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;
				IsWar = m_pClient->m_aClients[i].m_IsWar || m_pClient->m_aClients[i].m_IsTempWar || m_pClient->m_aClients[i].m_IsWarClanmate;
			}

			const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
			const vec2 SelfPos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

			const vec2 EnemyPos = vec2(pPrevCharO->m_X, pPrevCharO->m_Y) / 32.0f;
			const vec2 EnemyVel = mix(vec2(pPrevCharO->m_VelX, pPrevCharO->m_VelY), vec2(pCurCharO->m_VelX, pCurCharO->m_VelY), IntraTick) / 1000 / 1.5;

			const float RangeX = g_Config.m_ClAutoKillRangeX / static_cast<float>(100);

			float RangeY = g_Config.m_ClAutoKillRangeY / static_cast<float>(100);

			const bool Grounded = m_pCollision->CheckPoint(SelfPos.x + CCharacterCore::PhysicalSize() / 2, SelfPos.y + CCharacterCore::PhysicalSize() / 2 + 5) || m_pCollision->CheckPoint(SelfPos.x - CCharacterCore::PhysicalSize() / 2, SelfPos.y + CCharacterCore::PhysicalSize() / 2 + 5);

			int ClientId = !Local;

			CCharacter *pChar = m_pClient->m_PredictedWorld.GetCharacterById(~Local);

			if(g_Config.m_ClAutoKillDebug)
			{
				char aBuf[100];
				str_format(aBuf, sizeof(aBuf), "%d", round_to_int(SelfPos.x));
				TextRender()->Text(100, 100, 20, aBuf);

				char bBuf[100];
				str_format(bBuf, sizeof(bBuf), "%d", round_to_int(EnemyPos.x));
				TextRender()->Text(200, 100, 20, bBuf);

				if(pChar && pChar->IsGrounded())
					TextRender()->Text(300, 100, 20, "grounded");
			}

			if((pCharacter->m_IsInFreeze && !g_Config.m_ClAutoKillWarOnly) || (g_Config.m_ClAutoKillWarOnly && IsWar && pCharacter->m_IsInFreeze))
			{
				if((EnemyPos.y < SelfPos.y && EnemyPos.y > SelfPos.y - 1 - RangeY - EnemyVel.y) && ((EnemyPos.x <= SelfPos.x + RangeX) && (EnemyPos.x + RangeX >= SelfPos.x)))
				{
					GameClient()->SendKill(Local);
					return;
				}
			}
		}
	}
	if(g_Config.m_ClAutoJoinTest)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_pClient->m_Teams.Team(i))
			{
				if(str_comp(m_pClient->m_aClients[i].m_aName, g_Config.m_ClAutoJoinTeamName) == 0)
				{
					if(i == Local)
						return;

					int Team = GameClient()->m_Teams.Team(i);

					char TeamChar[256];
					str_format(TeamChar, sizeof(TeamChar), "%d", Team);

					char Joined[2048] = "Auto Joined ";
					str_append(Joined, m_pClient->m_aClients[i].m_aName);

					m_pClient->m_Chat.AddLine(-3, 0, Joined);
					return;

					if(!m_pClient->m_Teams.SameTeam(Local, i))
					{
						if(Team > 0)
						{
							char aBuf[2048] = "/team ";
							str_append(aBuf, TeamChar);
							m_pClient->m_Chat.SendChat(0, aBuf);
						}
					}
				}
			}
		}
	}
}