#ifndef GAME_CLIENT_COMPONENTS_AIODOB_FREEZEKILL_H
#define GAME_CLIENT_COMPONENTS_AIODOB_FREEZEKILL_H
#include <game/client/component.h>
#include <cstdint>

class CFreezeKill : public CComponent
{
public:
	int m_Local;
	bool m_SentFreezeKill;
	int64_t m_LastFreeze;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif