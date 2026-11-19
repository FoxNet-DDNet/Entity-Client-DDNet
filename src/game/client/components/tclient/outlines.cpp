#include "outlines.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/mapitems.h>

// The order of this is the order of priority for outlines
enum
{
	OUTLINE_NONE = 0,
	OUTLINE_UNFREEZE,
	OUTLINE_FREEZE,
	OUTLINE_TELE,
	OUTLINE_KILL,
	OUTLINE_SOLID,
	NUM_OUTLINES,
};

enum class OutlineLayer
{
	GAME,
	FRONT,
	TELE
};

class COutLineLayer
{
private:
	CMapItemLayerTilemap *GetLayer(CGameClient *pThis) const
	{
		if(m_Type == OutlineLayer::GAME)
			return pThis->Layers()->GameLayer();
		if(m_Type == OutlineLayer::FRONT)
			return pThis->Layers()->FrontLayer();
		if(m_Type == OutlineLayer::TELE)
			return pThis->Layers()->TeleLayer();
		dbg_assert(false, "Invalid value for m_Type");
	}
	int GetLayerData(CGameClient *pThis) const
	{
		if(m_Type == OutlineLayer::GAME)
			return pThis->Layers()->GameLayer()->m_Data;
		if(m_Type == OutlineLayer::FRONT)
			return pThis->Layers()->FrontLayer()->m_Front;
		if(m_Type == OutlineLayer::TELE)
			return pThis->Layers()->TeleLayer()->m_Tele;
		dbg_assert(false, "Invalid value for m_Type");
	}

public:
	const OutlineLayer m_Type;
	void GetMeta(CGameClient *pThis, ivec2 &Size) const
	{
		Size = {0, 0};
		const auto *pLayer = GetLayer(pThis);
		if(!pLayer)
			return;
		const int DataIndex = GetLayerData(pThis);
		if(!pThis->Layers()->Map()->GetData(DataIndex))
			return;
		const size_t TileSize = m_Type == OutlineLayer::TELE ? sizeof(CTeleTile) : sizeof(CTile);
		const int DataSize = pThis->Layers()->Map()->GetDataSize(DataIndex);
		if(DataSize <= 0 || (size_t)DataSize < (size_t)pLayer->m_Width * (size_t)pLayer->m_Height * TileSize)
			return;
		Size = {pLayer->m_Width, pLayer->m_Height};
	}
	void SetData(CGameClient *pThis, uint8_t *pData, const ivec2 &Size) const
	{
		const auto *pLayer = GetLayer(pThis);
		const auto *pTiles = (CTile *)pThis->Layers()->Map()->GetData(GetLayerData(pThis));
		if(!pLayer || !pTiles)
			return;
		for(int y = 0; y < pLayer->m_Height; ++y)
		{
			for(int x = 0; x < pLayer->m_Width; ++x)
			{
				const int Index = y * pLayer->m_Width + x;
				const int IndexOut = y * Size.x + x;
				if(m_Type == OutlineLayer::TELE)
				{
					const auto &Tile = ((CTeleTile *)pTiles)[Index];
					if(Tile.m_Number != 0 && Tile.m_Type != 0)
						pData[IndexOut] = OUTLINE_TELE;
				}
				else
				{
					const auto Tile = pTiles[Index].m_Index;
					if(Tile == TILE_SOLID || Tile == TILE_NOHOOK)
						pData[IndexOut] = OUTLINE_SOLID;
					else if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
						pData[IndexOut] = OUTLINE_FREEZE;
					else if(Tile == TILE_UNFREEZE || Tile == TILE_DUNFREEZE || Tile == TILE_LUNFREEZE)
						pData[IndexOut] = OUTLINE_UNFREEZE;
					else if(Tile == TILE_DEATH)
						pData[IndexOut] = OUTLINE_KILL;
				}
			}
		}
	}
};

// The order of this determines order of priority into the one map (tele + freeze = tele)
static constexpr COutLineLayer OUTLINE_LAYERS[] = {{OutlineLayer::TELE}, {OutlineLayer::GAME}, {OutlineLayer::FRONT}};

// Offsets of the eight neighbours, in the order the edge tests below expect them.
static constexpr int NEIGHBOR_OFFSETS[8][2] = {
	{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

void COutlines::OnMapLoad()
{
	if(m_pTileInfo)
	{
		delete[] m_pTileInfo;
		m_pTileInfo = nullptr;
	}

	// Find valid layers and size
	std::vector<const COutLineLayer *> vValidOutlineLayers;
	m_MapDataSize = {0, 0};
	for(const auto &Layer : OUTLINE_LAYERS)
	{
		ivec2 LayerSize;
		Layer.GetMeta(GameClient(), LayerSize);
		if(LayerSize.x <= 0 || LayerSize.y <= 0)
			continue;
		m_MapDataSize.x = std::max(m_MapDataSize.x, LayerSize.x);
		m_MapDataSize.y = std::max(m_MapDataSize.y, LayerSize.y);
		vValidOutlineLayers.push_back(&Layer);
	}
	if(m_MapDataSize.x <= 0 || m_MapDataSize.y <= 0)
		return;

	const size_t NumTiles = (size_t)m_MapDataSize.x * (size_t)m_MapDataSize.y;
	std::vector<uint8_t> vTypes(NumTiles, OUTLINE_NONE);

	// Do it
	for(const auto *pLayer : vValidOutlineLayers)
	{
		pLayer->SetData(GameClient(), vTypes.data(), m_MapDataSize);
	}

	// Resolve the neighbourhood of every tile once, so rendering a tile is a single lookup rather
	// than nine clamped ones. Which neighbours count is decided against the tile's own type, which
	// is why the answer can be baked in here at all.
	m_pTileInfo = new uint16_t[NumTiles];
	const int Width = m_MapDataSize.x;
	const int Height = m_MapDataSize.y;
	for(int y = 0; y < Height; ++y)
	{
		for(int x = 0; x < Width; ++x)
		{
			const size_t Index = (size_t)y * Width + x;
			const uint8_t Type = vTypes[Index];
			uint16_t Info = Type;
			if(Type != OUTLINE_NONE)
			{
				for(int n = 0; n < 8; ++n)
				{
					const int Nx = std::clamp(x + NEIGHBOR_OFFSETS[n][0], 0, Width - 1);
					const int Ny = std::clamp(y + NEIGHBOR_OFFSETS[n][1], 0, Height - 1);
					if(vTypes[(size_t)Ny * Width + Nx] >= Type)
						Info |= (uint16_t)(1u << (NEIGHBOR_SHIFT + n));
				}
			}
			m_pTileInfo[Index] = Info;
		}
	}
}

void COutlines::OnRender()
{
	if(!m_pTileInfo)
		return;
	if(GameClient()->m_MapLayersBackground.m_OnlineOnly && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClOverlayEntities && g_Config.m_ClOutlineEntities)
		return;
	if(!g_Config.m_ClOutline)
		return;

	const float Scale = 32.0f;

	const CScreenRect ScreenRect = Graphics()->GetScreen();
	int StartY = (int)(ScreenRect.m_TopLeft.y / Scale) - 1;
	int StartX = (int)(ScreenRect.m_TopLeft.x / Scale) - 1;
	int EndY = (int)(ScreenRect.m_BottomRight.y / Scale) + 1;
	int EndX = (int)(ScreenRect.m_BottomRight.x / Scale) + 1;
	int MaxScale = 12;
	if(EndX - StartX > Graphics()->ScreenWidth() / MaxScale || EndY - StartY > Graphics()->ScreenHeight() / MaxScale)
	{
		int EdgeX = (EndX - StartX) - (Graphics()->ScreenWidth() / MaxScale);
		StartX += EdgeX / 2;
		EndX -= EdgeX / 2;
		int EdgeY = (EndY - StartY) - (Graphics()->ScreenHeight() / MaxScale);
		StartY += EdgeY / 2;
		EndY -= EdgeY / 2;
	}

	// These only depend on config, but the HSL to RGB conversion used to run once per outlined
	// tile on screen, every frame. Resolve all of them up front instead.
	class COutlineConfig
	{
	public:
		bool m_Used;
		float m_Width;
		ColorRGBA m_Color;
	};
	const auto MakeConfig = [](int Enable, int Width, unsigned int Color) {
		const bool Used = Enable && Width > 0;
		return COutlineConfig{Used, (float)Width, Used ? color_cast<ColorRGBA>(ColorHSLA(Color, true)) : ColorRGBA()};
	};
	// Indexed by the outline type, so the order has to match the enum above.
	const COutlineConfig aConfigs[NUM_OUTLINES] = {
		COutlineConfig{false, 0.0f, ColorRGBA()}, // OUTLINE_NONE, never drawn
		MakeConfig(g_Config.m_ClOutlineUnfreeze, g_Config.m_ClOutlineWidthUnfreeze, g_Config.m_ClOutlineColorUnfreeze),
		MakeConfig(g_Config.m_ClOutlineFreeze, g_Config.m_ClOutlineWidthFreeze, g_Config.m_ClOutlineColorFreeze),
		MakeConfig(g_Config.m_ClOutlineTele, g_Config.m_ClOutlineWidthTele, g_Config.m_ClOutlineColorTele),
		MakeConfig(g_Config.m_ClOutlineKill, g_Config.m_ClOutlineWidthKill, g_Config.m_ClOutlineColorKill),
		MakeConfig(g_Config.m_ClOutlineSolid, g_Config.m_ClOutlineWidthSolid, g_Config.m_ClOutlineColorSolid)};

	bool AnyUsed = false;
	for(const COutlineConfig &Config : aConfigs)
		AnyUsed = AnyUsed || Config.m_Used;
	if(!AnyUsed)
		return;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// Neighbouring tiles are nearly always the same type, so a quad batch is held open across
	// them and only handed to the backend when the colour changes or it fills up. One draw call
	// per tile was most of what this loop cost.
	IGraphics::CQuadItem aBatch[256];
	int NumBatched = 0;
	int LastColorType = OUTLINE_NONE;
	const auto Flush = [&]() {
		if(NumBatched > 0)
		{
			Graphics()->QuadsDrawTL(aBatch, NumBatched);
			NumBatched = 0;
		}
	};

	const int Width = m_MapDataSize.x;
	const int Height = m_MapDataSize.y;
	for(int y = StartY; y < EndY; y++)
	{
		// Hoisted: every tile of this row resolves to the same clamped row.
		const uint16_t *pRow = &m_pTileInfo[(size_t)std::clamp(y, 0, Height - 1) * Width];
		const float TileY = y * Scale;
		for(int x = StartX; x < EndX; x++)
		{
			const uint16_t Info = pRow[std::clamp(x, 0, Width - 1)];
			const int Type = Info & TYPE_MASK;
			if(Type == OUTLINE_NONE)
				continue;
			dbg_assert(Type > OUTLINE_NONE && Type < NUM_OUTLINES, "Invalid value for Type at %d, %d/%d, %d", x, y, m_MapDataSize.x, m_MapDataSize.y);
			const COutlineConfig &Config = aConfigs[Type];
			if(!Config.m_Used)
				continue;
			const unsigned Neighbors = Info >> NEIGHBOR_SHIFT;
			const auto Neighbor = [Neighbors](int Index) { return (Neighbors & (1u << Index)) != 0; };
			const float OutlineWidth = Config.m_Width;
			const float TileX = x * Scale;
			// Figure out edges
			IGraphics::CQuadItem aQuads[8];
			int NumQuads = 0;
			// Lone corners first
			if(!Neighbor(0) && Neighbor(1) && Neighbor(3))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY, OutlineWidth, OutlineWidth);
			if(!Neighbor(2) && Neighbor(1) && Neighbor(4))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY, OutlineWidth, OutlineWidth);
			if(!Neighbor(5) && Neighbor(3) && Neighbor(6))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY + Scale - OutlineWidth, OutlineWidth, OutlineWidth);
			if(!Neighbor(7) && Neighbor(6) && Neighbor(4))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY + Scale - OutlineWidth, OutlineWidth, OutlineWidth);
			// Top
			if(!Neighbor(1))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY, Scale, OutlineWidth);
			// Bottom
			if(!Neighbor(6))
				aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY + Scale - OutlineWidth, Scale, OutlineWidth);
			// Left
			if(!Neighbor(3))
			{
				if(Neighbor(1) && Neighbor(6))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY, OutlineWidth, Scale);
				else if(Neighbor(6))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY + OutlineWidth, OutlineWidth, Scale - OutlineWidth);
				else if(Neighbor(1))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY, OutlineWidth, Scale - OutlineWidth);
				else
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX, TileY + OutlineWidth, OutlineWidth, Scale - OutlineWidth * 2.0f);
			}
			// Right
			if(!Neighbor(4))
			{
				if(Neighbor(1) && Neighbor(6))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY, OutlineWidth, Scale);
				else if(Neighbor(6))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY + OutlineWidth, OutlineWidth, Scale - OutlineWidth);
				else if(Neighbor(1))
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY, OutlineWidth, Scale - OutlineWidth);
				else
					aQuads[NumQuads++] = IGraphics::CQuadItem(TileX + Scale - OutlineWidth, TileY + OutlineWidth, OutlineWidth, Scale - OutlineWidth * 2.0f);
			}
			if(NumQuads <= 0)
				continue;
			if(Type != LastColorType)
			{
				Flush();
				Graphics()->SetColor(Config.m_Color);
				LastColorType = Type;
			}
			else if(NumBatched + NumQuads > (int)std::size(aBatch))
			{
				Flush();
			}
			for(int i = 0; i < NumQuads; ++i)
				aBatch[NumBatched++] = aQuads[i];
		}
	}
	Flush();

	Graphics()->QuadsEnd();
}
