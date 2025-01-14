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

	CTempEntry(const char *pTempWar, const char *pTempHelper, const char *pTempMute)
	{
		if(!str_comp(pTempWar, ""))
			str_copy(m_aTempWar, pTempWar);
		if(!str_comp(pTempHelper, ""))
			str_copy(m_aTempHelper, pTempHelper);
		if(!str_comp(pTempMute, ""))
			str_copy(m_aTempMute, pTempMute);
	}

	bool operator==(const CTempEntry &Other) const
	{
		return !str_comp(m_aTempWar, Other.m_aTempWar) || !str_comp(m_aTempHelper, Other.m_aTempHelper) || !str_comp(m_aTempMute, Other.m_aTempMute);
	}

};

class CTempData
{
public:
	bool IsTempWar = false;
	bool IsTempHelper = false;
	bool IsTempMute = false;
};

class CAiodob : public CComponent
{
	bool m_AttempedJoinTeam;
	bool m_JoinedTeam;
	
	bool m_KogModeRebound;
	bool m_WeaponsGot;
	bool m_GoresModeWasOn;
	bool m_GoresServer;

	// Reply to Ping
	struct CLastPing
	{
		void Reset()
		{
			m_aName[0] = '\0';
			m_aClan[0] = '\0';
			m_aMessage[0] = '\0';
			m_ReciveTime = 0;
			m_Team = 0;
		}

		CLastPing()
		{
			Reset();
		}

		char m_aName[32];
		char m_aClan[32];
		char m_aMessage[2048];
		int m_Team;
		int64_t m_ReciveTime;
	};

	enum
	{
		PING_QUEUE_SIZE = 16
	};

	/*
		m_aLastPings

		A stack holding the most recent 16 pings in chat.
		Index 0 will be the latest message.
		Popping of the stack will always give you the most recent message.
	*/
	CLastPing m_aLastPings[PING_QUEUE_SIZE];

	// Chat Message Stuff
	bool LineShouldHighlight(const char *pLine, const char *pName);
	int Get128Name(const char *pMsg, char *pName);
	void OnChatMessage(int ClientId, int Team, const char *pMsg);
	virtual void OnMessage(int MsgType, void *pRawMsg) override;

	// Console Commands
	virtual void OnConsoleInit() override;

	static void ConServerRainbowSpeed(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowSaturation(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowLightness(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowBody(IConsole::IResult *pResult, void *pUserData);
	static void ConServerRainbowFeet(IConsole::IResult *pResult, void *pUserData);


	static void ConVotekick(IConsole::IResult *pResult, void *pUserData);

	static void ConOnlineInfo(IConsole::IResult *pResult, void *pUserData);

	static void ConPlayerInfo(IConsole::IResult *pResult, void *pUserData);

	static void ConTempWar(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempWar(IConsole::IResult *pResult, void *pUserData);

	static void ConTempHelper(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempHelper(IConsole::IResult *pResult, void *pUserData);

	static void ConTempMute(IConsole::IResult *pResult, void *pUserData);
	static void ConUnTempMute(IConsole::IResult *pResult, void *pUserData);

	static void ConSaveSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConRestoreSkin(IConsole::IResult *pResult, void *pUserData);

public:

	void TempWar(const char *pName);
	void UnTempWar(const char *pName, bool Silent = false);

	void TempHelper(const char *pName);
	void UnTempHelper(const char *pName, bool Silent = false);

	void TempMute(const char *pName);
	void UnTempMute(const char *pName, bool Silent = false);

	void Votekick(const char *pName, const char *pReason);

	void PlayerInfo(const char *pName);

	void SaveSkin();
	void RestoreSkin();
	void OnlineInfo(bool Integrate = false);


	// Get ClientId per name
	int IdWithName(const char *pName);


	// Temporary War Entries
	std::vector<CTempEntry> m_TempEntries;
	CTempData m_TempPlayers[MAX_CLIENTS];
	void UpdateTempPlayers();
	void RemoveWarEntryDuplicates(const char *pName);
	void RemoveWarEntry(const char *pNameW, const char *pNameH, const char *pNameM);

	// Movement Notification if tabbed out
	int64_t m_LastNotification;
	int m_LastTile = -1;
	void ChangeTileNotifyTick();


	// Rainbow
	void Rainbow();

	int m_RainbowBody;
	int m_RainbowFeet;
	int m_RainbowColor;
	int m_RainbowSpeed;
	int m_Saturation;
	int m_Lightness;

	// Preview
	unsigned int m_PreviewRainbowColor;
	int m_ShowServerSide;
	int64_t m_ServersideDelay;

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

	bool m_RainbowWasOn;
	int64_t m_RainbowDelay;


	void GoresMode();
	int64_t m_JoinTeam;
	void AutoJoinTeam();
	void OnConnect();

	/* Last Movement 
	*	Tracks
	*	+left
	*	+right
	*	+jump
	*	Mouse Inputs such as aiming 
	*   or shooting are excluded
	*/
	int test;
	int64_t m_LastMovement = 10.0f;

private:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
	virtual void OnNewSnapshot() override;
	virtual void OnShutdown() override;
};

#endif