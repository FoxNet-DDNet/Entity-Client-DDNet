#ifndef GAME_CLIENT_COMPONENTS_ENTITY_BROWSER_MAP_PREVIEW_H
#define GAME_CLIENT_COMPONENTS_ENTITY_BROWSER_MAP_PREVIEW_H

#include <base/color.h>
#include <base/types.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/image.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>
#include <game/mapitems.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

class IMap;
class IMapImages;
class CPreviewRenderLayerTile;
class CRenderMap;

class CMapPreviewData
{
public:
	struct CImage
	{
		std::string m_Path;
		int m_Width = 0;
		int m_Height = 0;
		bool m_UsedByTiles = false;
		CImageInfo m_ImageInfo;
	};

	struct CLayer
	{
		enum class EType
		{
			TILES,
			QUADS
		} m_Type;
		int m_Image = -1;
		int m_Width = 0;
		int m_Height = 0;
		ColorRGBA m_Color{1, 1, 1, 1};
		int m_ColorEnv = -1;
		int m_ColorEnvOffset = 0;
		std::vector<CTile> m_vTiles;
		std::vector<CQuad> m_vQuads;
	};
	struct CEnvelope
	{
		int m_Channels = 0;
		bool m_HasBezier = false;
		std::vector<CEnvPoint_runtime> m_vPoints;
	};

	struct CGroup
	{
		int m_OffsetX = 0;
		int m_OffsetY = 0;
		int m_ParallaxX = 100;
		int m_ParallaxY = 100;
		bool m_UseClipping = false;
		int m_ClipX = 0;
		int m_ClipY = 0;
		int m_ClipW = 0;
		int m_ClipH = 0;
		std::vector<CLayer> m_vLayers;
	};

	vec2 m_SpawnPosition{0, 0};
	std::vector<CImage> m_vImages;
	std::vector<CGroup> m_vGroups;
	std::vector<CEnvelope> m_vEnvelopes;

	static bool Extract(IMap *pMap, IStorage *pStorage, const std::atomic_bool &Cancel, CMapPreviewData &Out);
	void EvaluateEnvelope(int TimeOffsetMillis, int EnvelopeIndex, ColorRGBA &Result, size_t Channels, std::chrono::nanoseconds Time) const;
};

class CBrowserMapPreview : public CComponent
{
	enum class EState
	{
		LOADING,
		MISSING,
		INVALID,
		READY
	};
	struct CTileLayerCache
	{
		std::shared_ptr<CMapItemLayerTilemap> m_pMapItem;
		std::shared_ptr<CPreviewRenderLayerTile> m_pRenderLayer;
	};
	struct CMapCandidate
	{
		std::string m_Filename;
		time_t m_Modified = 0;
	};
	struct CJob
	{
		std::string m_MapName;
		std::string m_PreferredFilename;
		unsigned m_MapCrc = 0;
		IStorage *m_pStorage = nullptr;
		std::atomic_bool m_Cancel{false};
		std::atomic_bool m_Finished{false};
		std::atomic<EState> m_State{EState::LOADING};
		std::unique_ptr<CMapPreviewData> m_pData;
		char m_aSelectedFilename[IO_MAX_PATH_LENGTH]{};
		std::vector<CMapCandidate> m_vCandidates;
	};
	struct CWorker
	{
		std::shared_ptr<CJob> m_pJob;
		std::thread m_Thread;
	};
	struct CMapSearchData
	{
		const char *m_pMapName;
		const std::atomic_bool *m_pCancel;
		std::vector<CMapCandidate> m_vCandidates;
	};
	struct CAvailableMapsJob
	{
		IStorage *m_pStorage = nullptr;
		std::atomic_bool m_Cancel{false};
		std::atomic_bool m_Finished{false};
		std::atomic<int64_t> m_FinishedAt{0};
		std::set<std::string> m_MapNames;
	};
	struct CMapIndexScan
	{
		CAvailableMapsJob *m_pJob;
		std::string m_Directory;
		int m_StorageType;
		int m_Depth = 0;
	};

	CWorker m_CurrentWorker;
	std::vector<CWorker> m_vRetiredWorkers;
	std::shared_ptr<CAvailableMapsJob> m_pAvailableMapsJob;
	std::thread m_AvailableMapsThread;
	std::unique_ptr<CMapPreviewData> m_pData;
	std::vector<IGraphics::CTextureHandle> m_vTextures;
	std::shared_ptr<IMapImages> m_pMapImages;
	std::shared_ptr<CRenderMap> m_pRenderMap;
	size_t m_NextImageToUpload = 0;
	std::vector<std::vector<CTileLayerCache>> m_vTileCaches;
	bool m_TileCacheBuiltOnce = false;
	size_t m_NextInitialTileGroup = 0;
	size_t m_NextInitialTileLayer = 0;
	std::string m_MapName;
	std::string m_CommunityId;
	unsigned m_MapCrc = 0;
	std::map<std::string, std::string> m_Overrides;
	bool m_LegacyOverridesLoaded = false;
	vec2 m_CameraCenter{0, 0};
	vec2 m_LastDragMousePos{0, 0};
	float m_Zoom = 1.0f;
	bool m_Dragging = false;

	static void MapLoadThread(const std::shared_ptr<CJob> &pJob);
	static void ConAddMapPreview(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
	static int MapSearchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);
	static void IndexAvailableMaps(const std::shared_ptr<CAvailableMapsJob> &pJob);
	static int MapIndexCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);
	void ReapWorkers();
	void ClearPreview();
	void ClearTileCaches();
	void InitTileLayerCache(const CMapPreviewData::CLayer &Layer, CTileLayerCache &Cache, size_t GroupIndex, size_t LayerIndex);
	void UploadNextImage();
	bool RenderMap(const CUIRect &Rect);
	std::string OverrideKey() const;
	void LoadLegacyOverrides();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void LoadMap(const char *pMapName, const char *pCommunityId, unsigned MapCrc);
	bool HasLoadedMap() const;
	bool HasAvailableMap(const char *pMapName);
	bool IsDragging() const { return m_Dragging; }
	bool HasAlternativeMap() const;
	bool SelectNextMapVersion();
	size_t SelectedVersionIndex() const;
	size_t MapVersionCount() const;
	void OnShutdown() override;
	void Render(CUIRect *pRect);
};

#endif
