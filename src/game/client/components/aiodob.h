#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>

class CAiodob : public CComponent
{
public:
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif