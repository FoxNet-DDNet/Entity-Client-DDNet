#include <engine/shared/config.h>
#include <engine/client.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/gamecore.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>

#include <base/vmath.h>
#include <base/system.h>

#include "auto_kill.h"


void CAutoKill::OnRender()
{ 
	m_Local = m_pClient->m_Snap.m_LocalClientId;

	if(!GameClient()->CurrentRaceTime())
	{
		m_SentAutoKill = false;
		return;
	}

	CCharacterCore *pCharacter = &m_pClient->m_aClients[m_Local].m_Predicted;

	if(g_Config.m_ClAutoKill)
	{
		// if sent kill, stop

		if(m_SentAutoKill == true)
			return;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharacterOther = &m_pClient->m_aClients[i].m_Predicted;

			// if not on "Multeasymap", stop

			if(g_Config.m_ClAutoKillMultOnly)
			{
				if(str_comp(Client()->GetCurrentMap(), "Multeasymap") != 0)
				{
					if(g_Config.m_ClAutoKillDebug)
						TextRender()->Text(100, 50, 15, "Not on Mult");
					return;
				}
				else if(g_Config.m_ClAutoKillDebug)
					TextRender()->Text(100, 50, 15, "On Mult");
			}

			// stuff

			const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[m_Local].m_Prev;
			const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[m_Local].m_Cur;

			const CNetObj_Character *pPrevCharO = &m_pClient->m_Snap.m_aCharacters[!m_Local].m_Prev;
			const CNetObj_Character *pCurCharO = &m_pClient->m_Snap.m_aCharacters[!m_Local].m_Cur;

			auto IsWar = m_pClient->m_aClients[!m_Local].m_IsWar;
			bool EnemyFrozen = pCharacterOther->m_IsInFreeze;

			// so it only takes info from other players and not self (could probably be made smarter :p)

			if(i != m_Local)
			{
				pPrevCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
				pCurCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;
				IsWar = m_pClient->m_aClients[i].m_IsWar ;
				EnemyFrozen = pCharacterOther->m_IsInFreeze;
			}

			// position and vel

			const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
			const vec2 SelfPos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

			const vec2 EnemyPos = vec2(pPrevCharO->m_X, pPrevCharO->m_Y) / 32.0f;
			const vec2 EnemyVel = mix(vec2(pPrevCharO->m_VelX, pPrevCharO->m_VelY), vec2(pCurCharO->m_VelX, pCurCharO->m_VelY), IntraTick) / 1000 / 1.5;

			const float RangeX = g_Config.m_ClAutoKillRangeX / 100.0f;
			const float RangeY = g_Config.m_ClAutoKillRangeY / 100.0f;

			// debug

			if(g_Config.m_ClAutoKillDebug)
			{
				char aBuf[100];
				str_format(aBuf, sizeof(aBuf), "%d", round_to_int(SelfPos.x));
				TextRender()->Text(100, 100, 15, aBuf);

				char bBuf[100];
				str_format(bBuf, sizeof(bBuf), "%d", round_to_int(EnemyPos.x));
				TextRender()->Text(200, 100, 15, bBuf);
			}

			// if in freeze tile

			if((pCharacter->m_IsInFreeze && !g_Config.m_ClAutoKillWarOnly) || (g_Config.m_ClAutoKillWarOnly && IsWar && pCharacter->m_IsInFreeze))
			{
				// check where other player is

				if((EnemyPos.y < SelfPos.y && EnemyPos.y > SelfPos.y - 1 - RangeY - EnemyVel.y) && ((EnemyPos.x <= SelfPos.x + RangeX) && (EnemyPos.x + RangeX >= SelfPos.x)))
				{
					// if other player is frozen, stop, why? idk

					if(EnemyFrozen)
						return;

					// if other player is unfrozen
					if(m_pClient->m_aClients[i].m_FreezeEnd > 0)
					{
						// if player is ontop of you and their x coordinate is close to own players

						if(EnemyPos.x <= SelfPos.x + 0.04f && EnemyPos.x + +0.04f >= SelfPos.x)
						{
							// kill

							if(GameClient()->CurrentRaceTime() > 60 * g_Config.m_SvKillProtection && g_Config.m_ClFreezeKillIgnoreKillProt)
							{
								GameClient()->aMessage("2");
								m_pClient->m_Chat.SendChat(0, "/kill");
								m_SentAutoKill = true;
								m_LastFreeze = time_get() + time_freq() * 5;
								return;
							}
							else
							{
								GameClient()->aMessage("3");
								GameClient()->SendKill(m_Local);
								m_SentAutoKill = true;
								return;
							}
						}
						return;
					}
				}
			}
		}
	}
}