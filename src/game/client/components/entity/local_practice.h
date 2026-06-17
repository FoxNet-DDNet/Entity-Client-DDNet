#ifndef GAME_CLIENT_COMPONENTS_ENTITY_LOCAL_PRACTICE_H
#define GAME_CLIENT_COMPONENTS_ENTITY_LOCAL_PRACTICE_H

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>
#include <engine/client/enums.h>
#include <engine/console.h>
#include <game/gamecore.h>

class CLocalPractice : public CComponent
{
	bool m_Active = false;
	bool m_Initialized = false;
	bool m_WasDummyConnected = false;

	static void ConToggleLocalPractice(IConsole::IResult *pResult, void *pUserData);

	CCharacterCore m_PracticeCore[NUM_DUMMIES];

public:
	const CCharacterCore *GetPracticeCore(int Dummy) const { return &m_PracticeCore[Dummy]; }
	bool IsActive() const { return m_Active; }
	bool IsInitialized() const { return m_Initialized; }
	void SetInitialized(bool Initialized) { m_Initialized = Initialized; }
	bool ShouldPredictDummy() const;
	bool IsPracticeParticipant(int ClientId) const;
	void ApplyTeams(CTeamsCore &Teams) const;
	void SyncParticipants(CGameWorld &World) const;
	void PrepareWorld(CGameWorld &World);
	void ClampSendInput(CNetObj_PlayerInput &Input) const;
	bool GetInput(int Tee, CNetObj_PlayerInput &Input, bool Fast) const;
	void UpdatePracticeCores();

	void ResetPosition(bool Dummy);

	void OnConsoleInit() override;
	void OnNewSnapshot() override;
	void OnRender() override;
	void OnReset() override;

	int Sizeof() const override { return sizeof(*this); }
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_LOCAL_PRACTICE_H
