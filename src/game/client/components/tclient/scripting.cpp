#include "scripting.h"

#include "scripting/impl.h"

#include <base/log.h>
#include <base/str.h>
#include <base/system.h>
#include <base/types.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <algorithm>
#include <string>
#include <variant>

class CScriptRunner : CComponentInterfaces
{
private:
	CScriptingCtx m_ScriptingCtx;
	const CServerInfo *GetServerInfo()
	{
		if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			static CServerInfo s_ServerInfo; // Prevent use after stack return
			Client()->GetServerInfo(&s_ServerInfo);
			return &s_ServerInfo;
		}
		else if(GameClient()->m_ConnectServerInfo)
		{
			return &*GameClient()->m_ConnectServerInfo;
		}
		return nullptr;
	}
	CScriptingCtx::Any State(const std::string &Str, const CScriptingCtx::Any &Arg)
	{
		if(Str == "game_mode")
		{
			return GetServerInfo() ? GetServerInfo()->m_aGameType : "";
		}
		else if(Str == "game_mode_pvp")
		{
			return GameClient()->m_GameInfo.m_Pvp;
		}
		else if(Str == "game_mode_race")
		{
			return GameClient()->m_GameInfo.m_Race;
		}
		else if(Str == "eye_wheel_allowed")
		{
			return GameClient()->m_GameInfo.m_AllowEyeWheel;
		}
		else if(Str == "zoom_allowed")
		{
			return GameClient()->m_GameInfo.m_AllowZoom;
		}
		else if(Str == "dummy_allowed")
		{
			return Client()->DummyAllowed();
		}
		else if(Str == "dummy_connected")
		{
			return Client()->DummyConnected();
		}
		else if(Str == "rcon_authed")
		{
			return Client()->RconAuthed();
		}
		else if(Str == "team")
		{
			return GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_Team;
		}
		if(Str == "ddnet_team")
		{
			return GameClient()->m_Teams.Team(GameClient()->m_aLocalIds[g_Config.m_ClDummy]);
		}
		if(Str == "map")
		{
			if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
				return GameClient()->Map()->BaseName();
			else if(GameClient()->m_ConnectServerInfo)
				return GameClient()->m_ConnectServerInfo->m_aMap;
			else
				return "";
		}
		else if(Str == "server_ip")
		{
			const NETADDR *pAddress = nullptr;
			if(Client()->State() == IClient::STATE_ONLINE)
				pAddress = &Client()->ServerAddress();
			else if(Client()->ConnectAddressString())
				return Client()->ConnectAddressString();
			else if(GameClient()->m_ConnectServerInfo)
				pAddress = &GameClient()->m_ConnectServerInfo->m_aAddresses[0];
			else
				return "";
			char Addr[128];
			net_addr_str(pAddress, Addr, sizeof(Addr), true);
			return Addr;
		}
		else if(Str == "players_connected")
		{
			if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
				return GameClient()->m_Snap.m_NumPlayers;
			else
			{
				const CServerInfo *pServerInfo = GetServerInfo();
				if(!pServerInfo)
					return -1;
				return pServerInfo->m_NumClients;
			}
		}
		else if(Str == "players_cap")
		{
			const CServerInfo *pServerInfo = GetServerInfo();
			if(!pServerInfo)
				return -1;
			return pServerInfo->m_MaxClients;
		}
		else if(Str == "server_name")
		{
			const CServerInfo *pServerInfo = GetServerInfo();
			if(!pServerInfo)
				return "";
			return pServerInfo->m_aName;
		}
		else if(Str == "community")
		{
			const CServerInfo *pServerInfo = GetServerInfo();
			if(!pServerInfo)
				return "";
			return pServerInfo->m_aCommunityId;
		}
		else if(Str == "location")
		{
			if(GameClient()->m_GameInfo.m_Race)
				return "";
			float w = 100.0f, h = 100.0;
			float x = 50.0f, y = 50.0f;
			const CLayers *pLayers = GameClient()->m_MapLayersForeground.m_pLayers;
			const CMapItemLayerTilemap *pLayer = pLayers->GameLayer();
			if(pLayer)
			{
				w = (float)pLayer->m_Width * 30.0f;
				h = (float)pLayer->m_Height * 30.0f;
			}
			x = GameClient()->m_Camera.m_Center.x;
			y = GameClient()->m_Camera.m_Center.y;
			static const char *s_apLocations[] = {
				"NW", "N", "NE",
				"W", "C", "E",
				"SW", "S", "SE"};
			int i = std::clamp((int)(y / h * 3.0f), 0, 2) * 3 + std::clamp((int)(x / w * 3.0f), 0, 2);
			return s_apLocations[i];
		}
		else if(Str == "state")
		{
			const char *pState = nullptr;
			switch(Client()->State())
			{
			case IClient::EClientState::STATE_CONNECTING:
				pState = "connecting";
				break;
			case IClient::STATE_OFFLINE:
				pState = "offline";
				break;
			case IClient::STATE_LOADING:
				pState = "loading";
				break;
			case IClient::STATE_ONLINE:
				pState = "online";
				break;
			case IClient::STATE_DEMOPLAYBACK:
				pState = "demo";
				break;
			case IClient::STATE_QUITTING:
				pState = "quitting";
				break;
			case IClient::STATE_RESTARTING:
				pState = "restarting";
				break;
			}
			return pState;
		}
		else if(Str == "id")
		{
			if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
				return nullptr;
			if(!std::holds_alternative<std::string>(Arg))
				return nullptr;
			const std::string &Name = std::get<std::string>(Arg);
			for(const auto &Client : GameClient()->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(str_comp(Name.c_str(), Client.m_aName) == 0)
					return Client.ClientId();
			}
			for(const auto &Client : GameClient()->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(str_comp_nocase(Name.c_str(), Client.m_aName) == 0)
					return Client.ClientId();
			}
			return -1;
		}
		else if(Str == "name")
		{
			if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
				return nullptr;
			if(!std::holds_alternative<int>(Arg))
				return nullptr;
			int Id = std::get<int>(Arg);
			if(Id < 0 || Id >= MAX_CLIENTS)
				return nullptr;
			if(!GameClient()->m_aClients[Id].m_Active)
				return nullptr;
			return GameClient()->m_aClients[Id].m_aName;
		}
		else if(Str == "clan")
		{
			if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
				return nullptr;
			if(!std::holds_alternative<int>(Arg))
				return nullptr;
			int Id = std::get<int>(Arg);
			if(Id < 0 || Id >= MAX_CLIENTS)
				return nullptr;
			if(!GameClient()->m_aClients[Id].m_Active)
				return nullptr;
			return GameClient()->m_aClients[Id].m_aClan;
		}
		// <E-Client
		else if(Str == "client_id")
		{
			return GameClient()->m_aLocalIds[g_Config.m_ClDummy ? 1 : 0];
		}
		else if(Str == "dummy_id")
		{
			return GameClient()->m_aLocalIds[g_Config.m_ClDummy ? 0 : 1];
		}
		else if(Str == "in_freeze")
		{
			return Client()->m_IsLocalFrozen;
		}
		else if(Str == "server_passworded")
		{
			const CServerInfo *pServerInfo = GetServerInfo();
			if(!pServerInfo)
				return false;
			const bool Passworded = (pServerInfo->m_Flags & SERVER_FLAG_PASSWORD) != 0;
			return Passworded;
		}
		else if(Str == "player_name")
		{
			return g_Config.m_PlayerName;
		}
		else if(Str == "dummy_name")
		{
			return g_Config.m_ClDummyName;
		}
		// EClient>

		throw std::string("No state with name '") + Str + std::string("'");
	}
	// <E-Client
	CScriptingCtx::Any ClientInfo(const std::string &Str, const CScriptingCtx::Any &Arg)
	{
		if(!std::holds_alternative<int>(Arg))
			return nullptr;
		int ClientId = std::get<int>(Arg);

		if(Str == "exists")
		{
			return GameClient()->m_aClients[ClientId].m_Active;
		}

		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			return nullptr;

		if(!GameClient()->m_aClients[ClientId].m_Active)
			return nullptr;

		if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
			return nullptr;

		if(Str == "team")
		{
			return GameClient()->m_aClients[ClientId].m_Team;
		}
		else if(Str == "ddnet_team")
		{
			return GameClient()->m_Teams.Team(ClientId);
		}
		else if(Str == "name")
		{
			return GameClient()->m_aClients[ClientId].m_aName;
		}
		else if(Str == "clan")
		{
			return GameClient()->m_aClients[ClientId].m_aClan;
		}
		else if(Str == "skin_name")
		{
			return GameClient()->m_aClients[ClientId].m_aSkinName;
		}
		else if(Str == "skin_custom_color")
		{
			return GameClient()->m_aClients[ClientId].m_UseCustomColor;
		}
		else if(Str == "skin_color_feet")
		{
			return GameClient()->m_aClients[ClientId].m_ColorFeet;
		}
		else if(Str == "skin_color_body")
		{
			return GameClient()->m_aClients[ClientId].m_ColorBody;
		}
		else if(Str == "afk")
		{
			return GameClient()->m_aClients[ClientId].m_Afk;
		}
		else if(Str == "friend")
		{
			return GameClient()->m_aClients[ClientId].m_Friend;
		}
		else if(Str == "foe")
		{
			return GameClient()->m_aClients[ClientId].m_Foe;
		}
		else if(Str == "warlist_type")
		{
			for(size_t WarlistType = 0; WarlistType < GameClient()->m_WarList.m_WarTypes.size(); ++WarlistType)
			{
				bool Matches = GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[WarlistType];

				if(Matches)
					return (int)WarlistType;
			}
			return nullptr; // none
		}
		else if(Str == "warlist_type_name")
		{
			for(size_t WarlistType = 0; WarlistType < GameClient()->m_WarList.m_WarTypes.size(); ++WarlistType)
			{
				bool Matches = GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[WarlistType];

				if(Matches)
					return GameClient()->m_WarList.GetWarTypeName(ClientId);
			}
			return nullptr; // none
		}
		else if(Str == "muted")
		{
			return GameClient()->m_WarList.GetWarData(ClientId).m_IsMuted;
		}
		else if(Str == "auth_level")
		{
			return GameClient()->m_aClients[ClientId].m_AuthLevel; // 0 means not authed
		}

		throw std::string("No state with name '") + Str + std::string("'");
	}
	// EClient>

	CScriptingCtx::Any ToLower(const std::string &Str)
	{
		std::string LowerStr = Str;
		std::transform(LowerStr.begin(), LowerStr.end(), LowerStr.begin(), ::tolower);
		return LowerStr;
	}

	CScriptingCtx::Any ToUpper(const std::string &Str)
	{
		std::string LowerStr = Str;
		std::transform(LowerStr.begin(), LowerStr.end(), LowerStr.begin(), ::toupper);
		return LowerStr;
	}

public:
	CScriptRunner(CGameClient *pClient)
	{
		OnInterfacesInit(pClient);
		m_ScriptingCtx.AddFunction("exec", [this](const std::string &Str) {
			log_info(SCRIPTING_IMPL "/exec", "%s", Str.c_str());
			Console()->ExecuteLine(Str.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
		});
		m_ScriptingCtx.AddFunction("echo", [this](const std::string &Str) {
			GameClient()->ClientMessage(Str.c_str());
		});
		m_ScriptingCtx.AddFunction("state", [this](const std::string &Str, const CScriptingCtx::Any &Arg) {
			return State(Str, Arg);
		});
		// <E-Client

		m_ScriptingCtx.AddFunction("to_lower", [this](const std::string &Str, const CScriptingCtx::Any &Arg) {
			return ToLower(Str);
		});

		m_ScriptingCtx.AddFunction("to_upper", [this](const std::string &Str, const CScriptingCtx::Any &Arg) {
			return ToUpper(Str);
		});

		m_ScriptingCtx.AddFunction("client_info", [this](const std::string &Str, const CScriptingCtx::Any &Arg) {
			return ClientInfo(Str, Arg);
		});

		// EClient>
		m_ScriptingCtx.SaveState();
	}
	void Run(const char *pFilename, const char *pArgs)
	{
		m_ScriptingCtx.Run(Storage(), pFilename, pArgs);
	}
};

CScripting::~CScripting()
{
	delete m_pRunner;
}

void CScripting::ConExecScript(IConsole::IResult *pResult, void *pUserData)
{
	CScripting *pThis = static_cast<CScripting *>(pUserData);
	pThis->ExecScript(pResult->GetString(0), pResult->GetString(1));
}

void CScripting::ExecScript(const char *pFilename, const char *pArgs)
{
	if(!m_pRunner)
		m_pRunner = new CScriptRunner(GameClient());
	m_pRunner->Run(pFilename, pArgs);
}

void CScripting::OnConsoleInit()
{
	Console()->Register(SCRIPTING_IMPL, "s[file] ?r[args]", CFGFLAG_CLIENT, ConExecScript, this, "Execute a " SCRIPTING_IMPL " script");
}
