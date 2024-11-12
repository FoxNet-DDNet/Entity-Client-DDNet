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
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_pClient || !m_pClient->m_Snap.m_pLocalCharacter)
				return;

			CCharacter *pLocalChar = GameClient()->m_GameWorld.GetCharacterById(GameClient()->m_aLocalIds[0]);
			if(!pLocalChar)
				return;

			const int Local = m_pClient->m_Snap.m_LocalClientId;
			CCharacterCore *pCharacter = &m_pClient->m_aClients[Local].m_Predicted;

			const char *pTitle = "Testing";
			const char *map = Client()->GetCurrentMap();
			const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
			if(map == pTitle || pCharacter->m_IsInFreeze)
			{
			}
		}
	}
}