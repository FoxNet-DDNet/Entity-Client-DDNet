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
	if(g_Config.m_ClTest)
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			const int Local = m_pClient->m_Snap.m_LocalClientId;
			CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

			const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;

			
			const CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Prev;
			const CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[Local].m_Cur;

			const CNetObj_Character *pPrevCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
			const CNetObj_Character *pCurCharO = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;


			

			const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);

			// To make the player position relative to blocks we need to divide by the block size

			const vec2 Pos = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pCurChar->m_X, pCurChar->m_Y), IntraTick) / 32.0f;

			const vec2 Vel = mix(vec2(pPrevChar->m_VelX, pPrevChar->m_VelY), vec2(pCurChar->m_VelX, pCurChar->m_VelY), IntraTick);

			const vec2 Pos2 = vec2(pPrevCharO->m_X, pPrevCharO->m_Y) / 32.0f;

			const vec2 Vel2 = mix(vec2(pPrevCharO->m_VelX, pPrevCharO->m_VelY), vec2(pCurCharO->m_VelX, pCurCharO->m_VelY), IntraTick);

			char bbuf[1024];
			str_format(bbuf, sizeof(bbuf), "%d", round_to_int(Pos.x));

			char Abuf[1024];
			str_format(Abuf, sizeof(Abuf), "%d", round_to_int(Pos2.x));

			TextRender()->Text(100, 100, 25, Abuf);
			TextRender()->Text(200, 100, 25, bbuf);

			if(pCharacter->m_IsInFreeze)
			{
				if((Pos2.y < Pos.y && Pos2.y > Pos.y - 1.5) && (Pos2.x == Pos.x))
				{
					GameClient()->SendKill(Local);
				}
				else return;
			}
		
		}
}