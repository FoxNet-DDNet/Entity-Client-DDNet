/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mapimages.h"

#include <base/color.h>
#include <base/log.h>

#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/localization.h>
#include <game/mapitems.h>

// EClient
class CDominantColorBin
{
public:
	uint64_t m_Weight = 0;
	uint64_t m_R = 0;
	uint64_t m_G = 0;
	uint64_t m_B = 0;
	uint64_t m_A = 0;
};

static float GetLuma(const ColorRGBA &Color)
{
	return 0.2126f * Color.r + 0.7152f * Color.g + 0.0722f * Color.b;
}

static float GetSaturation(const ColorRGBA &Color)
{
	const float MaxValue = maximum(Color.r, maximum(Color.g, Color.b));
	const float MinValue = minimum(Color.r, minimum(Color.g, Color.b));
	if(MaxValue <= 0.0f)
		return 0.0f;
	return (MaxValue - MinValue) / MaxValue;
}

static ColorRGBA GetDominantColor(const CImageInfo &Image, size_t OffsetX, size_t OffsetY, size_t Width, size_t Height)
{
	if(Image.m_pData == nullptr || Width == 0 || Height == 0)
		return ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);

	const size_t EndX = minimum(OffsetX + Width, Image.m_Width);
	const size_t EndY = minimum(OffsetY + Height, Image.m_Height);
	if(OffsetX >= EndX || OffsetY >= EndY)
		return ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);

	std::array<CDominantColorBin, 16 * 16 * 16> aBins;
	size_t BestBinIndex = 0;
	uint64_t BestWeight = 0;

	auto ResetBins = [&]() {
		for(CDominantColorBin &Bin : aBins)
			Bin = {};

		BestBinIndex = 0;
		BestWeight = 0;
	};

	auto Accumulate = [&](size_t StartX, size_t StartY, size_t FinalX, size_t FinalY, bool IgnoreHighlights) {
		for(size_t y = StartY; y < FinalY; ++y)
		{
			for(size_t x = StartX; x < FinalX; ++x)
			{
				const ColorRGBA PixelColor = Image.PixelColor(x, y);

				const uint8_t Alpha = std::clamp<int>(PixelColor.a * 255.0f + 0.5f, 0, 255);
				if(Alpha < 32)
					continue;

				if(IgnoreHighlights)
				{
					const float Luma = GetLuma(PixelColor);
					const float Saturation = GetSaturation(PixelColor);
					if(Luma > 0.8f && Saturation < 0.15f)
						continue;
				}

				const uint8_t Red = std::clamp<int>(PixelColor.r * 255.0f + 0.5f, 0, 255);
				const uint8_t Green = std::clamp<int>(PixelColor.g * 255.0f + 0.5f, 0, 255);
				const uint8_t Blue = std::clamp<int>(PixelColor.b * 255.0f + 0.5f, 0, 255);

				const size_t BinIndex = ((size_t)(Red >> 4) << 8) | ((size_t)(Green >> 4) << 4) | (size_t)(Blue >> 4);
				CDominantColorBin &Bin = aBins[BinIndex];

				Bin.m_Weight += 1;
				Bin.m_R += Red;
				Bin.m_G += Green;
				Bin.m_B += Blue;
				Bin.m_A += Alpha;

				if(Bin.m_Weight > BestWeight)
				{
					BestWeight = Bin.m_Weight;
					BestBinIndex = BinIndex;
				}
			}
		}
	};

	const size_t BorderX = Width / 4;
	const size_t BorderY = Height / 4;
	const size_t InnerStartX = OffsetX + BorderX;
	const size_t InnerStartY = OffsetY + BorderY;
	const size_t InnerEndX = EndX > BorderX ? EndX - BorderX : EndX;
	const size_t InnerEndY = EndY > BorderY ? EndY - BorderY : EndY;

	ResetBins();
	if(InnerStartX < InnerEndX && InnerStartY < InnerEndY)
		Accumulate(InnerStartX, InnerStartY, InnerEndX, InnerEndY, true);

	if(BestWeight == 0)
	{
		ResetBins();
		Accumulate(OffsetX, OffsetY, EndX, EndY, false);
	}

	if(BestWeight == 0)
		return ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);

	const CDominantColorBin &BestBin = aBins[BestBinIndex];
	return ColorRGBA(
		BestBin.m_R / (float)BestBin.m_Weight / 255.0f,
		BestBin.m_G / (float)BestBin.m_Weight / 255.0f,
		BestBin.m_B / (float)BestBin.m_Weight / 255.0f,
		BestBin.m_A / (float)BestBin.m_Weight / 255.0f);
}

