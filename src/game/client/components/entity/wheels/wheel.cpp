#include "wheel.h"

#include <base/color.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

constexpr static const float InnerOuterMouseBoundaryRadius = 110.0f;
constexpr static const float OuterMouseLimitRadius = 170.0f;
constexpr static const float OuterItemRadius = 140.0f; // 10.0f less than emoticons for extra text space
constexpr static const float OuterCircleRadius = 190.0f;
// constexpr static const float InnerCircleRadius = 100.0f;
constexpr static const float FontSizeNormal = 12.0f;
constexpr static const float FontSizeSelected = 18.0f;

class IClient *CWheel::Client() const
{
	return m_pGameClient->Client();
}
class IGraphics *CWheel::Graphics() const
{
	return m_pGameClient->Graphics();
}
class ITextRender *CWheel::TextRender() const
{
	return m_pGameClient->TextRender();
}
class CUi *CWheel::Ui() const
{
	return m_pGameClient->Ui();
}

void CWheel::Reset()
{
	m_AnimationTime = 0.0f;

	m_Active = false;
	m_WasActive = false;

	m_SelectedIndex = -1;
	m_SelectorMouse = vec2(0.0f, 0.0f);

	m_TouchState = CUi::CTouchState();
	m_TouchPressedOutside = false;
}

CWheel::~CWheel()
{
	Reset();
}

CWheel::CWheel()
{
	Reset();
}

void CWheel::SetData(CGameClient *pGameClient, int MaxItems)
{
	m_pGameClient = pGameClient;
	m_MaxItems = MaxItems;
}

void CWheel::AddItem(const char *pName, const char *pId, std::function<void(class CGameClient &)> Execute)
{
	if((pName[0] == '\0' && pId[0] == '\0') || m_vItems.size() >= m_MaxItems)
		return;

	m_vItems.emplace_back(pName, pId, Execute);
}

void CWheel::RemoveItem(const char *pName, const char *pId)
{
	auto It = std::find_if(m_vItems.begin(), m_vItems.end(),
		[pName, pId](const CWheelItem &Item) {
			return !str_comp(Item.m_Name.c_str(), pName) &&
			       !str_comp(Item.m_Id.c_str(), pId);
		});

	if(It != m_vItems.end())
		m_vItems.erase(It);
}

void CWheel::RemoveItem(int Index)
{
	if(Index >= static_cast<int>(m_vItems.size()) || Index < 0)
		return;
	auto Pos = m_vItems.begin() + Index;
	m_vItems.erase(Pos);
}

void CWheel::RemoveAllItems()
{
	m_vItems.clear();
}

void CWheel::ExecuteHoveredItem()
{
	ExecuteItem(m_SelectedIndex);
}

void CWheel::ExecuteItem(int Index)
{
	if(Index >= static_cast<int>(m_vItems.size()) || Index < 0)
		return;
	if(!m_vItems[Index].m_Execute)
		return;

	m_vItems[Index].m_Execute(*GameClient());
}

bool CWheel::Render(bool ResetWheelMouse)
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return false;

	static const auto QuadEaseInOut = [](float t) -> float {
		if(t == 0.0f)
			return 0.0f;
		if(t == 1.0f)
			return 1.0f;
		return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f);
	};
	static const auto PositiveMod = [](float x, float y) -> float {
		return std::fmod(x + y, y);
	};

	const bool ItemsEmpty = m_vItems.empty();

	const float AnimationTime = (float)g_Config.m_ClAnimateWheelTime / 1000.0f;
	const float ItemAnimationTime = AnimationTime / 2.0f;

	if(AnimationTime != 0.0f)
	{
		for(auto &Item : m_vItems)
			Item.m_AnimationTime = std::max(0.0f, Item.m_AnimationTime - Client()->RenderFrameTime());
	}

	if(!m_Active)
	{
		if(m_WasActive)
		{
			if(!ItemsEmpty)
			{
				if(ResetWheelMouse)
					m_SelectorMouse = vec2(0.0f, 0.0f);
				if(m_SelectedIndex != -1)
					ExecuteItem(m_SelectedIndex);
			}
		}
		m_WasActive = false;

		if(AnimationTime == 0.0f)
			return false;

		m_AnimationTime -= Client()->RenderFrameTime() * 3.0f; // Close animation 3x faster
		if(m_AnimationTime <= 0.0f)
		{
			m_AnimationTime = 0.0f;
			return false;
		}
	}
	else
	{
		m_AnimationTime += Client()->RenderFrameTime();
		if(m_AnimationTime > AnimationTime)
			m_AnimationTime = AnimationTime;
		m_WasActive = true;
	}

	const CUIRect &Screen = *Ui()->Screen();

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
		const float TouchCenterDistance = length(TouchPos);
		if(TouchCenterDistance <= OuterMouseLimitRadius)
		{
			m_SelectorMouse = TouchPos;
		}
		else if(TouchCenterDistance > OuterCircleRadius)
		{
			m_TouchPressedOutside = true;
		}
	}
	else if(WasTouchPressed)
	{
		m_Active = false;
	}

	if(AnimationTime == 0.0f)
	{
		m_aAnimationPhase.fill(1.0f);
	}
	else
	{
		m_aAnimationPhase[0] = QuadEaseInOut(m_AnimationTime / AnimationTime);
		m_aAnimationPhase[1] = m_aAnimationPhase[0] * m_aAnimationPhase[0];
		m_aAnimationPhase[2] = m_aAnimationPhase[1] * m_aAnimationPhase[1];
	}

	if(length(m_SelectorMouse) > OuterMouseLimitRadius)
		m_SelectorMouse = normalize(m_SelectorMouse) * OuterMouseLimitRadius;

	const size_t NumItems = m_vItems.size();
	if(NumItems == 0)
	{
		m_SelectedIndex = -1;
	}
	else
	{
		const float SelectedAngle = angle(m_SelectorMouse);
		if(length(m_SelectorMouse) > InnerOuterMouseBoundaryRadius)
			m_SelectedIndex = PositiveMod(std::round(SelectedAngle / (2.0f * pi) * NumItems), NumItems);
		else
			m_SelectedIndex = -1;
	}

	if(m_SelectedIndex != -1)
	{
		m_vItems[m_SelectedIndex].m_AnimationTime += Client()->RenderFrameTime() * 2.0f; // To counteract earlier decrement
		if(m_vItems[m_SelectedIndex].m_AnimationTime >= ItemAnimationTime)
			m_vItems[m_SelectedIndex].m_AnimationTime = ItemAnimationTime;
	}

	Ui()->MapScreen();

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f * m_aAnimationPhase[0]);
	Graphics()->DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, OuterCircleRadius * m_aAnimationPhase[0], 64);
	Graphics()->QuadsEnd();
	Graphics()->WrapClamp();

	const ColorRGBA TextColor = TextRender()->DefaultTextColor();
	const ColorRGBA OutlineColor = TextRender()->DefaultTextOutlineColor();
	TextRender()->TextColor(TextColor.WithMultipliedAlpha(m_aAnimationPhase[1]));
	TextRender()->TextOutlineColor(OutlineColor.WithMultipliedAlpha(m_aAnimationPhase[1]));

	if(ItemsEmpty)
	{
		const float FontSize = 20.0f * m_aAnimationPhase[1];
		TextRender()->Text(Screen.w / 2.0f - TextRender()->TextWidth(FontSize, "Empty") / 2.0f, Screen.h / 2.0f - FontSize / 2, FontSize, "Empty");
	}

	const float Theta = pi * 2.0f / std::max<float>(1.0f, NumItems); // Prevent divide by 0
	for(size_t i = 0; i < NumItems; i++)
	{
		const CWheelItem &Bind = m_vItems[i];
		const float Angle = Theta * i;
		const float Phase = ItemAnimationTime == 0.0f ? ((int)i == m_SelectedIndex ? 1.0f : 0.0f) : QuadEaseInOut(m_vItems[i].m_AnimationTime / ItemAnimationTime);
		const float FontSize = (FontSizeNormal + Phase * (FontSizeSelected - FontSizeNormal)) * m_aAnimationPhase[1];
		const char *pName = Bind.m_Name.c_str();
		if(pName[0] == '\0')
			pName = "Empty";

		const float Width = TextRender()->TextWidth(FontSize, pName);
		const vec2 Pos = direction(Angle) * OuterItemRadius * m_aAnimationPhase[1];
		TextRender()->Text(Screen.w / 2.0f + Pos.x - Width / 2.0f, Screen.h / 2.0f + Pos.y - FontSize / 2.0f, FontSize, pName);
	}

	TextRender()->TextColor(TextColor);
	TextRender()->TextOutlineColor(OutlineColor);

	Graphics()->WrapNormal();
	return true;
}
