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

	constexpr float TextSize = 23.5f;
	constexpr float Padding = 22.0f;

	auto RenderModule = [this](CUIRect &Rect, const char *pText, float LabelWidth, int Value) {
		static float s_DigitWidth0 = TextRender()->TextWidth(TextSize, "0", -1, -1.0f);
		static float s_DigitWidth00 = TextRender()->TextWidth(TextSize, "00", -1, -1.0f);
		static float s_DigitWidth000 = TextRender()->TextWidth(TextSize, "000", -1, -1.0f);
		static float s_DigitWidth0000 = TextRender()->TextWidth(TextSize, "0000", -1, -1.0f);
		static float s_DigitWidth00000 = TextRender()->TextWidth(TextSize, "00000", -1, -1.0f);
		static constexpr int NUM_DIGIT_WIDTHS = 5;
		static const float s_aDigitWidth[NUM_DIGIT_WIDTHS] = {s_DigitWidth0, s_DigitWidth00, s_DigitWidth000, s_DigitWidth0000, s_DigitWidth00000};

		int DigitIndex = GetDigitsIndex(Value, NUM_DIGIT_WIDTHS - 1);
		float TotalWidth = LabelWidth + s_aDigitWidth[DigitIndex];

		if(Rect.x <= 0.0f)
			Rect = {Padding, 0.0f, TotalWidth + TextSize, TextSize * 1.35f};
		else
		{
			Rect.x += Rect.w + Padding;
			Rect.w = TotalWidth + TextSize;
		}

		Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f), 0, 0);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(Rect.x + Padding * 0.5f, TextSize * 0.2f));
		Cursor.m_FontSize = TextSize;
		Cursor.m_LineWidth = -1;
		Cursor.m_Flags = TEXTFLAG_RENDER;

		TextRender()->TextEx(&Cursor, pText, -1);
		TextRender()->TextColor(ColorRGBA(0.3f, 0.8f, 0.6f, 1.0f));
		char aBuf[24];
		str_format(aBuf, sizeof(aBuf), "%d", Value);
		TextRender()->TextEx(&Cursor, aBuf, -1);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	// The labels never change and TextSize is constexpr, so measure them once instead of laying
	// them out again every frame.
	static const float s_FpsLabelWidth = TextRender()->TextWidth(TextSize, "FPS: ", -1, -1.0f);
	static const float s_PingLabelWidth = TextRender()->TextWidth(TextSize, "Ping: ", -1, -1.0f);
	static const float s_SnapRateLabelWidth = TextRender()->TextWidth(TextSize, "Snap Rate: ", -1, -1.0f);

	CUIRect CurRect = {-1, -1, -1, -1};
	if(g_Config.m_ClStatisticsShowFps)
		RenderModule(CurRect, "FPS: ", s_FpsLabelWidth, m_CurrentFPS);
	if(g_Config.m_ClStatisticsShowPing)
		RenderModule(CurRect, "Ping: ", s_PingLabelWidth, Client()->GetPredictionTime());
	if(g_Config.m_ClStatisticsShowSnapRate)
		RenderModule(CurRect, "Snap Rate: ", s_SnapRateLabelWidth, m_SnapshotRate + 1);
}
