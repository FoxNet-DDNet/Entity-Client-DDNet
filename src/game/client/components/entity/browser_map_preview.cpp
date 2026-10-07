#include "browser_map_preview.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/gfx/image_loader.h>
#include <engine/image.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/client/components/camera.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/map/render_layer.h>
#include <game/map/render_map.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <utility>

class CPreviewEnvelopePointAccess : public IEnvelopePointAccess
{
	const CMapPreviewData::CEnvelope &m_Envelope;

public:
	explicit CPreviewEnvelopePointAccess(const CMapPreviewData::CEnvelope &Envelope) :
		m_Envelope(Envelope) {}
	int NumPoints() const override { return static_cast<int>(m_Envelope.m_vPoints.size()); }
	const CEnvPoint *GetPoint(int Index) const override { return &m_Envelope.m_vPoints[Index]; }
	const CEnvPointBezier *GetBezier(int Index) const override { return m_Envelope.m_HasBezier ? &m_Envelope.m_vPoints[Index].m_Bezier : nullptr; }
};

class CPreviewMapImages : public IMapImages
{
	const std::vector<IGraphics::CTextureHandle> &m_vTextures;

public:
	explicit CPreviewMapImages(const std::vector<IGraphics::CTextureHandle> &vTextures) :
		m_vTextures(vTextures) {}
	IGraphics::CTextureHandle Get(int Index) const override { return m_vTextures[Index]; }
	int Num() const override { return static_cast<int>(m_vTextures.size()); }
	IGraphics::CTextureHandle GetEntities(EMapImageEntityLayerType) override { return {}; }
	IGraphics::CTextureHandle GetSpeedupArrow() override { return {}; }
	IGraphics::CTextureHandle GetTuneColors() override { return {}; }
	IGraphics::CTextureHandle GetOverlayBottom() override { return {}; }
	IGraphics::CTextureHandle GetOverlayTop() override { return {}; }
	IGraphics::CTextureHandle GetOverlayCenter() override { return {}; }
};

class CPreviewRenderLayerTile : public CRenderLayerTile
{
	CTile *m_pPreviewTiles;

protected:
	void *GetRawData() const override { return m_pPreviewTiles; }

public:
	CPreviewRenderLayerTile(int GroupId, int LayerId, CMapItemLayerTilemap *pMapItem, CTile *pTiles) :
		CRenderLayerTile(GroupId, LayerId, 0, pMapItem), m_pPreviewTiles(pTiles) {}

	void RenderPreview(const ColorRGBA &Color)
	{
		CRenderLayerParams Params{};
		Params.m_RenderTileBorder = true;
		if(Graphics()->IsTileBufferingEnabled())
			RenderTileLayer(Color, Params);
		else
		{
			Graphics()->BlendNone();
			RenderMap()->RenderTilemap(m_pPreviewTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_OPAQUE);
			Graphics()->BlendNormal();
			RenderMap()->RenderTilemap(m_pPreviewTiles, m_pLayerTilemap->m_Width, m_pLayerTilemap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND | LAYERRENDERFLAG_TRANSPARENT);
		}
	}
};

void CMapPreviewData::EvaluateEnvelope(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels, std::chrono::nanoseconds Time) const
{
	if(EnvelopeIndex < 0 || EnvelopeIndex >= static_cast<int>(m_vEnvelopes.size()))
		return;
	const CEnvelope &Envelope = m_vEnvelopes[EnvelopeIndex];
	if(Envelope.m_vPoints.empty())
		return;
	CPreviewEnvelopePointAccess Points(Envelope);
	CRenderMap::RenderEvalEnvelope(&Points, Time + std::chrono::milliseconds(TimeOffsetMillis), Result, std::min(Channels, static_cast<size_t>(Envelope.m_Channels)));
}

