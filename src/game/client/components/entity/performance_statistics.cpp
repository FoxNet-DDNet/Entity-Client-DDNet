#include "performance_statistics.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/hud.h>
#include <game/client/gameclient.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstdint>
#include <vector>

constexpr float TextSize = 23.5f;
constexpr float Padding = 22.0f;

void CPerformanceStatistics::UpdateServerStats()
{
	const int CurGameTick = Client()->GameTick(g_Config.m_ClDummy);
	const int64_t Now = time_get();

	if(m_LastTime == 0)
	{
		m_LastTime = Now;
		m_LastGameTick = CurGameTick;
		m_LastSnapshotTick = CurGameTick;
	}
	else
	{
		if(CurGameTick != m_LastSnapshotTick)
		{
			m_SnapshotCount++;
			m_LastSnapshotTick = CurGameTick;
		}

		const float Elapsed = (Now - m_LastTime) / (float)time_freq();
		if(Elapsed >= 1.0f)
		{
			m_EffectiveTps = (CurGameTick - m_LastGameTick) / Elapsed;
			m_SnapshotRate = m_SnapshotCount / Elapsed;
			m_LastTime = Now;
			m_LastGameTick = CurGameTick;
			m_SnapshotCount = 0;
		}
	}
}

void CPerformanceStatistics::DeleteTextContainers()
{
	for(CModule &Module : m_aModules)
	{
		Module.m_Text.Reset(TextRender());
	}
}

void CPerformanceStatistics::OnReset()
{
	m_LastGameTick = -1;
	m_LastSnapshotTick = -1;
	m_LastTime = 0;
	m_EffectiveTps = 0.0f;
	m_SnapshotRate = 0.0f;
	m_SnapshotCount = 0;

	m_CurrentFPS = 0;
	m_LastFpsUpdateTime = 0;
	OnWindowResize();
}

void CPerformanceStatistics::OnShutdown()
{
	DeleteTextContainers();
}

void CPerformanceStatistics::OnWindowResize()
{
	// The containers were laid out for the old screen, so they go with it
	DeleteTextContainers();

	m_DigitWidth0 = TextRender()->TextWidth(TextSize, "0", -1, -1.0f);
	m_DigitWidth00 = TextRender()->TextWidth(TextSize, "00", -1, -1.0f);
	m_DigitWidth000 = TextRender()->TextWidth(TextSize, "000", -1, -1.0f);
	m_DigitWidth0000 = TextRender()->TextWidth(TextSize, "0000", -1, -1.0f);
	m_DigitWidth00000 = TextRender()->TextWidth(TextSize, "00000", -1, -1.0f);

	m_FpsLabelWidth = TextRender()->TextWidth(TextSize, "FPS: ");
	m_PingLabelWidth = TextRender()->TextWidth(TextSize, "Ping: ");
	m_SnapRateLabelWidth = TextRender()->TextWidth(TextSize, "Snap Rate: ");
}

inline int GetDigitsIndex(int Value, int Max)
{
	if(Value < 0)
	{
		Value *= -1;
	}
	int DigitsIndex = std::log10((Value ? Value : 1));
	return std::clamp(DigitsIndex, 0, Max);
}

void CPerformanceStatistics::RenderModule(CUIRect &Rect, int Module, const char *pLabel, float LabelWidth, int Value)
{
	static constexpr int NumDigitWidths = 5;
	const float s_aDigitWidth[NumDigitWidths] = {m_DigitWidth0, m_DigitWidth00, m_DigitWidth000, m_DigitWidth0000, m_DigitWidth00000};

	const int DigitIndex = GetDigitsIndex(Value, NumDigitWidths - 1);
	const float TotalWidth = LabelWidth + s_aDigitWidth[DigitIndex];

	if(Rect.x <= 0.0f)
		Rect = {Padding, 0.0f, TotalWidth + TextSize, TextSize * 1.35f};
	else
	{
		Rect.x += Rect.w + Padding;
		Rect.w = TotalWidth + TextSize;
	}

	Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f), 0, 0);

	// Label and value in one string. They were two TextEx calls off a shared cursor, which laid
	// out and uploaded both every frame; one container holds the same run and the colour split
	// keeps the value in its own colour.
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s%d", pLabel, Value);
	CModule &Cache = m_aModules[Module];
	if(Cache.m_vColorSplits.empty())
		Cache.m_vColorSplits.emplace_back(str_length(pLabel), -1, ColorRGBA(0.3f, 0.8f, 0.6f, 1.0f));
	Cache.m_Text.Update(TextRender(), aBuf, TextSize, -1.0f, TEXTFLAG_RENDER, Cache.m_vColorSplits);
	Cache.m_Text.Render(TextRender(), vec2(Rect.x + Padding * 0.5f, TextSize * 0.2f), TextRender()->DefaultTextColor());
}

void CPerformanceStatistics::OnRender()
{
	if(g_Config.m_Debug)
		return;

	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	UpdateServerStats();
	if(m_LastFpsUpdateTime == 0 || m_LastFpsUpdateTime + time_freq() * 0.05f < time_get())
	{
		m_CurrentFPS = round_to_int(1.0f / Client()->FrameTimeAverage());

		if(g_Config.m_ClRefreshRate > 0)
			m_CurrentFPS = std::clamp(m_CurrentFPS, 0, g_Config.m_ClRefreshRate);
		if(g_Config.m_GfxRefreshRate > 0)
			m_CurrentFPS = std::clamp(m_CurrentFPS, 0, g_Config.m_GfxRefreshRate);

		m_LastFpsUpdateTime = time_get();
	}

	// The labels never change and TextSize is constexpr, so measure them once instead of laying
	// them out again every frame.

	CUIRect CurRect = {-1, -1, -1, -1};
	if(g_Config.m_ClStatisticsShowFps)
		RenderModule(CurRect, MODULE_FPS, "FPS: ", m_FpsLabelWidth, m_CurrentFPS);
	if(g_Config.m_ClStatisticsShowPing)
		RenderModule(CurRect, MODULE_PING, "Ping: ", m_PingLabelWidth, Client()->GetPredictionTime());
	if(g_Config.m_ClStatisticsShowSnapRate)
		RenderModule(CurRect, MODULE_SNAP_RATE, "Snap Rate: ", m_SnapRateLabelWidth, m_SnapshotRate + 1);
}
