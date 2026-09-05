// EClient
#ifndef GAME_CLIENT_COMPONENTS_ENTITY_HUD_HUD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_ENTITY_HUD_HUD_LAYOUT_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

class IConfigManager;

/**
 * The HUD elements that the layout system can move and scale.
 *
 * Adding an element means adding it here, adding the matching row to gs_aHudElements in
 * hud_layout.cpp, and wrapping its render call in a CHudLayout::CScope. Nothing inside the
 * render function itself has to change, which is what keeps upstream merges cheap.
 */
enum class EHudElement
{
	HEALTH_AMMO = 0,
	PLAYER_STATE,
	SCORE,
	MOVEMENT_INFO,
	SPECTATOR_COUNT,
	DUMMY_ACTIONS,
	FPS,
	PREDICTION,
	RECORD,
	SPECTATOR_HUD,
	WARMUP_TIMER,
	MEDIA_ISLAND,
	FROZEN_TEES,
	KILL_FEED,
	FROZEN_TEXT,
	VOTING,
	CHAT,
	LOCAL_TIME,
	NOTIFY_LAST,
	PRACTICE_ALERT,
	NUM_HUD_ELEMENTS,
};

/**
 * The point of an element's rect that stays put while the element is scaled.
 *
 * This is what makes a right-aligned element grow to the left instead of off the screen.
 */
enum class EHudAnchor
{
	TOP_LEFT = 0,
	TOP_CENTER,
	TOP_RIGHT,
	CENTER_LEFT,
	CENTER,
	CENTER_RIGHT,
	BOTTOM_LEFT,
	BOTTOM_CENTER,
	BOTTOM_RIGHT,
	NUM_HUD_ANCHORS,
};

/**
 * Which way an element gets shoved when something with a higher push priority is in its way.
 *
 * Priority decides who yields; this decides where they go. An element with no direction is never
 * moved by anything, it only ever pushes.
 */
enum class EHudPushDirection
{
	NONE = 0,
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NUM_HUD_PUSH_DIRECTIONS,
};

/**
 * Moves and scales HUD elements without touching the code that draws them.
 *
 * Every HUD element draws into a fixed 300-unit-tall virtual screen using hardcoded
 * coordinates. Rather than rewriting those coordinates, this remaps the virtual screen for the
 * duration of a single element's render call, so that the coordinates it already uses land
 * wherever the layout wants them.
 *
 * For an element with natural (upstream) rect N, anchor pivot P, user offset O and user scale S,
 * every coordinate n it draws at should end up at
 *
 *     appear(n) = S * n + D,  where D = P + O - S * P
 *
 * so that the pivot lands exactly O away from where it used to be and does not drift as S
 * changes. A virtual screen window of (L, T) to (L + Width / S, T + Height / S) with
 * (L, T) = -D / S produces exactly that mapping, because a coordinate n then normalizes to
 * (n - L) / (Width / S) = (S * n + D) / Width, which is where appear(n) normalizes to under the
 * base window.
 *
 * Elements report the rect they actually drew into, so the table in hud_layout.cpp is only a
 * fallback for anything that has not drawn yet. On top of the user's own offset, SolvePushes
 * shoves each element clear of everything with a higher push priority, which is what replaced the
 * offsets the HUD used to subtract by hand for whatever sat below it.
 */
class CHudLayout : public CComponentInterfaces
{
public:
	class CRect
	{
	public:
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Size = vec2(0.0f, 0.0f);
	};

	class CPlacement
	{
	public:
		vec2 m_Offset = vec2(0.0f, 0.0f);
		float m_Scale = 1.0f;
		EHudAnchor m_Anchor = EHudAnchor::TOP_LEFT;

		// Higher priority wins a collision and stays put. Ties are broken by declaration order, so
		// the outcome never depends on which element happened to be looked at first.
		int m_PushPriority = 0;
		EHudPushDirection m_PushDirection = EHudPushDirection::NONE;
		float m_PushGap = 0.0f;

		// Stuck to one side of another element, so it travels with it rather than merely getting
		// out of its way on a collision. NUM_HUD_ELEMENTS means not attached to anything.
		EHudElement m_AttachTarget = EHudElement::NUM_HUD_ELEMENTS;
		EHudPushDirection m_AttachSide = EHudPushDirection::NONE;
		float m_AttachGap = 0.0f;

		// Whether the scoreboard covering this element is reason enough to stop drawing it. Off for
		// the few that carry something the player still needs while it is up.
		bool m_HideWhenCovered = true;
	};

	/**
	 * Applies one element's transform for as long as it is alive, and restores the base HUD
	 * screen when it goes out of scope. Wrap a render call in it:
	 *
	 *     {
	 *         CHudLayout::CScope Scope(&m_HudLayout, EHudElement::SCORE);
	 *         RenderScoreHud();
	 *     }
	 */
	class CScope
	{
	public:
		/**
		 * WorkingSize is the virtual screen the element draws into, for the few that do not use
		 * the HUD's own. Leave it out for anything drawing in HUD units.
		 */
		CScope(CHudLayout *pLayout, EHudElement Element, vec2 WorkingSize = vec2(0.0f, 0.0f));
		~CScope();

		CScope(const CScope &Other) = delete;
		CScope &operator=(const CScope &Other) = delete;

	private:
		CHudLayout *m_pLayout;
		vec2 m_WorkingSize;
		bool m_Applied;
	};

	CHudLayout();

	void OnConsoleInit();

	/**
	 * Must be called once per frame from CHud::OnRender, after the base HUD screen has been
	 * mapped, so that the layout knows what it is restoring to and how wide the screen is.
	 */
	void OnBaseScreenSet(vec2 BaseSize);

	/**
	 * Returns true once after any element's scale changed.
	 *
	 * Text containers bake their glyph raster size from the screen mapping that was active when
	 * they were created, so a container that outlives a scale change renders blurry. The caller
	 * has to drop its cached containers when this fires.
	 */
	bool TakeContainersDirty();

	/**
	 * Holds back the container reset above while a resize is in progress.
	 *
	 * A drag lands on a different scale every frame, and rebuilding every text container on each
	 * of those frames is wasted work. The dirty flag is kept, not dropped, so the rebuild still
	 * happens once the drag ends.
	 */
	void SetContainerResetSuspended(bool Suspended) { m_ContainerResetSuspended = Suspended; }

	/**
	 * Reports the rect an element actually drew into, in the element's own untransformed
	 * coordinates. Called from inside the render functions, because several elements only know
	 * their extent once they have laid themselves out.
	 *
	 * The reported rect is used from the next frame on. Being one frame behind is harmless here:
	 * it only feeds the scaling pivot and, later, hit testing.
	 */
	void ReportNaturalRect(EHudElement Element, vec2 Pos, vec2 Size);

	/**
	 * Reports where an element sits when it is switched off, without marking it as drawing.
	 *
	 * The table in hud_layout.cpp can only hold a constant, which is no use to an element whose
	 * resting position depends on the aspect ratio or on a config value. Such an element works out
	 * its own and hands it over, so that switching it on does not move it.
	 */
	void ReportNominalRect(EHudElement Element, vec2 Pos, vec2 Size);

	/**
	 * Withdraws a nominal rect, for an element that has stopped having a resting position at all.
	 *
	 * This is stronger than never having reported one. An element that has withdrawn its rect is
	 * taken to be absent rather than merely idle, so nothing falls back to the table on its behalf
	 * and the editor leaves it out entirely.
	 */
	void ClearNominalRect(EHudElement Element) { m_aNominalState[(int)Element] = ENominalState::WITHDRAWN; }

	/**
	 * Whether the element exists at all right now, as against being switched off but still having
	 * somewhere it would go.
	 */
	bool IsPresent(EHudElement Element) const;

	/**
	 * The rect an element occupies when it is left alone, in base HUD units.
	 *
	 * This is whatever the element last reported, or the nominal entry from the table for an
	 * element that has not rendered recently.
	 */
	CRect NaturalRect(EHudElement Element) const;

	/**
	 * Where an element actually ends up once its offset and scale are applied.
	 */
	CRect ResolvedRect(EHudElement Element) const;

	const CPlacement &Placement(EHudElement Element) const { return m_aPlacements[(int)Element]; }

	void SetOffset(EHudElement Element, vec2 Offset);
	void SetPush(EHudElement Element, EHudPushDirection Direction, int Priority, float Gap);

	/**
	 * Sticks an element to one side of another.
	 *
	 * A push only acts when two elements run into each other. An attachment holds every frame: the
	 * element sits flush against the given side of its target and carries whatever that target has
	 * been moved by, so the pair travel together.
	 *
	 * Attaching to a target with a lower push priority costs a frame of lag, because the target
	 * has not been placed yet when the attachment is resolved.
	 */
	void SetAttach(EHudElement Element, EHudElement Target, EHudPushDirection Side, float Gap);

	/**
	 * Exempts one element from being shoved around and from being pulled flush.
	 *
	 * The editor holds the element under the cursor here. Without it the solver keeps relocating
	 * whatever is being dragged, which reads as the HUD fighting the drag.
	 */
	void SetHeld(EHudElement Element) { m_Held = Element; }
	void ClearHeld() { m_Held = EHudElement::NUM_HUD_ELEMENTS; }

	/**
	 * Holds every element still, wherever it has been put, instead of shoving them clear of one
	 * another.
	 *
	 * The editor turns this on while it is open, so what is on screen is what has actually been
	 * placed. Watching the solver rearrange things underneath while trying to position them makes
	 * it very hard to tell what is your doing and what is its.
	 */
	void SetPushSuspended(bool Suspended) { m_PushSuspended = Suspended; }

	/**
	 * Recomputes where everything sits.
	 *
	 * Done once a frame from CHud, but the editor draws before that and has to see the result of
	 * a drag it just applied rather than the previous frame's.
	 */
	void Resolve() { SolvePushes(); }

	/**
	 * How far an element has been shoved by everything above it in priority, on top of whatever
	 * the user moved it by.
	 */
	vec2 PushOffset(EHudElement Element) const { return m_aPushOffsets[(int)Element]; }

	/**
	 * Which corners of an element should be rounded.
	 *
	 * A corner that sits against the screen edge, or against another element's edge, is squared
	 * off so that neighbouring panels read as one surface instead of two boxes with a seam.
	 */
	int CornerFlags(EHudElement Element) const;

	/**
	 * Whether anything lies flush against one whole side of an element, the screen edge included.
	 *
	 * CornerFlags answers this a corner at a time, which is the right question for a panel drawn as
	 * one rect. An element drawn as several stacked boxes has to ask about the side as a whole
	 * instead, so that its boxes all round the same way rather than each following its own corner.
	 */
	bool SideBlocked(EHudElement Element, EHudPushDirection Side) const;

	/**
	 * The offset with an element pulled back onto the screen, if it would otherwise hang over an
	 * edge. Applied on the way in and again on the way out, so that an element cannot end up out
	 * of bounds through a config file, a resize, or a change in its own size either.
	 */
	vec2 ClampOffset(EHudElement Element, vec2 Offset) const;

	/**
	 * The range a scale is allowed to take.
	 *
	 * The bottom is a constant, since anything smaller stops being readable at any screen size.
	 * The top is whatever leaves the element still fitting on the screen, so that growing one
	 * cannot push it over an edge the way clamping its position alone would allow. An element
	 * already larger than the screen keeps life size available rather than being forced below it.
	 */
	void SetHideWhenCovered(EHudElement Element, bool Hide);

	static float MinScale();
	float MaxScale(EHudElement Element) const;

	void SetScale(EHudElement Element, float Scale);
	void SetAnchor(EHudElement Element, EHudAnchor Anchor);
	void ResetElement(EHudElement Element);

	/**
	 * Whether the element drew this frame. The editor dims the ones that did not, because their
	 * rect is the nominal one from the table rather than anything they reported.
	 */
	bool IsLive(EHudElement Element) const;

	/**
	 * Where an element's anchor sits once the placement is applied. This is the one point on the
	 * element that scaling leaves alone.
	 */
	vec2 AnchorPos(EHudElement Element) const;

	/**
	 * The anchor as a fraction of the rect, (0,0) for the top left through (1,1) for the bottom
	 * right.
	 */
	static vec2 AnchorFraction(EHudAnchor Anchor);

	static const char *ElementName(EHudElement Element);

	/**
	 * The element's name as it should be shown to someone, rather than the identifier the console
	 * and the config file use. Pass it through Localize at the point it is drawn.
	 */
	static const char *ElementDisplayName(EHudElement Element);
	static const char *AnchorName(EHudAnchor Anchor);
	static const char *PushDirectionName(EHudPushDirection Direction);

	/**
	 * Whether an element can be moved and scaled, as opposed to only being measured.
	 *
	 * The media island reports where it is so that other elements can be stacked against it, but
	 * it does its own hover testing and clipping in untransformed coordinates, so moving it would
	 * put its hit box somewhere other than its pixels.
	 */
	static bool IsMovable(EHudElement Element);

	/**
	 * The config setting that switches an element on and off, or null where it has none or is
	 * governed by several at once.
	 */
	static int *EnabledSetting(EHudElement Element);

	/**
	 * What to call that setting in the editor, for the elements where "Enabled" would not say
	 * enough. An element named for what it shows is not always named for the setting that switches
	 * it on. Null where the plain wording is clear enough. Pass it through Localize.
	 */
	static const char *EnabledLabel(EHudElement Element);

	/**
	 * A second setting that governs the element, for the few that are switched by two things at
	 * once. The race timer is the one that matters: one setting decides whether the timer is shown
	 * at all, another whether it is presented inside the media island.
	 */
	static int *SecondEnabledSetting(EHudElement Element);
	static const char *SecondEnabledLabel(EHudElement Element);

	/**
	 * A third setting, which only applies while the second one is on.
	 *
	 * The race timer is again the case: the clock is a setting of its own while the island is off,
	 * and folded into the island while it is on, so it belongs in the island's panel only then.
	 */
	static int *ThirdEnabledSetting(EHudElement Element);
	static const char *ThirdEnabledLabel(EHudElement Element);

	/**
	 * The panel colour every HUD element draws its background with.
	 *
	 * Kept in one place so that elements sitting next to each other, which the corner rounding
	 * already merges into what looks like one surface, are actually the same shade.
	 */
	static ColorRGBA BackgroundColor() { return ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f); }

	/**
	 * Tells the layout that something large is covering part of the screen, in HUD units.
	 *
	 * The scoreboard is the one that matters. Elements underneath it are worth hiding, but only
	 * the ones actually underneath it, rather than every element on the screen.
	 */
	void SetOccluder(vec2 Pos, vec2 Size);
	void ClearOccluder() { m_OccluderSize = vec2(0.0f, 0.0f); }

	/**
	 * Whether an element is covered by whatever was last passed to SetOccluder.
	 *
	 * Asking holds the rect the answer was based on, because an element only measures itself while
	 * it draws. Left to lapse, a covered element would fall back to its nominal rect, and wherever
	 * the two disagree about being covered it would draw, measure, hide, lapse and draw again a
	 * few frames apart, which reads as a flicker.
	 */
	bool IsOccluded(EHudElement Element);

	/**
	 * The scale actually applied to an element, after clamping.
	 */
	float ElementScale(EHudElement Element) const;

	/**
	 * Converts between an element's own untransformed coordinates and the base HUD screen.
	 *
	 * An element that hit tests its own hover box, clips itself, or hands its rect to another
	 * component has to cross that boundary by hand, because those all sit outside the drawing that
	 * the screen mapping takes care of.
	 */
	vec2 ToBaseSpace(EHudElement Element, vec2 Pos) const;

	/**
	 * The horizontal room left free by every other element that is on screen and overlaps the
	 * given band, either side of a preferred x.
	 *
	 * For an element that sizes itself to whatever space is going, rather than one that stacks
	 * against a single named neighbour. Anything not currently drawing is ignored, so the space
	 * opens up as soon as its occupant disappears.
	 */
	void FreeSpanX(EHudElement Element, float Y, float Height, float PreferredX, float Padding, float &Left, float &Right) const;
	vec2 ToElementSpace(EHudElement Element, vec2 Pos) const;

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

private:
	/**
	 * The last rect an element reported, and the frame it did so on. An element that stops
	 * rendering, because it was switched off, must fall back to its nominal rect rather than
	 * keeping a rect that no longer reflects anything.
	 */
	/**
	 * Stops an element's last measurement ageing out over a frame in which it did not draw.
	 */
	void HoldNaturalRect(EHudElement Element);

	class CMeasurement
	{
	public:
		CRect m_Rect;
		int m_Frame = -1;
	};

	// Nominal 16:9 stand-in so that rects queried before the first HUD frame, for example by
	// hud_list run from a config file, are not measured against a zero-width screen.
	vec2 m_BaseSize = vec2(300.0f * 16.0f / 9.0f, 300.0f);
	CPlacement m_aPlacements[(int)EHudElement::NUM_HUD_ELEMENTS];
	bool m_ContainersDirty = false;
	bool m_ContainerResetSuspended = false;
	CMeasurement m_aMeasurements[(int)EHudElement::NUM_HUD_ELEMENTS];
	CRect m_aNominal[(int)EHudElement::NUM_HUD_ELEMENTS];

	/**
	 * Whether an element has told the layout where it rests. Never having said is not the same as
	 * having said there is nowhere: the first falls back to the table, the second means the element
	 * is not there at all.
	 */
	enum class ENominalState
	{
		UNSAID,
		REPORTED,
		WITHDRAWN,
	};
	ENominalState m_aNominalState[(int)EHudElement::NUM_HUD_ELEMENTS] = {};
	int m_Frame = 0;

	/**
	 * Elements are placed once a frame, highest priority first. Everything already placed is
	 * fixed, so a lower priority element is shoved clear of all of them and can never shove back.
	 * That is what keeps the result stable instead of two elements trading places every frame.
	 */
	vec2 m_aPushOffsets[(int)EHudElement::NUM_HUD_ELEMENTS];
	EHudElement m_Held = EHudElement::NUM_HUD_ELEMENTS;
	bool m_PushSuspended = false;
	vec2 m_OccluderPos = vec2(0.0f, 0.0f);
	vec2 m_OccluderSize = vec2(0.0f, 0.0f);
	CRect m_aPlacedRects[(int)EHudElement::NUM_HUD_ELEMENTS];

	void SolvePushes();

	/**
	 * Closes a hairline gap between an element and a neighbour it has ended up almost touching.
	 */
	void SnapFlush(EHudElement Element, const CRect &Rect, const int *pPlaced, int NumPlaced);

	/**
	 * The user's own offset plus whatever it was shoved by.
	 */
	vec2 TotalOffset(EHudElement Element) const;

	static constexpr int MAX_PUSH_PASSES = 8;

	bool IsDefault(EHudElement Element) const;

	CRect RectForOffset(EHudElement Element, vec2 Offset) const;

	/**
	 * The affine transform an element is drawn under: appear(n) = Scale * n + Translation.
	 * Returns false when the element is left exactly where it draws itself.
	 */
	bool ElementTransform(EHudElement Element, vec2 &Translation, float &Scale) const;

	bool Begin(EHudElement Element, vec2 WorkingSize);
	void End(vec2 WorkingSize);

	static vec2 AnchorPoint(const CRect &Rect, EHudAnchor Anchor);

	static bool ElementByName(const char *pName, EHudElement *pOut);
	static bool AnchorByName(const char *pName, EHudAnchor *pOut);

	static void ConHudMove(IConsole::IResult *pResult, void *pUserData);
	static void ConHudScale(IConsole::IResult *pResult, void *pUserData);
	static void ConHudAnchor(IConsole::IResult *pResult, void *pUserData);
	static void ConHudPush(IConsole::IResult *pResult, void *pUserData);
	static void ConHudAttach(IConsole::IResult *pResult, void *pUserData);
	static void ConHudHideCovered(IConsole::IResult *pResult, void *pUserData);
	static void ConHudReset(IConsole::IResult *pResult, void *pUserData);
	static void ConHudList(IConsole::IResult *pResult, void *pUserData);
};

#endif