static int QuadTypeToTileIndex(EQuadType QuadType)
{
	switch(QuadType)
	{
	case EQuadType::FREEZE:
		return TILE_FREEZE;
	case EQuadType::UNFREEZE:
		return TILE_UNFREEZE;
	case EQuadType::DEATH:
		return TILE_DEATH;
	case EQuadType::STOPA:
		return TILE_STOPA;
	case EQuadType::CFRM:
		return TILE_TELECHECKINEVIL;
	case EQuadType::HOOKABLE:
		return TILE_SOLID;
	case EQuadType::UNHOOKABLE:
		return TILE_NOHOOK;
	case EQuadType::NONE:
	case EQuadType::NUM:
		break;
	}

	return TILE_AIR;
}

static void UpdateTilesDominantColors(ColorRGBA *pTilesDominantColor, const CImageInfo &Image)
{
	const size_t TileWidth = Image.m_Width / 16;
	const size_t TileHeight = Image.m_Height / 16;
	if(TileWidth == 0 || TileHeight == 0)
		return;

	for(int QuadType = 0; QuadType < (int)EQuadType::NUM; ++QuadType)
	{
		const int TileIndex = QuadTypeToTileIndex((EQuadType)QuadType);
		const size_t OffsetX = (size_t)(TileIndex % 16) * TileWidth;
		const size_t OffsetY = (size_t)(TileIndex / 16) * TileHeight;
		pTilesDominantColor[QuadType] = GetDominantColor(Image, OffsetX, OffsetY, TileWidth, TileHeight);
	}
}

CMapImages::CMapImages()
{
	m_Count = 0;
	std::fill(std::begin(m_aEntitiesIsLoaded), std::end(m_aEntitiesIsLoaded), false);
	m_SpeedupArrowIsLoaded = false;

	str_copy(m_aEntitiesPath, "editor/entities_clear");

	static_assert(std::size(gs_apModEntitiesNames) == MAP_IMAGE_MOD_TYPE_COUNT, "Mod name string count is not equal to mod type count");
}

void CMapImages::OnInit()
{
	m_TextureScale = g_Config.m_ClTextEntitiesSize;
	InitOverlayTextures();

	if(str_comp(g_Config.m_ClAssetsEntities, "default") == 0)
		str_copy(m_aEntitiesPath, "editor/entities_clear");
	else
	{
		str_format(m_aEntitiesPath, sizeof(m_aEntitiesPath), "assets/entities/%s", g_Config.m_ClAssetsEntities);
	}

	Console()->Chain("cl_text_entities_size", ConchainClTextEntitiesSize, this);

	// EClient
	for(ColorRGBA &Color : m_aTilesDominantColor)
		Color = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
}

void CMapImages::Unload()
{
	// unload all textures
	for(int i = 0; i < m_Count; i++)
	{
		Graphics()->UnloadTexture(&m_aTextures[i]);
	}
}

void CMapImages::OnMapLoadImpl(class CLayers *pLayers, IMap *pMap)
{
	Unload();

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Count);
	m_Count = std::clamp<int>(m_Count, 0, MAX_MAPIMAGES);

	unsigned char aTextureUsedByTileOrQuadLayerFlag[MAX_MAPIMAGES] = {0}; // 0: nothing, 1(as flag): tile layer, 2(as flag): quad layer
	for(int GroupIndex = 0; GroupIndex < pLayers->NumGroups(); GroupIndex++)
	{
		const CMapItemGroup *pGroup = pLayers->GetGroup(GroupIndex);
		if(!pGroup)
		{
			continue;
		}

		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			const CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer + LayerIndex);
			if(!pLayer)
			{
				continue;
			}

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				const CMapItemLayerTilemap *pLayerTilemap = reinterpret_cast<const CMapItemLayerTilemap *>(pLayer);
				if(pLayerTilemap->m_Image >= 0 && pLayerTilemap->m_Image < m_Count)
				{
					aTextureUsedByTileOrQuadLayerFlag[pLayerTilemap->m_Image] |= 1;
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS)
			{
				const CMapItemLayerQuads *pLayerQuads = reinterpret_cast<const CMapItemLayerQuads *>(pLayer);
				if(pLayerQuads->m_Image >= 0 && pLayerQuads->m_Image < m_Count)
				{
					aTextureUsedByTileOrQuadLayerFlag[pLayerQuads->m_Image] |= 2;
				}
			}
		}
	}

	const int TextureLoadFlag = Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE;

	// load new textures
	bool ShowWarning = false;
	for(int i = 0; i < m_Count; i++)
	{
		if(aTextureUsedByTileOrQuadLayerFlag[i] == 0)
		{
			// skip loading unused images
			continue;
		}

		const int LoadFlag = (((aTextureUsedByTileOrQuadLayerFlag[i] & 1) != 0) ? TextureLoadFlag : 0) | (((aTextureUsedByTileOrQuadLayerFlag[i] & 2) != 0) ? 0 : (Graphics()->HasTextureArraysSupport() ? IGraphics::TEXLOAD_NO_2D_TEXTURE : 0));
		const CMapItemImage_v2 *pImg = static_cast<const CMapItemImage_v2 *>(pMap->GetItem(Start + i));

		const char *pName = pMap->GetDataString(pImg->m_ImageName);
		if(pName == nullptr || pName[0] == '\0')
		{
			if(pImg->m_External)
			{
				log_error("mapimages", "Failed to load map image %d: failed to load name.", i);
				ShowWarning = true;
				continue;
			}
			pName = "(error)";
		}

		if(pImg->m_Version > 1 && pImg->m_MustBe1 != 1)
		{
			log_error("mapimages", "Failed to load map image %d '%s': invalid map image type.", i, pName);
			ShowWarning = true;
			continue;
		}

		if(pImg->m_External)
		{
			char aPath[IO_MAX_PATH_LENGTH];
			bool Translated = false;
			if(Client()->IsSixup())
			{
				Translated =
					!str_comp(pName, "grass_doodads") ||
					!str_comp(pName, "grass_main") ||
					!str_comp(pName, "winter_main") ||
					!str_comp(pName, "generic_shadows") ||
					!str_comp(pName, "generic_unhookable") ||
					!str_comp(pName, "easter");
			}
			str_format(aPath, sizeof(aPath), "mapres/%s%s.png", pName, Translated ? "_0.7" : "");
			m_aTextures[i] = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL, LoadFlag);
		}
		else
		{
			CImageInfo ImageInfo;
			ImageInfo.m_Width = pImg->m_Width;
			ImageInfo.m_Height = pImg->m_Height;
			ImageInfo.m_Format = CImageInfo::FORMAT_RGBA;
			ImageInfo.m_pData = static_cast<uint8_t *>(pMap->GetData(pImg->m_ImageData));
			if(ImageInfo.m_pData && (size_t)pMap->GetDataSize(pImg->m_ImageData) >= ImageInfo.DataSize())
			{
				char aTexName[IO_MAX_PATH_LENGTH];
				str_format(aTexName, sizeof(aTexName), "embedded: %s", pName);
				m_aTextures[i] = Graphics()->LoadTextureRaw(ImageInfo, LoadFlag, aTexName);
				pMap->UnloadData(pImg->m_ImageData);
			}
			else
			{
				pMap->UnloadData(pImg->m_ImageData);
				log_error("mapimages", "Failed to load map image %d: failed to load data.", i);
				ShowWarning = true;
				continue;
			}
		}
		pMap->UnloadData(pImg->m_ImageName);
		ShowWarning = ShowWarning || m_aTextures[i].IsNullTexture();
	}
	if(ShowWarning)
	{
		Client()->AddWarning(SWarning(Localize("Some map images could not be loaded. Check the local console for details.")));
	}
}

void CMapImages::OnMapLoad()
{
	OnMapLoadImpl(GameClient()->Layers(), GameClient()->Map());
}

void CMapImages::LoadBackground(class CLayers *pLayers, class IMap *pMap)
{
	OnMapLoadImpl(pLayers, pMap);
}

