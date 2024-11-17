#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>

class CAiodob : public CComponent
{
public:
	CNetObj_PlayerInput m_aInputData[NUM_DUMMIES];

	int JoinedTeam;
	int64_t m_JoinTeam;

	int64_t m_LastFreeze;

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;

	virtual void FreezeKill();

	virtual void AutoKill();
	virtual void AutoJoinTeam();

	
};

#endif