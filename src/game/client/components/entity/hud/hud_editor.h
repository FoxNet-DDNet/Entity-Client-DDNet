// EClient
#ifndef GAME_CLIENT_COMPONENTS_ENTITY_HUD_HUD_EDITOR_H
#define GAME_CLIENT_COMPONENTS_ENTITY_HUD_HUD_EDITOR_H

#include "hud_layout.h"

#include <base/color.h>
#include <base/vmath.h>

#include <engine/console.h>
#include <engine/input.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <cstdint>
#include <optional>

class CButtonContainer;

/**
 * In game overlay for placing HUD elements by hand.
 *
 * Toggled with the hud_editor command. While it is up the cursor is taken over, every element the
 * layout knows about is outlined, and dragging one writes straight into its placement. Nothing is
 * stored here: the editor only ever moves values that already live in CHudLayout, which is what
 * saves them.
 */
class CHudEditor : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	bool IsActive() const { return m_Active; }

	/**
	 * Whether the HUD should draw every element filled out with stand in data this frame.
	 */
	bool IsPreviewing() const;

	void ToggleHudEditor();

private:
	enum class EDrag
	{
		NONE,
		MOVE,
		RESIZE,
	};

	bool m_Active = false;

	EDrag m_Drag = EDrag::NONE;
	EHudElement m_DragElement = EHudElement::HEALTH_AMMO;
	vec2 m_DragStartMouse = vec2(0.0f, 0.0f);
	vec2 m_DragStartOffset = vec2(0.0f, 0.0f);
	float m_DragStartScale = 1.0f;
	float m_DragStartReach = 0.0f;

	bool m_SnapDisabled = false;
	// The lines the dragged element latched onto this frame, drawn so the snap is visible
	std::optional<float> m_SnapLineX;
	std::optional<float> m_SnapLineY;

	vec2 m_BaseSize = vec2(0.0f, 0.0f);

	// The properties popup, opened by right clicking an element. Its rect is kept so that clicks
	// landing on it can be told apart from clicks meant for the elements underneath.
	std::optional<EHudElement> m_Popup;
	// Where the panel was put when it opened, so it stays there rather than following its element
	std::optional<vec2> m_PopupAnchor;
	CUIRect m_PopupRect = {0.0f, 0.0f, 0.0f, 0.0f};

	CHudLayout &Layout() const;

	void SetActive(bool Active);
	void SetUiMousePos(vec2 Pos);

	/**
	 * The cursor in HUD units. The ui screen is 600 tall against the HUD's 300, so the two spaces
	 * differ by a constant factor.
	 */
	vec2 MousePos() const;

	/**
	 * One of an element's four corners, numbered top left, top right, bottom left, bottom right.
	 */
	vec2 CornerPos(EHudElement Element, int Corner) const;

	std::optional<int> CornerAt(EHudElement Element, vec2 Pos) const;

	std::optional<EHudElement> ElementAt(vec2 Pos) const;

	/**
	 * Nudges an offset so the dragged element lines up with a screen edge, a screen centre line
	 * or an edge of another element, unless snapping is held off.
	 */
	void SnapOffset(EHudElement Element, vec2 &Offset);

	/**
	 * Pulls a scale onto one that makes the element exactly as wide or as tall as a neighbour.
	 */
	float SnapScale(EHudElement Element, float Scale) const;

	void RenderElement(EHudElement Element, bool Hovered) const;
	void RenderSnapLines() const;
	void RenderHoveredInfo(EHudElement Element) const;
	void RenderHelp(vec2 Mouse) const;

	/**
	 * The properties popup for the element it was opened on. Drawn in ui coordinates rather than
	 * HUD ones, because that is the space CUi hit tests its widgets in.
	 */
	void RenderPopup();
	ColorRGBA ControlColor(CButtonContainer *pId);
	bool DoButton(CButtonContainer *pId, const CUIRect *pRect, const char *pLabel, ColorRGBA Color);
	bool DoCheckBox(CButtonContainer *pId, const CUIRect *pRect, const char *pLabel, bool Checked);
	void DoCycler(CButtonContainer *pIdLess, CButtonContainer *pIdMore, const CUIRect *pRect, const char *pLabel, const char *pValue, int *pDelta);
	int64_t DoNumber(const void *pId, CButtonContainer *pIdLess, CButtonContainer *pIdMore, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, int64_t Step);

	static void ConHudEditor(IConsole::IResult *pResult, void *pUserData);

	static constexpr int NUM_CORNERS = 4;
	static constexpr float HANDLE_SIZE = 4.0f;
	static constexpr float SNAP_DISTANCE = 3.0f;
	// A line already latched onto holds until the element is this far from it, so hovering at the
	// edge of the snap radius does not flicker in and out
	static constexpr float SNAP_RELEASE_DISTANCE = 6.0f;
	static constexpr float HELP_FADE_DISTANCE = 20.0f;
};

#endif
