#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>

class CAiodob : public CComponent
{
	bool AttempedJoinTeam;
	bool JoinedTeam;
	int Mult;
	bool m_Active;
	

	bool m_KogModeRebound;

public:
	bool m_SentKill;
	int64_t m_JoinTeam;
	int64_t m_LastFreeze;
	int64_t m_LastMovement = 10.0f;

private:
	virtual void OnInit() override;

	virtual void GoresMode();
	virtual void FreezeKill();
	virtual void AutoKill();
	virtual void AutoJoinTeam();

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif