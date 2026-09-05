// EClient
#include "hud_editor.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>

static const ColorRGBA COLOR_ELEMENT = ColorRGBA(0.10f, 0.20f, 0.60f, 0.05f);
static const ColorRGBA COLOR_ELEMENT_HOVERED = ColorRGBA(0.20f, 0.55f, 1.00f, 0.15f);
static const ColorRGBA COLOR_BORDER = ColorRGBA(0.30f, 0.65f, 0.90f, 0.60f);
static const ColorRGBA COLOR_HANDLE = ColorRGBA(0.90f, 0.75f, 0.20f, 0.90f);
// Something other elements can be stacked against, but which cannot itself be moved
static const ColorRGBA COLOR_FIXED = ColorRGBA(0.55f, 0.55f, 0.60f, 0.12f);
static const ColorRGBA COLOR_FIXED_BORDER = ColorRGBA(0.70f, 0.70f, 0.75f, 0.70f);
static const ColorRGBA COLOR_SNAP_LINE = ColorRGBA(1.00f, 0.85f, 0.30f, 0.60f);
static const ColorRGBA COLOR_POPUP = ColorRGBA(0.10f, 0.10f, 0.12f, 0.92f);
static const ColorRGBA COLOR_POPUP_CONTROL = ColorRGBA(0.30f, 0.30f, 0.34f, 0.85f);
static const ColorRGBA COLOR_POPUP_DANGER = ColorRGBA(0.45f, 0.08f, 0.08f, 0.90f);
static const ColorRGBA COLOR_POPUP_BORDER = ColorRGBA(0.55f, 0.60f, 0.70f, 0.90f);
static const ColorRGBA COLOR_POPUP_FIELD = ColorRGBA(0.05f, 0.05f, 0.06f, 0.85f);

static constexpr float POPUP_FONT_SIZE = 10.0f;
static constexpr float POPUP_TITLE_SIZE = 12.0f;

// An element that is not drawing right now is only showing its nominal rect, so it is drawn faint
static constexpr float DORMANT_ALPHA = 0.35f;

// Matches the rounding the HUD's own panels are drawn with, so the boxes sit over them rather than
// squaring off their corners
static constexpr float ELEMENT_ROUNDING = 2.0f;

CHudLayout &CHudEditor::Layout() const
{
	return GameClient()->m_Hud.HudLayout();
}

void CHudEditor::OnReset()
{
	SetActive(false);
}

void CHudEditor::SetUiMousePos(vec2 Pos)
{
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pScreen = Ui()->Screen();
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();

	Pos = Pos / vec2(pScreen->w, pScreen->h) * WindowSize;
	Ui()->OnCursorMove(Pos.x - UpdatedMousePos.x, Pos.y - UpdatedMousePos.y);
}

void CHudEditor::SetActive(bool Active)
{
	const bool WasActive = m_Active;
	m_Active = Active;

	// Cleared whether or not this is a change of state. Anything that stops the editor without
	// going through here, a disconnect or a reset mid drag, would otherwise leave the layout
	// holding an element that then never gets pushed or pulled flush again.
	m_Drag = EDrag::NONE;
	m_SnapDisabled = false;
	m_Popup.reset();
	Layout().ClearHeld();
	Layout().SetPushSuspended(false);
	// Leaving this on would stop the HUD ever rebuilding its containers again
	Layout().SetContainerResetSuspended(false);

	if(m_Active && !WasActive)
	{
		// Whatever else was holding the cursor gets out of the way, rather than the editor
		// refusing to open on top of it
		GameClient()->m_GameConsole.Close();
		GameClient()->m_Chat.DisableMode();
		if(GameClient()->m_Menus.IsActive())
			GameClient()->m_Menus.SetActive(false);
		GameClient()->m_Scoreboard.OnRelease();

		// Start in the middle rather than wherever the aim cursor happened to be sitting
		SetUiMousePos(Ui()->Screen()->Center());
	}
}

vec2 CHudEditor::MousePos() const
{
	const CUIRect *pScreen = Ui()->Screen();
	return Ui()->MousePos() / vec2(pScreen->w, pScreen->h) * m_BaseSize;
}

vec2 CHudEditor::CornerPos(EHudElement Element, int Corner) const
{
	const CHudLayout::CRect Rect = Layout().ResolvedRect(Element);
	const float FractionX = (Corner == 0 || Corner == 2) ? 0.0f : 1.0f;
	const float FractionY = Corner < 2 ? 0.0f : 1.0f;
	return Rect.m_Pos + vec2(Rect.m_Size.x * FractionX, Rect.m_Size.y * FractionY);
}

std::optional<int> CHudEditor::CornerAt(EHudElement Element, vec2 Pos) const
{
	for(int Corner = 0; Corner < NUM_CORNERS; Corner++)
	{
		if(distance(Pos, CornerPos(Element, Corner)) <= HANDLE_SIZE)
			return Corner;
	}
	return std::nullopt;
}

std::optional<EHudElement> CHudEditor::ElementAt(vec2 Pos) const
{
	std::optional<EHudElement> Found;
	float SmallestArea = 0.0f;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		if(!CHudLayout::IsMovable((EHudElement)i) || !Layout().IsPresent((EHudElement)i))
			continue;

		const CHudLayout::CRect Rect = Layout().ResolvedRect((EHudElement)i);
		if(Rect.m_Size.x <= 0.0f || Rect.m_Size.y <= 0.0f)
			continue;
		if(Pos.x < Rect.m_Pos.x || Pos.x > Rect.m_Pos.x + Rect.m_Size.x ||
			Pos.y < Rect.m_Pos.y || Pos.y > Rect.m_Pos.y + Rect.m_Size.y)
			continue;

		// Overlapping elements would otherwise make the one underneath unreachable
		const float Area = Rect.m_Size.x * Rect.m_Size.y;
		if(!Found.has_value() || Area < SmallestArea)
		{
			Found = (EHudElement)i;
			SmallestArea = Area;
		}
	}

	return Found;
}

