// EClient
#include "hud_layout.h"

#include <base/system.h>

#include <engine/config.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

/**
 * Which screen edge an element's nominal x position is measured from.
 *
 * The HUD's virtual screen is always 300 units tall but its width follows the aspect ratio, so
 * right-aligned and centered elements cannot have a constant x.
 */
enum class EHudOriginX
{
	FROM_LEFT,
	FROM_RIGHT,
	FROM_CENTER,
};

class CHudElementDef
{
public:
	// The name shown in the editor, as against m_pName which is what the console and the config
	// file address it by and so has to stay a stable identifier
	const char *m_pDisplayName;

	// The element this row describes. The table is indexed by EHudElement, so a row in the wrong
	// place silently hands every element its neighbour's name, position and push order. The
	// constructor checks each row against its own index rather than leaving that to be noticed.
	EHudElement m_Element;
	const char *m_pName;
	EHudAnchor m_DefaultAnchor;
	EHudOriginX m_OriginX;
	/**
	 * Position of the rect's top left corner. For FROM_RIGHT, x is the distance from the right
	 * screen edge to the rect's left edge. For FROM_CENTER, x nudges the rect off center.
	 */
	vec2 m_Pos;
	vec2 m_Size;

	// The push order that replaces the offsets these elements used to subtract by hand for
	// everything below them
	int m_PushPriority;
	EHudPushDirection m_PushDirection;
	float m_PushGap;

	EHudElement m_AttachTarget;
	EHudPushDirection m_AttachSide;
	float m_AttachGap;

	// A measure only element is reported so others can be pushed by it, but is never transformed
	bool m_Movable;

	// What switches this element on and off, where a single setting does
	int *m_pEnabled;

	// What to call that setting, where "Enabled" would leave someone guessing what it does
	const char *m_pEnabledLabel;

	// A second setting, for an element that two of them govern
	int *m_pEnabled2;
	const char *m_pEnabledLabel2;

	// A third, shown only while the second is on, for a setting that only means anything then
	int *m_pEnabled3;
	const char *m_pEnabledLabel3;
};

// Nothing to attach to
static constexpr EHudElement ATTACH_NONE = EHudElement::NUM_HUD_ELEMENTS;

/**
 * The nominal rects that the upstream render functions draw into.
 *
 * These are read off the hardcoded constants in hud.cpp. Several elements have a height or a
 * width that changes with the snapshot or with config (the movement info box grows per enabled
 * row, the score box widens with the score text), and some are pushed around by whatever else is
 * currently visible. Those rows are approximations: they are only used as the pivot for scaling,
 * so being slightly off makes an element grow from a slightly wrong point, nothing worse.
 *
 * Elements report their real rect while they render, so these values are only used before an
 * element has drawn for the first time, or while it is switched off.
 */
static const CHudElementDef gs_aHudElements[] = {
	// hud.cpp PrepareAmmoHealthAndArmorQuads: x = 5, y = 5, ten 12-wide slots, ammo row at y + 24
	{"Health and ammo", EHudElement::HEALTH_AMMO, "health_ammo", EHudAnchor::TOP_LEFT, EHudOriginX::FROM_LEFT, vec2(5.0f, 5.0f), vec2(120.0f, 36.0f), 90, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudHealthAmmo, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderPlayerState: starts under the ammo row, one 12 unit row per populated group
	{"Player state", EHudElement::PLAYER_STATE, "player_state", EHudAnchor::TOP_LEFT, EHudOriginX::FROM_LEFT, vec2(5.0f, 8.5f), vec2(120.0f, 69.0f), 85, EHudPushDirection::DOWN, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudDDRace, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderScoreHud: StartY = 229, height 56, right aligned, width follows the score text
	{"Score", EHudElement::SCORE, "score", EHudAnchor::TOP_RIGHT, EHudOriginX::FROM_RIGHT, vec2(46.0f, 229.0f), vec2(46.0f, 56.0f), 90, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudScore, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderMovementInformation: BoxWidth = 62, bottom at 281, or 225 with the score shown
	{"Movement info", EHudElement::MOVEMENT_INFO, "movement_info", EHudAnchor::BOTTOM_RIGHT, EHudOriginX::FROM_RIGHT, vec2(62.0f, 173.0f), vec2(62.0f, 108.0f), 60, EHudPushDirection::UP, 4.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderSpectatorCount: BoxHeight = 14, pushed up by everything below it
	{"Spectator count", EHudElement::SPECTATOR_COUNT, "spectator_count", EHudAnchor::BOTTOM_RIGHT, EHudOriginX::FROM_RIGHT, vec2(30.0f, 267.0f), vec2(30.0f, 14.0f), 40, EHudPushDirection::UP, 4.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudSpectatorCount, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderDummyActions: BoxWidth = 16, BoxHeight = 29, bottom at 281
	{"Dummy actions", EHudElement::DUMMY_ACTIONS, "dummy_actions", EHudAnchor::BOTTOM_RIGHT, EHudOriginX::FROM_RIGHT, vec2(16.0f, 252.0f), vec2(16.0f, 29.0f), 50, EHudPushDirection::UP, 4.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudDummyActions, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderFps: one 12 unit line in the top right corner
	{"FPS", EHudElement::FPS, "fps", EHudAnchor::TOP_RIGHT, EHudOriginX::FROM_RIGHT, vec2(48.0f, 5.0f), vec2(38.0f, 12.0f), 90, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowfps, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderPrediction: pushed under the fps counter, so on its own it goes where that would
	{"Prediction", EHudElement::PREDICTION, "prediction", EHudAnchor::TOP_RIGHT, EHudOriginX::FROM_RIGHT, vec2(34.0f, 5.0f), vec2(24.0f, 12.0f), 70, EHudPushDirection::DOWN, 3.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowpred, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderRecord: two 6-unit lines at y = 117 and y = 82
	{"Records", EHudElement::RECORD, "record", EHudAnchor::TOP_LEFT, EHudOriginX::FROM_LEFT, vec2(0.0f, 110.0f), vec2(84.0f, 13.0f), 80, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowRecord, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderSpectatorHud: 180 wide, 15 tall, flush with the bottom right corner
	{"Spectator HUD", EHudElement::SPECTATOR_HUD, "spectator_hud", EHudAnchor::BOTTOM_RIGHT, EHudOriginX::FROM_RIGHT, vec2(180.0f, 285.0f), vec2(180.0f, 15.0f), 70, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderWarmupTimer: centered, title at y = 50 and the time at y = 75, font size 20
	{"Warmup timer", EHudElement::WARMUP_TIMER, "warmup_timer", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(0.0f, 50.0f), vec2(120.0f, 45.0f), 70, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderIsland. It hover tests and clips itself, so those cross into base coordinates
	// through CHudLayout::ToBaseSpace and ToElementSpace rather than riding the screen mapping.
	{"Race timer", EHudElement::MEDIA_ISLAND, "race_timer", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(0.0f, 1.0f), vec2(60.0f, 16.0f), 100, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowhudTimer, "Race timer", &g_Config.m_ClMediaIsland, "Media island", &g_Config.m_ClShowLocalTimeAlways, "Local time"},
	// hud.cpp FreezeHelpers: a row of tees along the top. Its resting position depends on the
	// aspect ratio and the configured tee size, so it reports its own through ReportNominalRect
	// and these numbers are only ever seen before the first frame.
	{"Frozen tees", EHudElement::FROZEN_TEES, "frozen_tees", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(75.5f, 0.0f), vec2(90.0f, 15.0f), 30, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowFrozenHud, nullptr, nullptr, nullptr, nullptr, nullptr},
	// infomessages.cpp CInfoMessages::OnRender. Drawn in its own 1800 tall space, so its rect is
	// reported in HUD units and its transform is applied through that space instead.
	{"Kill feed", EHudElement::KILL_FEED, "kill_feed", EHudAnchor::TOP_RIGHT, EHudOriginX::FROM_RIGHT, vec2(93.3f, 5.0f), vec2(91.7f, 23.0f), 65, EHudPushDirection::DOWN, 2.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowKillMessages, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp FreezeHelpers: the frozen count, centred near the top
	{"Frozen count", EHudElement::FROZEN_TEXT, "frozen_text", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(0.0f, 12.0f), vec2(30.0f, 8.0f), 60, EHudPushDirection::DOWN, 2.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowFrozenText, nullptr, nullptr, nullptr, nullptr, nullptr},
	// voting.cpp CVoting::Render: a fixed panel against the left edge
	{"Vote", EHudElement::VOTING, "voting", EHudAnchor::CENTER_LEFT, EHudOriginX::FROM_LEFT, vec2(0.0f, 60.0f), vec2(120.0f, 38.0f), 75, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
	// chat.cpp CChat::OnRender: grows upward from the bottom left. It hit tests its own messages
	// and clips its input line, so both cross into base coordinates the way the island's do.
	{"Chat", EHudElement::CHAT, "chat", EHudAnchor::BOTTOM_LEFT, EHudOriginX::FROM_LEFT, vec2(5.0f, 200.0f), vec2(200.0f, 90.0f), 55, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowChat, nullptr, nullptr, nullptr, nullptr, nullptr},
	// hud.cpp RenderLocalTime. Only its own element while the media island is switched off, since
	// the island draws the clock inside itself and reports it as part of race_timer instead.
	{"Local time", EHudElement::LOCAL_TIME, "local_time", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(-40.0f, 0.0f), vec2(22.0f, 12.5f), 80, EHudPushDirection::NONE, 0.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClShowLocalTimeAlways, "Always show", nullptr, nullptr, nullptr, nullptr},
	// hud.cpp FreezeHelpers: the last one alive shout, near the top left
	{"Last alive notice", EHudElement::NOTIFY_LAST, "notify_last", EHudAnchor::TOP_LEFT, EHudOriginX::FROM_LEFT, vec2(170.0f, 4.0f), vec2(90.0f, 14.0f), 55, EHudPushDirection::RIGHT, 2.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClNotifyWhenLast, nullptr, nullptr, nullptr, nullptr, nullptr},
	// local_practice.cpp CLocalPractice::RenderMovedAlert: centred under the top of the screen, and
	// only ever present while practicing. It measures its own text, so this is the size before it
	// has said anything.
	{"Local practice notice", EHudElement::PRACTICE_ALERT, "local_practice_alert", EHudAnchor::TOP_CENTER, EHudOriginX::FROM_CENTER, vec2(-45.0f, 30.0f), vec2(90.0f, 10.0f), 45, EHudPushDirection::DOWN, 2.0f, ATTACH_NONE, EHudPushDirection::NONE, 0.0f, true, &g_Config.m_ClLocalPracticeAlert, nullptr, nullptr, nullptr, nullptr, nullptr},
};

static_assert(std::size(gs_aHudElements) == (size_t)EHudElement::NUM_HUD_ELEMENTS,
	"gs_aHudElements is out of sync with EHudElement");

static const char *gs_apPushDirectionNames[] = {
	"none",
	"up",
	"down",
	"left",
	"right",
};

static_assert(std::size(gs_apPushDirectionNames) == (size_t)EHudPushDirection::NUM_HUD_PUSH_DIRECTIONS,
	"gs_apPushDirectionNames is out of sync with EHudPushDirection");

static const char *gs_apAnchorNames[] = {
	"topleft",
	"top",
	"topright",
	"left",
	"center",
	"right",
	"bottomleft",
	"bottom",
	"bottomright",
};

static_assert(std::size(gs_apAnchorNames) == (size_t)EHudAnchor::NUM_HUD_ANCHORS,
	"gs_apAnchorNames is out of sync with EHudAnchor");

/**
 * Scaling by anything near zero would blow up the virtual screen window, and a mistyped console
 * value should not be able to wedge the HUD.
 */
// How many frames a reported rect stays valid for. Two covers an element that skips a frame,
// while still dropping the rect promptly once the element stops rendering.
static constexpr int MEASUREMENT_MAX_AGE = 2;

// How close two edges have to be before they count as touching, in HUD units
static constexpr float CORNER_TOUCH_DISTANCE = 0.5f;

// How much of an element has to lie across its neighbour before it counts as being on top of it
// rather than beside it, as a fraction of its own size on that axis
static constexpr float PUSH_CROSS_FRACTION = 0.5f;

// Placements are held to these steps, so that what the editor shows is exactly what is stored
static constexpr float OFFSET_STEP = 0.1f;
static constexpr float SCALE_STEP = 0.01f;

static constexpr float MIN_SCALE = 0.35f;
static constexpr float MAX_SCALE = 10.0f;

CHudLayout::CHudLayout()
{
	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		dbg_assert(gs_aHudElements[i].m_Element == (EHudElement)i, "gs_aHudElements is out of order");
		m_aPlacements[i].m_Anchor = gs_aHudElements[i].m_DefaultAnchor;
		m_aPlacements[i].m_PushPriority = gs_aHudElements[i].m_PushPriority;
		m_aPlacements[i].m_PushDirection = gs_aHudElements[i].m_PushDirection;
		m_aPlacements[i].m_PushGap = gs_aHudElements[i].m_PushGap;
		m_aPlacements[i].m_AttachTarget = gs_aHudElements[i].m_AttachTarget;
		m_aPlacements[i].m_AttachSide = gs_aHudElements[i].m_AttachSide;
		m_aPlacements[i].m_AttachGap = gs_aHudElements[i].m_AttachGap;
		m_aPushOffsets[i] = vec2(0.0f, 0.0f);
	}
}

void CHudLayout::OnBaseScreenSet(vec2 BaseSize)
{
	m_BaseSize = BaseSize;
	m_Frame++;
	SolvePushes();
}

void CHudLayout::SolvePushes()
{
	// Highest priority first, ties broken by declaration order so the result never depends on
	// iteration order
	int aOrder[(int)EHudElement::NUM_HUD_ELEMENTS];
	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
		aOrder[i] = i;
	std::stable_sort(std::begin(aOrder), std::end(aOrder), [this](int a, int b) {
		return m_aPlacements[a].m_PushPriority > m_aPlacements[b].m_PushPriority;
	});

	int NumPlaced = 0;
	int aPlaced[(int)EHudElement::NUM_HUD_ELEMENTS];

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		const int Index = aOrder[i];
		const EHudElement Element = (EHudElement)Index;
		const CPlacement &Placement = m_aPlacements[Index];

		m_aPushOffsets[Index] = vec2(0.0f, 0.0f);

		if(!IsLive(Element))
		{
			// Not on screen, so it neither pushes nor is pushed
			m_aPlacedRects[Index] = CRect();
			continue;
		}

		CRect Rect = RectForOffset(Element, Placement.m_Offset);

		// An attachment holds every frame, so it is settled before anything gets shoved around.
		// Skipped for whatever is being dragged, so the cursor still wins over it.
		if(Element != m_Held &&
			Placement.m_AttachSide != EHudPushDirection::NONE &&
			Placement.m_AttachTarget != ATTACH_NONE &&
			Placement.m_AttachTarget != Element &&
			IsLive(Placement.m_AttachTarget))
		{
			const CRect Target = m_aPlacedRects[(int)Placement.m_AttachTarget];
			if(Target.m_Size.x > 0.0f && Target.m_Size.y > 0.0f)
			{
				// Carrying however far the target has been moved from where it draws itself is
				// what makes the two travel together, rather than only lining up on one axis
				const CRect TargetNatural = NaturalRect(Placement.m_AttachTarget);
				const vec2 Carry = Target.m_Pos - TargetNatural.m_Pos;
				const float Gap = Placement.m_AttachGap;

				vec2 Wanted = Rect.m_Pos + Carry;
				switch(Placement.m_AttachSide)
				{
				case EHudPushDirection::LEFT:
					Wanted.x = Target.m_Pos.x - Gap - Rect.m_Size.x;
					break;
				case EHudPushDirection::RIGHT:
					Wanted.x = Target.m_Pos.x + Target.m_Size.x + Gap;
					break;
				case EHudPushDirection::UP:
					Wanted.y = Target.m_Pos.y - Gap - Rect.m_Size.y;
					break;
				case EHudPushDirection::DOWN:
					Wanted.y = Target.m_Pos.y + Target.m_Size.y + Gap;
					break;
				default:
					break;
				}

				m_aPushOffsets[Index] += Wanted - Rect.m_Pos;
				Rect.m_Pos = Wanted;
			}
		}

		if(Placement.m_PushDirection != EHudPushDirection::NONE && Element != m_Held && !m_PushSuspended)
		{
			// Shoving clear of one element can walk into another, so this repeats until nothing
			// is in the way. Everything it is being shoved out of is already fixed, so it always
			// settles rather than trading places forever.
			for(int Pass = 0; Pass < MAX_PUSH_PASSES; Pass++)
			{
				bool Moved = false;
				for(int j = 0; j < NumPlaced; j++)
				{
					const CRect &Other = m_aPlacedRects[aPlaced[j]];
					if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
						continue;

					const float Gap = Placement.m_PushGap;
					const bool Vertical = Placement.m_PushDirection == EHudPushDirection::UP ||
							      Placement.m_PushDirection == EHudPushDirection::DOWN;

					// The gap is the room this element wants kept between itself and whatever it
					// yields to, so the neighbour is treated as that much larger on the side being
					// pushed away from. Testing the bare rects instead would only apply the gap
					// once the two already overlapped, and an element that happened to clear its
					// neighbour by a hair would sit right up against it with the gap never used.
					CRect Padded = Other;
					switch(Placement.m_PushDirection)
					{
					case EHudPushDirection::UP:
						Padded.m_Pos.y -= Gap;
						Padded.m_Size.y += Gap;
						break;
					case EHudPushDirection::DOWN:
						Padded.m_Size.y += Gap;
						break;
					case EHudPushDirection::LEFT:
						Padded.m_Pos.x -= Gap;
						Padded.m_Size.x += Gap;
						break;
					case EHudPushDirection::RIGHT:
						Padded.m_Size.x += Gap;
						break;
					default:
						break;
					}

					if(Rect.m_Pos.x >= Padded.m_Pos.x + Padded.m_Size.x || Rect.m_Pos.x + Rect.m_Size.x <= Padded.m_Pos.x)
						continue;
					if(Rect.m_Pos.y >= Padded.m_Pos.y + Padded.m_Size.y || Rect.m_Pos.y + Rect.m_Size.y <= Padded.m_Pos.y)
						continue;

					// How much of this element actually lies across the other one, as a fraction
					// of its own size on that axis. Something barely clipping the corner of its
					// neighbour is trying to sit beside it, not on top of it, so shoving it the
					// declared way would fight the drag instead of getting out of the way.
					const float CrossOverlap =
						Vertical ?
							std::min(Rect.m_Pos.x + Rect.m_Size.x, Other.m_Pos.x + Other.m_Size.x) - std::max(Rect.m_Pos.x, Other.m_Pos.x) :
							std::min(Rect.m_Pos.y + Rect.m_Size.y, Other.m_Pos.y + Other.m_Size.y) - std::max(Rect.m_Pos.y, Other.m_Pos.y);
					// Measured against whichever of the two is narrower across the push. Judging it
					// by the pushed element alone means a small element sitting wholly inside a
					// wide one still counts as barely touching it, and the wide one slides out
					// sideways when what it should do is move out from under it.
					const float CrossSize = Vertical ?
									std::min(Rect.m_Size.x, Other.m_Size.x) :
									std::min(Rect.m_Size.y, Other.m_Size.y);
					const bool Sideways = CrossSize > 0.0f && CrossOverlap < CrossSize * PUSH_CROSS_FRACTION;

					vec2 Delta = vec2(0.0f, 0.0f);
					if(Sideways)
					{
						// Shove it the rest of the way clear along the other axis instead, whichever
						// side it is already closer to
						if(Vertical)
						{
							const float Left = (Other.m_Pos.x - Gap - Rect.m_Size.x) - Rect.m_Pos.x;
							const float Right = (Other.m_Pos.x + Other.m_Size.x + Gap) - Rect.m_Pos.x;
							Delta.x = absolute(Left) <= absolute(Right) ? Left : Right;
						}
						else
						{
							const float Up = (Other.m_Pos.y - Gap - Rect.m_Size.y) - Rect.m_Pos.y;
							const float Down = (Other.m_Pos.y + Other.m_Size.y + Gap) - Rect.m_Pos.y;
							Delta.y = absolute(Up) <= absolute(Down) ? Up : Down;
						}
					}
					else
					{
						switch(Placement.m_PushDirection)
						{
						case EHudPushDirection::UP:
							Delta.y = (Padded.m_Pos.y - Rect.m_Size.y) - Rect.m_Pos.y;
							break;
						case EHudPushDirection::DOWN:
							Delta.y = (Padded.m_Pos.y + Padded.m_Size.y) - Rect.m_Pos.y;
							break;
						case EHudPushDirection::LEFT:
							Delta.x = (Padded.m_Pos.x - Rect.m_Size.x) - Rect.m_Pos.x;
							break;
						case EHudPushDirection::RIGHT:
							Delta.x = (Padded.m_Pos.x + Padded.m_Size.x) - Rect.m_Pos.x;
							break;
						default:
							break;
						}
					}

					m_aPushOffsets[Index] += Delta;
					Rect.m_Pos += Delta;
					Moved = true;
				}
				if(!Moved)
					break;
			}
		}

		// Only elements that already accept being moved by the layout. Pulling a fixed element
		// flush with a neighbour moves it without anyone having asked, and an element set to never
		// be pushed should stay exactly where it puts itself.
		if(Element != m_Held && Placement.m_PushDirection != EHudPushDirection::NONE && !m_PushSuspended)
			SnapFlush(Element, Rect, aPlaced, NumPlaced);

		m_aPlacedRects[Index] = RectForOffset(Element, ClampOffset(Element, TotalOffset(Element)));
		aPlaced[NumPlaced++] = Index;
	}
}

void CHudLayout::SnapFlush(EHudElement Element, const CRect &Rect, const int *pPlaced, int NumPlaced)
{
	// The corner rounding treats two edges within CORNER_TOUCH_DISTANCE as a seam and squares the
	// corners there. Anything less than exactly flush then leaves a hairline of background showing
	// through a join that is being drawn as if it were solid, so it is closed here using the very
	// same distance, and the two cannot disagree about what counts as touching.
	vec2 Adjust = vec2(0.0f, 0.0f);

	for(int i = 0; i < NumPlaced; i++)
	{
		const CRect &Other = m_aPlacedRects[pPlaced[i]];
		if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
			continue;

		const float Left = Rect.m_Pos.x;
		const float Right = Rect.m_Pos.x + Rect.m_Size.x;
		const float Top = Rect.m_Pos.y;
		const float Bottom = Rect.m_Pos.y + Rect.m_Size.y;
		const float OtherLeft = Other.m_Pos.x;
		const float OtherRight = Other.m_Pos.x + Other.m_Size.x;
		const float OtherTop = Other.m_Pos.y;
		const float OtherBottom = Other.m_Pos.y + Other.m_Size.y;

		// Only edges that actually face each other, so an element merely passing nearby is not
		// dragged onto its neighbour
		const bool OverlapsY = Top < OtherBottom + CORNER_TOUCH_DISTANCE && Bottom > OtherTop - CORNER_TOUCH_DISTANCE;
		const bool OverlapsX = Left < OtherRight + CORNER_TOUCH_DISTANCE && Right > OtherLeft - CORNER_TOUCH_DISTANCE;

		if(OverlapsY && Adjust.x == 0.0f)
		{
			if(absolute(OtherLeft - Right) <= CORNER_TOUCH_DISTANCE)
				Adjust.x = OtherLeft - Right;
			else if(absolute(OtherRight - Left) <= CORNER_TOUCH_DISTANCE)
				Adjust.x = OtherRight - Left;
		}
		if(OverlapsX && Adjust.y == 0.0f)
		{
			if(absolute(OtherTop - Bottom) <= CORNER_TOUCH_DISTANCE)
				Adjust.y = OtherTop - Bottom;
			else if(absolute(OtherBottom - Top) <= CORNER_TOUCH_DISTANCE)
				Adjust.y = OtherBottom - Top;
		}
	}

	m_aPushOffsets[(int)Element] += Adjust;
}

vec2 CHudLayout::TotalOffset(EHudElement Element) const
{
	return m_aPlacements[(int)Element].m_Offset + m_aPushOffsets[(int)Element];
}

void CHudLayout::ReportNominalRect(EHudElement Element, vec2 Pos, vec2 Size)
{
	m_aNominal[(int)Element].m_Pos = Pos;
	m_aNominal[(int)Element].m_Size = Size;
	m_aNominalState[(int)Element] = ENominalState::REPORTED;
}

void CHudLayout::ReportNaturalRect(EHudElement Element, vec2 Pos, vec2 Size)
{
	CMeasurement &Measurement = m_aMeasurements[(int)Element];
	Measurement.m_Rect.m_Pos = Pos;
	Measurement.m_Rect.m_Size = Size;
	Measurement.m_Frame = m_Frame;
}

bool CHudLayout::TakeContainersDirty()
{
	if(m_ContainerResetSuspended)
		return false;

	const bool Dirty = m_ContainersDirty;
	m_ContainersDirty = false;
	return Dirty;
}

CHudLayout::CRect CHudLayout::NaturalRect(EHudElement Element) const
{
	const CMeasurement &Measurement = m_aMeasurements[(int)Element];
	if(Measurement.m_Frame >= 0 && m_Frame - Measurement.m_Frame <= MEASUREMENT_MAX_AGE)
		return Measurement.m_Rect;

	// What the element says it would occupy, which beats the table because the table can only hold
	// a constant and some elements rest somewhere that depends on the screen or on config
	if(m_aNominalState[(int)Element] == ENominalState::REPORTED)
		return m_aNominal[(int)Element];
	if(m_aNominalState[(int)Element] == ENominalState::WITHDRAWN)
		return CRect();

	const CHudElementDef &Def = gs_aHudElements[(int)Element];

	CRect Rect;
	Rect.m_Size = Def.m_Size;
	Rect.m_Pos.y = Def.m_Pos.y;
	switch(Def.m_OriginX)
	{
	case EHudOriginX::FROM_LEFT:
		Rect.m_Pos.x = Def.m_Pos.x;
		break;
	case EHudOriginX::FROM_RIGHT:
		Rect.m_Pos.x = m_BaseSize.x - Def.m_Pos.x;
		break;
	case EHudOriginX::FROM_CENTER:
		Rect.m_Pos.x = m_BaseSize.x * 0.5f - Def.m_Size.x * 0.5f + Def.m_Pos.x;
		break;
	}
	return Rect;
}

CHudLayout::CRect CHudLayout::RectForOffset(EHudElement Element, vec2 Offset) const
{
	const CPlacement &Placement = m_aPlacements[(int)Element];
	const CRect Natural = NaturalRect(Element);
	const vec2 Pivot = AnchorPoint(Natural, Placement.m_Anchor);
	const float Scale = Placement.m_Scale;

	CRect Rect;
	Rect.m_Size = Natural.m_Size * Scale;
	// The pivot moves by exactly the offset, and the rest of the rect follows it.
	Rect.m_Pos = Pivot + Offset - (Pivot - Natural.m_Pos) * Scale;
	return Rect;
}

vec2 CHudLayout::ClampOffset(EHudElement Element, vec2 Offset) const
{
	if(m_BaseSize.x <= 0.0f || m_BaseSize.y <= 0.0f)
		return Offset;

	const CRect Rect = RectForOffset(Element, Offset);

	vec2 Adjust = vec2(0.0f, 0.0f);
	// An element wider or taller than the screen cannot be contained, so it is pinned to the top
	// left corner rather than being pushed around by two edges that disagree
	if(Rect.m_Size.x >= m_BaseSize.x)
		Adjust.x = -Rect.m_Pos.x;
	else if(Rect.m_Pos.x < 0.0f)
		Adjust.x = -Rect.m_Pos.x;
	else if(Rect.m_Pos.x + Rect.m_Size.x > m_BaseSize.x)
		Adjust.x = m_BaseSize.x - (Rect.m_Pos.x + Rect.m_Size.x);

	if(Rect.m_Size.y >= m_BaseSize.y)
		Adjust.y = -Rect.m_Pos.y;
	else if(Rect.m_Pos.y < 0.0f)
		Adjust.y = -Rect.m_Pos.y;
	else if(Rect.m_Pos.y + Rect.m_Size.y > m_BaseSize.y)
		Adjust.y = m_BaseSize.y - (Rect.m_Pos.y + Rect.m_Size.y);

	return Offset + Adjust;
}

CHudLayout::CRect CHudLayout::ResolvedRect(EHudElement Element) const
{
	return RectForOffset(Element, ClampOffset(Element, TotalOffset(Element)));
}

vec2 CHudLayout::AnchorPos(EHudElement Element) const
{
	return AnchorPoint(ResolvedRect(Element), m_aPlacements[(int)Element].m_Anchor);
}

vec2 CHudLayout::AnchorPoint(const CRect &Rect, EHudAnchor Anchor)
{
	const vec2 Fraction = AnchorFraction(Anchor);
	return Rect.m_Pos + vec2(Rect.m_Size.x * Fraction.x, Rect.m_Size.y * Fraction.y);
}

vec2 CHudLayout::AnchorFraction(EHudAnchor Anchor)
{
	float FractionX = 0.0f;
	float FractionY = 0.0f;
	switch(Anchor)
	{
	case EHudAnchor::TOP_LEFT: break;
	case EHudAnchor::TOP_CENTER: FractionX = 0.5f; break;
	case EHudAnchor::TOP_RIGHT: FractionX = 1.0f; break;
	case EHudAnchor::CENTER_LEFT: FractionY = 0.5f; break;
	case EHudAnchor::CENTER:
		FractionX = 0.5f;
		FractionY = 0.5f;
		break;
	case EHudAnchor::CENTER_RIGHT:
		FractionX = 1.0f;
		FractionY = 0.5f;
		break;
	case EHudAnchor::BOTTOM_LEFT: FractionY = 1.0f; break;
	case EHudAnchor::BOTTOM_CENTER:
		FractionX = 0.5f;
		FractionY = 1.0f;
		break;
	case EHudAnchor::BOTTOM_RIGHT:
		FractionX = 1.0f;
		FractionY = 1.0f;
		break;
	default: break;
	}
	return vec2(FractionX, FractionY);
}

bool CHudLayout::ElementTransform(EHudElement Element, vec2 &Translation, float &Scale) const
{
	const CPlacement &Placement = m_aPlacements[(int)Element];

	Translation = vec2(0.0f, 0.0f);
	Scale = 1.0f;

	if(!IsMovable(Element))
		return false;

	if(m_BaseSize.x <= 0.0f || m_BaseSize.y <= 0.0f)
		return false;

	// The same clamp ResolvedRect applies, so that what is drawn and the box the editor puts
	// around it cannot disagree. It has to be worked out before deciding there is nothing to do,
	// because an element whose own offset is zero can still need pulling back onto the screen, and
	// skipping the transform then would draw it in one place while reporting it in another.
	const vec2 Offset = ClampOffset(Element, TotalOffset(Element));

	// Leaving the screen mapping alone for untouched elements keeps this free for anyone who never
	// moves anything, which is the overwhelmingly common case.
	if(Placement.m_Scale == 1.0f && Offset.x == 0.0f && Offset.y == 0.0f)
		return false;

	Scale = std::clamp(Placement.m_Scale, MIN_SCALE, MAX_SCALE);
	const vec2 Pivot = AnchorPoint(NaturalRect(Element), Placement.m_Anchor);

	// appear(n) = Scale * n + Translation, chosen so the pivot lands exactly Offset away from
	// where it was and stays put as the scale changes.
	Translation = Pivot + Offset - Pivot * Scale;
	return true;
}

float CHudLayout::ElementScale(EHudElement Element) const
{
	vec2 Translation;
	float Scale;
	ElementTransform(Element, Translation, Scale);
	return Scale;
}

vec2 CHudLayout::ToBaseSpace(EHudElement Element, vec2 Pos) const
{
	vec2 Translation;
	float Scale;
	ElementTransform(Element, Translation, Scale);
	return Pos * Scale + Translation;
}

int CHudLayout::CornerFlags(EHudElement Element) const
{
	const CRect Rect = ResolvedRect(Element);
	if(Rect.m_Size.x <= 0.0f || Rect.m_Size.y <= 0.0f)
		return IGraphics::CORNER_ALL;

	const float Left = Rect.m_Pos.x;
	const float Right = Rect.m_Pos.x + Rect.m_Size.x;
	const float Top = Rect.m_Pos.y;
	const float Bottom = Rect.m_Pos.y + Rect.m_Size.y;

	// Each side is asked separately whether something is flush against it at a given point along
	// it. Asking only whether two elements are adjacent somewhere would square the corners at the
	// far end of the element too, which is what made a panel go square all the way round when only
	// one of its sides was touching something.
	auto SideTouched = [&](bool Vertical, float SideCoord, float Along) {
		for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
		{
			if((EHudElement)i == Element || !IsLive((EHudElement)i))
				continue;

			const CRect Other = m_aPlacedRects[i];
			if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
				continue;

			if(Vertical)
			{
				// Something whose left or right edge lands on this vertical side of ours
				const bool Flush = absolute(Other.m_Pos.x - SideCoord) <= CORNER_TOUCH_DISTANCE ||
						   absolute(Other.m_Pos.x + Other.m_Size.x - SideCoord) <= CORNER_TOUCH_DISTANCE;
				if(Flush && Along >= Other.m_Pos.y - CORNER_TOUCH_DISTANCE && Along <= Other.m_Pos.y + Other.m_Size.y + CORNER_TOUCH_DISTANCE)
					return true;
			}
			else
			{
				const bool Flush = absolute(Other.m_Pos.y - SideCoord) <= CORNER_TOUCH_DISTANCE ||
						   absolute(Other.m_Pos.y + Other.m_Size.y - SideCoord) <= CORNER_TOUCH_DISTANCE;
				if(Flush && Along >= Other.m_Pos.x - CORNER_TOUCH_DISTANCE && Along <= Other.m_Pos.x + Other.m_Size.x + CORNER_TOUCH_DISTANCE)
					return true;
			}
		}
		return false;
	};

	const bool AtLeftEdge = Left <= CORNER_TOUCH_DISTANCE;
	const bool AtRightEdge = Right >= m_BaseSize.x - CORNER_TOUCH_DISTANCE;
	const bool AtTopEdge = Top <= CORNER_TOUCH_DISTANCE;
	const bool AtBottomEdge = Bottom >= m_BaseSize.y - CORNER_TOUCH_DISTANCE;

	auto Squared = [&](bool LeftSide, bool TopSide) {
		const float X = LeftSide ? Left : Right;
		const float Y = TopSide ? Top : Bottom;
		if(LeftSide ? AtLeftEdge : AtRightEdge)
			return true;
		if(TopSide ? AtTopEdge : AtBottomEdge)
			return true;
		if(SideTouched(true, X, Y))
			return true;
		return SideTouched(false, Y, X);
	};

	int Flags = 0;
	if(!Squared(true, true))
		Flags |= IGraphics::CORNER_TL;
	if(!Squared(false, true))
		Flags |= IGraphics::CORNER_TR;
	if(!Squared(true, false))
		Flags |= IGraphics::CORNER_BL;
	if(!Squared(false, false))
		Flags |= IGraphics::CORNER_BR;
	return Flags;
}

void CHudLayout::FreeSpanX(EHudElement Element, float Y, float Height, float PreferredX, float Padding, float &Left, float &Right) const
{
	Left = 0.0f;
	Right = m_BaseSize.x;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		if((EHudElement)i == Element || !IsLive((EHudElement)i))
			continue;

		const CRect Other = ResolvedRect((EHudElement)i);
		if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
			continue;
		if(Y >= Other.m_Pos.y + Other.m_Size.y || Y + Height <= Other.m_Pos.y)
			continue;

		// Which side it takes room from is decided by where its middle sits, so an element that
		// straddles the preferred x still gives up one side rather than blocking both
		const float OtherCenter = Other.m_Pos.x + Other.m_Size.x * 0.5f;
		if(OtherCenter <= PreferredX)
			Left = std::max(Left, Other.m_Pos.x + Other.m_Size.x + Padding);
		else
			Right = std::min(Right, Other.m_Pos.x - Padding);
	}

	if(Right < Left)
		Right = Left;
}

vec2 CHudLayout::ToElementSpace(EHudElement Element, vec2 Pos) const
{
	vec2 Translation;
	float Scale;
	ElementTransform(Element, Translation, Scale);
	return (Pos - Translation) / Scale;
}

bool CHudLayout::Begin(EHudElement Element, vec2 WorkingSize)
{
	vec2 Translation;
	float Scale;
	if(!ElementTransform(Element, Translation, Scale))
		return false;

	// An element drawing into a taller virtual screen than the HUD's needs the same transform
	// expressed in its own units, which is only a matter of scaling the translation by the ratio
	// between the two. The multiplier is the same on both axes, since both spaces share an aspect.
	const vec2 Working = WorkingSize.y > 0.0f ? WorkingSize : m_BaseSize;
	const vec2 WorkingTranslation = Translation * (Working.y / m_BaseSize.y);

	const vec2 TopLeft = -WorkingTranslation / Scale;
	Graphics()->MapScreen(CScreenRect(TopLeft, TopLeft + Working / Scale));
	return true;
}

void CHudLayout::End(vec2 WorkingSize)
{
	const vec2 Working = WorkingSize.y > 0.0f ? WorkingSize : m_BaseSize;
	Graphics()->MapScreenToSize(Working.x, Working.y);
}

CHudLayout::CScope::CScope(CHudLayout *pLayout, EHudElement Element, vec2 WorkingSize) :
	m_pLayout(pLayout), m_WorkingSize(WorkingSize)
{
	m_Applied = m_pLayout->Begin(Element, m_WorkingSize);
}

CHudLayout::CScope::~CScope()
{
	if(m_Applied)
		m_pLayout->End(m_WorkingSize);
}

bool CHudLayout::ElementByName(const char *pName, EHudElement *pOut)
{
	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		if(str_comp_nocase(pName, gs_aHudElements[i].m_pName) == 0)
		{
			*pOut = (EHudElement)i;
			return true;
		}
	}
	return false;
}

bool CHudLayout::AnchorByName(const char *pName, EHudAnchor *pOut)
{
	for(int i = 0; i < (int)EHudAnchor::NUM_HUD_ANCHORS; i++)
	{
		if(str_comp_nocase(pName, gs_apAnchorNames[i]) == 0)
		{
			*pOut = (EHudAnchor)i;
			return true;
		}
	}
	return false;
}

void CHudLayout::ConHudMove(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	EHudElement Element;
	if(!ElementByName(pResult->GetString(0), &Element))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
		return;
	}

	pThis->SetOffset(Element, vec2(pResult->GetFloat(1), pResult->GetFloat(2)));
}

void CHudLayout::ConHudScale(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	EHudElement Element;
	if(!ElementByName(pResult->GetString(0), &Element))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
		return;
	}

	pThis->SetScale(Element, pResult->GetFloat(1));
}

void CHudLayout::ConHudAnchor(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	EHudElement Element;
	if(!ElementByName(pResult->GetString(0), &Element))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
		return;
	}

	EHudAnchor Anchor;
	if(!AnchorByName(pResult->GetString(1), &Anchor))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout",
			"unknown anchor, expected one of: topleft top topright left center right bottomleft bottom bottomright");
		return;
	}

	pThis->SetAnchor(Element, Anchor);
}

void CHudLayout::ConHudPush(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	EHudElement Element;
	if(!ElementByName(pResult->GetString(0), &Element))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
		return;
	}

	EHudPushDirection Direction = EHudPushDirection::NONE;
	bool Found = false;
	for(int i = 0; i < (int)EHudPushDirection::NUM_HUD_PUSH_DIRECTIONS; i++)
	{
		if(str_comp_nocase(pResult->GetString(1), gs_apPushDirectionNames[i]) == 0)
		{
			Direction = (EHudPushDirection)i;
			Found = true;
			break;
		}
	}
	if(!Found)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown direction, expected one of: none up down left right");
		return;
	}

	const CPlacement &Placement = pThis->m_aPlacements[(int)Element];
	const int Priority = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : Placement.m_PushPriority;
	const float Gap = pResult->NumArguments() > 3 ? pResult->GetFloat(3) : Placement.m_PushGap;
	pThis->SetPush(Element, Direction, Priority, Gap);
}

void CHudLayout::ConHudAttach(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	EHudElement Element;
	if(!ElementByName(pResult->GetString(0), &Element))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
		return;
	}

	EHudElement Target = ATTACH_NONE;
	const bool Detach = str_comp_nocase(pResult->GetString(1), "none") == 0;
	if(!Detach && !ElementByName(pResult->GetString(1), &Target))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown attach target, use hud_list or none");
		return;
	}
	if(!Detach && Target == Element)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "an element cannot be attached to itself");
		return;
	}

	EHudPushDirection Side = EHudPushDirection::NONE;
	if(!Detach)
	{
		bool Found = false;
		for(int i = 0; i < (int)EHudPushDirection::NUM_HUD_PUSH_DIRECTIONS; i++)
		{
			if(str_comp_nocase(pResult->GetString(2), gs_apPushDirectionNames[i]) == 0)
			{
				Side = (EHudPushDirection)i;
				Found = true;
				break;
			}
		}
		if(!Found || Side == EHudPushDirection::NONE)
		{
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown side, expected one of: up down left right");
			return;
		}
	}

	const float Gap = pResult->NumArguments() > 3 ? pResult->GetFloat(3) : 2.0f;
	pThis->SetAttach(Element, Target, Side, Gap);
}

