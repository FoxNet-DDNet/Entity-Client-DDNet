#ifndef GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H

#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/ui.h>

#include <cstdint>
#include <vector>

class CPerformanceStatistics : public CComponent
{
	int m_LastGameTick = -1;
	int m_LastSnapshotTick = -1;
	int64_t m_LastTime = 0;
	float m_EffectiveTps = 0.0f;
	float m_SnapshotRate = 0.0f;
	int m_SnapshotCount = 0;

	int m_CurrentFPS = 0;
	int64_t m_LastFpsUpdateTime = 0;

	float m_DigitWidth0 = 0;
	float m_DigitWidth00 = 0;
	float m_DigitWidth000 = 0;
	float m_DigitWidth0000 = 0;
	float m_DigitWidth00000 = 0;
	float m_FpsLabelWidth = 0;
	float m_PingLabelWidth = 0;
	float m_SnapRateLabelWidth = 0;

	// One readout of the bar. Label and value share a container with a colour split, so a value
	// that has not moved since the last frame is a single draw call instead of two full text
	// layouts. The readouts only change a few times a second, the bar is drawn every frame.
	class CModule
	{
	public:
		CCachedText m_Text;
		// Where the value starts within the text. Kept here because it is handed to the cached
		// text on every frame to be compared against, and building it there would allocate.
		std::vector<STextColorSplit> m_vColorSplits;
	};
	enum
	{
		MODULE_FPS = 0,
		MODULE_PING,
		MODULE_SNAP_RATE,
		NUM_MODULES,
	};
	CModule m_aModules[NUM_MODULES];

	void RenderModule(CUIRect &Rect, int Module, const char *pLabel, float LabelWidth, int Value);
	void DeleteTextContainers();

public:
	void UpdateServerStats();

	void OnReset() override;
	void OnWindowResize() override;
	void OnShutdown() override;

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H