void CHudEditor::SnapOffset(EHudElement Element, vec2 &Offset)
{
	m_SnapLineX.reset();
	m_SnapLineY.reset();

	if(m_SnapDisabled)
		return;

	Layout().SetOffset(Element, Offset);
	const CHudLayout::CRect Rect = Layout().ResolvedRect(Element);

	// A line that has already been latched onto is kept as a fixed number and released only once
	// the element is clearly away from it, so a line that shifts underneath cannot drag the
	// element along with it
	const std::optional<float> HeldX = m_SnapLineX;
	const std::optional<float> HeldY = m_SnapLineY;

	float aLinesX[3 + (int)EHudElement::NUM_HUD_ELEMENTS * 3];
	float aLinesY[3 + (int)EHudElement::NUM_HUD_ELEMENTS * 3];
	int NumLinesX = 0;
	int NumLinesY = 0;

	if(HeldX.has_value())
		aLinesX[NumLinesX++] = HeldX.value();
	if(HeldY.has_value())
		aLinesY[NumLinesY++] = HeldY.value();

	aLinesX[NumLinesX++] = 0.0f;
	aLinesX[NumLinesX++] = m_BaseSize.x * 0.5f;
	aLinesX[NumLinesX++] = m_BaseSize.x;
	aLinesY[NumLinesY++] = 0.0f;
	aLinesY[NumLinesY++] = m_BaseSize.y * 0.5f;
	aLinesY[NumLinesY++] = m_BaseSize.y;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		if((EHudElement)i == Element)
			continue;

		const CHudLayout::CRect Other = Layout().ResolvedRect((EHudElement)i);
		if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
			continue;

		aLinesX[NumLinesX++] = Other.m_Pos.x;
		aLinesX[NumLinesX++] = Other.m_Pos.x + Other.m_Size.x * 0.5f;
		aLinesX[NumLinesX++] = Other.m_Pos.x + Other.m_Size.x;
		aLinesY[NumLinesY++] = Other.m_Pos.y;
		aLinesY[NumLinesY++] = Other.m_Pos.y + Other.m_Size.y * 0.5f;
		aLinesY[NumLinesY++] = Other.m_Pos.y + Other.m_Size.y;
	}

	const float aEdgesX[3] = {Rect.m_Pos.x, Rect.m_Pos.x + Rect.m_Size.x * 0.5f, Rect.m_Pos.x + Rect.m_Size.x};
	const float aEdgesY[3] = {Rect.m_Pos.y, Rect.m_Pos.y + Rect.m_Size.y * 0.5f, Rect.m_Pos.y + Rect.m_Size.y};

	float BestX = 0.0f;
	float BestDistanceX = SNAP_RELEASE_DISTANCE;
	for(int i = 0; i < NumLinesX; i++)
	{
		const bool Latched = HeldX.has_value() && aLinesX[i] == HeldX.value();
		const float Limit = Latched ? SNAP_RELEASE_DISTANCE : SNAP_DISTANCE;
		for(const float Edge : aEdgesX)
		{
			const float Delta = aLinesX[i] - Edge;
			if(absolute(Delta) < Limit && absolute(Delta) < BestDistanceX)
			{
				BestDistanceX = absolute(Delta);
				BestX = Delta;
				m_SnapLineX = aLinesX[i];
			}
		}
	}

	float BestY = 0.0f;
	float BestDistanceY = SNAP_RELEASE_DISTANCE;
	for(int i = 0; i < NumLinesY; i++)
	{
		const bool Latched = HeldY.has_value() && aLinesY[i] == HeldY.value();
		const float Limit = Latched ? SNAP_RELEASE_DISTANCE : SNAP_DISTANCE;
		for(const float Edge : aEdgesY)
		{
			const float Delta = aLinesY[i] - Edge;
			if(absolute(Delta) < Limit && absolute(Delta) < BestDistanceY)
			{
				BestDistanceY = absolute(Delta);
				BestY = Delta;
				m_SnapLineY = aLinesY[i];
			}
		}
	}

	// Both axes together, so an element can land on a corner rather than having to be nudged onto
	// one edge and then the other. What stops that jittering is the latching above: each axis
	// holds the line it caught instead of picking a new one every frame as its neighbours shift.
	Offset += vec2(BestX, BestY);
}

void CHudEditor::RenderElement(EHudElement Element, bool Hovered) const
{
	if(!Layout().IsPresent(Element))
		return;

	const CHudLayout::CRect Rect = Layout().ResolvedRect(Element);
	if(Rect.m_Size.x <= 0.0f || Rect.m_Size.y <= 0.0f)
		return;

	const bool Live = Layout().IsLive(Element);
	const bool Movable = CHudLayout::IsMovable(Element);

	const float Alpha = Live ? 1.0f : DORMANT_ALPHA;

	ColorRGBA Fill = Movable ? (Hovered ? COLOR_ELEMENT_HOVERED : COLOR_ELEMENT) : COLOR_FIXED;
	Fill.a *= Alpha;
	Graphics()->DrawRect(Rect.m_Pos.x, Rect.m_Pos.y, Rect.m_Size.x, Rect.m_Size.y, Fill, IGraphics::CORNER_ALL, ELEMENT_ROUNDING);

	ColorRGBA Border = Movable ? COLOR_BORDER : COLOR_FIXED_BORDER;
	Border.a *= Alpha;
	Graphics()->DrawRectOutline(Rect.m_Pos.x, Rect.m_Pos.y, Rect.m_Size.x, Rect.m_Size.y, Border,
		IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, ELEMENT_ROUNDING);

	// Two short strokes meeting at each corner read as a grab point without covering the element.
	// An element that cannot be moved gets none, so it does not look draggable.
	if(Hovered || (m_Drag != EDrag::NONE && m_DragElement == Element))
	{
		ColorRGBA Handle = COLOR_HANDLE;
		Handle.a *= Alpha;
		const float Thickness = 0.8f;
		// Kept short enough that the arms reaching in from opposite corners cannot meet and lay
		// over each other along a short edge
		const float Arm = std::min(HANDLE_SIZE, std::min(Rect.m_Size.x, Rect.m_Size.y) * 0.45f);
		for(int Corner = 0; Movable && Corner < NUM_CORNERS; Corner++)
		{
			const vec2 Pos = CornerPos(Element, Corner);
			const float DirX = (Corner == 0 || Corner == 2) ? 1.0f : -1.0f;
			const float DirY = Corner < 2 ? 1.0f : -1.0f;

			// The horizontal stroke already covers the square where the two meet, so the vertical
			// one starts past it. Overlapping them draws that square twice, and at this alpha a
			// twice drawn corner is visibly brighter than the arms leading into it.
			const float ArmX = DirX > 0.0f ? Pos.x : Pos.x - Arm;
			const float ArmY = DirY > 0.0f ? Pos.y : Pos.y - Thickness;
			Graphics()->DrawRect(ArmX, ArmY, Arm, Thickness, Handle, IGraphics::CORNER_NONE, 0.0f);

			const float StemX = DirX > 0.0f ? Pos.x : Pos.x - Thickness;
			const float StemY = DirY > 0.0f ? Pos.y + Thickness : Pos.y - Arm;
			Graphics()->DrawRect(StemX, StemY, Thickness, Arm - Thickness, Handle, IGraphics::CORNER_NONE, 0.0f);
		}
	}
}

float CHudEditor::SnapScale(EHudElement Element, float Scale) const
{
	if(m_SnapDisabled)
		return Scale;

	const CHudLayout::CRect Natural = Layout().NaturalRect(Element);
	if(Natural.m_Size.x <= 0.0f || Natural.m_Size.y <= 0.0f)
		return Scale;

	// Matching another element by eye is hopeless, because one step of scale moves the edge by a
	// fraction of a unit and the difference that is left is smaller than a pixel but still visible
	// as a seam. Landing exactly on a neighbour's width or height is what people are aiming at, so
	// those are offered as snap targets the same way edges are when moving.
	float Best = Scale;
	float BestDistance = SNAP_DISTANCE;

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		if((EHudElement)i == Element || !Layout().IsLive((EHudElement)i))
			continue;

		const CHudLayout::CRect Other = Layout().ResolvedRect((EHudElement)i);
		if(Other.m_Size.x <= 0.0f || Other.m_Size.y <= 0.0f)
			continue;

		const float aTargets[2] = {Other.m_Size.x / Natural.m_Size.x, Other.m_Size.y / Natural.m_Size.y};
		const float aExtents[2] = {Natural.m_Size.x, Natural.m_Size.y};
		for(int Axis = 0; Axis < 2; Axis++)
		{
			// Measured as the gap it would leave on screen rather than as a difference in scale,
			// so a large element and a small one both snap once they are a couple of units apart
			const float Distance = absolute((aTargets[Axis] - Scale) * aExtents[Axis]);
			if(Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = aTargets[Axis];
			}
		}
	}

	return Best;
}

void CHudEditor::RenderSnapLines() const
{
	const float Thickness = 0.4f;
	if(m_SnapLineX.has_value())
		Graphics()->DrawRect(m_SnapLineX.value() - Thickness * 0.5f, 0.0f, Thickness, m_BaseSize.y, COLOR_SNAP_LINE, IGraphics::CORNER_NONE, 0.0f);
	if(m_SnapLineY.has_value())
		Graphics()->DrawRect(0.0f, m_SnapLineY.value() - Thickness * 0.5f, m_BaseSize.x, Thickness, COLOR_SNAP_LINE, IGraphics::CORNER_NONE, 0.0f);
}

void CHudEditor::RenderHoveredInfo(EHudElement Element) const
{
	if(m_Drag != EDrag::NONE && m_DragElement == Element)
		return;

	const CHudLayout::CRect Rect = Layout().ResolvedRect(Element);

	const char *pName = Localize(CHudLayout::ElementDisplayName(Element));

	const float FontSize = 6.0f;
	const float TextWidth = TextRender()->TextWidth(FontSize, pName);
	const float BoxWidth = TextWidth + 6.0f;
	const float BoxHeight = FontSize + 5.0f;

	// Centred on the element by the box, not by the text inside it, since the box is wider than
	// its text by the padding either side
	float BoxX = std::min(Rect.m_Pos.x + Rect.m_Size.x * 0.5f - BoxWidth * 0.5f, m_BaseSize.x - BoxWidth);
	BoxX = std::max(BoxX, 0.0f);

	// Above the element, dropping below it only when there is genuinely no room left above
	float BoxY = Rect.m_Pos.y - BoxHeight - 1.0f;
	if(BoxY < 0.0f)
		BoxY = Rect.m_Pos.y + Rect.m_Size.y + 1.0f;

	Graphics()->DrawRect(BoxX, BoxY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.55f), IGraphics::CORNER_ALL, 2.0f);
	TextRender()->Text(BoxX + 3.0f, BoxY + 2.5f, FontSize, pName, -1.0f);
}

void CHudEditor::RenderHelp(vec2 Mouse) const
{
	const float FontSize = 6.0f;
	const char *pHelp = Localize("Drag to move, corner to resize, right click for options, alt ignores snapping, shift shows pushing, ctrl shows everything, esc to leave");

	const float Width = TextRender()->TextWidth(FontSize, pHelp);
	const float BoxWidth = Width + 8.0f;
	const float BoxHeight = FontSize + 4.0f;
	const float BoxX = (m_BaseSize.x - BoxWidth) * 0.5f;
	const float BoxY = m_BaseSize.y - BoxHeight - 3.0f;

	// Fades out as the cursor approaches so it never sits between you and an element down there
	const float DistanceX = std::max(std::max(BoxX - Mouse.x, Mouse.x - (BoxX + BoxWidth + 80.0f)), 0.0f);
	const float DistanceY = std::max(std::max(BoxY - Mouse.y, Mouse.y - (BoxY + BoxHeight + 100.0f)), 0.0f);
	const float Distance = std::sqrt(DistanceX * DistanceX + DistanceY * DistanceY);
	const float Alpha = std::clamp(Distance / HELP_FADE_DISTANCE, 0.0f, 1.0f);
	if(Alpha <= 0.0f)
		return;

	Graphics()->DrawRect(BoxX, BoxY, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.35f * Alpha), IGraphics::CORNER_ALL, 2.0f);
	CTextCursor Cursor;
	Cursor.SetPosition(vec2(BoxX + 4.0f, BoxY + 2.0f));
	Cursor.m_FontSize = FontSize;

	STextContainerIndex Container;
	TextRender()->CreateTextContainer(Container, &Cursor, pHelp);
	if(Container.Valid())
	{
		TextRender()->RenderTextContainer(Container,
			TextRender()->DefaultTextColor().WithMultipliedAlpha(Alpha),
			TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha));
		TextRender()->DeleteTextContainer(Container);
	}
}

ColorRGBA CHudEditor::ControlColor(CButtonContainer *pId)
{
	// DoButton_FontIcon only folds in the hover and press multiplier when it picks the colour
	// itself, so a caller handing it one has to do that part.
	//
	// The multiplier goes on the colour rather than the alpha. These controls sit on an almost
	// opaque panel, so raising the alpha of an already opaque fill changes almost nothing, whereas
	// lightening it actually reads as a highlight.
	const float Mul = Ui()->ButtonColorMul(pId);
	return ColorRGBA(COLOR_POPUP_CONTROL.r * Mul, COLOR_POPUP_CONTROL.g * Mul, COLOR_POPUP_CONTROL.b * Mul, COLOR_POPUP_CONTROL.a);
}

bool CHudEditor::DoButton(CButtonContainer *pId, const CUIRect *pRect, const char *pLabel, ColorRGBA Color)
{
	// ButtonColorMul is what every other button in the client brightens with, so these react the
	// same way rather than having feedback of their own invention. It lightens the colour rather
	// than the alpha, which on an opaque panel is the part that shows.
	const float Mul = Ui()->ButtonColorMul(pId);
	pRect->Draw(ColorRGBA(Color.r * Mul, Color.g * Mul, Color.b * Mul, Color.a), IGraphics::CORNER_ALL, 3.0f);
	Ui()->DoLabel(pRect, pLabel, POPUP_FONT_SIZE, TEXTALIGN_MC);
	return Ui()->DoButtonLogic(pId, 0, pRect, BUTTONFLAG_LEFT) != 0;
}

bool CHudEditor::DoCheckBox(CButtonContainer *pId, const CUIRect *pRect, const char *pLabel, bool Checked)
{
	// Same shape as CMenus::DoButton_CheckBox_Common, so it reads as the checkbox people know
	CUIRect Box, Label;
	pRect->VSplitLeft(pRect->h, &Box, &Label);
	Label.VSplitLeft(5.0f, nullptr, &Label);

	Box.Margin(2.0f, &Box);
	Box.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f * Ui()->ButtonColorMul(pId)), IGraphics::CORNER_ALL, 3.0f);

	if(Checked)
	{
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		Ui()->DoLabel(&Box, FontIcon::XMARK, Box.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		TextRender()->SetRenderFlags(0);
	}

	Ui()->DoLabel(&Label, pLabel, POPUP_FONT_SIZE, TEXTALIGN_ML);
	return Ui()->DoButtonLogic(pId, 0, pRect, BUTTONFLAG_LEFT) != 0;
}

void CHudEditor::DoCycler(CButtonContainer *pIdLess, CButtonContainer *pIdMore, const CUIRect *pRect, const char *pLabel, const char *pValue, int *pDelta)
{
	// Arrows rather than plus and minus, because these step through a list of names where there is
	// nothing to add to
	CUIRect Label, Shifter, Less, More;
	pRect->VSplitLeft(pRect->w * 0.38f, &Label, &Shifter);
	Shifter.VSplitLeft(14.0f, &Less, &Shifter);
	Shifter.VSplitRight(14.0f, &Shifter, &More);

	Ui()->DoLabel(&Label, pLabel, POPUP_FONT_SIZE, TEXTALIGN_ML);
	Shifter.Draw(COLOR_POPUP_FIELD, IGraphics::CORNER_NONE, 0.0f);
	Ui()->DoLabel(&Shifter, pValue, POPUP_FONT_SIZE, TEXTALIGN_MC);

	*pDelta = 0;
	if(Ui()->DoButton_FontIcon(pIdLess, FontIcon::CHEVRON_LEFT, 0, &Less, BUTTONFLAG_LEFT, IGraphics::CORNER_L, true, ControlColor(pIdLess)))
		*pDelta = -1;
	if(Ui()->DoButton_FontIcon(pIdMore, FontIcon::CHEVRON_RIGHT, 0, &More, BUTTONFLAG_LEFT, IGraphics::CORNER_R, true, ControlColor(pIdMore)))
		*pDelta = 1;
}

int64_t CHudEditor::DoNumber(const void *pId, CButtonContainer *pIdLess, CButtonContainer *pIdMore, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, int64_t Step)
{
	// The map editor's property shifter: minus and plus either side of a value that can also be
	// dragged, or right clicked to type
	CUIRect Label, Shifter, Less, More;
	pRect->VSplitLeft(pRect->w * 0.38f, &Label, &Shifter);
	Shifter.VSplitLeft(14.0f, &Less, &Shifter);
	Shifter.VSplitRight(14.0f, &Shifter, &More);

	Ui()->DoLabel(&Label, pLabel, POPUP_FONT_SIZE, TEXTALIGN_ML);

	SValueSelectorProperties Props;
	Props.m_Step = Step;
	Props.m_Color = COLOR_POPUP_FIELD;
	const SEditResult<int64_t> Result = Ui()->DoValueSelectorWithState(pId, &Shifter, "", Current, Min, Max, Props);
	if(Result.m_State == EEditState::START || Result.m_State == EEditState::EDITING)
		m_ValueEditing = true;
	int64_t Value = Result.m_Value;

	if(Ui()->DoButton_FontIcon(pIdLess, FontIcon::MINUS, 0, &Less, BUTTONFLAG_LEFT, IGraphics::CORNER_L, true, ControlColor(pIdLess)))
		Value = Current - Step;
	if(Ui()->DoButton_FontIcon(pIdMore, FontIcon::PLUS, 0, &More, BUTTONFLAG_LEFT, IGraphics::CORNER_R, true, ControlColor(pIdMore)))
		Value = Current + Step;

	return std::clamp(Value, Min, Max);
}

void CHudEditor::RenderPopup()
{
	m_ValueEditing = false;

	if(!m_Popup.has_value())
	{
		m_PopupRect = {0.0f, 0.0f, 0.0f, 0.0f};
		return;
	}

	const EHudElement Element = m_Popup.value();
	const CHudLayout::CPlacement &Placement = Layout().Placement(Element);

	// CUi hit tests in its own screen, so the panel is laid out there rather than in HUD units
	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();

	const float LineHeight = 18.0f;
	const float Spacing = 4.0f;
	const float Padding = 7.0f;
	int *pEnabled = CHudLayout::EnabledSetting(Element);
	int *pEnabled2 = CHudLayout::SecondEnabledSetting(Element);
	// Only while the setting above it is on, which is the whole point of it being a third
	int *pEnabled3 = CHudLayout::ThirdEnabledSetting(Element);
	if(pEnabled2 == nullptr || *pEnabled2 == 0)
		pEnabled3 = nullptr;
	const bool Pushes = Placement.m_PushDirection != EHudPushDirection::NONE;
	const bool Attached = Placement.m_AttachTarget != EHudElement::NUM_HUD_ELEMENTS;

	// Title, hide when covered, anchor, push, priority, scale, attach target, the two resets side
	// by side and the full reset, plus the rows that only appear when they have something to say.
	// Counted rather than guessed, so the panel is never left with a dead strip along the bottom or
	// a row cut off the end of it.
	const int NumRows = 9 + (pEnabled == nullptr ? 0 : 1) + (pEnabled2 == nullptr ? 0 : 1) + (pEnabled3 == nullptr ? 0 : 1) + (Pushes ? 1 : 0) + (Attached ? 2 : 0);

	CUIRect Popup;
	Popup.w = 210.0f;
	Popup.h = Padding * 2.0f + LineHeight * NumRows + Spacing * (NumRows - 1);
	// Placed beside the element it belongs to rather than under the cursor, so it never covers the
	// thing being edited. Right first, then left, then below, then above, taking the first side it
	// actually fits on. The ui screen is twice the height of the HUD's, hence the doubling.
	//
	// Worked out once, when the panel opens. Recomputing it every frame means the panel chases the
	// element around as it is pushed or resized, which is distracting to read while using it.
	const CHudLayout::CRect Element2D = Layout().ResolvedRect(Element);
	const CUIRect Target = {Element2D.m_Pos.x * 2.0f, Element2D.m_Pos.y * 2.0f, Element2D.m_Size.x * 2.0f, Element2D.m_Size.y * 2.0f};
	const float Gap = 5.0f;

	float WantedX, WantedY;
	if(Target.x + Target.w + Gap + Popup.w <= Screen.w)
	{
		WantedX = Target.x + Target.w + Gap;
		WantedY = Target.y;
	}
	else if(Target.x - Gap - Popup.w >= 0.0f)
	{
		WantedX = Target.x - Gap - Popup.w;
		WantedY = Target.y;
	}
	else if(Target.y + Target.h + Gap + Popup.h <= Screen.h)
	{
		WantedX = Target.x + Target.w * 0.5f - Popup.w * 0.5f;
		WantedY = Target.y + Target.h + Gap;
	}
	else
	{
		WantedX = Target.x + Target.w * 0.5f - Popup.w * 0.5f;
		WantedY = Target.y - Gap - Popup.h;
	}

	if(!m_PopupAnchor.has_value())
	{
		// Held inside the screen, for the case where no side had room for it
		m_PopupAnchor = vec2(std::clamp(WantedX, 0.0f, Screen.w - Popup.w), std::clamp(WantedY, 0.0f, Screen.h - Popup.h));
	}
	Popup.x = m_PopupAnchor.value().x;
	Popup.y = m_PopupAnchor.value().y;
	m_PopupRect = Popup;

	Popup.Draw(COLOR_POPUP, IGraphics::CORNER_ALL, 4.0f);
	Graphics()->DrawRectOutline(Popup.x, Popup.y, Popup.w, Popup.h, COLOR_POPUP_BORDER,
		IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, 4.0f);

	CUIRect Body;
	Popup.Margin(Padding, &Body);

	CUIRect Row;
	Body.HSplitTop(LineHeight, &Row, &Body);
	Ui()->DoLabel(&Row, Localize(CHudLayout::ElementDisplayName(Element)), POPUP_TITLE_SIZE, TEXTALIGN_ML);

	auto NextRow = [&]() {
		Body.HSplitTop(Spacing, nullptr, &Body);
		Body.HSplitTop(LineHeight, &Row, &Body);
	};

	if(pEnabled != nullptr)
	{
		static CButtonContainer s_Enabled;
		NextRow();
		const char *pEnabledLabel = CHudLayout::EnabledLabel(Element);
		if(DoCheckBox(&s_Enabled, &Row, pEnabledLabel == nullptr ? Localize("Enabled") : Localize(pEnabledLabel), *pEnabled != 0))
			*pEnabled = *pEnabled != 0 ? 0 : 1;
	}

	if(pEnabled2 != nullptr)
	{
		static CButtonContainer s_Enabled2;
		const char *pLabel = CHudLayout::SecondEnabledLabel(Element);
		NextRow();
		if(DoCheckBox(&s_Enabled2, &Row, pLabel == nullptr ? Localize("Enabled") : Localize(pLabel), *pEnabled2 != 0))
			*pEnabled2 = *pEnabled2 != 0 ? 0 : 1;
	}

	if(pEnabled3 != nullptr)
	{
		static CButtonContainer s_Enabled3;
		const char *pLabel = CHudLayout::ThirdEnabledLabel(Element);
		NextRow();
		if(DoCheckBox(&s_Enabled3, &Row, pLabel == nullptr ? Localize("Enabled") : Localize(pLabel), *pEnabled3 != 0))
			*pEnabled3 = *pEnabled3 != 0 ? 0 : 1;
	}

	{
		// The scoreboard is the only thing that covers anything, so it is named rather than being
		// described as an occluder, which would mean nothing to anyone reading the panel
		static CButtonContainer s_HideWhenCovered;
		NextRow();
		if(DoCheckBox(&s_HideWhenCovered, &Row, Localize("Hide under scoreboard"), Placement.m_HideWhenCovered))
			Layout().SetHideWhenCovered(Element, !Placement.m_HideWhenCovered);
	}

	{
		static CButtonContainer s_Less, s_More;
		int Delta = 0;
		NextRow();
		DoCycler(&s_Less, &s_More, &Row, Localize("Anchor"), CHudLayout::AnchorName(Placement.m_Anchor), &Delta);
		if(Delta != 0)
		{
			const int Count = (int)EHudAnchor::NUM_HUD_ANCHORS;
			Layout().SetAnchor(Element, (EHudAnchor)((((int)Placement.m_Anchor + Delta) % Count + Count) % Count));
		}
	}

	{
		static CButtonContainer s_Less, s_More;
		int Delta = 0;
		NextRow();
		DoCycler(&s_Less, &s_More, &Row, Localize("Push"), CHudLayout::PushDirectionName(Placement.m_PushDirection), &Delta);
		if(Delta != 0)
		{
			const int Count = (int)EHudPushDirection::NUM_HUD_PUSH_DIRECTIONS;
			const EHudPushDirection Next = (EHudPushDirection)((((int)Placement.m_PushDirection + Delta) % Count + Count) % Count);
			Layout().SetPush(Element, Next, Placement.m_PushPriority, Placement.m_PushGap);
		}
	}

	// The gap only means anything alongside a direction to be pushed in, so it keeps that company
	// and stays out of the way when there is nothing to push against
	if(Pushes)
	{
		// Tenths, matching the step positions are held at
		static int s_Gap;
		static CButtonContainer s_Less, s_More;
		NextRow();
		const int64_t Current = round_to_int(Placement.m_PushGap * 10.0f);
		const int64_t Value = DoNumber(&s_Gap, &s_Less, &s_More, &Row, Localize("Push gap"), Current, 0, 400, 1);
		if(Value != Current)
			Layout().SetPush(Element, Placement.m_PushDirection, Placement.m_PushPriority, Value / 10.0f);
	}

	// Priority stays whether or not this one is pushed, since it also decides what it pushes
	{
		static int s_Priority;
		static CButtonContainer s_Less, s_More;
		NextRow();
		const int64_t Value = DoNumber(&s_Priority, &s_Less, &s_More, &Row, Localize("Priority"), Placement.m_PushPriority, 0, 200, 1);
		if(Value != Placement.m_PushPriority)
			Layout().SetPush(Element, Placement.m_PushDirection, (int)Value, Placement.m_PushGap);
	}

	{
		// Held in hundredths, which is the step the scale is stored at anyway
		static int s_Scale;
		static CButtonContainer s_Less, s_More;
		NextRow();
		const int64_t Current = round_to_int(Placement.m_Scale * 100.0f);
		// The top of the range is the one that leaves the element on the screen, and never below
		// what it is already set to, so opening the panel cannot shrink it on its own
		const int64_t Min = round_to_int(CHudLayout::MinScale() * 100.0f);
		const int64_t Max = std::max<int64_t>(round_to_int(Layout().MaxScale(Element) * 100.0f), Current);
		const int64_t Value = DoNumber(&s_Scale, &s_Less, &s_More, &Row, Localize("Scale %"), Current, Min, Max, 1);
		if(Value != Current)
			Layout().SetScale(Element, Value / 100.0f);
	}

	// Attaching is not the same as being pushed: it holds every frame rather than only when the
	// two would collide
	{
		static CButtonContainer s_Less, s_More;
		int Delta = 0;
		NextRow();
		DoCycler(&s_Less, &s_More, &Row, Localize("Attach to"),
			Attached ? Localize(CHudLayout::ElementDisplayName(Placement.m_AttachTarget)) : Localize("none"), &Delta);
		if(Delta != 0)
		{
			// Steps through every element and a none at the end, skipping itself
			const int Count = (int)EHudElement::NUM_HUD_ELEMENTS + 1;
			int Next = Attached ? (int)Placement.m_AttachTarget : (int)EHudElement::NUM_HUD_ELEMENTS;
			do
			{
				Next = ((Next + Delta) % Count + Count) % Count;
			} while(Next == (int)Element);

			if(Next == (int)EHudElement::NUM_HUD_ELEMENTS)
			{
				Layout().SetAttach(Element, EHudElement::NUM_HUD_ELEMENTS, EHudPushDirection::NONE, Placement.m_AttachGap);
			}
			else
			{
				// Picking a target with no side yet would attach it to nothing in particular, so
				// it starts on one
				const EHudPushDirection Side = Placement.m_AttachSide == EHudPushDirection::NONE ? EHudPushDirection::RIGHT : Placement.m_AttachSide;
				Layout().SetAttach(Element, (EHudElement)Next, Side, Placement.m_AttachGap);
			}
		}
	}

	if(Attached)
	{
		static CButtonContainer s_Less, s_More;
		int Delta = 0;
		NextRow();
		DoCycler(&s_Less, &s_More, &Row, Localize("Attach side"), CHudLayout::PushDirectionName(Placement.m_AttachSide), &Delta);
		if(Delta != 0)
		{
			// None would detach it, which the row above is for, so it is stepped over here
			const int Count = (int)EHudPushDirection::NUM_HUD_PUSH_DIRECTIONS - 1;
			const int Current = (int)Placement.m_AttachSide - 1;
			const EHudPushDirection Next = (EHudPushDirection)(((Current + Delta) % Count + Count) % Count + 1);
			Layout().SetAttach(Element, Placement.m_AttachTarget, Next, Placement.m_AttachGap);
		}
	}

	if(Attached)
	{
		static int s_AttachGap;
		static CButtonContainer s_Less, s_More;
		NextRow();
		const int64_t Current = round_to_int(Placement.m_AttachGap * 10.0f);
		const int64_t Value = DoNumber(&s_AttachGap, &s_Less, &s_More, &Row, Localize("Attach gap"), Current, 0, 400, 1);
		if(Value != Current)
			Layout().SetAttach(Element, Placement.m_AttachTarget, Placement.m_AttachSide, Value / 10.0f);
	}

	{
		static CButtonContainer s_Position, s_Size;
		NextRow();
		CUIRect Left, Right;
		Row.VSplitMid(&Left, &Right, Spacing);
		if(DoButton(&s_Position, &Left, Localize("Reset pos"), COLOR_POPUP_CONTROL))
			Layout().SetOffset(Element, vec2(0.0f, 0.0f));
		if(DoButton(&s_Size, &Right, Localize("Reset size"), COLOR_POPUP_CONTROL))
			Layout().SetScale(Element, 1.0f);
	}

	{
		static CButtonContainer s_Reset;
		NextRow();
		if(DoButton(&s_Reset, &Row, Localize("Reset element"), COLOR_POPUP_DANGER))
			Layout().ResetElement(Element);
	}

	// Clicking away puts the panel down again
	if(Ui()->MouseButtonClicked(0) && !Ui()->MouseInside(&m_PopupRect))
		m_Popup.reset();

	Graphics()->MapScreenToSize(m_BaseSize.x, m_BaseSize.y);
}

void CHudEditor::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		SetActive(false);
		return;
	}

	if(!m_Active)
		return;

	// Only the menu closes the editor, and only because it means the player left the game behind.
	// The console is deliberately not checked: it spends several frames closing after the command
	// that opened the editor ran, and would shut it straight back off.
	if(GameClient()->m_Menus.IsActive())
	{
		SetActive(false);
		return;
	}

	Ui()->StartCheck();
	Ui()->Update();

	m_BaseSize = vec2(300.0f * Graphics()->ScreenAspect(), 300.0f);
	Graphics()->MapScreenToSize(m_BaseSize.x, m_BaseSize.y);

	const vec2 Mouse = MousePos();

	const bool OverPopup = m_Popup.has_value() && Ui()->MouseInside(&m_PopupRect);

	if(m_Drag == EDrag::NONE)
	{
		if(!OverPopup && Ui()->MouseButtonClicked(0))
		{
			// The corners sit on the element's edge, so they have to be tested before the body
			for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
			{
				if(!CHudLayout::IsMovable((EHudElement)i))
					continue;

				const CHudLayout::CRect Rect = Layout().ResolvedRect((EHudElement)i);
				if(Rect.m_Size.x <= 0.0f || Rect.m_Size.y <= 0.0f)
					continue;
				if(!CornerAt((EHudElement)i, Mouse).has_value())
					continue;

				m_Drag = EDrag::RESIZE;
				m_DragElement = (EHudElement)i;
				m_DragStartScale = Layout().Placement(m_DragElement).m_Scale;
				m_DragStartReach = std::max(distance(Mouse, Layout().AnchorPos(m_DragElement)), 0.001f);
				break;
			}

			if(m_Drag == EDrag::NONE)
			{
				const std::optional<EHudElement> Hit = ElementAt(Mouse);
				if(Hit.has_value())
				{
					m_Drag = EDrag::MOVE;
					m_DragElement = Hit.value();
					m_DragStartMouse = Mouse;
					m_DragStartOffset = Layout().Placement(m_DragElement).m_Offset;
				}
			}
		}
		else if(!OverPopup && Ui()->MouseButtonClicked(1))
		{
			m_Popup = ElementAt(Mouse);
			m_PopupAnchor.reset();
		}
	}
	else if(!Ui()->MouseButton(0))
	{
		m_Drag = EDrag::NONE;
		m_SnapLineX.reset();
		m_SnapLineY.reset();
	}
	else if(m_Drag == EDrag::MOVE)
	{
		vec2 Offset = m_DragStartOffset + (Mouse - m_DragStartMouse);
		SnapOffset(m_DragElement, Offset);
		Layout().SetOffset(m_DragElement, Offset);
	}
	else if(m_Drag == EDrag::RESIZE)
	{
		// Scaling follows how much further the cursor got from the anchor, which is the one point
		// on the element that scaling leaves alone
		const float Reach = distance(Mouse, Layout().AnchorPos(m_DragElement));
		Layout().SetScale(m_DragElement, SnapScale(m_DragElement, m_DragStartScale * Reach / m_DragStartReach));
		m_SnapLineX.reset();
		m_SnapLineY.reset();
	}

	// A drag lands on a new scale every frame, so the HUD's text containers are rebuilt once the
	// drag is over rather than on each of those frames
	Layout().SetContainerResetSuspended(m_Drag == EDrag::RESIZE);

	// Elements stay where they have been put while the editor is open, so what is on screen is the
	// placement rather than the placement plus whatever the solver made of it. Holding shift lets
	// it run, which is how to see where things will actually end up.
	Layout().SetPushSuspended(!Input()->KeyIsPressed(KEY_LSHIFT) && !Input()->KeyIsPressed(KEY_RSHIFT));

	// Whatever is under the cursor mid drag goes exactly where it is put, instead of the solver
	// shoving it somewhere else while the drag is still going on
	if(m_Drag == EDrag::MOVE)
		Layout().SetHeld(m_DragElement);
	else
		Layout().ClearHeld();

	// The HUD solves once per frame after this, so a drag applied above would otherwise not show
	// up in the outlines until the next frame
	Layout().Resolve();

	// An element lighting up under a panel the cursor is actually over reads as the editor
	// disagreeing with itself about what is being pointed at
	std::optional<EHudElement> Hovered;
	if(m_Drag != EDrag::NONE)
		Hovered = m_DragElement;
	else if(!OverPopup)
		Hovered = ElementAt(Mouse);

	for(int i = 0; i < (int)EHudElement::NUM_HUD_ELEMENTS; i++)
	{
		const EHudElement Element = (EHudElement)i;
		RenderElement(Element, Hovered == Element);
	}

	RenderSnapLines();
	RenderHelp(Mouse);

	// Last, so it is never buried under an element that happens to be drawn after its owner
	if(Hovered.has_value())
		RenderHoveredInfo(Hovered.value());

	RenderPopup();

	// A field that has been committed, cancelled, or closed along with the panel leaves its line
	// input active behind it, because nothing draws it any more to deactivate it. Left that way it
	// would go on swallowing keystrokes below and hold the client in text input mode. The editor
	// shuts the console, chat and menus when it opens, so a value field is the only thing that can
	// own a line input while it is up.
	if(!m_ValueEditing && CLineInput::GetActiveInput() != nullptr)
		CLineInput::GetActiveInput()->Deactivate();

	// Nothing else draws a cursor in game, and the aim crosshair is not where the ui cursor is
	RenderTools()->RenderCursor(Mouse, 12.0f);

	Ui()->FinishCheck();
	Ui()->ClearHotkeys();
}