void CHudLayout::ConHudReset(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	if(pResult->NumArguments() > 0)
	{
		EHudElement Element;
		if(!ElementByName(pResult->GetString(0), &Element))
		{
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", "unknown element, use hud_list");
			return;
		}

		pThis->m_aPlacements[(int)Element] = CPlacement();
		pThis->m_aPlacements[(int)Element].m_Anchor = gs_aHudElements[(int)Element].m_DefaultAnchor;
	}
	else
	{
		for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
		{
			pThis->m_aPlacements[i] = CPlacement();
			pThis->m_aPlacements[i].m_Anchor = gs_aHudElements[i].m_DefaultAnchor;
		}
	}

	pThis->m_ContainersDirty = true;
}

void CHudLayout::ConHudList(IConsole::IResult *pResult, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		const CPlacement &Placement = pThis->m_aPlacements[i];
		const CRect Rect = pThis->ResolvedRect((EHudElement)i);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf),
			"%-16s offset %7.1f %7.1f  scale %5.2f  anchor %-11s  push %-5s pri %3d  attach %-16s %-5s  rect %6.1f %6.1f %6.1f %6.1f",
			gs_aHudElements[i].m_pName,
			Placement.m_Offset.x, Placement.m_Offset.y,
			Placement.m_Scale,
			gs_apAnchorNames[(int)Placement.m_Anchor],
			gs_apPushDirectionNames[(int)Placement.m_PushDirection],
			Placement.m_PushPriority,
			Placement.m_AttachTarget == ATTACH_NONE ? "none" : gs_aHudElements[(int)Placement.m_AttachTarget].m_pName,
			gs_apPushDirectionNames[(int)Placement.m_AttachSide],
			Rect.m_Pos.x, Rect.m_Pos.y, Rect.m_Size.x, Rect.m_Size.y);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "hud_layout", aBuf);
	}
}

void CHudLayout::SetOffset(EHudElement Element, vec2 Offset)
{
	// Quantised so that the number the editor reports back is the number that is actually stored.
	// Two elements told to sit at the same place should end up at the same place, rather than a
	// fraction of a unit apart because the value carried precision that was never shown.
	Offset.x = std::round(Offset.x / OFFSET_STEP) * OFFSET_STEP;
	Offset.y = std::round(Offset.y / OFFSET_STEP) * OFFSET_STEP;

	// Clamped on the way in so that dragging past an edge does not build up an offset that has to
	// be unwound before the element moves again. Whatever it was shoved by is taken out again, so
	// that only the user's own part is stored.
	const vec2 Push = m_aPushOffsets[(int)Element];
	m_aPlacements[(int)Element].m_Offset = ClampOffset(Element, Offset + Push) - Push;
}

float CHudLayout::MinScale()
{
	return MIN_SCALE;
}

float CHudLayout::MaxScale(EHudElement Element) const
{
	if(m_BaseSize.x <= 0.0f || m_BaseSize.y <= 0.0f)
		return MAX_SCALE;

	const CRect Natural = NaturalRect(Element);
	if(Natural.m_Size.x <= 0.0f || Natural.m_Size.y <= 0.0f)
		return MAX_SCALE;

	// The natural size is what the element reports in its own units, which does not move with the
	// scale, so this does not chase itself while an element is being dragged larger.
	const float Fit = std::min(m_BaseSize.x / Natural.m_Size.x, m_BaseSize.y / Natural.m_Size.y);
	return std::clamp(Fit, 1.0f, MAX_SCALE);
}

void CHudLayout::SetScale(EHudElement Element, float Scale)
{
	Scale = std::round(Scale / SCALE_STEP) * SCALE_STEP;
	// Never below the element's current scale, so that an element which grew, or a screen that got
	// smaller, cannot quietly shrink a size the user picked. It only ever caps growth.
	const float Max = std::max(MaxScale(Element), m_aPlacements[(int)Element].m_Scale);
	Scale = std::clamp(Scale, MIN_SCALE, Max);
	if(m_aPlacements[(int)Element].m_Scale == Scale)
		return;

	m_aPlacements[(int)Element].m_Scale = Scale;
	// Growing an element that was already against an edge pushes it over
	SetOffset(Element, m_aPlacements[(int)Element].m_Offset);
	// Cached text containers were rasterized at the old scale and would render blurry.
	m_ContainersDirty = true;
}

void CHudLayout::SetAnchor(EHudElement Element, EHudAnchor Anchor)
{
	m_aPlacements[(int)Element].m_Anchor = Anchor;
	// A different anchor moves the rect, which can put it over an edge
	SetOffset(Element, m_aPlacements[(int)Element].m_Offset);
}

void CHudLayout::ResetElement(EHudElement Element)
{
	const CHudElementDef &Def = gs_aHudElements[(int)Element];
	m_aPlacements[(int)Element] = CPlacement();
	m_aPlacements[(int)Element].m_Anchor = Def.m_DefaultAnchor;
	m_aPlacements[(int)Element].m_PushPriority = Def.m_PushPriority;
	m_aPlacements[(int)Element].m_PushDirection = Def.m_PushDirection;
	m_aPlacements[(int)Element].m_PushGap = Def.m_PushGap;
	m_aPlacements[(int)Element].m_AttachTarget = Def.m_AttachTarget;
	m_aPlacements[(int)Element].m_AttachSide = Def.m_AttachSide;
	m_aPlacements[(int)Element].m_AttachGap = Def.m_AttachGap;
	m_ContainersDirty = true;
}

void CHudLayout::SetOccluder(vec2 Pos, vec2 Size)
{
	m_OccluderPos = Pos;
	m_OccluderSize = Size;
}

void CHudLayout::HoldNaturalRect(EHudElement Element)
{
	CMeasurement &Measurement = m_aMeasurements[(int)Element];
	// Only a measurement that is still good is worth keeping. One that lapsed already, or that was
	// never taken, is left alone so that an element which simply is not being drawn any more goes
	// on falling back to its nominal rect.
	if(Measurement.m_Frame < 0 || m_Frame - Measurement.m_Frame > MEASUREMENT_MAX_AGE)
		return;

	Measurement.m_Frame = m_Frame;
}

bool CHudLayout::IsOccluded(EHudElement Element)
{
	if(m_OccluderSize.x <= 0.0f || m_OccluderSize.y <= 0.0f)
		return false;

	const CRect Rect = ResolvedRect(Element);
	if(Rect.m_Size.x <= 0.0f || Rect.m_Size.y <= 0.0f)
		return false;

	const bool Occluded = Rect.m_Pos.x < m_OccluderPos.x + m_OccluderSize.x &&
			      Rect.m_Pos.x + Rect.m_Size.x > m_OccluderPos.x &&
			      Rect.m_Pos.y < m_OccluderPos.y + m_OccluderSize.y &&
			      Rect.m_Pos.y + Rect.m_Size.y > m_OccluderPos.y;

	// The element is about to be told not to draw, so nothing else will keep this rect alive
	if(Occluded)
		HoldNaturalRect(Element);

	return Occluded;
}

bool CHudLayout::IsPresent(EHudElement Element) const
{
	return IsLive(Element) || m_aNominalState[(int)Element] != ENominalState::WITHDRAWN;
}

bool CHudLayout::IsLive(EHudElement Element) const
{
	const CMeasurement &Measurement = m_aMeasurements[(int)Element];
	return Measurement.m_Frame >= 0 && m_Frame - Measurement.m_Frame <= MEASUREMENT_MAX_AGE;
}

const char *CHudLayout::ElementName(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pName;
}

const char *CHudLayout::ElementDisplayName(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pDisplayName;
}

const char *CHudLayout::AnchorName(EHudAnchor Anchor)
{
	return gs_apAnchorNames[(int)Anchor];
}

const char *CHudLayout::PushDirectionName(EHudPushDirection Direction)
{
	return gs_apPushDirectionNames[(int)Direction];
}

int *CHudLayout::EnabledSetting(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabled;
}

const char *CHudLayout::EnabledLabel(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabledLabel;
}

int *CHudLayout::SecondEnabledSetting(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabled2;
}

const char *CHudLayout::SecondEnabledLabel(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabledLabel2;
}

int *CHudLayout::ThirdEnabledSetting(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabled3;
}

const char *CHudLayout::ThirdEnabledLabel(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_pEnabledLabel3;
}

