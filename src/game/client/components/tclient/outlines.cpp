#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>

#include "outlines.h"

void COutlines::OnMapLoad()
{
	m_pGameTiles = nullptr;
	m_pTeleTiles = nullptr;
	m_Width = 0;
	m_Height = 0;

	// Find the group containing the game layer
	for(int g = 0; g < GameClient()->Layers()->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = GameClient()->Layers()->GetGroup(g);
		if(!pGroup)
			continue;

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = GameClient()->Layers()->GetLayer(pGroup->m_StartLayer + l);
			if(!pLayer || pLayer->m_Type != LAYERTYPE_TILES)
				continue;

			CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
			if(!pTMap)
				continue;

			CTile *pTiles = static_cast<CTile *>(GameClient()->Layers()->Map()->GetData(pTMap->m_Data));
			if(!pTiles)
				continue;

			if(pLayer == reinterpret_cast<CMapItemLayer *>(GameClient()->Layers()->GameLayer()))
			{
				m_pGameTiles = pTiles;
				m_Width = pTMap->m_Width;
				m_Height = pTMap->m_Height;
			}
			if(pLayer == reinterpret_cast<CMapItemLayer *>(GameClient()->Layers()->TeleLayer()))
			{
				CMapItemLayerTilemap *pTeleTMap = (CMapItemLayerTilemap *)pLayer;
				m_pTeleTiles = static_cast<CTeleTile *>(GameClient()->Layers()->Map()->GetData(pTeleTMap->m_Tele));
			}
		}
	}
}

void COutlines::OnRender()
{
	if(GameClient()->m_MapLayersBackground.m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClOverlayEntities && g_Config.m_ClOutlineEntities)
		return;
	if(!g_Config.m_ClOutline)
		return;

	if(m_pGameTiles)
	{
		if(g_Config.m_ClOutlineUnFreeze)
			RenderMap()->RenderGameTileOutlines(m_pGameTiles, m_Width, m_Height, 32.0f, TILE_UNFREEZE);
		if(g_Config.m_ClOutlineFreeze)
			RenderMap()->RenderGameTileOutlines(m_pGameTiles, m_Width, m_Height, 32.0f, TILE_FREEZE);
		if(g_Config.m_ClOutlineSolid)
			RenderMap()->RenderGameTileOutlines(m_pGameTiles, m_Width, m_Height, 32.0f, TILE_SOLID);
		if(g_Config.m_ClOutlineKill)
			RenderMap()->RenderGameTileOutlines(m_pGameTiles, m_Width, m_Height, 32.0f, TILE_DEATH);
	}

	if(m_pGameTiles && m_pTeleTiles && g_Config.m_ClOutlineTele)
	{
		RenderMap()->RenderTeleOutlines(m_pGameTiles, m_pTeleTiles, m_Width, m_Height, 32.0f);
	}
}
