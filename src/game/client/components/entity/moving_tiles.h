#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MOVING_TILES_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MOVING_TILES_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>
#include <game/client/components/envelope_state.h>
#include <game/mapitems.h>

#include <vector>

class CQuad;
class CMapItemGroup;
class CMapItemLayerQuads;

enum class EMovingTileRenderType
{
	BELOW,
	ABOVE
};

enum class EQuadType
{
	NONE = -1,
	FREEZE,
	UNFREEZE,
	DEATH,
	STOPA,
	CFRM,
	HOOKABLE,
	UNHOOKABLE,
	NUM
};

static constexpr char ValidQuadNames[][30] = {
	"QFr",
	"QUnFr",
	"QDeath",
	"QStopa",
	"QCfrm",
	"QHook",
	"QUnHook"};

/*
 * One envelope read at one offset. A quad layer points whole runs of quads at the same pair, so
 * the envelope is walked once per pair per frame and every quad behind it reads the answer, which
 * is what keeps a layer of thousands off the envelope code, see CMovingTiles::OnRender.
 */
class CMovingTilesEnvKey
{
public:
	int m_Env = -1;
	int m_EnvOffset = 0;
	bool m_Evaluated = false; // whether this frame has walked it yet

	vec2 m_Offset = vec2(0.0f, 0.0f);
	float m_Rotation = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
};

class CRenderQuad
{
public:
	CQuad *m_pQuad = nullptr;
	CMapItemGroup *m_pGroup = nullptr;
	CMapItemLayerQuads *m_pLayer = nullptr;
	EQuadType m_Type = EQuadType::NONE;
	vec2 m_Pos[5] = {vec2(0, 0)}; // 4 corners + center
	float m_Angle = 0.0f;

	// Settled once at map load, so that a frame only ever reads what follows
	int m_PosEnvKey = -1;
	int m_ColorEnvKey = -1;
	// The corners' bounds before any envelope moves them, for the on-screen test
	vec2 m_LocalAabbMin = vec2(0, 0);
	vec2 m_LocalAabbMax = vec2(0, 0);
	// The furthest corner from the pivot, which bounds the quad at any angle it may turn to
	float m_PivotRadius = 0.0f;
	float m_aTexcoords[8] = {0.0f};
	// The map's own corner colours, already out of bytes and in the order SetColor4 takes them
	ColorRGBA m_aCorners[4] = {ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)};
	bool m_Invisible = false;
};

class CMovingTiles : public CComponent
{
	void Reset();
	// The pair this quad reads from, adding it to the table when it is a new one
	int AddEnvKey(std::vector<CMovingTilesEnvKey> &vKeys, int Env, int EnvOffset) const;

	std::vector<CRenderQuad> m_vQuads;
	std::vector<CMovingTilesEnvKey> m_vPosEnvKeys;
	std::vector<CMovingTilesEnvKey> m_vColorEnvKeys;

	bool m_RenderAbove; // solids are rendered above tilemap, the rest below
	CEnvelopeState m_EnvEvaluator;

public:
	CMovingTiles(bool Above) :
		m_RenderAbove(Above) {}

	void OnStateChange(int NewState, int OldState) override;
	void OnMapLoad() override;
	void OnRender() override;

	int Sizeof() const override { return sizeof(*this); }
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_MOVING_TILES_H