bool CMapPreviewData::Extract(IMap *pMap, IStorage *pStorage, const std::atomic_bool &Cancel, CMapPreviewData &Out)
{
	int ImageStart, ImageCount;
	pMap->GetType(MAPITEMTYPE_IMAGE, &ImageStart, &ImageCount);
	ImageCount = std::clamp(ImageCount, 0, static_cast<int>(MAX_MAPIMAGES));
	Out.m_vImages.resize(ImageCount);

	int GroupStart, GroupCount, LayerStart, LayerCount;
	pMap->GetType(MAPITEMTYPE_GROUP, &GroupStart, &GroupCount);
	pMap->GetType(MAPITEMTYPE_LAYER, &LayerStart, &LayerCount);
	vec2 FirstSpawn(0, 0);
	bool HasSpawn = false;
	vec2 FallbackCenter(0, 0);
	for(int g = 0; g < GroupCount && !Cancel.load(); ++g)
	{
		const auto *pGroup = static_cast<const CMapItemGroup *>(pMap->GetItem(GroupStart + g));
		if(!pGroup || pMap->GetItemSize(GroupStart + g) < static_cast<int>(sizeof(CMapItemGroup_v1)))
			continue;
		CGroup Group;
		Group.m_OffsetX = pGroup->m_OffsetX;
		Group.m_OffsetY = pGroup->m_OffsetY;
		Group.m_ParallaxX = pGroup->m_ParallaxX;
		Group.m_ParallaxY = pGroup->m_ParallaxY;
		if(pGroup->m_Version >= 2 && pMap->GetItemSize(GroupStart + g) >= static_cast<int>(sizeof(CMapItemGroup_v1) + 5 * sizeof(int)) && pGroup->m_UseClipping)
		{
			Group.m_UseClipping = true;
			Group.m_ClipX = pGroup->m_ClipX;
			Group.m_ClipY = pGroup->m_ClipY;
			Group.m_ClipW = pGroup->m_ClipW;
			Group.m_ClipH = pGroup->m_ClipH;
		}
		const int End = std::min<int64_t>(LayerCount, static_cast<int64_t>(pGroup->m_StartLayer) + pGroup->m_NumLayers);
		for(int l = std::max(0, pGroup->m_StartLayer); l < End && !Cancel.load(); ++l)
		{
			const auto *pBase = static_cast<const CMapItemLayer *>(pMap->GetItem(LayerStart + l));
			if(!pBase)
				continue;
			if(pBase->m_Type == LAYERTYPE_TILES && pMap->GetItemSize(LayerStart + l) >= static_cast<int>(sizeof(CMapItemLayerTilemap_v2)))
			{
				const auto *pTilemap = reinterpret_cast<const CMapItemLayerTilemap_v2 *>(pBase);
				if(pTilemap->m_Width <= 0 || pTilemap->m_Height <= 0 || pTilemap->m_Width > 32768 || pTilemap->m_Height > 32768)
					continue;
				const size_t Count = static_cast<size_t>(pTilemap->m_Width) * pTilemap->m_Height;
				const auto *pTiles = static_cast<const CTile *>(pMap->GetData(pTilemap->m_Data));
				if(!pTiles || pMap->GetDataSize(pTilemap->m_Data) < 0 || static_cast<size_t>(pMap->GetDataSize(pTilemap->m_Data)) < Count * sizeof(CTile))
					continue;
				if(pTilemap->m_Flags & TILESLAYERFLAG_GAME)
				{
					FallbackCenter = vec2(pTilemap->m_Width * 16.0f, pTilemap->m_Height * 16.0f);
					for(int y = 0; y < pTilemap->m_Height && !Cancel.load() && !HasSpawn; ++y)
						for(int x = 0; x < pTilemap->m_Width && !HasSpawn; ++x)
						{
							const int Index = pTiles[y * pTilemap->m_Width + x].m_Index;
							if(Index >= ENTITY_OFFSET + ENTITY_SPAWN && Index <= ENTITY_OFFSET + ENTITY_SPAWN_BLUE)
							{
								FirstSpawn = vec2(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
								HasSpawn = true;
							}
						}
				}
				if(pTilemap->m_Flags & (TILESLAYERFLAG_GAME | TILESLAYERFLAG_FRONT | TILESLAYERFLAG_TELE | TILESLAYERFLAG_SPEEDUP | TILESLAYERFLAG_SWITCH | TILESLAYERFLAG_TUNE))
					continue;
				CLayer Layer;
				Layer.m_Type = CLayer::EType::TILES;
				Layer.m_Image = pTilemap->m_Image;
				if(Layer.m_Image >= 0 && Layer.m_Image < ImageCount)
					Out.m_vImages[Layer.m_Image].m_UsedByTiles = true;
				Layer.m_Width = pTilemap->m_Width;
				Layer.m_Height = pTilemap->m_Height;
				Layer.m_Color = ColorRGBA(pTilemap->m_Color.r / 255.0f, pTilemap->m_Color.g / 255.0f, pTilemap->m_Color.b / 255.0f, pTilemap->m_Color.a / 255.0f);
				Layer.m_ColorEnv = pTilemap->m_ColorEnv;
				Layer.m_ColorEnvOffset = pTilemap->m_ColorEnvOffset;
				Layer.m_vTiles.assign(pTiles, pTiles + Count);
				Group.m_vLayers.push_back(std::move(Layer));
			}
			else if(pBase->m_Type == LAYERTYPE_QUADS && pMap->GetItemSize(LayerStart + l) >= static_cast<int>(sizeof(CMapItemLayerQuads)))
			{
				const auto *pQuadsLayer = reinterpret_cast<const CMapItemLayerQuads *>(pBase);
				if(pQuadsLayer->m_NumQuads <= 0 || pQuadsLayer->m_NumQuads > 1000000)
					continue;
				const size_t Count = pQuadsLayer->m_NumQuads;
				const auto *pQuads = static_cast<const CQuad *>(pMap->GetData(pQuadsLayer->m_Data));
				if(!pQuads || pMap->GetDataSize(pQuadsLayer->m_Data) < 0 || static_cast<size_t>(pMap->GetDataSize(pQuadsLayer->m_Data)) < Count * sizeof(CQuad))
					continue;
				CLayer Layer;
				Layer.m_Type = CLayer::EType::QUADS;
				Layer.m_Image = pQuadsLayer->m_Image;
				Layer.m_vQuads.assign(pQuads, pQuads + Count);
				Group.m_vLayers.push_back(std::move(Layer));
			}
		}
		Out.m_vGroups.push_back(std::move(Group));
	}
	int EnvelopeStart, EnvelopeCount;
	pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeCount);
	EnvelopeCount = std::clamp(EnvelopeCount, 0, 4096);
	Out.m_vEnvelopes.resize(EnvelopeCount);
	std::vector<bool> vUsedEnvelopes(EnvelopeCount, false);
	for(const auto &Group : Out.m_vGroups)
		for(const auto &Layer : Group.m_vLayers)
		{
			if(Layer.m_ColorEnv >= 0 && Layer.m_ColorEnv < EnvelopeCount)
				vUsedEnvelopes[Layer.m_ColorEnv] = true;
			for(const auto &Quad : Layer.m_vQuads)
			{
				if(Quad.m_PosEnv >= 0 && Quad.m_PosEnv < EnvelopeCount)
					vUsedEnvelopes[Quad.m_PosEnv] = true;
				if(Quad.m_ColorEnv >= 0 && Quad.m_ColorEnv < EnvelopeCount)
					vUsedEnvelopes[Quad.m_ColorEnv] = true;
			}
		}
	if(std::any_of(vUsedEnvelopes.begin(), vUsedEnvelopes.end(), [](bool Used) { return Used; }))
	{
		CMapBasedEnvelopePointAccess SourcePoints(pMap);
		for(int i = 0; i < EnvelopeCount && !Cancel.load(); ++i)
		{
			if(!vUsedEnvelopes[i])
				continue;
			const auto *pEnvelope = static_cast<const CMapItemEnvelope *>(pMap->GetItem(EnvelopeStart + i));
			if(!pEnvelope || pMap->GetItemSize(EnvelopeStart + i) < static_cast<int>(sizeof(CMapItemEnvelope_v1)))
				continue;
			CEnvelope &Envelope = Out.m_vEnvelopes[i];
			Envelope.m_Channels = std::clamp(pEnvelope->m_Channels, 0, static_cast<int>(CEnvPoint::MAX_CHANNELS));
			SourcePoints.SetPointsRange(pEnvelope->m_StartPoint, pEnvelope->m_NumPoints);
			const int NumPoints = std::min(SourcePoints.NumPoints(), 100000);
			Envelope.m_HasBezier = NumPoints > 0 && SourcePoints.GetBezier(0);
			Envelope.m_vPoints.reserve(NumPoints);
			for(int j = 0; j < NumPoints && !Cancel.load(); ++j)
			{
				const CEnvPoint *pPoint = SourcePoints.GetPoint(j);
				if(!pPoint)
					break;
				CEnvPoint_runtime Point{};
				static_cast<CEnvPoint &>(Point) = *pPoint;
				if(const CEnvPointBezier *pBezier = SourcePoints.GetBezier(j))
					Point.m_Bezier = *pBezier;
				Envelope.m_vPoints.push_back(Point);
			}
		}
	}
	std::vector<bool> vUsedImages(ImageCount, false);
	for(const auto &Group : Out.m_vGroups)
		for(const auto &Layer : Group.m_vLayers)
			if(Layer.m_Image >= 0 && Layer.m_Image < ImageCount)
				vUsedImages[Layer.m_Image] = true;
	for(int i = 0; i < ImageCount && !Cancel.load(); ++i)
	{
		if(!vUsedImages[i])
			continue;
		const auto *pImage = static_cast<const CMapItemImage_v1 *>(pMap->GetItem(ImageStart + i));
		if(!pImage || pMap->GetItemSize(ImageStart + i) < static_cast<int>(sizeof(CMapItemImage_v1)))
			continue;
		CImage &Image = Out.m_vImages[i];
		Image.m_Width = pImage->m_Width;
		Image.m_Height = pImage->m_Height;
		const char *pName = pMap->GetDataString(pImage->m_ImageName);
		if(pImage->m_External)
		{
			if(pName && pName[0])
			{
				Image.m_Path = std::string("mapres/") + pName + ".png";
				int PngliteIncompatible = 0;
				CImageLoader::LoadPng(pStorage->OpenFile(Image.m_Path.c_str(), IOFLAG_READ, IStorage::TYPE_ALL), Image.m_Path.c_str(), Image.m_ImageInfo, PngliteIncompatible);
				if(Image.m_ImageInfo.m_Format == CImageInfo::FORMAT_RGB)
				{
					CImageInfo Rgba;
					Rgba.m_Width = Image.m_ImageInfo.m_Width;
					Rgba.m_Height = Image.m_ImageInfo.m_Height;
					Rgba.m_Format = CImageInfo::FORMAT_RGBA;
					Rgba.Allocate();
					if(Rgba.m_pData)
					{
						const size_t NumPixels = Rgba.m_Width * Rgba.m_Height;
						for(size_t Pixel = 0; Pixel < NumPixels; ++Pixel)
						{
							std::memcpy(Rgba.m_pData + Pixel * 4, Image.m_ImageInfo.m_pData + Pixel * 3, 3);
							Rgba.m_pData[Pixel * 4 + 3] = 255;
						}
						Image.m_ImageInfo.Free();
						Image.m_ImageInfo = std::move(Rgba);
					}
				}
			}
		}
		else if(pImage->m_Width > 0 && pImage->m_Height > 0 && pImage->m_Width <= 4096 && pImage->m_Height <= 4096)
		{
			const size_t Bytes = static_cast<size_t>(pImage->m_Width) * pImage->m_Height * 4;
			const void *pPixels = pMap->GetData(pImage->m_ImageData);
			if(pPixels && pMap->GetDataSize(pImage->m_ImageData) >= 0 && static_cast<size_t>(pMap->GetDataSize(pImage->m_ImageData)) >= Bytes)
			{
				Image.m_ImageInfo.m_Width = pImage->m_Width;
				Image.m_ImageInfo.m_Height = pImage->m_Height;
				Image.m_ImageInfo.m_Format = CImageInfo::FORMAT_RGBA;
				Image.m_ImageInfo.Allocate();
				if(Image.m_ImageInfo.m_pData)
					std::memcpy(Image.m_ImageInfo.m_pData, pPixels, Bytes);
			}
		}
	}
	Out.m_SpawnPosition = HasSpawn ? FirstSpawn : FallbackCenter;
	return !Cancel.load();
}

