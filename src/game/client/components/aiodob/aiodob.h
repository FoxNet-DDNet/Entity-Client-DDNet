#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>

class CAiodob : public CComponent
{
	bool AttempedJoinTeam;
	bool JoinedTeam;
	int Mult;
	
	bool m_KogModeRebound;

public:
	bool m_SentKill;
	bool m_SentAutoKill;
	int64_t m_JoinTeam;
	int64_t m_LastFreeze;
	int64_t m_LastMovement = 10.0f;

private:
	virtual void OnInit() override;

	virtual void GoresMode();
	virtual void FreezeKill();
	virtual void AutoKill();
	virtual void AutoJoinTeam();

	virtual void OnConnect();

	int c_X;
	int c_Y;
	int c_VelX;
	int c_VelY;

	virtual void Test();



	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif