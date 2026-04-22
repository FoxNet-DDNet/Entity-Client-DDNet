#ifndef GAME_CLIENT_COMPONENTS_ENTITY_FREEZE_KILL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_FREEZE_KILL_H

#include <game/client/component.h>

#include <cstdint>

class CFreezeKill : public CComponent
{
	bool HandleClients(int ClientId);

	void ResetTimer();

	bool m_SentFreezeKill = false;
	int64_t m_LastFreeze = 0;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;

	friend class CFreezeBars;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_FREEZE_KILL_H
