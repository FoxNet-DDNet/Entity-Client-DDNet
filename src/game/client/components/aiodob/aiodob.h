#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>

class CAiodob : public CComponent
{
	bool AttempedJoinTeam;
	bool JoinedTeam;
	int Mult;

	bool m_Active;
	unsigned int m_DummyFire;

	struct CInputState
	{
		CAiodob *m_pControls;
		int *m_apVariables[NUM_DUMMIES];
	};

public:
	int64_t m_JoinTeam;
	int64_t m_LastFreeze;

private:
	virtual void OnInit() override;

	virtual void FreezeKill();
	virtual void AutoKill();
	virtual void AutoJoinTeam();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif