#ifndef GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H
#define GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H

#include <game/client/component.h>

#include <cstdint>

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

public:
	void UpdateServerStats();

	void OnReset() override;
	void OnWindowResize() override;

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_PERFORMANCE_STATISTICS_H
