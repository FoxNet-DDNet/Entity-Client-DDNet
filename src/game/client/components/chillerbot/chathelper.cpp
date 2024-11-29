// ChillerDragon 2020 - chillerbot ux

#include <cinttypes>

#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol.h>

#include "chathelper.h"

void CChatHelper::RegisterCommand(const char *pName, const char *pParams, int Flags, const char *pHelp)
{
	m_vCommands.emplace_back(pName, pParams);
}

int CChatHelper::ChatCommandGetROffset(const char *pCmd)
{
	for(const auto &Command : m_vCommands)
	{
		if(str_comp(Command.m_pName, pCmd))
			continue;
		return Command.m_ROffset;
	}
	return -1;
}

void CChatHelper::OnInit()
{
	m_pChillerBot = &m_pClient->m_ChillerBotUX;

	m_aGreetName[0] = '\0';
	m_NextGreetClear = 0;
	m_NextMessageSend = 0;
	m_aLastAfkPing[0] = '\0';
	mem_zero(m_aaaSendBuffer, sizeof(m_aaaSendBuffer));
}

void CChatHelper::OnRender()
{
	int64_t TimeNow = time_get();
	if(TimeNow % 10 == 0)
	{
		for(auto &Ping : m_aLastPings)
		{
			if(!Ping.m_ReciveTime)
				continue;
			if(Ping.m_ReciveTime < TimeNow - time_freq() * 60)
			{
				Ping.m_aName[0] = '\0';
				Ping.m_aClan[0] = '\0';
				Ping.m_aMessage[0] = '\0';
				Ping.m_ReciveTime = 0;
			}
		}
		if(m_NextGreetClear < TimeNow)
		{
			m_NextGreetClear = 0;
			m_aGreetName[0] = '\0';
		}
		if(m_NextMessageSend < TimeNow)
		{
			for(int Team = 0; Team < NUM_BUFFERS; Team++)
			{
				if(m_aaaSendBuffer[Team][0][0])
				{
					// the BUFFER_ enum overlaps with 0 for public chat and 1 for team chat chat.cpp api
					m_pClient->m_Chat.SendChat(Team, m_aaaSendBuffer[Team][0]);
					for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
						str_copy(m_aaaSendBuffer[Team][i], m_aaaSendBuffer[Team][i + 1], sizeof(m_aaaSendBuffer[Team][i]));
					m_aaaSendBuffer[Team][MAX_CHAT_BUFFER_LEN - 1][0] = '\0';
					m_NextMessageSend = TimeNow + time_freq() * 5;
				}
			}
		}
	}
}

void CChatHelper::SayBuffer(const char *pMsg, int Team, bool StayAfk)
{
	if(StayAfk)
		m_pClient->m_ChillerBotUX.m_IgnoreChatAfk++;
	// append at end
	for(auto &Buf : m_aaaSendBuffer[Team])
	{
		if(Buf[0])
			continue;
		str_copy(Buf, pMsg, sizeof(Buf));
		return;
	}
	// full -> shift buffer and overwrite oldest element (index 0)
	for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
		str_copy(m_aaaSendBuffer[Team][i], m_aaaSendBuffer[Team][i + 1], sizeof(m_aaaSendBuffer[Team][i]));
	str_copy(m_aaaSendBuffer[Team][MAX_CHAT_BUFFER_LEN - 1], pMsg, sizeof(m_aaaSendBuffer[Team][MAX_CHAT_BUFFER_LEN - 1]));
}

void CChatHelper::OnConsoleInit()
{
	Console()->Register("say_hi", "", CFGFLAG_CLIENT, ConSayHi, this, "Respond to the last greeting in chat");
	Console()->Register("say_format", "s[message]", CFGFLAG_CLIENT, ConSayFormat, this, "send message replacing %n with last ping name");
}

void CChatHelper::ConSayHi(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->DoGreet();
}

void CChatHelper::ConSayFormat(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->SayFormat(pResult->GetString(0));
}

bool CChatHelper::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_utf8_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}

	return false;
}

void CChatHelper::SayFormat(const char *pMsg)
{
	char aBuf[1028] = {0};
	long unsigned int i = 0;
	long unsigned int BufI = 0;
	for(i = 0; pMsg[i] && i < sizeof(aBuf); i++)
	{
		if(pMsg[i] == '%' && pMsg[maximum((int)i - 1, 0)] != '\\')
		{
			if(pMsg[i + 1] == 'n')
			{
				str_append(aBuf, m_aLastPings[0].m_aName, sizeof(aBuf));
				BufI += str_length(m_aLastPings[0].m_aName);
				i++;
				continue;
			}
			else if(pMsg[i + 1] == 'g')
			{
				str_append(aBuf, m_aGreetName, sizeof(aBuf));
				BufI += str_length(m_aGreetName);
				i++;
				continue;
			}
		}
		aBuf[BufI++] = pMsg[i];
	}
	// string end
	aBuf[BufI] = '\0';
	// fallback nullterm at buffer end
	aBuf[sizeof(aBuf) - 1] = '\0';
	m_pClient->m_Chat.SendChat(0, aBuf);
}


void CChatHelper::DoGreet()
{
	if(m_aGreetName[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "hi %s", m_aGreetName);
		m_pClient->m_Chat.SendChat(0, aBuf);
		return;
	}
	m_pClient->m_Chat.SendChat(0, "hi");
}

int CChatHelper::Get128Name(const char *pMsg, char *pName)
{
	int i = 0;
	for(i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':' && pMsg[i + 1] == ' ')
		{
			str_copy(pName, pMsg, i + 1);
			return i;
		}
	}
	str_copy(pName, " ", 2);
	return -1;
}

void CChatHelper::OnChatMessage(int ClientId, int Team, const char *pMsg)
{
	if(ClientId < 0 || ClientId > MAX_CLIENTS)
		return;

	bool Highlighted = false;

	if(Team == 3) // whisper recv
		Highlighted = true;


	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(m_pClient->m_aLocalIds[0] == -1)
			return;
		if(m_pClient->Client()->DummyConnected() && m_pClient->m_aLocalIds[1] == -1)
			return;

		if(ClientId >= 0 && ClientId != m_pClient->m_aLocalIds[0] && (!m_pClient->Client()->DummyConnected() || ClientId != m_pClient->m_aLocalIds[1]))
		{
			// main character
			Highlighted |= LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName);
			// dummy
			Highlighted |= m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName);
		}
	}
	else
	{
		if(m_pClient->m_Snap.m_LocalClientId == -1)
			return;
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= m_pClient->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_aName);
	}

	if(!Highlighted)
		return;

	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientId].m_aName, sizeof(aName));
	if(ClientId == 63 && !str_comp_num(m_pClient->m_aClients[ClientId].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientId].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName))
		return;

	// could iterate over ping history and also ignore older duplicates
	// ignore duplicated messages
	if(!str_comp(m_aLastPings[0].m_aMessage, pMsg))
		return;
	PushPing(aName, m_pClient->m_aClients[ClientId].m_aClan, pMsg, Team);

	if((g_Config.m_ClReplyMuted && GameClient()->m_WarList.IsMutelist(m_pClient->m_aClients[ClientId].m_aName)) || (GameClient()->m_WarList.IsWarlist(m_pClient->m_aClients[ClientId].m_aName) && g_Config.m_ClHideEnemyChat))
	{
		if(Team == 3) // whisper recv
		{
			char bBuf[2048] = "/w %n ";
			str_append(bBuf, g_Config.m_ClAutoReplyMutedMsg);
			SayFormat(bBuf);
		}
		else
		{
			char bBuf[2048] = "%n: ";
			str_append(bBuf, g_Config.m_ClAutoReplyMutedMsg);
			SayFormat(bBuf);
		}
	}
	if(g_Config.m_ClTabbedOutMsg && !GameClient()->m_WarList.IsMutelist(m_pClient->m_aClients[ClientId].m_aName))
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{

			if(Team == 3) // whisper recv
			{
				char bBuf[2048] = "/w %n ";
				str_append(bBuf, g_Config.m_ClAutoReplyMsg);
				SayFormat(bBuf);
			}
			else
			{
				char bBuf[2048] = "%n: ";
				str_append(bBuf, g_Config.m_ClAutoReplyMsg);
				SayFormat(bBuf);
			}
		}
	}

}

void CChatHelper::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
	}
}



int CChatHelper::IsSpam(int ClientId, int Team, const char *pMsg)
{
	
	int MsgLen = str_length(pMsg);
	int NameLen = 0;
	const char *pName = m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName;
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, pName))
	{
		Highlighted = true;
		NameLen = str_length(pName);
	}
	else if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, pDummyName))
	{
		Highlighted = true;
		NameLen = str_length(pDummyName);
	}
	if(Team == 3) // whisper recv
		Highlighted = true;

	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientId].m_aName, sizeof(aName));
	if(ClientId == 63 && !str_comp_num(m_pClient->m_aClients[ClientId].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		MsgLen -= str_length(aName) + 2;
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientId].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[0]].m_aName))
		return SPAM_NONE;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_aLocalIds[1]].m_aName))
		return SPAM_NONE;

	// ping without further context
	if(MsgLen < NameLen + 2)
		return SPAM_OTHER;
	else if(m_LangParser.IsAskToAsk(pMsg))
		return SPAM_OTHER;
	else if(!str_comp(aName, "nameless tee") || !str_comp(aName, "brainless tee") || str_find(aName, ")nameless tee") || str_find(aName, ")brainless te"))
		return SPAM_OTHER;
	else if(str_find(pMsg, "bro, check out this client: krxclient.pages.dev"))
		return SPAM_OTHER;
	else if((str_find_nocase(pMsg, "help") || str_find_nocase(pMsg, "hilfe")) && MsgLen < NameLen + 16)
		return SPAM_OTHER;
	else if((str_find_nocase(pMsg, "give") || str_find_nocase(pMsg, "need") || str_find_nocase(pMsg, "want") || str_find_nocase(pMsg, "please") || str_find_nocase(pMsg, "pls") || str_find_nocase(pMsg, "plz")) &&
		(str_find_nocase(pMsg, "rcon") || str_find_nocase(pMsg, "password") || str_find_nocase(pMsg, "admin") || str_find_nocase(pMsg, "helper") || str_find_nocase(pMsg, "mod") || str_find_nocase(pMsg, "money") || str_find_nocase(pMsg, "moni") || str_find_nocase(pMsg, "flag")))
	{
		// "I give you money"
		// "Do you want me to give you the flag"
		// "I give money back chillerdragon"
		// "ChillerDragon: Can you please post a tutorial on how to download the DDNet++ mod"
		if((str_find_nocase(pMsg, " i ") && str_find_nocase(pMsg, "you")) || str_find_nocase(pMsg, "give you") || m_LangParser.StrFindOrder(pMsg, 2, "give", "back") ||
			(str_find_nocase(pMsg, "mod") && (str_find_nocase(pMsg, "tutorial") || str_find_nocase(pMsg, "code") || str_find_nocase(pMsg, "download"))))
			return SPAM_NONE;
		else
			return SPAM_OTHER;
	}
	return SPAM_NONE;
}

bool CChatHelper::OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB)
{
	if(pCompletionBuffer[0] != '.')
		return false;

	CCommand *pCompletionCommand = 0;

	const size_t NumCommands = m_vCommands.size();

	if(ReverseTAB)
		*pCompletionChosen = (*pCompletionChosen - 1 + 2 * NumCommands) % (2 * NumCommands);
	else
		*pCompletionChosen = (*pCompletionChosen + 1) % (2 * NumCommands);

	const char *pCommandStart = pCompletionBuffer + 1;
	for(size_t i = 0; i < 2 * NumCommands; ++i)
	{
		int SearchType;
		int Index;

		if(ReverseTAB)
		{
			SearchType = ((*pCompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen - i + NumCommands) % NumCommands;
		}
		else
		{
			SearchType = ((*pCompletionChosen + i) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen + i) % NumCommands;
		}

		auto &Command = m_vCommands[Index];

		if(str_comp_nocase_num(Command.m_pName, pCommandStart, str_length(pCommandStart)) == 0)
		{
			pCompletionCommand = &Command;
			*pCompletionChosen = Index + SearchType * NumCommands;
			break;
		}
	}
	if(!pCompletionCommand)
		return false;

	char aBuf[256];
	// add part before the name
	str_truncate(aBuf, sizeof(aBuf), pInput->GetString(), PlaceholderOffset);

	// add the command
	str_append(aBuf, ".", sizeof(aBuf));
	str_append(aBuf, pCompletionCommand->m_pName, sizeof(aBuf));

	// add separator
	const char *pSeparator = pCompletionCommand->m_pParams[0] == '\0' ? "" : " ";
	str_append(aBuf, pSeparator, sizeof(aBuf));
	if(*pSeparator)
		str_append(aBuf, pSeparator, sizeof(aBuf));

	// add part after the name
	str_append(aBuf, pInput->GetString() + PlaceholderOffset + PlaceholderLength, sizeof(aBuf));

	PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_pName) + 1;
	*pOldChatStringLength = pInput->GetLength();
	pInput->Set(aBuf); // TODO: Use Add instead
	pInput->SetCursorOffset(PlaceholderOffset + PlaceholderLength);
	return true;
}