bool CHudEditor::IsPreviewing() const
{
	// Holding ctrl fills every element out with stand in data. Without it the editor shows the
	// HUD as it actually is, so switched off elements are not misrepresented as switched on.
	return m_Active && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL));
}

void CHudEditor::ToggleHudEditor()
{
	const IClient::EClientState State = Client()->State();
	if(State != IClient::EClientState::STATE_ONLINE && State != IClient::EClientState::STATE_DEMOPLAYBACK)
		return;

	SetActive(!m_Active);
}

bool CHudEditor::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

bool CHudEditor::OnInput(const IInput::CEvent &Event)
{
	if(!m_Active)
		return false;

	// A value in the popup being typed into owns the keyboard until it is done with it. The panel
	// drives CUi itself rather than going through the menus, so nothing else in the input stack
	// forwards keystrokes to the field, and this has to come before the handling below so that
	// escape cancels the edit rather than closing the popup out from under it.
	if(CLineInput::GetActiveInput() != nullptr)
	{
		Ui()->OnInput(Event);
		return true;
	}

	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		// The popup closes first, so escape does not tear down the whole editor while a panel is
		// still open on top of it
		if(m_Popup.has_value())
			m_Popup.reset();
		else
			SetActive(false);
		return true;
	}

	if(Event.m_Key == KEY_LALT || Event.m_Key == KEY_RALT)
		m_SnapDisabled = (Event.m_Flags & IInput::FLAG_PRESS) != 0;

	// Everything else is swallowed so that the game does not act on clicks meant for the editor
	return true;
}

void CHudEditor::ConHudEditor(IConsole::IResult *pResult, void *pUserData)
{
	CHudEditor *pSelf = (CHudEditor *)pUserData;
	pSelf->ToggleHudEditor();
}

void CHudEditor::OnConsoleInit()
{
	Console()->Register("hud_editor", "", CFGFLAG_CLIENT, ConHudEditor, this, "Toggle the HUD editor overlay, or force it on or off");
}