void CBrowserMapPreview::ReapWorkers()
{
	for(auto It = m_vRetiredWorkers.begin(); It != m_vRetiredWorkers.end();)
	{
		if(It->m_pJob->m_Finished.load(std::memory_order_acquire))
		{
			It->m_Thread.join();
			It = m_vRetiredWorkers.erase(It);
		}
		else
			++It;
	}
}

void CBrowserMapPreview::ClearTileCaches()
{
	for(auto &vGroupCaches : m_vTileCaches)
		for(auto &Cache : vGroupCaches)
			if(Cache.m_pRenderLayer)
				Cache.m_pRenderLayer->Unload();
	m_vTileCaches.clear();
	m_NextInitialTileGroup = 0;
	m_NextInitialTileLayer = 0;
}

void CBrowserMapPreview::ClearPreview()
{
	ClearTileCaches();
	m_pMapImages.reset();
	m_pRenderMap.reset();
	for(auto &Texture : m_vTextures)
		if(!Texture.IsNullTexture())
			Graphics()->UnloadTexture(&Texture);
	m_vTextures.clear();
	m_NextImageToUpload = 0;
	m_pData.reset();
	m_TileCacheBuiltOnce = false;
	m_Dragging = false;
}

std::string CBrowserMapPreview::OverrideKey() const
{
	return m_CommunityId + '\t' + m_MapName;
}

void CBrowserMapPreview::ConAddMapPreview(IConsole::IResult *pResult, void *pUserData)
{
	auto *pThis = static_cast<CBrowserMapPreview *>(pUserData);
	const char *pCommunity = pResult->GetString(0);
	const char *pMapName = pResult->GetString(1);
	const char *pFilename = pResult->GetString(2);
	if(!pMapName[0] || !pFilename[0] ||
		str_length(pCommunity) >= CServerInfo::MAX_COMMUNITY_ID_LENGTH ||
		str_length(pMapName) >= MAX_MAP_LENGTH || str_length(pFilename) >= IO_MAX_PATH_LENGTH ||
		std::strpbrk(pCommunity, "\t\r\n") || std::strpbrk(pMapName, "\t\r\n") || std::strpbrk(pFilename, "\t\r\n"))
		return;
	pThis->m_Overrides[std::string(pCommunity) + '\t' + pMapName] = pFilename;
}

void CBrowserMapPreview::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	const auto *pThis = static_cast<const CBrowserMapPreview *>(pUserData);
	for(const auto &[Key, Filename] : pThis->m_Overrides)
	{
		const size_t Separator = Key.find('\t');
		if(Separator == std::string::npos || Separator >= CServerInfo::MAX_COMMUNITY_ID_LENGTH ||
			Key.size() - Separator - 1 >= MAX_MAP_LENGTH || Filename.size() >= IO_MAX_PATH_LENGTH)
			continue;
		const std::string Community = Key.substr(0, Separator);
		const std::string MapName = Key.substr(Separator + 1);
		char aBuf[((int)CServerInfo::MAX_COMMUNITY_ID_LENGTH + (int)MAX_MAP_LENGTH + IO_MAX_PATH_LENGTH) * 2 + 64] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_map_preview \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Community.c_str(), pEnd);
		str_append(aBuf, "\" \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, MapName.c_str(), pEnd);
		str_append(aBuf, "\" \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Filename.c_str(), pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYMAPPREVIEWS);
	}
}

