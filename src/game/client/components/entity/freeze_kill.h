#ifndef GAME_CLIENT_COMPONENTS_ENTITY_FREEZEKILL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_FREEZEKILL_H
#include <cstdint>
#include <game/client/component.h>

class CFreezeKill : public CComponent
{
public:
	bool m_SentFreezeKill;
	int64_t m_LastFreeze;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif
