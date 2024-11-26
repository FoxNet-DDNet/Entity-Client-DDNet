// ChillerDragon 2020 - chillerbot ux

#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include "chillerbotux.h"

void CChillerBotUX::OnRender()
{
	RenderEnabledComponents();
	FinishRenameTick();
	ChangeTileNotifyTick();
	m_ForceDir = 0;
	CampHackTick();
	if(!m_ForceDir && m_LastForceDir)
	{
		m_pClient->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] = 0;
		
	}
	m_LastForceDir = m_ForceDir;
}

void CChillerBotUX::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_OFFLINE && m_pClient->Client()->ReconnectTime() == 0)
	{
		if(g_Config.m_ClAlwaysReconnect)
			m_pClient->Client()->SetReconnectTime(time_get() + time_freq() * g_Config.m_ClReconnectTimeout + 10);
	}
}

inline bool CChillerBotUX::IsPlayerInfoAvailable(int ClientId) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientId);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientId);
	return pPrevInfo && pInfo;
}

void CChillerBotUX::CheckEmptyTick()
{
	if(!g_Config.m_ClReconnectWhenEmpty)
		return;

	static int s_LastPlayerCount = 0;
	int PlayerCount = CountOnlinePlayers();
	if(s_LastPlayerCount > PlayerCount && PlayerCount == 1)
		m_pClient->Client()->Connect(m_pClient->Client()->ConnectAddressString());
	else
		s_LastPlayerCount = CountOnlinePlayers();
}

bool CChillerBotUX::OnSendChat(int Team, const char* pLine)
{
	char aTrimmedLine[512];
	str_copy(aTrimmedLine, pLine);
	int Length = 0;
	char *p = aTrimmedLine;
	char *pEnd = nullptr;
	while(*p)
	{
		char *pStrOld = p;
		int Code = str_utf8_decode((const char **)(&p));
		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;
		if(++Length >= 256)
		{
			*p = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*pEnd = '\0';

	int ClientId = m_pClient->m_aLocalIds[g_Config.m_ClDummy];
	if(m_pClient->m_ChatCommand.OnChatMsg(ClientId, Team, aTrimmedLine))
		return false;

	return true;
}

void CChillerBotUX::ChangeTileNotifyTick()
{
	if(!g_Config.m_ClChangeTileNotification)
		return;
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;

	float X = m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_X;
	float Y = m_pClient->m_Snap.m_aCharacters[m_pClient->m_aLocalIds[g_Config.m_ClDummy]].m_Cur.m_Y;
	int CurrentTile = Collision()->GetTileIndex(Collision()->GetPureMapIndex(X, Y));
	if(m_LastTile != CurrentTile && m_LastNotification + time_freq() * 10 < time_get())
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			Client()->Notify("chillerbot-ux", "current tile changed");
			Graphics()->NotifyWindow();
		}
		m_LastNotification = time_get();
	}
	m_LastTile = CurrentTile;
}


void CChillerBotUX::RenderEnabledComponents()
{
	if(m_pClient->m_Menus.IsActive())
		return;
	if(m_pClient->m_Voting.IsVoting())
		return;
	if(!g_Config.m_ClChillerbotHud)
		return;
	int offset = 0;
	if(m_IsLeftSidedBroadcast && Client()->GameTick(g_Config.m_ClDummy) < m_BroadcastTick)
		offset = 2;
	for(auto &EnabledComponent : m_aEnabledComponents)
	{
		if(EnabledComponent.m_aName[0] == '\0')
			continue;
		float TwName = TextRender()->TextWidth(10.0f, EnabledComponent.m_aName, -1, -1);
		float TwNoteShort = 2.f;
		if(EnabledComponent.m_aNoteShort[0])
			TwNoteShort += TextRender()->TextWidth(10.0f, EnabledComponent.m_aNoteShort, -1, -1);
		Graphics()->DrawRect(4.0f, 60.f + offset * 15, TwName + TwNoteShort, 14.0f, ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 3.0f);

		TextRender()->Text(5.0f, 60.f + offset * 15, 10.0f, EnabledComponent.m_aName, -1);
		TextRender()->Text(5.0f + TwName + 2.f, 60.f + offset * 15, 10.0f, EnabledComponent.m_aNoteShort, -1);
		TextRender()->Text(5.0f, 60.f + offset * 15 + 10, 4.0f, EnabledComponent.m_aNoteLong, -1);
		offset++;
	}
}

void CChillerBotUX::EnableComponent(const char *pComponent, const char *pNoteShort, const char *pNoteLong)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
	// insert
	for(auto &Component : m_aEnabledComponents)
	{
		if(Component.m_aName[0] != '\0')
			continue;
		str_copy(Component.m_aName, pComponent, sizeof(Component.m_aName));
		Component.m_aNoteShort[0] = '\0';
		Component.m_aNoteLong[0] = '\0';
		if(pNoteShort)
			str_copy(Component.m_aNoteShort, pNoteShort, sizeof(Component.m_aNoteShort));
		if(pNoteLong)
			str_copy(Component.m_aNoteLong, pNoteLong, sizeof(Component.m_aNoteLong));
		return;
	}
}

void CChillerBotUX::DisableComponent(const char *pComponent)
{
	// update
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		Component.m_aName[0] = '\0';
		return;
	}
}

bool CChillerBotUX::SetComponentNoteShort(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteShort, pNote, sizeof(Component.m_aNoteShort));
		return true;
	}
	return false;
}

bool CChillerBotUX::SetComponentNoteLong(const char *pComponent, const char *pNote)
{
	if(!pNote)
		return false;
	for(auto &Component : m_aEnabledComponents)
	{
		if(str_comp(Component.m_aName, pComponent))
			continue;
		str_copy(Component.m_aNoteLong, pNote, sizeof(Component.m_aNoteLong));
		return true;
	}
	return false;
}


void CChillerBotUX::CampHackTick()
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClCampHack)
		return;
	if(Layers()->GameGroup())
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		RenderTools()->MapScreenToGroup(m_pClient->m_Camera.m_Center.x, m_pClient->m_Camera.m_Center.y, Layers()->GameGroup(), m_pClient->m_Camera.m_Zoom);
		Graphics()->DrawRect(m_CampHackX1, m_CampHackY1, 20.0f, 20.0f, ColorRGBA(0, 0, 0, 0.4f), IGraphics::CORNER_ALL, 3.0f);
		Graphics()->DrawRect(m_CampHackX2, m_CampHackY2, 20.0f, 20.0f, ColorRGBA(0, 0, 0, 0.4f), IGraphics::CORNER_ALL, 3.0f);
		if(m_CampHackX1 && m_CampHackX2 && m_CampHackY1 && m_CampHackY2)
		{
			if(m_CampHackX1 < m_CampHackX2)
				Graphics()->DrawRect(m_CampHackX1, m_CampHackY1, m_CampHackX2 - m_CampHackX1, m_CampHackY2 - m_CampHackY1, ColorRGBA(0, 1, 0, 0.2f), IGraphics::CORNER_ALL, 3.0f);
			else
				Graphics()->DrawRect(m_CampHackX1, m_CampHackY1, m_CampHackX2 - m_CampHackX1, m_CampHackY2 - m_CampHackY1, ColorRGBA(1, 0, 0, 0.2f), IGraphics::CORNER_ALL, 3.0f);
		}
		TextRender()->Text(m_CampHackX1, m_CampHackY1, 10.0f, "1", -1);
		TextRender()->Text(m_CampHackX2, m_CampHackY2, 10.0f, "2", -1);
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
	if(!m_CampHackX1 || !m_CampHackX2 || !m_CampHackY1 || !m_CampHackY2)
		return;
	if(g_Config.m_ClCampHack < 2 || GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_HAMMER)
		return;
	if(m_CampHackX1 > GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy] = 1;
		m_pClient->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] = 0;
		m_ForceDir = 1;
		m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = 1;
	}
	else if(m_CampHackX2 < GameClient()->m_Snap.m_pLocalCharacter->m_X)
	{
		m_pClient->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy] = 0;
		m_pClient->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] = 1;
		m_ForceDir = -1;
	
			m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Direction = -1;
	}
}

void CChillerBotUX::SelectCampArea(int Key)
{
	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;
	if(g_Config.m_ClCampHack != 1)
		return;
	if(Key != KEY_MOUSE_1)
		return;
	if(GameClient()->m_Snap.m_pLocalCharacter->m_Weapon != WEAPON_GUN)
		return;
	m_CampClick++;
	if(m_CampClick % 2 == 0)
	{
		// UNSET IF CLOSE
		vec2 Current = vec2(GameClient()->m_Snap.m_pLocalCharacter->m_X, GameClient()->m_Snap.m_pLocalCharacter->m_Y);
		vec2 CrackPos1 = vec2(m_CampHackX1, m_CampHackY1);
		float dist = distance(CrackPos1, Current);
		if(dist < 100.0f)
		{
			m_CampHackX1 = 0;
			m_CampHackY1 = 0;
			GameClient()->m_Chat.AddLine(-2, 0, "Unset camp[1]");
			return;
		}
		vec2 CrackPos2 = vec2(m_CampHackX2, m_CampHackY2);
		dist = distance(CrackPos2, Current);
		if(dist < 100.0f)
		{
			m_CampHackX2 = 0;
			m_CampHackY2 = 0;
			GameClient()->m_Chat.AddLine(-2, 0, "Unset camp[2]");
			return;
		}
		// SET OTHERWISE
		if(m_CampClick == 2)
		{
			m_CampHackX1 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY1 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		else
		{
			m_CampHackX2 = GameClient()->m_Snap.m_pLocalCharacter->m_X;
			m_CampHackY2 = GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf),
			"Set camp[%d] %d",
			m_CampClick == 2 ? 1 : 2,
			GameClient()->m_Snap.m_pLocalCharacter->m_X / 32);
		GameClient()->m_Chat.AddLine(-2, 0, aBuf);
	}
	if(m_CampClick > 3)
		m_CampClick = 0;
}

void CChillerBotUX::FinishRenameTick()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	if(!g_Config.m_ClFinishRename)
		return;
	vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
	if(m_pClient->RaceHelper()->IsNearFinish(Pos))
	{
		if(Client()->State() == IClient::STATE_ONLINE && !m_pClient->m_Menus.IsActive() && g_Config.m_ClEditor == 0)
		{
			Graphics()->DrawRect(105.0f, 5.f, 75.0f, 25.0f, ColorRGBA(0, 0, 0, 0.35f), IGraphics::CORNER_ALL, 10.0f);
			TextRender()->Text(126.0f, 8.f, 9.0f, "Aiodob", -1);
			TextRender()->Text(115.0f, 18.f, 8.0f, "finish rename", -1);
		}
		if(!m_IsNearFinish)
		{
			m_IsNearFinish = true;
			m_pClient->SendFinishName();
		}
	}
	else
	{
		m_IsNearFinish = false;
	}
}

void CChillerBotUX::OnInit()
{
	m_pChatHelper = &m_pClient->m_ChatHelper;

	m_AfkActivity = 0;
	m_aAfkMessage[0] = '\0';

	for(auto &c : m_aEnabledComponents)
	{
		c.m_aName[0] = '\0';
		c.m_aNoteShort[0] = '\0';
		c.m_aNoteLong[0] = '\0';
	}

	m_aLastKiller[0][0] = '\0';
	m_aLastKiller[1][0] = '\0';
	m_aLastKillerTime[0][0] = '\0';
	m_aLastKillerTime[1][0] = '\0';
	m_BroadcastTick = 0;
	m_IsLeftSidedBroadcast = false;
	// TODO: replace this with priv pub key pairs otherwise account ownership claims are trash
	m_LastTile = -1;
}