void CBrowserMapPreview::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYMAPPREVIEWS);
	Console()->Register("add_map_preview", "s[community] s[map] s[filename]", CFGFLAG_CLIENT, ConAddMapPreview, this, "Select a downloaded map for browser previews in a community");
	LoadLegacyOverrides();
}

void CBrowserMapPreview::LoadLegacyOverrides()
{
	if(m_LegacyOverridesLoaded)
		return;
	m_LegacyOverridesLoaded = true;
	if(Storage()->FileExists(s_aConfigDomains[ConfigDomain::ENTITYMAPPREVIEWS].m_aConfigPath, IStorage::TYPE_ALL))
		return;
	CLineReader Reader;
	if(!Reader.OpenFile(Storage()->OpenFile("browser_map_preview_overrides.txt", IOFLAG_READ, IStorage::TYPE_SAVE)))
		return;
	while(const char *pLine = Reader.Get())
	{
		const char *pFirst = std::strchr(pLine, '\t');
		const char *pSecond = pFirst ? std::strchr(pFirst + 1, '\t') : nullptr;
		if(pFirst && pSecond && pSecond[1] && !std::strchr(pSecond + 1, '\t') &&
			pFirst - pLine < CServerInfo::MAX_COMMUNITY_ID_LENGTH && pSecond - pFirst - 1 < MAX_MAP_LENGTH &&
			str_length(pSecond + 1) < IO_MAX_PATH_LENGTH)
			m_Overrides.try_emplace(std::string(pLine, pSecond), pSecond + 1);
	}
}

void CBrowserMapPreview::LoadMap(const char *pMapName, const char *pCommunityId, unsigned MapCrc)
{
	ReapWorkers();
	if(m_MapName == pMapName && m_CommunityId == pCommunityId && m_MapCrc == MapCrc)
		return;
	if(m_CurrentWorker.m_pJob)
	{
		m_CurrentWorker.m_pJob->m_Cancel.store(true);
		m_vRetiredWorkers.push_back(std::move(m_CurrentWorker));
	}
	ClearPreview();
	m_MapName = pMapName;
	m_CommunityId = pCommunityId;
	m_MapCrc = MapCrc;
	m_Zoom = 1.0f;
	if(m_MapName.empty())
		return;
	m_CurrentWorker.m_pJob = std::make_shared<CJob>();
	m_CurrentWorker.m_pJob->m_MapName = m_MapName;
	m_CurrentWorker.m_pJob->m_MapCrc = m_MapCrc;
	if(const auto It = m_Overrides.find(OverrideKey()); It != m_Overrides.end())
		m_CurrentWorker.m_pJob->m_PreferredFilename = It->second;
	m_CurrentWorker.m_pJob->m_pStorage = Storage();
	m_CurrentWorker.m_Thread = std::thread(MapLoadThread, m_CurrentWorker.m_pJob);
}

bool CBrowserMapPreview::HasLoadedMap() const
{
	return m_CurrentWorker.m_pJob && m_CurrentWorker.m_pJob->m_State.load(std::memory_order_acquire) == EState::READY &&
	       m_pData && m_NextImageToUpload >= m_vTextures.size();
}

bool CBrowserMapPreview::HasAvailableMap(const char *pMapName)
{
	if(!pMapName || !pMapName[0])
		return false;
	if(m_pAvailableMapsJob && m_pAvailableMapsJob->m_Finished.load(std::memory_order_acquire) &&
		m_pAvailableMapsJob->m_MapNames.find(pMapName) == m_pAvailableMapsJob->m_MapNames.end() &&
		time_get() - m_pAvailableMapsJob->m_FinishedAt.load(std::memory_order_relaxed) > time_freq() * 30)
	{
		m_AvailableMapsThread.join();
		m_pAvailableMapsJob.reset();
	}
	if(!m_pAvailableMapsJob)
	{
		m_pAvailableMapsJob = std::make_shared<CAvailableMapsJob>();
		m_pAvailableMapsJob->m_pStorage = Storage();
		m_AvailableMapsThread = std::thread(IndexAvailableMaps, m_pAvailableMapsJob);
	}
	return m_pAvailableMapsJob->m_Finished.load(std::memory_order_acquire) && m_pAvailableMapsJob->m_MapNames.find(pMapName) != m_pAvailableMapsJob->m_MapNames.end();
}

bool CBrowserMapPreview::HasAlternativeMap() const
{
	if(!m_CurrentWorker.m_pJob)
		return false;
	const CJob &Job = *m_CurrentWorker.m_pJob;
	const EState State = Job.m_State.load(std::memory_order_acquire);
	return Job.m_Finished.load(std::memory_order_acquire) &&
	       (State == EState::READY || State == EState::INVALID) &&
	       Job.m_vCandidates.size() > 1 && Job.m_aSelectedFilename[0] != '\0';
}

size_t CBrowserMapPreview::MapVersionCount() const
{
	return HasAlternativeMap() ? m_CurrentWorker.m_pJob->m_vCandidates.size() : 0;
}

size_t CBrowserMapPreview::SelectedVersionIndex() const
{
	if(!HasAlternativeMap())
		return 0;
	const auto &vCandidates = m_CurrentWorker.m_pJob->m_vCandidates;
	const auto It = std::find_if(vCandidates.begin(), vCandidates.end(), [&](const CMapCandidate &Candidate) {
		return Candidate.m_Filename == m_CurrentWorker.m_pJob->m_aSelectedFilename;
	});
	return It == vCandidates.end() ? 0 : std::distance(vCandidates.begin(), It) + 1;
}

bool CBrowserMapPreview::SelectNextMapVersion()
{
	if(!HasAlternativeMap())
		return false;
	if(OverrideKey().find_first_of("\r\n") != std::string::npos || m_CommunityId.find('\t') != std::string::npos || m_MapName.find('\t') != std::string::npos)
		return false;
	const auto &vCandidates = m_CurrentWorker.m_pJob->m_vCandidates;
	auto It = std::find_if(vCandidates.begin(), vCandidates.end(), [&](const CMapCandidate &Candidate) {
		return Candidate.m_Filename == m_CurrentWorker.m_pJob->m_aSelectedFilename;
	});
	const size_t Next = It == vCandidates.end() ? 0 : (std::distance(vCandidates.begin(), It) + 1) % vCandidates.size();
	if(vCandidates[Next].m_Filename.find_first_of("\t\r\n") != std::string::npos)
		return false;
	m_Overrides[OverrideKey()] = vCandidates[Next].m_Filename;
	const std::string MapName = m_MapName;
	const std::string CommunityId = m_CommunityId;
	const unsigned MapCrc = m_MapCrc;
	m_CurrentWorker.m_Thread.join();
	m_CurrentWorker = CWorker{};
	ClearPreview();
	m_MapName.clear();
	LoadMap(MapName.c_str(), CommunityId.c_str(), MapCrc);
	return true;
}