bool CHudLayout::IsMovable(EHudElement Element)
{
	return gs_aHudElements[(int)Element].m_Movable;
}

void CHudLayout::SetAttach(EHudElement Element, EHudElement Target, EHudPushDirection Side, float Gap)
{
	CPlacement &Placement = m_aPlacements[(int)Element];
	Placement.m_AttachTarget = Side == EHudPushDirection::NONE ? ATTACH_NONE : Target;
	Placement.m_AttachSide = Side;
	// A gap means nothing without a side, and leaving one behind would make a detached element
	// compare unequal to one that was never attached
	Placement.m_AttachGap = Side == EHudPushDirection::NONE ? 0.0f : Gap;
}

void CHudLayout::SetPush(EHudElement Element, EHudPushDirection Direction, int Priority, float Gap)
{
	CPlacement &Placement = m_aPlacements[(int)Element];
	Placement.m_PushDirection = Direction;
	Placement.m_PushPriority = Priority;
	// A gap means nothing without a direction, and leaving one behind would make an element that
	// never moves compare unequal to a default one that never moves
	Placement.m_PushGap = Direction == EHudPushDirection::NONE ? 0.0f : Gap;
}

bool CHudLayout::IsDefault(EHudElement Element) const
{
	const CPlacement &Placement = m_aPlacements[(int)Element];
	const CHudElementDef &Def = gs_aHudElements[(int)Element];
	return Placement.m_Offset.x == 0.0f && Placement.m_Offset.y == 0.0f &&
	       Placement.m_Scale == 1.0f &&
	       Placement.m_Anchor == Def.m_DefaultAnchor &&
	       Placement.m_PushPriority == Def.m_PushPriority &&
	       Placement.m_PushDirection == Def.m_PushDirection &&
	       Placement.m_PushGap == Def.m_PushGap &&
	       Placement.m_AttachTarget == Def.m_AttachTarget &&
	       Placement.m_AttachSide == Def.m_AttachSide &&
	       Placement.m_AttachGap == Def.m_AttachGap;
}

void CHudLayout::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CHudLayout *pThis = (CHudLayout *)pUserData;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		// Only elements the user actually moved are worth writing out, so that an untouched HUD
		// leaves no trace in the config and picks up any future change to the defaults.
		if(pThis->IsDefault((EHudElement)i))
			continue;

		const CPlacement &Placement = pThis->m_aPlacements[i];
		const char *pName = gs_aHudElements[i].m_pName;
		char aBuf[128];

		if(Placement.m_Offset.x != 0.0f || Placement.m_Offset.y != 0.0f)
		{
			str_format(aBuf, sizeof(aBuf), "hud_move %s %.3f %.3f", pName, Placement.m_Offset.x, Placement.m_Offset.y);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYHUDLAYOUT);
		}
		if(Placement.m_Scale != 1.0f)
		{
			str_format(aBuf, sizeof(aBuf), "hud_scale %s %.3f", pName, Placement.m_Scale);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYHUDLAYOUT);
		}
		if(Placement.m_Anchor != gs_aHudElements[i].m_DefaultAnchor)
		{
			str_format(aBuf, sizeof(aBuf), "hud_anchor %s %s", pName, gs_apAnchorNames[(int)Placement.m_Anchor]);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYHUDLAYOUT);
		}
		if(Placement.m_PushPriority != gs_aHudElements[i].m_PushPriority ||
			Placement.m_PushDirection != gs_aHudElements[i].m_PushDirection ||
			Placement.m_PushGap != gs_aHudElements[i].m_PushGap)
		{
			str_format(aBuf, sizeof(aBuf), "hud_push %s %s %d %.3f", pName,
				gs_apPushDirectionNames[(int)Placement.m_PushDirection], Placement.m_PushPriority, Placement.m_PushGap);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYHUDLAYOUT);
		}
		if(Placement.m_AttachTarget != gs_aHudElements[i].m_AttachTarget ||
			Placement.m_AttachSide != gs_aHudElements[i].m_AttachSide ||
			Placement.m_AttachGap != gs_aHudElements[i].m_AttachGap)
		{
			const char *pTarget = Placement.m_AttachTarget == ATTACH_NONE ? "none" : gs_aHudElements[(int)Placement.m_AttachTarget].m_pName;
			str_format(aBuf, sizeof(aBuf), "hud_attach %s %s %s %.3f", pName, pTarget,
				gs_apPushDirectionNames[(int)Placement.m_AttachSide], Placement.m_AttachGap);
			pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYHUDLAYOUT);
		}
	}
}

void CHudLayout::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYHUDLAYOUT);

	Console()->Register("hud_move", "s[element] f[x] f[y]", CFGFLAG_CLIENT, ConHudMove, this, "Offset a HUD element from its default position, in HUD units");
	Console()->Register("hud_scale", "s[element] f[scale]", CFGFLAG_CLIENT, ConHudScale, this, "Scale a HUD element around its anchor (1 = default)");
	Console()->Register("hud_anchor", "s[element] s[anchor]", CFGFLAG_CLIENT, ConHudAnchor, this, "Set which point of a HUD element stays put while it is scaled");
	Console()->Register("hud_push", "s[element] s[direction] ?i[priority] ?f[gap]", CFGFLAG_CLIENT, ConHudPush, this,
		"Set which way a HUD element is shoved when something with a higher priority is in its way");
	Console()->Register("hud_attach", "s[element] s[target] s[side] ?f[gap]", CFGFLAG_CLIENT, ConHudAttach, this,
		"Stick a HUD element to one side of another so it travels with it. Target none detaches it");
	Console()->Register("hud_reset", "?s[element]", CFGFLAG_CLIENT, ConHudReset, this, "Reset one HUD element, or all of them, to its default placement");
	Console()->Register("hud_list", "", CFGFLAG_CLIENT, ConHudList, this, "List every HUD element the layout system knows about");
}