static EMapImageModType GetEntitiesModType(const CGameInfo &GameInfo)
{
	if(GameInfo.m_EntitiesFDDrace)
		return MAP_IMAGE_MOD_TYPE_FDDRACE;
	else if(GameInfo.m_EntitiesDDNet)
		return MAP_IMAGE_MOD_TYPE_DDNET;
	else if(GameInfo.m_EntitiesDDRace)
		return MAP_IMAGE_MOD_TYPE_DDRACE;
	else if(GameInfo.m_EntitiesRace)
		return MAP_IMAGE_MOD_TYPE_RACE;
	else if(GameInfo.m_EntitiesBW)
		return MAP_IMAGE_MOD_TYPE_BLOCKWORLDS;
	else if(GameInfo.m_EntitiesFNG)
		return MAP_IMAGE_MOD_TYPE_FNG;
	else if(GameInfo.m_EntitiesVanilla)
		return MAP_IMAGE_MOD_TYPE_VANILLA;
	else
		return MAP_IMAGE_MOD_TYPE_DDNET;
}

static bool IsValidTile(int LayerType, bool EntitiesAreMasked, EMapImageModType EntitiesModType, int TileIndex)
{
	if(TileIndex == TILE_AIR)
		return false;
	if(!EntitiesAreMasked)
		return true;

	if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET || EntitiesModType == MAP_IMAGE_MOD_TYPE_DDRACE)
	{
		if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET || TileIndex != TILE_SPEED_BOOST_OLD)
		{
			if(LayerType == MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH &&
				!IsValidGameTile(TileIndex) &&
				!IsValidFrontTile(TileIndex) &&
				!IsValidSpeedupTile(TileIndex) &&
				!IsValidTeleTile(TileIndex) &&
				!IsValidTuneTile(TileIndex))
			{
				return false;
			}
			else if(LayerType == MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH &&
				!IsValidSwitchTile(TileIndex))
			{
				return false;
			}
		}
	}
	else if(EntitiesModType == MAP_IMAGE_MOD_TYPE_RACE && IsCreditsTile(TileIndex))
	{
		return false;
	}
	else if(EntitiesModType == MAP_IMAGE_MOD_TYPE_FNG && IsCreditsTile(TileIndex))
	{
		return false;
	}
	else if(EntitiesModType == MAP_IMAGE_MOD_TYPE_VANILLA && IsCreditsTile(TileIndex))
	{
		return false;
	}
	return true;
}

IGraphics::CTextureHandle CMapImages::GetEntities(EMapImageEntityLayerType EntityLayerType)
{
	const bool EntitiesAreMasked = !GameClient()->m_GameInfo.m_DontMaskEntities;
	const EMapImageModType EntitiesModType = GetEntitiesModType(GameClient()->m_GameInfo);

	if(!m_aEntitiesIsLoaded[(EntitiesModType * 2) + (int)EntitiesAreMasked])
	{
		m_aEntitiesIsLoaded[(EntitiesModType * 2) + (int)EntitiesAreMasked] = true;

		int TextureLoadFlag = 0;
		if(Graphics()->HasTextureArraysSupport())
			TextureLoadFlag = (Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE) | IGraphics::TEXLOAD_NO_2D_TEXTURE;

		CImageInfo ImgInfo;
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s.png", m_aEntitiesPath, gs_apModEntitiesNames[EntitiesModType]);
		Graphics()->LoadPng(ImgInfo, aPath, IStorage::TYPE_ALL);

		// try as single ddnet replacement
		if(ImgInfo.m_pData == nullptr && EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET)
		{
			str_format(aPath, sizeof(aPath), "%s.png", m_aEntitiesPath);
			Graphics()->LoadPng(ImgInfo, aPath, IStorage::TYPE_ALL);
		}

		// try default
		if(ImgInfo.m_pData == nullptr)
		{
			str_format(aPath, sizeof(aPath), "editor/entities_clear/%s.png", gs_apModEntitiesNames[EntitiesModType]);
			Graphics()->LoadPng(ImgInfo, aPath, IStorage::TYPE_ALL);
		}

		if(ImgInfo.m_pData != nullptr)
		{
			CImageInfo BuildImageInfo;
			BuildImageInfo.m_Width = ImgInfo.m_Width;
			BuildImageInfo.m_Height = ImgInfo.m_Height;
			BuildImageInfo.m_Format = ImgInfo.m_Format;
			BuildImageInfo.m_pData = static_cast<uint8_t *>(malloc(BuildImageInfo.DataSize()));

			// build game layer
			for(int LayerType = 0; LayerType < MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT; ++LayerType)
			{
				dbg_assert(!m_aaEntitiesTextures[(EntitiesModType * 2) + (int)EntitiesAreMasked][LayerType].IsValid(), "entities texture already loaded when it should not be");

				// set everything transparent
				mem_zero(BuildImageInfo.m_pData, BuildImageInfo.DataSize());

				for(int i = 0; i < 256; ++i)
				{
					int TileIndex = i;
					if(IsValidTile(LayerType, EntitiesAreMasked, EntitiesModType, TileIndex))
					{
						if(LayerType == MAP_IMAGE_ENTITY_LAYER_TYPE_SWITCH && TileIndex == TILE_SWITCHTIMEDOPEN)
						{
							TileIndex = 8;
						}

						const size_t CopyWidth = ImgInfo.m_Width / 16;
						const size_t CopyHeight = ImgInfo.m_Height / 16;
						const size_t OffsetX = (size_t)(TileIndex % 16) * CopyWidth;
						const size_t OffsetY = (size_t)(TileIndex / 16) * CopyHeight;
						BuildImageInfo.CopyRectFrom(ImgInfo, OffsetX, OffsetY, CopyWidth, CopyHeight, OffsetX, OffsetY);
					}
				}

				m_aaEntitiesTextures[(EntitiesModType * 2) + (int)EntitiesAreMasked][LayerType] = Graphics()->LoadTextureRaw(BuildImageInfo, TextureLoadFlag, aPath);
			}

			// EClient
			if(EntitiesModType == MAP_IMAGE_MOD_TYPE_DDNET)
			{
				UpdateTilesDominantColors(m_aTilesDominantColor, ImgInfo);
			}

			BuildImageInfo.Free();
			ImgInfo.Free();
		}
	}

	return m_aaEntitiesTextures[(EntitiesModType * 2) + (int)EntitiesAreMasked][EntityLayerType];
}

IGraphics::CTextureHandle CMapImages::GetSpeedupArrow()
{
	if(!m_SpeedupArrowIsLoaded)
	{
		int TextureLoadFlag = (Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
		m_SpeedupArrowTexture = Graphics()->LoadTexture("editor/speed_arrow_array.png", IStorage::TYPE_ALL, TextureLoadFlag);
		m_SpeedupArrowIsLoaded = true;
	}
	return m_SpeedupArrowTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayBottom()
{
	return m_OverlayBottomTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayTop()
{
	return m_OverlayTopTexture;
}

IGraphics::CTextureHandle CMapImages::GetOverlayCenter()
{
	return m_OverlayCenterTexture;
}

void CMapImages::ChangeEntitiesPath(const char *pPath)
{
	if(str_comp(pPath, "default") == 0)
		str_copy(m_aEntitiesPath, "editor/entities_clear");
	else
	{
		str_format(m_aEntitiesPath, sizeof(m_aEntitiesPath), "assets/entities/%s", pPath);
	}

	for(int ModType = 0; ModType < MAP_IMAGE_MOD_TYPE_COUNT * 2; ++ModType)
	{
		if(m_aEntitiesIsLoaded[ModType])
		{
			for(int LayerType = 0; LayerType < MAP_IMAGE_ENTITY_LAYER_TYPE_COUNT; ++LayerType)
			{
				Graphics()->UnloadTexture(&m_aaEntitiesTextures[ModType][LayerType]);
			}
			m_aEntitiesIsLoaded[ModType] = false;
		}
	}
}

void CMapImages::ConchainClTextEntitiesSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CMapImages *pThis = static_cast<CMapImages *>(pUserData);
		pThis->SetTextureScale(g_Config.m_ClTextEntitiesSize);
	}
}

void CMapImages::SetTextureScale(int Scale)
{
	if(m_TextureScale == Scale)
		return;

	m_TextureScale = Scale;

	if(Graphics() && m_OverlayCenterTexture.IsValid()) // check if component was initialized
	{
		// reinitialize component
		Graphics()->UnloadTexture(&m_OverlayBottomTexture);
		Graphics()->UnloadTexture(&m_OverlayTopTexture);
		Graphics()->UnloadTexture(&m_OverlayCenterTexture);

		InitOverlayTextures();
	}
}

int CMapImages::GetTextureScale() const
{
	return m_TextureScale;
}

IGraphics::CTextureHandle CMapImages::UploadEntityLayerText(int TextureSize, int MaxWidth, int YOffset)
{
	CImageInfo TextImage;
	TextImage.m_Width = 1024;
	TextImage.m_Height = 1024;
	TextImage.m_Format = CImageInfo::FORMAT_RGBA;
	TextImage.m_pData = static_cast<uint8_t *>(calloc(TextImage.DataSize(), sizeof(uint8_t)));

	UpdateEntityLayerText(TextImage, TextureSize, MaxWidth, YOffset, 0);
	UpdateEntityLayerText(TextImage, TextureSize, MaxWidth, YOffset, 1);
	UpdateEntityLayerText(TextImage, TextureSize, MaxWidth, YOffset, 2, 255);

	const int TextureLoadFlag = (Graphics()->Uses2DTextureArrays() ? IGraphics::TEXLOAD_TO_2D_ARRAY_TEXTURE : IGraphics::TEXLOAD_TO_3D_TEXTURE) | IGraphics::TEXLOAD_NO_2D_TEXTURE;
	return Graphics()->LoadTextureRawMove(TextImage, TextureLoadFlag);
}

void CMapImages::UpdateEntityLayerText(CImageInfo &TextImage, int TextureSize, int MaxWidth, int YOffset, int NumbersPower, int MaxNumber)
{
	char aBuf[4];
	int DigitsCount = NumbersPower + 1;

	int CurrentNumber = std::pow(10, NumbersPower);

	if(MaxNumber == -1)
		MaxNumber = CurrentNumber * 10 - 1;

	str_format(aBuf, sizeof(aBuf), "%d", CurrentNumber);

	int CurrentNumberSuitableFontSize = TextRender()->AdjustFontSize(aBuf, DigitsCount, TextureSize, MaxWidth);
	int UniversalSuitableFontSize = CurrentNumberSuitableFontSize * 0.92f; // should be smoothed enough to fit any digits combination

	YOffset += ((TextureSize - UniversalSuitableFontSize) / 2);

	for(; CurrentNumber <= MaxNumber; ++CurrentNumber)
	{
		str_format(aBuf, sizeof(aBuf), "%d", CurrentNumber);

		float x = (CurrentNumber % 16) * 64;
		float y = (CurrentNumber / 16) * 64;

		int ApproximateTextWidth = TextRender()->CalculateTextWidth(aBuf, DigitsCount, 0, UniversalSuitableFontSize);
		int XOffSet = (MaxWidth - std::clamp(ApproximateTextWidth, 0, MaxWidth)) / 2;

		TextRender()->UploadEntityLayerText(TextImage, (TextImage.m_Width / 16) - XOffSet, (TextImage.m_Height / 16) - YOffset, aBuf, DigitsCount, x + XOffSet, y + YOffset, UniversalSuitableFontSize);
	}
}

void CMapImages::InitOverlayTextures()
{
	int TextureSize = 64 * m_TextureScale / 100;
	TextureSize = std::clamp(TextureSize, 2, 64);
	int TextureToVerticalCenterOffset = (64 - TextureSize) / 2 + TextureSize * 0.1f; // should be used to move texture to the center of 64 pixels area

	if(!m_OverlayBottomTexture.IsValid())
	{
		m_OverlayBottomTexture = UploadEntityLayerText(TextureSize / 2, 64, 32 + TextureToVerticalCenterOffset / 2);
	}

	if(!m_OverlayTopTexture.IsValid())
	{
		m_OverlayTopTexture = UploadEntityLayerText(TextureSize / 2, 64, TextureToVerticalCenterOffset / 2);
	}

	if(!m_OverlayCenterTexture.IsValid())
	{
		m_OverlayCenterTexture = UploadEntityLayerText(TextureSize, 64, TextureToVerticalCenterOffset);
	}
}