void CBrowserMapPreview::MapLoadThread(const std::shared_ptr<CJob> &pJob)
{
	if(pJob->m_Cancel.load())
	{
		pJob->m_Finished.store(true, std::memory_order_release);
		return;
	}
	CMapSearchData SearchData{pJob->m_MapName.c_str(), &pJob->m_Cancel};
	pJob->m_pStorage->ListDirectoryInfo(IStorage::TYPE_SAVE, "downloadedmaps", MapSearchCallback, &SearchData);
	if(!pJob->m_Cancel.load())
	{
		std::sort(SearchData.m_vCandidates.begin(), SearchData.m_vCandidates.end(), [](const CMapCandidate &A, const CMapCandidate &B) {
			return A.m_Modified == B.m_Modified ? A.m_Filename < B.m_Filename : A.m_Modified > B.m_Modified;
		});
		pJob->m_vCandidates = std::move(SearchData.m_vCandidates);
		char aMapFilename[IO_MAX_PATH_LENGTH]{};
		int StorageType = IStorage::TYPE_SAVE;
		if(!pJob->m_vCandidates.empty())
		{
			const CMapCandidate *pSelected = &pJob->m_vCandidates.front();
			bool HasPreferred = false;
			for(const auto &Candidate : pJob->m_vCandidates)
				if(Candidate.m_Filename == pJob->m_PreferredFilename)
				{
					pSelected = &Candidate;
					HasPreferred = true;
				}
			if(!HasPreferred && pJob->m_MapCrc != 0)
			{
				char aExpected[16];
				str_format(aExpected, sizeof(aExpected), "_%08x.map", pJob->m_MapCrc);
				for(const auto &Candidate : pJob->m_vCandidates)
					if(str_endswith(Candidate.m_Filename.c_str(), aExpected))
						pSelected = &Candidate;
			}
			str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s", pSelected->m_Filename.c_str());
			str_copy(pJob->m_aSelectedFilename, pSelected->m_Filename.c_str());
			if(!HasPreferred)
				pJob->m_PreferredFilename.clear();
		}
		else
		{
			StorageType = IStorage::TYPE_ALL;
			str_format(aMapFilename, sizeof(aMapFilename), "maps/%s.map", pJob->m_MapName.c_str());
			if(!pJob->m_pStorage->FileExists(aMapFilename, StorageType))
			{
				char aMapBasename[IO_MAX_PATH_LENGTH];
				str_format(aMapBasename, sizeof(aMapBasename), "%s.map", pJob->m_MapName.c_str());
				if(!pJob->m_pStorage->FindFile(aMapBasename, "maps", StorageType, aMapFilename, sizeof(aMapFilename)))
					aMapFilename[0] = '\0';
			}
			// Some local downloads use spaces while the browser advertises underscores.
			// Use this only as a last resort and never offer deletion for an alias.
			if(aMapFilename[0] == '\0' && pJob->m_MapName.find('_') != std::string::npos)
			{
				std::string Alias = pJob->m_MapName;
				std::replace(Alias.begin(), Alias.end(), '_', ' ');
				CMapSearchData AliasSearch{Alias.c_str(), &pJob->m_Cancel};
				pJob->m_pStorage->ListDirectoryInfo(IStorage::TYPE_SAVE, "downloadedmaps", MapSearchCallback, &AliasSearch);
				if(!AliasSearch.m_vCandidates.empty())
				{
					const auto It = std::max_element(AliasSearch.m_vCandidates.begin(), AliasSearch.m_vCandidates.end(), [](const CMapCandidate &A, const CMapCandidate &B) { return A.m_Modified < B.m_Modified; });
					str_format(aMapFilename, sizeof(aMapFilename), "downloadedmaps/%s", It->m_Filename.c_str());
					StorageType = IStorage::TYPE_SAVE;
				}
			}
		}
		if(aMapFilename[0] == '\0')
			pJob->m_State.store(EState::MISSING, std::memory_order_release);
		else if(!pJob->m_Cancel.load())
		{
			auto pMap = CreateMap();
			if(!pMap->Load(pJob->m_pStorage, aMapFilename, StorageType))
				pJob->m_State.store(EState::INVALID, std::memory_order_release);
			else if(!pJob->m_Cancel.load())
			{
				// SHA-named downloads do not expose their CRC in the filename.
				// Check their actual map CRC before falling back to file time.
				if(pJob->m_PreferredFilename.empty() && pJob->m_MapCrc != 0 && pMap->Crc() != pJob->m_MapCrc)
				{
					for(const auto &Candidate : pJob->m_vCandidates)
					{
						if(pJob->m_Cancel.load())
							break;
						if(Candidate.m_Filename == pJob->m_aSelectedFilename)
							continue;
						char aCandidatePath[IO_MAX_PATH_LENGTH];
						str_format(aCandidatePath, sizeof(aCandidatePath), "downloadedmaps/%s", Candidate.m_Filename.c_str());
						auto pCandidateMap = CreateMap();
						if(pCandidateMap->Load(pJob->m_pStorage, aCandidatePath, IStorage::TYPE_SAVE) && pCandidateMap->Crc() == pJob->m_MapCrc)
						{
							pMap = std::move(pCandidateMap);
							str_copy(pJob->m_aSelectedFilename, Candidate.m_Filename.c_str());
							break;
						}
					}
				}
				auto pData = std::make_unique<CMapPreviewData>();
				if(CMapPreviewData::Extract(pMap.get(), pJob->m_pStorage, pJob->m_Cancel, *pData) && !pJob->m_Cancel.load())
				{
					pJob->m_pData = std::move(pData);
					pJob->m_State.store(EState::READY, std::memory_order_release);
				}
				else if(!pJob->m_Cancel.load())
					pJob->m_State.store(EState::INVALID, std::memory_order_release);
			}
		}
	}
	pJob->m_Finished.store(true, std::memory_order_release);
}

void CBrowserMapPreview::OnShutdown()
{
	if(m_pAvailableMapsJob)
		m_pAvailableMapsJob->m_Cancel.store(true);
	if(m_AvailableMapsThread.joinable())
		m_AvailableMapsThread.join();
	m_pAvailableMapsJob.reset();
	if(m_CurrentWorker.m_pJob)
		m_CurrentWorker.m_pJob->m_Cancel.store(true);
	for(auto &Worker : m_vRetiredWorkers)
		Worker.m_pJob->m_Cancel.store(true);
	if(m_CurrentWorker.m_Thread.joinable())
		m_CurrentWorker.m_Thread.join();
	for(auto &Worker : m_vRetiredWorkers)
		Worker.m_Thread.join();
	m_vRetiredWorkers.clear();
	ClearPreview();
}

