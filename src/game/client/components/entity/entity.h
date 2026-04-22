#ifndef GAME_CLIENT_COMPONENTS_ENTITY_ENTITY_H
#define GAME_CLIENT_COMPONENTS_ENTITY_ENTITY_H
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <atomic>
#include <vector>

class CEClient : public CComponent
{
	bool m_AttemptedJoinTeam;
	bool m_JoinedTeam;

	bool m_GoresServer;

	// Reply to Ping
	class CLastPing
	{
	public:
		char m_aName[MAX_NAME_LENGTH] = "";
		char m_aMessage[256] = "";
		int m_Team = -1;
	};
	CLastPing m_aLastPing;

	void OnChatMessage(int ClientId, int Team, const char *pMsg);
	void OnMessage(int MsgType, void *pRawMsg) override;

	int m_LastReplyId = -1;

	// Console Commands
	void OnConsoleInit() override;

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	static void ConServerRainbowSpeed(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowSaturation(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowLightness(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowBody(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowFeet(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowBothPlayers(IConsole::IResult *pResult, void *pUserData);

	static void ConVotekick(IConsole::IResult *pResult, void *pUserData);

	static void ConOnlineInfo(IConsole::IResult *pResult, void *pUserData);

	static void ConPlayerInfo(IConsole::IResult *pResult, void *pUserData);

	static void ConViewLink(IConsole::IResult *pResult, void *pUserData);

	static void ConSaveSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConRestoreSkin(IConsole::IResult *pResult, void *pUserData);

	static void ConReplyLast(IConsole::IResult *pResult, void *pUserData);

	static void ConSpectateId(IConsole::IResult *pResult, void *pUserData);

	static void ConCrash(IConsole::IResult *pResult, void *pUserData);

	static void ConchainFastInputs(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainDiscordUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConchainDDNetProcessPriority(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainDiscordProcessPriority(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void DiscordPriorityThread(void *pUserData);

public:
	bool m_WeaponsGot;
	int m_KillCount;

	void Votekick(const char *pName, const char *pReason);

	void PlayerInfo(const char *pName);

	void SaveSkin();
	void RestoreSkin();
	void OnlineInfo();

	// Movement Notification if tabbed out
	vec2 m_LastPos = vec2(0, 0);
	void NotifyOnMove();
	void UpdateVolleyball();

	// Rainbow
	void UpdateRainbow();

	int m_BothPlayers = true;

	int m_RainbowBody[2] = {true, true};
	int m_RainbowFeet[2] = {false, false};
	int m_RainbowColor[2];
	int m_RainbowSpeed = 10;
	int m_RainbowSat[2] = {200, 200};
	int m_RainbowLht[2] = {30, 30};

	// Preview
	unsigned int m_PreviewRainbowColor[2];
	int m_ShowServerSide;
	int64_t m_ServersideDelay[2];

	int getIntFromColor(float Hue, float Sat, float LhT)
	{
		int R = round_to_int(255 * Hue);
		int G = round_to_int(255 * Sat);
		int B = round_to_int(255 * LhT);
		R = (R << 16) & 0x00FF0000;
		G = (G << 8) & 0x0000FF00;
		B = B & 0x000000FF;
		return 0xFF000000 | R | G | B;
	}

	int64_t m_RainbowDelay;

	void GoresMode();

	int64_t m_JoinTeam;
	void AutoJoinTeam();
	void OnConnect();

	/* Last Movement
	 *	+left
	 *	+right
	 *	+jump
	 */
	int64_t m_LastMovement;

	bool m_FirstLaunch = false;

	void SetDDNetProcessPriority(bool Set);
	std::atomic<int64_t> m_DiscordPriorityDelay{0};
	std::atomic_bool m_DiscordPriorityThreadRunning{false};
	void *m_pDiscordPriorityThread = nullptr;
	void StartDiscordPriorityThread();
	void SetDiscordProcessesNormalPriority();

private:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnNewSnapshot() override;
	void OnShutdown() override;
	void OnSelfDeath() override;
	void OnFocusChange(bool IsFocused) override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_ENTITY_H
