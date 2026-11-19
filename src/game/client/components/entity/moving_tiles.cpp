#include "moving_tiles.h"

#include <base/color.h>
#include <base/dbg.h>
#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/components/envelope_state.h>
#include <game/client/gameclient.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <iterator>

inline static void RotatePoint(const vec2 &Center, vec2 &Point, float Rotation)
{
	const vec2 RelativePos = Point - Center;
	Point.x = RelativePos.x * std::cos(Rotation) - RelativePos.y * std::sin(Rotation) + Center.x;
	Point.y = RelativePos.x * std::sin(Rotation) + RelativePos.y * std::cos(Rotation) + Center.y;
}

void CMovingTiles::Reset()
{
	m_vQuads.clear();
	m_vPosEnvKeys.clear();
	m_vColorEnvKeys.clear();
}

int CMovingTiles::AddEnvKey(std::vector<CMovingTilesEnvKey> &vKeys, int Env, int EnvOffset) const
{
	if(Env < 0)
		return -1;

	for(size_t Index = 0; Index < vKeys.size(); Index++)
	{
		if(vKeys[Index].m_Env == Env && vKeys[Index].m_EnvOffset == EnvOffset)
			return (int)Index;
	}

	CMovingTilesEnvKey Key;
	Key.m_Env = Env;
	Key.m_EnvOffset = EnvOffset;
	vKeys.push_back(Key);
	return (int)vKeys.size() - 1;
}

void CMovingTiles::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState && NewState != IClient::STATE_ONLINE)
		Reset();
}

void CMovingTiles::OnMapLoad()
{
	Reset();

	int GroupsStart, LayersStart, GroupsNum, LayersNum;

	GameClient()->Map()->GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	GameClient()->Map()->GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

	// size_t NumQuadLayers = 0;
	for(int GroupIndex = 0; GroupIndex < GroupsNum; GroupIndex++)
	{
		CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(GameClient()->Map()->GetItem(GroupsStart + GroupIndex));
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(GameClient()->Map()->GetItem(LayersStart + pGroup->m_StartLayer + LayerIndex));

			if(pLayer->m_Type != LAYERTYPE_QUADS)
				continue;

			char aLayerName[30];
			CMapItemLayerQuads *pTilemap = reinterpret_cast<CMapItemLayerQuads *>(pLayer);
			IntsToStr(pTilemap->m_aName, std::size(pTilemap->m_aName), aLayerName, std::size(aLayerName));

			for(size_t Names = 0; Names < std::size(ValidQuadNames); Names++)
			{
				if(!str_comp(ValidQuadNames[Names], aLayerName))
				{
					EQuadType Type = static_cast<EQuadType>(Names);
					if(!m_RenderAbove && (Type == EQuadType::HOOKABLE || Type == EQuadType::UNHOOKABLE))
						continue;
					else if(m_RenderAbove && Type != EQuadType::HOOKABLE && Type != EQuadType::UNHOOKABLE)
						continue;

					// NumQuadLayers++;
					CQuad *pQuads = (CQuad *)GameClient()->Map()->GetDataSwapped(pTilemap->m_Data);
					for(int NumQuads = 0; NumQuads < pTilemap->m_NumQuads; NumQuads++)
					{
						const CQuad *pQuad = &pQuads[NumQuads];

						CRenderQuad QuadData;
						QuadData.m_pQuad = &pQuads[NumQuads];
						QuadData.m_pGroup = pGroup;
						QuadData.m_pLayer = pTilemap;
						QuadData.m_Type = Type;
						for(int j = 0; j < 5; j++)
							QuadData.m_Pos[j] = vec2(fx2f(pQuad->m_aPoints[j].x), fx2f(pQuad->m_aPoints[j].y));

						QuadData.m_PosEnvKey = AddEnvKey(m_vPosEnvKeys, pQuad->m_PosEnv, pQuad->m_PosEnvOffset);
						QuadData.m_ColorEnvKey = AddEnvKey(m_vColorEnvKeys, pQuad->m_ColorEnv, pQuad->m_ColorEnvOffset);

						QuadData.m_LocalAabbMin = QuadData.m_Pos[0];
						QuadData.m_LocalAabbMax = QuadData.m_Pos[0];
						for(int j = 1; j < 4; j++)
						{
							QuadData.m_LocalAabbMin.x = std::min(QuadData.m_LocalAabbMin.x, QuadData.m_Pos[j].x);
							QuadData.m_LocalAabbMin.y = std::min(QuadData.m_LocalAabbMin.y, QuadData.m_Pos[j].y);
							QuadData.m_LocalAabbMax.x = std::max(QuadData.m_LocalAabbMax.x, QuadData.m_Pos[j].x);
							QuadData.m_LocalAabbMax.y = std::max(QuadData.m_LocalAabbMax.y, QuadData.m_Pos[j].y);
						}
						for(int j = 0; j < 4; j++)
							QuadData.m_PivotRadius = std::max(QuadData.m_PivotRadius, distance(QuadData.m_Pos[4], QuadData.m_Pos[j]));

						for(int j = 0; j < 4; j++)
						{
							QuadData.m_aTexcoords[j * 2] = fx2f(pQuad->m_aTexcoords[j].x);
							QuadData.m_aTexcoords[j * 2 + 1] = fx2f(pQuad->m_aTexcoords[j].y);
						}

						// SetColor4 takes top left, top right, bottom left then bottom right
						constexpr float ColorConv = 1.0f / 255.0f;
						static const int s_aCornerOrder[4] = {0, 1, 3, 2};
						for(int j = 0; j < 4; j++)
						{
							const CColor &Color = pQuad->m_aColors[s_aCornerOrder[j]];
							QuadData.m_aCorners[j] = ColorRGBA(Color.r * ColorConv, Color.g * ColorConv, Color.b * ColorConv, Color.a * ColorConv);
						}

						QuadData.m_Invisible = pQuad->m_aColors[0].a <= 0.0f && pQuad->m_aColors[1].a <= 0.0f &&
								       pQuad->m_aColors[2].a <= 0.0f && pQuad->m_aColors[3].a <= 0.0f;

						m_vQuads.push_back(QuadData);
					}
					break;
				}
			}
		}
	}
	// log_info("moving-tiles", "%" PRIzu " valid quadlayer%s with %d quads", NumQuadLayers, NumQuadLayers == 1 ? "" : "s", (int)m_vQuads.size());
	m_EnvEvaluator = CEnvelopeState(GameClient()->Map(), true);
	m_EnvEvaluator.OnInterfacesInit(GameClient());
}