void CChillerBotUX::OnShutdown()
{
	str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClSavedPlayerSkin, sizeof(g_Config.m_ClPlayerSkin));
	g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClSavedPlayerUseCustomColor;
	g_Config.m_ClPlayerColorBody = g_Config.m_ClSavedPlayerColorBody;
	g_Config.m_ClPlayerColorFeet = g_Config.m_ClSavedPlayerColorFeet;

	str_copy(g_Config.m_ClDummySkin, g_Config.m_ClSavedDummySkin, sizeof(g_Config.m_ClDummySkin));
	g_Config.m_ClDummyUseCustomColor = g_Config.m_ClSavedDummyUseCustomColor;
	g_Config.m_ClDummyColorBody = g_Config.m_ClSavedDummyColorBody;
	g_Config.m_ClDummyColorFeet = g_Config.m_ClSavedDummyColorFeet;

	Storage()->RemoveFile("chillerbot/templist/temp/tempwar/names.txt", IStorage::TYPE_SAVE);
}

void CChillerBotUX::UpdateComponents()
{
	// TODO: make this auto or nicer
	if(g_Config.m_ClChillerbotHud)
		EnableComponent("chillerbot hud");
	else
		DisableComponent("chillerbot hud");
	if(g_Config.m_ClCampHack)
		EnableComponent("camp hack");
	else
		DisableComponent("camp hack");
	if(g_Config.m_ClFinishRename)
		EnableComponent("finish rename");
	else
		DisableComponent("finish rename");
	if(g_Config.m_ClWarList)
		EnableComponent("war list");
	else
		DisableComponent("war list");
}

void CChillerBotUX::OnConsoleInit()
{
		Console()->Register("camp", "?i[left]i[right]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHack, this, "Activate camp mode relative to tee");
	Console()->Register("camp_abs", "i[x1]i[y1]i[x2]i[y2]?s[tile|raw]", CFGFLAG_CLIENT, ConCampHackAbs, this, "Activate camp mode absolute in the map");
	Console()->Register("uncamp", "", CFGFLAG_CLIENT, ConUnCampHack, this, "Same as cl_camp_hack 0 but resets walk input");
	Console()->Register("dump_players", "?s[search]", CFGFLAG_CLIENT, ConDumpPlayers, this, "Prints players to console");
	Console()->Register("force_quit", "", CFGFLAG_CLIENT, ConForceQuit, this, "Forces a dirty client quit all data will be lost");

	Console()->Chain("ac_camp_hack", ConchainCampHack, this);
	Console()->Chain("ac_auto_reply", ConchainAutoReply, this);
	Console()->Chain("ac_finish_rename", ConchainFinishRename, this);
	
	
	
	Console()->Chain("ac_show_last_killer", ConchainShowLastKiller, this);
	Console()->Chain("ac_show_last_ping", ConchainShowLastPing, this);
}

void CChillerBotUX::ConForceQuit(IConsole::IResult *pResult, void *pUserData)
{
	exit(0);
}

void CChillerBotUX::ConDumpPlayers(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pSelf->DumpPlayers(pResult->GetString(0));
}

void CChillerBotUX::ConchainCampHack(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
}

void CChillerBotUX::ConchainAutoReply(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
}

void CChillerBotUX::ConchainFinishRename(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);

}

void CChillerBotUX::ConchainShowLastKiller(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
}

void CChillerBotUX::ConchainShowLastPing(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
}

void CChillerBotUX::ConCampHackAbs(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	pSelf->EnableComponent("camp hack");
	// absolute all coords
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = Tile * pResult->GetInteger(1);
			pSelf->m_CampHackX2 = Tile * pResult->GetInteger(2);
			pSelf->m_CampHackY2 = Tile * pResult->GetInteger(3);
		}
		return;
	}
}

void CChillerBotUX::DumpPlayers(const char *pSearch)
{
	char aBuf[128];
	char aTime[128];
	char aLine[512];
	int OldDDTeam = -1;
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
	dbg_msg("dump_players", "|score     |id|name            |clan            |lat|team   |");
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apInfoByDDTeamScore[i];
		if(!pInfo)
			continue;

		bool IsMatch = !(pSearch && pSearch[0] != 0);
		aLine[0] = '\0';
		// score
		if(m_pClient->m_GameInfo.m_TimeScore)
		{
			if(pInfo->m_Score == -9999)
				str_format(aBuf, sizeof(aBuf), "|%10s|", " ");
			else
			{
				str_time((int64_t)abs(pInfo->m_Score) * 100, TIME_HOURS, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "|%10s|", aTime);
			}
		}
		else
			str_format(aBuf, sizeof(aBuf), "|%10d|", clamp(pInfo->m_Score, -999, 99999));
		str_append(aLine, aBuf, sizeof(aLine));

		// id | name
		if(pSearch && pSearch[0] != 0)
			if(str_find_nocase(m_pClient->m_aClients[pInfo->m_ClientId].m_aName, pSearch))
				IsMatch = true;
		str_format(aBuf, sizeof(aBuf), "%2d|%16s|", pInfo->m_ClientId, m_pClient->m_aClients[pInfo->m_ClientId].m_aName);
		str_append(aLine, aBuf, sizeof(aLine));

		// clan
		if(pSearch && pSearch[0] != 0)
			if(str_find_nocase(m_pClient->m_aClients[pInfo->m_ClientId].m_aClan, pSearch))
				IsMatch = true;
		str_format(aBuf, sizeof(aBuf), "%16s|", m_pClient->m_aClients[pInfo->m_ClientId].m_aClan);
		str_append(aLine, aBuf, sizeof(aLine));

		// ping
		str_format(aBuf, sizeof(aBuf), "%3d|", clamp(pInfo->m_Latency, 0, 999));
		str_append(aLine, aBuf, sizeof(aLine));

		// team
		int DDTeam = m_pClient->m_Teams.Team(pInfo->m_ClientId);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_apInfoByDDTeamScore[j];

			if(!pInfo2)
				continue;

			NextDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientId);
			break;
		}

		if(OldDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_apInfoByDDTeamScore[j];

				if(!pInfo2)
					continue;

				OldDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientId);
				break;
			}
		}

		if(DDTeam != TEAM_FLOCK)
		{
			if(NextDDTeam != DDTeam)
			{
				if(m_pClient->m_Snap.m_aTeamSize[0] > 8)
					str_format(aBuf, sizeof(aBuf), "%7d|", DDTeam);
				else
					str_format(aBuf, sizeof(aBuf), "Team %2d|", DDTeam);
			}
			else
				str_format(aBuf, sizeof(aBuf), "%7s|", " ");
			str_append(aLine, aBuf, sizeof(aLine));
			if(OldDDTeam != DDTeam)
				dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------/");
			if(NextDDTeam != DDTeam)
				dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------\\");
		}

		OldDDTeam = DDTeam;
		if(IsMatch)
			dbg_msg("dump_players", "%s", aLine);
	}
	dbg_msg("dump_players", "+----------+--+----------------+----------------+---+-------+");
}

void CChillerBotUX::ConCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	int Tile = 32;
	if(!str_comp(pResult->GetString(0), "raw"))
		Tile = 1;
	g_Config.m_ClCampHack = 2;
	if(!pResult->NumArguments())
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - 32 * 3;
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + 32 * 3;
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
	// relative left and right
	if(pResult->NumArguments() > 1)
	{
		if(pSelf->GameClient()->m_Snap.m_pLocalCharacter)
		{
			pSelf->m_CampHackX1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X - Tile * pResult->GetInteger(0);
			pSelf->m_CampHackY1 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
			pSelf->m_CampHackX2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_X + Tile * pResult->GetInteger(1);
			pSelf->m_CampHackY2 = pSelf->GameClient()->m_Snap.m_pLocalCharacter->m_Y;
		}
		return;
	}
}

void CChillerBotUX::ConUnCampHack(IConsole::IResult *pResult, void *pUserData)
{
	CChillerBotUX *pSelf = (CChillerBotUX *)pUserData;
	g_Config.m_ClCampHack = 0;
	pSelf->m_pClient->m_Controls.m_aInputDirectionRight[g_Config.m_ClDummy] = 0;
	pSelf->m_pClient->m_Controls.m_aInputDirectionLeft[g_Config.m_ClDummy] = 0;
}


void CChillerBotUX::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		// unpack messages
		CKillMsg Kill;
		Kill.m_aVictimName[0] = '\0';
		Kill.m_aKillerName[0] = '\0';

		Kill.m_VictimId = pMsg->m_Victim;
		if(Kill.m_VictimId >= 0 && Kill.m_VictimId < MAX_CLIENTS)
		{
			Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimId].m_Team;
			Kill.m_VictimDDTeam = m_pClient->m_Teams.Team(Kill.m_VictimId);
			str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_VictimId].m_aName, sizeof(Kill.m_aVictimName));
			Kill.m_VictimRenderInfo = m_pClient->m_aClients[Kill.m_VictimId].m_RenderInfo;
		}

		Kill.m_KillerId = pMsg->m_Killer;
		if(Kill.m_KillerId >= 0 && Kill.m_KillerId < MAX_CLIENTS)
		{
			Kill.m_KillerTeam = m_pClient->m_aClients[Kill.m_KillerId].m_Team;
			str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerId].m_aName, sizeof(Kill.m_aKillerName));
			Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerId].m_RenderInfo;
		}

		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

		Kill.m_FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataObj ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : -1;

		bool KillMsgValid = (Kill.m_VictimRenderInfo.m_CustomColoredSkin && Kill.m_VictimRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_VictimRenderInfo.m_CustomColoredSkin && Kill.m_VictimRenderInfo.m_OriginalRenderSkin.m_Body.IsValid());
		// if killer != victim, killer must be valid too
		KillMsgValid &= Kill.m_KillerId == Kill.m_VictimId || ((Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_OriginalRenderSkin.m_Body.IsValid()));
		if(KillMsgValid && Kill.m_KillerId != Kill.m_VictimId)
		{
			for(int i = 0; i < 2; i++)
			{
				if(m_pClient->m_aLocalIds[i] != Kill.m_VictimId)
					continue;

				str_copy(m_aLastKiller[i], Kill.m_aKillerName, sizeof(m_aLastKiller[i]));
				char aBuf[512];
				str_timestamp_format(m_aLastKillerTime[i], sizeof(m_aLastKillerTime[i]), "%H:%M");
				str_format(
					aBuf,
					sizeof(aBuf),
					"[%s]main: %s [%s]dummy: %s",
					m_aLastKillerTime[0],
					m_aLastKiller[0],
					m_aLastKillerTime[1],
					m_aLastKiller[1]);
				SetComponentNoteLong("last killer", aBuf);
			}
		}
	}
	else if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		str_copy(m_aBroadcastText, pMsg->m_pMessage, sizeof(m_aBroadcastText));
		m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
		m_IsLeftSidedBroadcast = str_find(m_aBroadcastText, "                                ") != NULL;
	}
}

int CChillerBotUX::CountOnlinePlayers()
{
	// Code from scoreboard. There is probably a better way to do this
	int Num = 0;
	for(const auto *pInfo : m_pClient->m_Snap.m_apInfoByDDTeamScore)
		if(pInfo)
			Num++;
	return Num;
}