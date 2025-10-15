/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_rect.h"

#include <engine/graphics.h>

IGraphics *CUIRect::ms_pGraphics = nullptr;

void CUIRect::HSplitMid(CUIRect *pTop, CUIRect *pBottom, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.h / 2;
	const float HalfSpacing = Spacing / 2;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut - HalfSpacing;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut + HalfSpacing;
		pBottom->w = r.w;
		pBottom->h = Cut - HalfSpacing;
	}
}

void CUIRect::HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut;
	}
}

void CUIRect::HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = r.h - Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + r.h - Cut;
		pBottom->w = r.w;
		pBottom->h = Cut;
	}
}

void CUIRect::VSplitMid(CUIRect *pLeft, CUIRect *pRight, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.w / 2;
	const float HalfSpacing = Spacing / 2;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut - HalfSpacing;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut + HalfSpacing;
		pRight->y = r.y;
		pRight->w = Cut - HalfSpacing;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut;
		pRight->y = r.y;
		pRight->w = r.w - Cut;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = r.w - Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + r.w - Cut;
		pRight->y = r.y;
		pRight->w = Cut;
		pRight->h = r.h;
	}
}

void CUIRect::Margin(vec2 Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;

	pOtherRect->x = r.x + Cut.x;
	pOtherRect->y = r.y + Cut.y;
	pOtherRect->w = r.w - 2 * Cut.x;
	pOtherRect->h = r.h - 2 * Cut.y;
}

void CUIRect::Margin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(Cut, Cut), pOtherRect);
}

void CUIRect::VMargin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(Cut, 0.0f), pOtherRect);
}

void CUIRect::HMargin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(0.0f, Cut), pOtherRect);
}

bool CUIRect::Inside(vec2 Point) const
{
	return Point.x >= x && Point.x < x + w && Point.y >= y && Point.y < y + h;
}

void CUIRect::Draw(ColorRGBA Color, int Corners, float Rounding) const
{
	ms_pGraphics->DrawRect(x, y, w, h, Color, Corners, Rounding);
}

void CUIRect::Draw4(ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, int Corners, float Rounding) const
{
	ms_pGraphics->DrawRect4(x, y, w, h, ColorTopLeft, ColorTopRight, ColorBottomLeft, ColorBottomRight, Corners, Rounding);
}

void CUIRect::DrawOutline(ColorRGBA Color) const
{
	const IGraphics::CLineItem aArray[] = {
		IGraphics::CLineItem(x, y, x + w, y),
		IGraphics::CLineItem(x + w, y, x + w, y + h),
		IGraphics::CLineItem(x + w, y + h, x, y + h),
		IGraphics::CLineItem(x, y + h, x, y)};
	ms_pGraphics->TextureClear();
	ms_pGraphics->LinesBegin();
	ms_pGraphics->SetColor(Color);
	ms_pGraphics->LinesDraw(aArray, std::size(aArray));
	ms_pGraphics->LinesEnd();
}

void CUIRect::DrawOutline(ColorRGBA Color, float Rounding, int Corners) const
{
	const int NumSegments = 8;
	const float AngleStep = (pi / 2.0f) / NumSegments;

	std::vector<IGraphics::CLineItem> Lines;

	auto AddCornerArc = [&](float cx, float cy, float startAngle, int cornerFlag) {
		if((Corners & cornerFlag) && Rounding > 0.0f)
		{
			for(int i = 0; i < NumSegments; ++i)
			{
				float a0 = startAngle + i * AngleStep;
				float a1 = startAngle + (i + 1) * AngleStep;
				float x0 = cx + std::cos(a0) * Rounding;
				float y0 = cy + std::sin(a0) * Rounding;
				float x1 = cx + std::cos(a1) * Rounding;
				float y1 = cy + std::sin(a1) * Rounding;
				Lines.emplace_back(x0, y0, x1, y1);
			}
		}
	};

	const bool Tl = (Corners & IGraphics::CORNER_TL);
	const bool Tr = (Corners & IGraphics::CORNER_TR);
	const bool Bl = (Corners & IGraphics::CORNER_BL);
	const bool Br = (Corners & IGraphics::CORNER_BR);

	float LengthEnd = 0.0f;
	float LengthStart = 0.0f;

	// Top
	LengthEnd = (Rounding > 0.0f && Tl) ? Rounding : 0.0f;
	LengthStart = (Rounding > 0.0f && Tr) ? Rounding : 0.0f;
	Lines.emplace_back(x + LengthEnd, y, x + w - LengthStart, y);

	// Right
	LengthEnd = (Rounding > 0.0f && Tr) ? Rounding : 0.0f;
	LengthStart = (Rounding > 0.0f && Br) ? Rounding : 0.0f;
	Lines.emplace_back(x + w, y + LengthEnd, x + w, y + h - LengthStart);

	// Bottom
	LengthEnd = (Rounding > 0.0f && Br) ? Rounding : 0.0f;
	LengthStart = (Rounding > 0.0f && Bl) ? Rounding : 0.0f;
	Lines.emplace_back(x + w - LengthEnd, y + h, x + LengthStart, y + h);

	// Left
	LengthEnd = (Rounding > 0.0f && Bl) ? Rounding : 0.0f;
	LengthStart = (Rounding > 0.0f && Tl) ? Rounding : 0.0f;
	Lines.emplace_back(x, y + h - LengthEnd, x, y + LengthStart);

	AddCornerArc(x + Rounding, y + Rounding, pi, IGraphics::CORNER_TL); // Top-left
	AddCornerArc(x + w - Rounding, y + Rounding, -pi / 2.0f, IGraphics::CORNER_TR); // Top-right
	AddCornerArc(x + w - Rounding, y + h - Rounding, 0.0f, IGraphics::CORNER_BR); // Bottom-right
	AddCornerArc(x + Rounding, y + h - Rounding, pi / 2.0f, IGraphics::CORNER_BL); // Bottom-left

	s_pGraphics->TextureClear();
	s_pGraphics->LinesBegin();
	s_pGraphics->SetColor(Color);
	s_pGraphics->LinesDraw(Lines.data(), Lines.size());
	s_pGraphics->LinesEnd();
}

void CUIRect::DrawSpecificOutline(ColorRGBA Color, float Rounding, int Corners, int Sides) const
{
	const int NumSegments = 8;
	const float AngleStep = (pi / 2.0f) / NumSegments;

	std::vector<IGraphics::CLineItem> Lines;

	auto AddCornerArc = [&](float cx, float cy, float startAngle, int cornerFlag) {
		if((Corners & cornerFlag) && Rounding > 0.0f)
		{
			for(int i = 0; i < NumSegments; ++i)
			{
				float a0 = startAngle + i * AngleStep;
				float a1 = startAngle + (i + 1) * AngleStep;
				float x0 = cx + std::cos(a0) * Rounding;
				float y0 = cy + std::sin(a0) * Rounding;
				float x1 = cx + std::cos(a1) * Rounding;
				float y1 = cy + std::sin(a1) * Rounding;
				Lines.emplace_back(x0, y0, x1, y1);
			}
		}
	};

	const bool Tl = (Corners & IGraphics::CORNER_TL);
	const bool Tr = (Corners & IGraphics::CORNER_TR);
	const bool Bl = (Corners & IGraphics::CORNER_BL);
	const bool Br = (Corners & IGraphics::CORNER_BR);

	float LengthEnd = 0.0f;
	float LengthStart = 0.0f;

	// Top
	if(Sides & IGraphics::SIDE_T)
	{
		LengthEnd = (Rounding > 0.0f && Tl) ? Rounding : 0.0f;
		LengthStart = (Rounding > 0.0f && Tr) ? Rounding : 0.0f;
		Lines.emplace_back(x + LengthEnd, y, x + w - LengthStart, y);
	}

	// Right
	if(Sides & IGraphics::SIDE_R)
	{
		LengthEnd = (Rounding > 0.0f && Tr) ? Rounding : 0.0f;
		LengthStart = (Rounding > 0.0f && Br) ? Rounding : 0.0f;
		Lines.emplace_back(x + w, y + LengthEnd, x + w, y + h - LengthStart);
	}

	// Bottom
	if(Sides & IGraphics::SIDE_B)
	{
		LengthEnd = (Rounding > 0.0f && Br) ? Rounding : 0.0f;
		LengthStart = (Rounding > 0.0f && Bl) ? Rounding : 0.0f;
		Lines.emplace_back(x + w - LengthEnd, y + h, x + LengthStart, y + h);
	}

	// Left
	if(Sides & IGraphics::SIDE_L)
	{
		LengthEnd = (Rounding > 0.0f && Bl) ? Rounding : 0.0f;
		LengthStart = (Rounding > 0.0f && Tl) ? Rounding : 0.0f;
		Lines.emplace_back(x, y + h - LengthEnd, x, y + LengthStart);
	}

	AddCornerArc(x + Rounding, y + Rounding, pi, IGraphics::CORNER_TL); // Top-left
	AddCornerArc(x + w - Rounding, y + Rounding, -pi / 2.0f, IGraphics::CORNER_TR); // Top-right
	AddCornerArc(x + w - Rounding, y + h - Rounding, 0.0f, IGraphics::CORNER_BR); // Bottom-right
	AddCornerArc(x + Rounding, y + h - Rounding, pi / 2.0f, IGraphics::CORNER_BL); // Bottom-left

	s_pGraphics->TextureClear();
	s_pGraphics->LinesBegin();
	s_pGraphics->SetColor(Color);
	s_pGraphics->LinesDraw(Lines.data(), Lines.size());
	s_pGraphics->LinesEnd();
}