void CMovingTiles::OnRender()
{
	if(g_Config.m_ClOverlayEntities == 0)
		return;
	if(!g_Config.m_ClShowMovingTilesEntities)
		return;
	if(m_vQuads.empty())
		return;

	// `CEnvelopeState` caches the envelope time, so it has to be refreshed once per frame
	m_EnvEvaluator.Update();

	// and so the envelopes behind the quads have to be walked again for it, but only once each
	for(CMovingTilesEnvKey &Key : m_vPosEnvKeys)
		Key.m_Evaluated = false;
	for(CMovingTilesEnvKey &Key : m_vColorEnvKeys)
		Key.m_Evaluated = false;

	const float Alpha = g_Config.m_ClOverlayEntities * 0.01f;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_Zoom;

	auto PosEnv = [&](int KeyIndex, vec2 &Offset, float &Rotation) {
		if(KeyIndex < 0)
			return;

		CMovingTilesEnvKey &Key = m_vPosEnvKeys[KeyIndex];
		if(!Key.m_Evaluated)
		{
			ColorRGBA Position(0.0f, 0.0f, 0.0f, 0.0f);
			m_EnvEvaluator.EnvelopeEval(Key.m_EnvOffset, Key.m_Env, Position, 3);
			Key.m_Offset = vec2(Position.r, Position.g);
			Key.m_Rotation = Position.b / 180.0f * pi;
			Key.m_Evaluated = true;
		}

		Offset = Key.m_Offset;
		Rotation = Key.m_Rotation;
	};

	auto ColorEnv = [&](int KeyIndex) -> ColorRGBA {
		if(KeyIndex < 0)
			return ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

		CMovingTilesEnvKey &Key = m_vColorEnvKeys[KeyIndex];
		if(!Key.m_Evaluated)
		{
			Key.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			m_EnvEvaluator.EnvelopeEval(Key.m_EnvOffset, Key.m_Env, Key.m_Color, 4);
			Key.m_Evaluated = true;
		}

		return Key.m_Color;
	};

	CScreenRect WorldRect(0.0f, 0.0f, 0.0f, 0.0f);

	auto ApplyGroupState = [&](const CMapItemGroup *pGroup) -> bool {
		Graphics()->ClipDisable();

		if(!pGroup)
			return false;

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			Graphics()->MapScreenToInterface(Center.x, Center.y, Zoom);

			const CScreenRect ScreenRect = Graphics()->GetScreen();

			const float ScreenWidth = ScreenRect.Width();
			const float ScreenHeight = ScreenRect.Height();
			const float Left = pGroup->m_ClipX - ScreenRect.m_TopLeft.x;
			const float Top = pGroup->m_ClipY - ScreenRect.m_TopLeft.y;
			const float Right = pGroup->m_ClipX + pGroup->m_ClipW - ScreenRect.m_TopLeft.x;
			const float Bottom = pGroup->m_ClipY + pGroup->m_ClipH - ScreenRect.m_TopLeft.y;

			if(Right < 0.0f || Left > ScreenWidth || Bottom < 0.0f || Top > ScreenHeight)
				return false;

			const int ClipX = (int)std::round(Left * Graphics()->ScreenWidth() / ScreenWidth);
			const int ClipY = (int)std::round(Top * Graphics()->ScreenHeight() / ScreenHeight);
			const int ClipW = (int)std::round((Right - Left) * Graphics()->ScreenWidth() / ScreenWidth);
			const int ClipH = (int)std::round((Bottom - Top) * Graphics()->ScreenHeight() / ScreenHeight);

			Graphics()->ClipEnable(ClipX, ClipY, ClipW, ClipH);
		}

		const int ParallaxZoom = std::clamp(std::max(pGroup->m_ParallaxX, pGroup->m_ParallaxY), 0, 100);
		WorldRect = Graphics()->MapScreenToWorld(
			Center.x, Center.y,
			pGroup->m_ParallaxX, pGroup->m_ParallaxY, (float)ParallaxZoom,
			pGroup->m_OffsetX, pGroup->m_OffsetY,
			Graphics()->ScreenAspect(), Zoom);
		Graphics()->MapScreen(WorldRect);

		return true;
	};

	auto RenderPass = [&]() {
		size_t QuadStart = 0;
		while(QuadStart < m_vQuads.size())
		{
			const CMapItemGroup *pGroup = m_vQuads[QuadStart].m_pGroup;
			const CMapItemLayerQuads *pLayer = m_vQuads[QuadStart].m_pLayer;
			if(!pLayer)
			{
				QuadStart++;
				continue;
			}

			size_t QuadEnd = QuadStart + 1;
			while(QuadEnd < m_vQuads.size() &&
				m_vQuads[QuadEnd].m_pGroup == pGroup &&
				m_vQuads[QuadEnd].m_pLayer == pLayer)
			{
				QuadEnd++;
			}

			if(!ApplyGroupState(pGroup))
			{
				QuadStart = QuadEnd;
				continue;
			}

			if(g_Config.m_ClShowMovingTilesEntities != 2 && pLayer->m_Image >= 0 && pLayer->m_Image < GameClient()->m_MapImages.Num())
				Graphics()->TextureSet(GameClient()->m_MapImages.Get(pLayer->m_Image));
			else
				Graphics()->TextureClear();

			// one layer is one zone, so the colour it stands for is the same for all of its quads
			const ColorRGBA EntityColor = GameClient()->m_MapImages.m_aTilesDominantColor[(int)m_vQuads[QuadStart].m_Type];
			const bool ForceEntityColor = g_Config.m_ClShowMovingTilesEntities > 1;
			const bool WithColorEnv = g_Config.m_ClShowMovingTilesEntities != 2;

			Graphics()->TrianglesBegin();

			for(size_t QuadIndex = QuadStart; QuadIndex < QuadEnd; QuadIndex++)
			{
				const CRenderQuad &QuadData = m_vQuads[QuadIndex];
				if(!QuadData.m_pQuad)
					continue;

				vec2 Offset = vec2(0.0f, 0.0f);
				float Rotation = 0.0f;
				PosEnv(QuadData.m_PosEnvKey, Offset, Rotation);
				Rotation += QuadData.m_Angle;

				/*
				 * A layer of thousands of these covers far more than the screen, and everything
				 * below costs the same whether the quad lands on it or not, so the ones that do
				 * not are dropped here. A quad that turns is bounded by its own pivot circle,
				 * since where its corners end up depends on the angle.
				 */
				vec2 QuadMin, QuadMax;
				if(Rotation == 0.0f)
				{
					QuadMin = QuadData.m_LocalAabbMin + Offset;
					QuadMax = QuadData.m_LocalAabbMax + Offset;
				}
				else
				{
					const vec2 Pivot = QuadData.m_Pos[4] + Offset;
					QuadMin = Pivot - vec2(QuadData.m_PivotRadius, QuadData.m_PivotRadius);
					QuadMax = Pivot + vec2(QuadData.m_PivotRadius, QuadData.m_PivotRadius);
				}

				if(QuadMax.x < WorldRect.m_TopLeft.x || QuadMin.x > WorldRect.m_BottomRight.x ||
					QuadMax.y < WorldRect.m_TopLeft.y || QuadMin.y > WorldRect.m_BottomRight.y)
					continue;

				ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
				if(ForceEntityColor || QuadData.m_Invisible)
					Color = EntityColor;

				if(WithColorEnv)
				{
					const ColorRGBA EnvColor = ColorEnv(QuadData.m_ColorEnvKey);
					Color.r *= EnvColor.r;
					Color.g *= EnvColor.g;
					Color.b *= EnvColor.b;
					Color.a *= EnvColor.a;
				}

				Color.a *= Alpha;

				Graphics()->QuadsSetSubsetFree(
					QuadData.m_aTexcoords[0], QuadData.m_aTexcoords[1],
					QuadData.m_aTexcoords[2], QuadData.m_aTexcoords[3],
					QuadData.m_aTexcoords[4], QuadData.m_aTexcoords[5],
					QuadData.m_aTexcoords[6], QuadData.m_aTexcoords[7]);

				if(ForceEntityColor || QuadData.m_Invisible)
				{
					Graphics()->SetColor4(Color, Color, Color, Color);
				}
				else
				{
					const ColorRGBA *pCorners = QuadData.m_aCorners;
					Graphics()->SetColor4(
						ColorRGBA(pCorners[0].r * Color.r, pCorners[0].g * Color.g, pCorners[0].b * Color.b, pCorners[0].a * Color.a),
						ColorRGBA(pCorners[1].r * Color.r, pCorners[1].g * Color.g, pCorners[1].b * Color.b, pCorners[1].a * Color.a),
						ColorRGBA(pCorners[2].r * Color.r, pCorners[2].g * Color.g, pCorners[2].b * Color.b, pCorners[2].a * Color.a),
						ColorRGBA(pCorners[3].r * Color.r, pCorners[3].g * Color.g, pCorners[3].b * Color.b, pCorners[3].a * Color.a));
				}

				vec2 aPoints[4] = {
					QuadData.m_Pos[0],
					QuadData.m_Pos[1],
					QuadData.m_Pos[2],
					QuadData.m_Pos[3],
				};

				if(Rotation != 0.0f)
				{
					for(vec2 &Point : aPoints)
						RotatePoint(QuadData.m_Pos[4], Point, Rotation);
				}

				const IGraphics::CFreeformItem Freeform(
					aPoints[0] + Offset,
					aPoints[1] + Offset,
					aPoints[2] + Offset,
					aPoints[3] + Offset);
				Graphics()->QuadsDrawFreeform(&Freeform, 1);
			}

			Graphics()->TrianglesEnd();
			QuadStart = QuadEnd;
		}
	};

	/*
	 * Nothing here is ever opaque, so the pass that used to walk every quad for
	 * LAYERRENDERFLAG_OPAQUE only ever threw its work away before drawing anything.
	 */
	Graphics()->BlendNormal();
	RenderPass();

	Graphics()->ClipDisable();
}
