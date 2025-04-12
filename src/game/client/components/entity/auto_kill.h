#ifndef GAME_CLIENT_COMPONENTS_AIODOB_AUTOKILL_H
#define GAME_CLIENT_COMPONENTS_AIODOB_AUTOKILL_H
#include <game/client/component.h>
#include <cstdint>

class CAutoKill : public CComponent
{
public:
	bool m_SentAutoKill;
	int64_t m_LastFreeze;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif