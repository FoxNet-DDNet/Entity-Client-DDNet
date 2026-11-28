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

// Playback has no dummy connection and only ever fills the first tick slot
int CPerformanceStatistics::CurrentGameTick() const
{
	return Client()->GameTick(Client()->State() == IClient::STATE_DEMOPLAYBACK ? 0 : g_Config.m_ClDummy);
}

void CPerformanceStatistics::OnNewSnapshot()
{
	// Snapshots are counted here instead of being sampled once a frame: a frame rate below the
	// snapshot rate misses snapshots, which a demo watched at speed easily brings about.
	const int CurGameTick = CurrentGameTick();
	const int TicksSinceSnapshot = CurGameTick - m_LastSnapshotTick;
	if(TicksSinceSnapshot == 0)
		return;

	m_SnapshotCount++;
	// A seek jumps the tick much further than any snapshot spacing does, and seeking backwards
	// moves it the wrong way, so those gaps say nothing about how the demo was recorded
	if(m_LastSnapshotTick >= 0 && TicksSinceSnapshot > 0 && TicksSinceSnapshot <= Client()->GameTickSpeed())
	{
		m_PlaybackSnapshotCount++;
		m_PlaybackSnapshotTicks += TicksSinceSnapshot;
	}
	m_LastSnapshotTick = CurGameTick;
}

void CPerformanceStatistics::UpdateServerStats()
{
	const bool Playback = Client()->State() == IClient::STATE_DEMOPLAYBACK;
	const int CurGameTick = CurrentGameTick();
	const int64_t Now = time_get();

	if(m_LastTime == 0 || Playback != m_MeasuringPlayback)
	{
		// First sample, or playback started or ended, in which case the window measured the
		// other source and has to be thrown away
		m_MeasuringPlayback = Playback;
		m_LastTime = Now;
		m_LastGameTick = CurGameTick;
		m_SnapshotCount = 0;
		m_PlaybackSnapshotCount = 0;
		m_PlaybackSnapshotTicks = 0;
		return;
	}

	const float Elapsed = (Now - m_LastTime) / (float)time_freq();
	if(Elapsed < 1.0f)
		return;

	m_EffectiveTps = (CurGameTick - m_LastGameTick) / Elapsed;
	if(Playback)
	{
		// Wall clock time would fold the playback speed into the reading, so the rate comes from
		// how many ticks each snapshot carried: that is the rate the demo holds, whatever speed
		// it is watched at. A window that played nothing, because playback is paused or sat on a
		// seek, keeps the last reading rather than claiming the demo has no snapshots.
		if(m_PlaybackSnapshotTicks > 0)
			m_SnapshotRate = Client()->GameTickSpeed() * m_PlaybackSnapshotCount / (float)m_PlaybackSnapshotTicks;
	}
	else
	{
		m_SnapshotRate = m_SnapshotCount / Elapsed;
	}

	m_LastTime = Now;
	m_LastGameTick = CurGameTick;
	m_SnapshotCount = 0;
	m_PlaybackSnapshotCount = 0;
	m_PlaybackSnapshotTicks = 0;
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
	m_PlaybackSnapshotCount = 0;
	m_PlaybackSnapshotTicks = 0;
	m_MeasuringPlayback = false;

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
	// Playback shows a ping of -1, and the sign is not covered by the digit widths
	m_MinusWidth = TextRender()->TextWidth(TextSize, "-", -1, -1.0f);

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
	const float TotalWidth = LabelWidth + s_aDigitWidth[DigitIndex] + (Value < 0 ? m_MinusWidth : 0.0f);

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

	const bool Playback = Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(Client()->State() != IClient::STATE_ONLINE && !Playback)
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
	{
		// There is no server to measure a round trip against during playback
		RenderModule(CurRect, MODULE_PING, "Ping: ", m_PingLabelWidth, Playback ? -1 : Client()->GetPredictionTime());
	}
	if(g_Config.m_ClStatisticsShowSnapRate)
		RenderModule(CurRect, MODULE_SNAP_RATE, "Snap Rate: ", m_SnapRateLabelWidth, round_to_int(m_SnapshotRate));
}
