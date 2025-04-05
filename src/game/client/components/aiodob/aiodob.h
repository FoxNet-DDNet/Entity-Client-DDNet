#ifndef GAME_CLIENT_COMPONENTS_AIODOB_H
#define GAME_CLIENT_COMPONENTS_AIODOB_H
#include <game/client/component.h>
#include <engine/console.h>
#include <base/system.h>
#include <vector>

class CTempEntry
{
public:
	// name matches the user with that name
	char m_aTempWar[16] = "";
	char m_aTempHelper[16] = "";
	char m_aTempMute[16] = "";
	char m_aReason[128] = "";

	/*
	* Type = 0 -> TempWar
	* Type = 1 -> TempHelper
	* type = 2 -> TempMute
	*/
	CTempEntry(int Type, const char *pName, const char *pReason)
	{
		if(Type == 0)
			str_copy(m_aTempWar, pName);
		else if(Type == 1)
			str_copy(m_aTempHelper, pName);
		else if(Type == 2)
			str_copy(m_aTempMute, pName);

		if(!str_comp(pReason, ""))
			str_copy(m_aReason, pReason);
	}

	bool operator==(const CTempEntry &Other) const
	{
		bool TempWarMatch = !str_comp(m_aTempWar, Other.m_aTempWar) && str_comp(m_aTempWar, "") != 0;
		bool TempHelperMatch = !str_comp(m_aTempHelper, Other.m_aTempHelper) && str_comp(m_aTempHelper, "") != 0;
		bool TempHelperMute = !str_comp(m_aTempMute, Other.m_aTempMute) && str_comp(m_aTempHelper, "") != 0;
		return (TempWarMatch || TempHelperMatch || TempHelperMute);
	}
};

class CTempData
{
public:
	bool IsTempWar = false;
	bool IsTempHelper = false;
	bool IsTempMute = false;

	char m_aReason[128] = "";
};

class CAiodob : public CComponent
{
	bool m_AttempedJoinTeam;
	bool m_JoinedTeam;
	
	bool m_KogModeRebound;
	bool m_WeaponsGot;
	bool m_GoresModeWasOn;
	bool m_GoresServer;

	// Chat Message Stuffc

	
	// Reply to Ping
	struct CLastPing
	{
		void Reset()
		{
			m_aName[0] = '\0';
			m_aMessage[0] = '\0';
			m_Team = 0;
		}

		char m_aName[MAX_NAME_LENGTH] = "";
		char m_aMessage[256] = "";
		int m_Team;
	};
	CLastPing m_aLastPing;

	bool LineShouldHighlight(const char *pLine, const char *pName);
	int Get128Name(const char *pMsg, char *pName);
	void OnChatMessage(int ClientId, int Team, const char *pMsg);
	virtual void OnMessage(int MsgType, void *pRawMsg) override;

	// Console Commands
	virtual void OnConsoleInit() override;

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

	static void ConTempWar(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempWar(IConsole::IResult *pResult, void *pUserData);

	static void ConTempHelper(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempHelper(IConsole::IResult *pResult, void *pUserData);

	static void ConTempMute(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempMute(IConsole::IResult *pResult, void *pUserData);

	static void ConSaveSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConRestoreSkin(IConsole::IResult *pResult, void *pUserData);

	static void ConReplyLast(IConsole::IResult *pResult, void *pUserData);

public:
	bool m_SentKill;
	int m_KillCount;

	void TempWar(const char *pName, const char *pReason, bool Silent = false);
	void TempHelper(const char *pName, const char *pReason, bool Silent = false);
	void TempMute(const char *pName, bool Silent = false);

	bool UnTempHelper(const char *pName, bool Silent = false);
	bool UnTempWar(const char *pName, bool Silent = false);
	bool UnTempMute(const char *pName, bool Silent = false);


	void Votekick(const char *pName, const char *pReason);

	void PlayerInfo(const char *pName);

	void SaveSkin();
	void RestoreSkin();
	void OnlineInfo(bool Integrate = false);

	// Temporary War Entries
	std::vector<CTempEntry> m_TempEntries;
	CTempData m_TempPlayers[MAX_CLIENTS];
	void UpdateTempPlayers();
	void RemoveWarEntryDuplicates(const char *pName);
	void RemoveWarEntry(int Type, const char *pName);

	// Movement Notification if tabbed out
	int64_t m_LastNotification;
	int m_LastTile = -1;
	void ChangeTileNotifyTick();

	// Rainbow
	void Rainbow();

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
		int R = round(255 * Hue);
		int G = round(255 * Sat);
		int B = round(255 * LhT);
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
	int64_t m_LastMovement = 10.0f;

	bool m_FirstLaunch = false;

private:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
	virtual void OnNewSnapshot() override;
	virtual void OnShutdown() override;
};

#endif