void CBrowserMapPreview::UploadNextImage()
{
	if(!m_pData || m_NextImageToUpload >= m_pData->m_vImages.size())
		return;
	for(; m_NextImageToUpload < m_pData->m_vImages.size(); ++m_NextImageToUpload)
	{
		const size_t ImageIndex = m_NextImageToUpload;
		auto &Image = m_pData->m_vImages[ImageIndex];
		if(Image.m_ImageInfo.m_pData)
		{
			const int Flags = Image.m_UsedByTiles ? Graphics()->TextureLoadFlags() : 0;
			m_vTextures[ImageIndex] = Graphics()->LoadTextureRawMove(Image.m_ImageInfo, Flags, Image.m_Path.empty() ? "map preview" : Image.m_Path.c_str());
			++m_NextImageToUpload;
			break;
		}
	}
}

void CBrowserMapPreview::InitTileLayerCache(const CMapPreviewData::CLayer &Layer, CTileLayerCache &Cache, size_t GroupIndex, size_t LayerIndex)
{
	if(Layer.m_Width <= 0 || Layer.m_Height <= 0 || Layer.m_vTiles.empty())
		return;
	if(!m_pMapImages)
		m_pMapImages = std::make_shared<CPreviewMapImages>(m_vTextures);
	if(!m_pRenderMap)
	{
		m_pRenderMap = std::make_shared<CRenderMap>();
		m_pRenderMap->Init(Graphics(), TextRender());
	}
	Cache.m_pMapItem = std::make_shared<CMapItemLayerTilemap>();
	auto &Item = *Cache.m_pMapItem;
	Item = CMapItemLayerTilemap{};
	Item.m_Layer.m_Type = LAYERTYPE_TILES;
	Item.m_Version = 3;
	Item.m_Width = Layer.m_Width;
	Item.m_Height = Layer.m_Height;
	Item.m_Image = Layer.m_Image;
	Item.m_Color = {255, 255, 255, 255};
	Item.m_ColorEnv = -1;
	Cache.m_pRenderLayer = std::make_shared<CPreviewRenderLayerTile>(static_cast<int>(GroupIndex), static_cast<int>(LayerIndex), Cache.m_pMapItem.get(), const_cast<CTile *>(Layer.m_vTiles.data()));
	std::shared_ptr<CEnvelopeManager> pNoEnvelopeManager;
	std::optional<FCallbackLayerInit> NoInitCallback;
	Cache.m_pRenderLayer->OnInit(Graphics(), TextRender(), m_pRenderMap.get(), pNoEnvelopeManager, nullptr, m_pMapImages.get(), NoInitCallback);
	Cache.m_pRenderLayer->Init();
}
bool CBrowserMapPreview::RenderMap(const CUIRect &Rect)
{
	const float BaseScale = std::min(Rect.w / 960.0f, Rect.h / 540.0f);
	if(BaseScale <= 0.0f)
		return false;
	const std::chrono::nanoseconds EnvelopeTime = time_get_nanoseconds();
	if(!m_TileCacheBuiltOnce && m_vTileCaches.empty())
	{
		m_vTileCaches.resize(m_pData->m_vGroups.size());
		for(size_t g = 0; g < m_vTileCaches.size(); ++g)
			m_vTileCaches[g].resize(m_pData->m_vGroups[g].m_vLayers.size());
	}
	if(!m_TileCacheBuiltOnce)
	{
		// Graphics containers must be created on the render thread. Limit the
		// initial uploads so opening a preview cannot submit every layer at once.
		int BuiltLayers = 0;
		while(m_NextInitialTileGroup < m_pData->m_vGroups.size() && BuiltLayers < 1)
		{
			const size_t GroupIndex = m_NextInitialTileGroup;
			const auto &Group = m_pData->m_vGroups[GroupIndex];
			if(m_NextInitialTileLayer >= Group.m_vLayers.size())
			{
				++m_NextInitialTileGroup;
				m_NextInitialTileLayer = 0;
				continue;
			}
			const size_t LayerIndex = m_NextInitialTileLayer++;
			const auto &Layer = Group.m_vLayers[LayerIndex];
			if(Layer.m_Type != CMapPreviewData::CLayer::EType::TILES)
				continue;
			++BuiltLayers;
			InitTileLayerCache(Layer, m_vTileCaches[GroupIndex][LayerIndex], GroupIndex, LayerIndex);
		}
		if(m_NextInitialTileGroup < m_pData->m_vGroups.size())
			return false;
		m_TileCacheBuiltOnce = true;
		// Let the graphics backend finish the last buffer upload before the
		// first frame that draws every preview layer.
		return false;
	}
	Ui()->ClipEnable(&Rect);
	for(size_t GroupIndex = 0; GroupIndex < m_pData->m_vGroups.size(); ++GroupIndex)
	{
		const auto &Group = m_pData->m_vGroups[GroupIndex];
		CUIRect VisibleRect = Rect;
		if(Group.m_UseClipping)
		{
			const float ClipScale = BaseScale * m_Zoom;
			const float ClipX = Rect.Center().x + (Group.m_ClipX - m_CameraCenter.x) * ClipScale;
			const float ClipY = Rect.Center().y + (Group.m_ClipY - m_CameraCenter.y) * ClipScale;
			const float ClipRight = ClipX + Group.m_ClipW * ClipScale;
			const float ClipBottom = ClipY + Group.m_ClipH * ClipScale;
			VisibleRect.x = std::max(Rect.x, ClipX);
			VisibleRect.y = std::max(Rect.y, ClipY);
			VisibleRect.w = std::min(Rect.x + Rect.w, ClipRight) - VisibleRect.x;
			VisibleRect.h = std::min(Rect.y + Rect.h, ClipBottom) - VisibleRect.y;
			if(VisibleRect.w <= 0.0f || VisibleRect.h <= 0.0f)
				continue;
			Ui()->ClipEnable(&VisibleRect);
		}
		// Match IGraphics::MapScreenToWorld: parallax changes both camera travel
		// and how strongly a group responds to zoom.
		const float ParallaxZoom = std::clamp(static_cast<float>(std::max(Group.m_ParallaxX, Group.m_ParallaxY)), 0.0f, 100.0f) / 100.0f;
		const float ZoomSpan = 1.0f + ParallaxZoom * (1.0f / m_Zoom - 1.0f);
		const float Scale = BaseScale / ZoomSpan;
		const vec2 GroupCenter(Group.m_OffsetX + m_CameraCenter.x * Group.m_ParallaxX / 100.0f, Group.m_OffsetY + m_CameraCenter.y * Group.m_ParallaxY / 100.0f);
		const vec2 Origin = Rect.Center() - GroupCenter * Scale;
		for(size_t LayerIndex = 0; LayerIndex < Group.m_vLayers.size(); ++LayerIndex)
		{
			const auto &Layer = Group.m_vLayers[LayerIndex];
			const bool HasImage = Layer.m_Image >= 0 && Layer.m_Image < static_cast<int>(m_vTextures.size()) && !m_vTextures[Layer.m_Image].IsNullTexture();
			if(HasImage)
				Graphics()->TextureSet(m_vTextures[Layer.m_Image]);
			else
				Graphics()->TextureClear();
			if(Layer.m_Type == CMapPreviewData::CLayer::EType::TILES)
			{
				auto &Cache = m_vTileCaches[GroupIndex][LayerIndex];
				ColorRGBA EnvColor(1, 1, 1, 1);
				m_pData->EvaluateEnvelope(Layer.m_ColorEnvOffset, Layer.m_ColorEnv, EnvColor, 4, EnvelopeTime);
				if(Cache.m_pRenderLayer)
				{
					const CScreenRect PreviousScreen = Graphics()->GetScreen();
					// MapScreen covers the full UI screen. The preview rectangle is only
					// a clip region, so its bounds cannot be used as the screen bounds.
					const vec2 WorldTopLeft = -Origin / Scale;
					const vec2 WorldBottomRight = (vec2(Ui()->Screen()->w, Ui()->Screen()->h) - Origin) / Scale;
					Graphics()->MapScreen(CScreenRect(WorldTopLeft, WorldBottomRight));
					Cache.m_pRenderLayer->RenderPreview(Layer.m_Color.Multiply(EnvColor));
					Graphics()->MapScreen(PreviousScreen);
				}
			}
			else
			{
				Graphics()->QuadsBegin();
				for(const auto &Quad : Layer.m_vQuads)
				{
					ColorRGBA EnvColor(1, 1, 1, 1);
					m_pData->EvaluateEnvelope(Quad.m_ColorEnvOffset, Quad.m_ColorEnv, EnvColor, 4, EnvelopeTime);
					if(EnvColor.a <= 0.0f)
						continue;
					ColorRGBA EnvPosition(0, 0, 0, 0);
					m_pData->EvaluateEnvelope(Quad.m_PosEnvOffset, Quad.m_PosEnv, EnvPosition, 3, EnvelopeTime);
					const float Rotation = EnvPosition.b / 180.0f * pi;
					const float Cos = std::cos(Rotation);
					const float Sin = std::sin(Rotation);
					const vec2 Center(Quad.m_aPoints[4].x / 1024.0f, Quad.m_aPoints[4].y / 1024.0f);
					const auto TransformPoint = [&](const CPoint &Point) {
						const vec2 Relative(Point.x / 1024.0f - Center.x, Point.y / 1024.0f - Center.y);
						const vec2 Rotated(Relative.x * Cos - Relative.y * Sin, Relative.x * Sin + Relative.y * Cos);
						return Origin + (Center + Rotated + vec2(EnvPosition.r, EnvPosition.g)) * Scale;
					};
					const vec2 P0 = TransformPoint(Quad.m_aPoints[0]);
					const vec2 P1 = TransformPoint(Quad.m_aPoints[1]);
					const vec2 P2 = TransformPoint(Quad.m_aPoints[2]);
					const vec2 P3 = TransformPoint(Quad.m_aPoints[3]);
					if(std::max({P0.x, P1.x, P2.x, P3.x}) < VisibleRect.x || std::min({P0.x, P1.x, P2.x, P3.x}) > VisibleRect.x + VisibleRect.w ||
						std::max({P0.y, P1.y, P2.y, P3.y}) < VisibleRect.y || std::min({P0.y, P1.y, P2.y, P3.y}) > VisibleRect.y + VisibleRect.h)
						continue;
					if(HasImage)
						Graphics()->QuadsSetSubsetFree(Quad.m_aTexcoords[0].x / 1024.0f, Quad.m_aTexcoords[0].y / 1024.0f, Quad.m_aTexcoords[1].x / 1024.0f, Quad.m_aTexcoords[1].y / 1024.0f, Quad.m_aTexcoords[2].x / 1024.0f, Quad.m_aTexcoords[2].y / 1024.0f, Quad.m_aTexcoords[3].x / 1024.0f, Quad.m_aTexcoords[3].y / 1024.0f);
					Graphics()->SetColor4(ColorRGBA(Quad.m_aColors[0].r / 255.0f, Quad.m_aColors[0].g / 255.0f, Quad.m_aColors[0].b / 255.0f, Quad.m_aColors[0].a / 255.0f).Multiply(EnvColor), ColorRGBA(Quad.m_aColors[1].r / 255.0f, Quad.m_aColors[1].g / 255.0f, Quad.m_aColors[1].b / 255.0f, Quad.m_aColors[1].a / 255.0f).Multiply(EnvColor), ColorRGBA(Quad.m_aColors[3].r / 255.0f, Quad.m_aColors[3].g / 255.0f, Quad.m_aColors[3].b / 255.0f, Quad.m_aColors[3].a / 255.0f).Multiply(EnvColor), ColorRGBA(Quad.m_aColors[2].r / 255.0f, Quad.m_aColors[2].g / 255.0f, Quad.m_aColors[2].b / 255.0f, Quad.m_aColors[2].a / 255.0f).Multiply(EnvColor));
					IGraphics::CFreeformItem Item;
					Item.m_X0 = P0.x;
					Item.m_Y0 = P0.y;
					Item.m_X1 = P1.x;
					Item.m_Y1 = P1.y;
					Item.m_X2 = P2.x;
					Item.m_Y2 = P2.y;
					Item.m_X3 = P3.x;
					Item.m_Y3 = P3.y;
					Graphics()->QuadsDrawFreeform(&Item, 1);
				}
				Graphics()->QuadsEnd();
			}
		}
		if(Group.m_UseClipping)
			Ui()->ClipDisable();
	}
	Ui()->ClipDisable();
	return true;
}

void CBrowserMapPreview::Render(CUIRect *pRect)
{
	pRect->Draw(ColorRGBA(0.08f, 0.09f, 0.12f, 1.0f), IGraphics::CORNER_ALL, 2.5f);
	const auto RenderLoading = [&](const char *pText) {
		Ui()->RenderProgressSpinner(pRect->Center(), 10.0f);
		CUIRect TextRect = *pRect;
		TextRect.y = pRect->Center().y + 14.0f;
		TextRect.h = 18.0f;
		Ui()->DoLabel(&TextRect, pText, 13.0f, TEXTALIGN_MC);
	};
	if(!m_CurrentWorker.m_pJob)
		return;
	const EState State = m_CurrentWorker.m_pJob->m_State.load(std::memory_order_acquire);
	if(State == EState::READY && !m_pData)
	{
		m_pData = std::move(m_CurrentWorker.m_pJob->m_pData);
		m_CameraCenter = m_pData->m_SpawnPosition;
		m_vTextures.resize(m_pData->m_vImages.size());
	}
	if(State == EState::READY && m_NextImageToUpload < m_vTextures.size())
	{
		UploadNextImage();
		RenderLoading(Localize("Preparing map preview..."));
		return;
	}
	if(State == EState::READY && m_pData)
	{
		if(Ui()->MouseHovered(pRect) && Ui()->MouseButtonClicked(0))
		{
			m_Dragging = true;
			m_LastDragMousePos = Ui()->MousePos();
		}
		if(!Ui()->MouseButton(0))
			m_Dragging = false;
		if(m_Dragging)
		{
			const float Scale = std::min(pRect->w / 960.0f, pRect->h / 540.0f) * m_Zoom;
			if(Scale > 0.0f)
				m_CameraCenter -= (Ui()->MousePos() - m_LastDragMousePos) / Scale;
			m_LastDragMousePos = Ui()->MousePos();
		}
		if(Ui()->MouseHovered(pRect))
		{
			// The camera stores zoom as world span; the preview stores its inverse.
			const float MinZoom = 1.0f / (g_Config.m_ClLimitMaxZoomLevel ? (Graphics()->IsTileBufferingEnabled() ? 240.0f : 30.0f) : 240.0f);
			const float MaxZoom = 1.0f / 0.01f;
			if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
				m_Zoom = std::min(m_Zoom / CCamera::ZOOM_STEP, MaxZoom);
			if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
				m_Zoom = std::max(m_Zoom * CCamera::ZOOM_STEP, MinZoom);
		}
		if(!RenderMap(*pRect))
		{
			RenderLoading(Localize("Preparing map preview..."));
			return;
		}
	}
	else
	{
		const char *pText = State == EState::MISSING ? Localize("Map not found in local maps or downloads") : State == EState::INVALID ? Localize("Could not load map preview") :
																		 Localize("Loading map preview...");
		if(State == EState::LOADING)
			RenderLoading(pText);
		else
			Ui()->DoLabel(pRect, pText, 13.0f, TEXTALIGN_MC);
	}
}

int CBrowserMapPreview::MapSearchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	auto *pData = static_cast<CMapSearchData *>(pUser);
	if(pData->m_pCancel->load())
		return 1;
	if(IsDir)
		return 0;
	const char *pFilename = pInfo->m_pName;
	if(!str_endswith(pFilename, ".map"))
		return 0;
	const int MapNameLength = str_length(pData->m_pMapName);
	if(str_comp_num(pFilename, pData->m_pMapName, MapNameLength) != 0)
		return 0;
	const char *pSuffix = pFilename + MapNameLength;
	if(str_comp(pSuffix, ".map") != 0)
	{
		if(*pSuffix++ != '_')
			return 0;
		const size_t SuffixLength = std::strlen(pSuffix);
		if(SuffixLength < 4)
			return 0;
		const size_t HashLength = SuffixLength - 4;
		if((HashLength != 8 && HashLength != 64) || str_comp(pSuffix + HashLength, ".map") != 0)
			return 0;
		for(size_t i = 0; i < HashLength; ++i)
			if(!std::isxdigit(static_cast<unsigned char>(pSuffix[i])))
				return 0;
	}
	pData->m_vCandidates.push_back({pFilename, pInfo->m_TimeModified});
	return 0;
}

void CBrowserMapPreview::IndexAvailableMaps(const std::shared_ptr<CAvailableMapsJob> &pJob)
{
	CMapIndexScan Downloads{pJob.get(), "downloadedmaps", IStorage::TYPE_SAVE};
	pJob->m_pStorage->ListDirectoryInfo(Downloads.m_StorageType, Downloads.m_Directory.c_str(), MapIndexCallback, &Downloads);
	if(!pJob->m_Cancel.load())
	{
		CMapIndexScan Maps{pJob.get(), "maps", IStorage::TYPE_ALL};
		pJob->m_pStorage->ListDirectoryInfo(Maps.m_StorageType, Maps.m_Directory.c_str(), MapIndexCallback, &Maps);
	}
	pJob->m_FinishedAt.store(time_get(), std::memory_order_relaxed);
	pJob->m_Finished.store(true, std::memory_order_release);
}

int CBrowserMapPreview::MapIndexCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	auto *pScan = static_cast<CMapIndexScan *>(pUser);
	if(pScan->m_pJob->m_Cancel.load())
		return 1;
	const char *pName = pInfo->m_pName;
	if(IsDir)
	{
		if(pScan->m_StorageType == IStorage::TYPE_ALL && pScan->m_Depth < 8 && str_comp(pName, ".") && str_comp(pName, ".."))
		{
			CMapIndexScan Child{pScan->m_pJob, pScan->m_Directory + "/" + pName, pScan->m_StorageType, pScan->m_Depth + 1};
			pScan->m_pJob->m_pStorage->ListDirectoryInfo(Child.m_StorageType, Child.m_Directory.c_str(), MapIndexCallback, &Child);
		}
		return 0;
	}
	if(!str_endswith(pName, ".map"))
		return 0;
	std::string MapName(pName, std::strlen(pName) - 4);
	pScan->m_pJob->m_MapNames.insert(MapName);
	const size_t HashSeparator = MapName.find_last_of('_');
	if(HashSeparator != std::string::npos)
	{
		const size_t HashLength = MapName.size() - HashSeparator - 1;
		if((HashLength == 8 || HashLength == 64) && std::all_of(MapName.begin() + HashSeparator + 1, MapName.end(), [](unsigned char Char) { return std::isxdigit(Char); }))
			MapName.resize(HashSeparator);
	}
	pScan->m_pJob->m_MapNames.insert(MapName);
	std::replace(MapName.begin(), MapName.end(), ' ', '_');
	pScan->m_pJob->m_MapNames.insert(std::move(MapName));
	return 0;
}
