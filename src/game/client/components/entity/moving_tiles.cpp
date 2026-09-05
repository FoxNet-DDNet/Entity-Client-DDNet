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
						CRenderQuad QuadData;
						QuadData.m_pQuad = &pQuads[NumQuads];
						QuadData.m_pGroup = pGroup;
						QuadData.m_pLayer = pTilemap;
						QuadData.m_Type = Type;
						for(int j = 0; j < 5; j++)
							QuadData.m_Pos[j] = vec2(fx2f(QuadData.m_pQuad->m_aPoints[j].x), fx2f(QuadData.m_pQuad->m_aPoints[j].y));
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

	const float Alpha = g_Config.m_ClOverlayEntities * 0.01f;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_Zoom;

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
		const CScreenRect WorldRect = Graphics()->MapScreenToWorld(
			Center.x, Center.y,
			pGroup->m_ParallaxX, pGroup->m_ParallaxY, (float)ParallaxZoom,
			pGroup->m_OffsetX, pGroup->m_OffsetY,
			Graphics()->ScreenAspect(), Zoom);
		Graphics()->MapScreen(WorldRect);

		return true;
	};

	auto RenderPass = [&](int RenderFlags) {
		constexpr float ColorConv = 1.0f / 255.0f;

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

			Graphics()->TrianglesBegin();

			for(size_t QuadIndex = QuadStart; QuadIndex < QuadEnd; QuadIndex++)
			{
				const CRenderQuad &QuadData = m_vQuads[QuadIndex];
				const CQuad *pQuad = QuadData.m_pQuad;
				if(!pQuad)
					continue;

				bool Invisible = pQuad->m_aColors[0].a <= 0.0f && pQuad->m_aColors[1].a <= 0.0f && pQuad->m_aColors[2].a <= 0.0f && pQuad->m_aColors[3].a <= 0.0f;

				ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
				ColorRGBA EntityColor = GameClient()->m_MapImages.m_aTilesDominantColor[(int)QuadData.m_Type];
				if(g_Config.m_ClShowMovingTilesEntities > 1 || Invisible)
					Color = EntityColor;

				ColorRGBA EnvColor(1.0f, 1.0f, 1.0f, 1.0f);
				m_EnvEvaluator.EnvelopeEval(pQuad->m_ColorEnvOffset, pQuad->m_ColorEnv, EnvColor, 4);

				if(g_Config.m_ClShowMovingTilesEntities != 2)
				{
					Color.r *= EnvColor.r;
					Color.g *= EnvColor.g;
					Color.b *= EnvColor.b;
					Color.a *= EnvColor.a;
				}

				bool Opaque = false;
				if(Opaque && !(RenderFlags & LAYERRENDERFLAG_OPAQUE))
					continue;
				if(!Opaque && !(RenderFlags & LAYERRENDERFLAG_TRANSPARENT))
					continue;

				Graphics()->QuadsSetSubsetFree(
					fx2f(pQuad->m_aTexcoords[0].x), fx2f(pQuad->m_aTexcoords[0].y),
					fx2f(pQuad->m_aTexcoords[1].x), fx2f(pQuad->m_aTexcoords[1].y),
					fx2f(pQuad->m_aTexcoords[2].x), fx2f(pQuad->m_aTexcoords[2].y),
					fx2f(pQuad->m_aTexcoords[3].x), fx2f(pQuad->m_aTexcoords[3].y));

				ColorRGBA Position(0.0f, 0.0f, 0.0f, 0.0f);
				m_EnvEvaluator.EnvelopeEval(pQuad->m_PosEnvOffset, pQuad->m_PosEnv, Position, 3);

				const vec2 Offset(Position.r, Position.g);
				const float Rotation = Position.b / 180.0f * pi + QuadData.m_Angle;

				Color.a *= Alpha;

				if(g_Config.m_ClShowMovingTilesEntities > 1 || Invisible)
				{
					Graphics()->SetColor4(Color, Color, Color, Color);
				}
				else
				{
					Graphics()->SetColor4(
						ColorRGBA(pQuad->m_aColors[0].r * ColorConv * Color.r, pQuad->m_aColors[0].g * ColorConv * Color.g, pQuad->m_aColors[0].b * ColorConv * Color.b, pQuad->m_aColors[0].a * ColorConv * Color.a),
						ColorRGBA(pQuad->m_aColors[1].r * ColorConv * Color.r, pQuad->m_aColors[1].g * ColorConv * Color.g, pQuad->m_aColors[1].b * ColorConv * Color.b, pQuad->m_aColors[1].a * ColorConv * Color.a),
						ColorRGBA(pQuad->m_aColors[3].r * ColorConv * Color.r, pQuad->m_aColors[3].g * ColorConv * Color.g, pQuad->m_aColors[3].b * ColorConv * Color.b, pQuad->m_aColors[3].a * ColorConv * Color.a),
						ColorRGBA(pQuad->m_aColors[2].r * ColorConv * Color.r, pQuad->m_aColors[2].g * ColorConv * Color.g, pQuad->m_aColors[2].b * ColorConv * Color.b, pQuad->m_aColors[2].a * ColorConv * Color.a));
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

	Graphics()->BlendNone();
	RenderPass(LAYERRENDERFLAG_OPAQUE);

	Graphics()->BlendNormal();
	RenderPass(LAYERRENDERFLAG_TRANSPARENT);

	Graphics()->ClipDisable();
}
