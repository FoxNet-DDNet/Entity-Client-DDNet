#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include "bindchat.h"

#include <vector>
#include <algorithm>
#include <engine/shared/console.h>

CBindChat::CBindChat()
{
	OnReset();
}

void CBindChat::ConAddBindchat(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CBindChat *pThis = static_cast<CBindChat *>(pUserData);
	pThis->AddBind(aName, aCommand);
}

void CBindChat::ConBindchats(IConsole::IResult *pResult, void *pUserData)
{
	CBindChat *pThis = static_cast<CBindChat *>(pUserData);
	char aBuf[BINDCHAT_MAX_NAME + BINDCHAT_MAX_CMD + 32];
	if(pResult->NumArguments() == 1)
	{
		const char *pName = pResult->GetString(0);
		for(const CBind &Bind : pThis->m_vBinds)
		{
			if(str_comp_nocase(Bind.m_aName, pName) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "%s = %s", Bind.m_aName, Bind.m_aCommand);
				pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindchat", aBuf);
				return;
			}
		}
		str_format(aBuf, sizeof(aBuf), "%s is not bound", pName);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindchat", aBuf);
	}
	else
	{
		for(const CBind &Bind : pThis->m_vBinds)
		{
			str_format(aBuf, sizeof(aBuf), "%s = %s", Bind.m_aName, Bind.m_aCommand);
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "bindchat", aBuf);
		}
	}
}

void CBindChat::ConRemoveBindchat(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	CBindChat *pThis = static_cast<CBindChat *>(pUserData);
	pThis->RemoveBind(aName);
}

void CBindChat::ConRemoveBindchatAll(IConsole::IResult *pResult, void *pUserData)
{
	CBindChat *pThis = static_cast<CBindChat *>(pUserData);
	pThis->RemoveAllBinds();
}

void CBindChat::ConBindchatDefaults(IConsole::IResult *pResult, void *pUserData)
{
	CBindChat *pThis = static_cast<CBindChat *>(pUserData);
	pThis->AddBind(".shrug", "say ¯\\_(ツ)_/¯");
	pThis->AddBind(".flip", "say (╯°□°)╯︵ ┻━┻");
	pThis->AddBind(".unflip", "say ┬─┬ノ( º _ ºノ)");
}

void CBindChat::AddBind(const char *pName, const char *pCommand)
{
	if((pName[0] == '\0' && pCommand[0] == '\0') || m_vBinds.size() >= BINDCHAT_MAX_BINDS)
		return;

	RemoveBind(pName); // Prevent duplicates

	CBind Bind;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	m_vBinds.push_back(Bind);
	SortChatBinds();
}

void CBindChat::AddBindDefault(const char *pName, const char *pCommand)
{
	if((pName[0] == '\0' && pCommand[0] == '\0') || m_vBinds.size() >= BINDCHAT_MAX_BINDS)
		return;

	CBind Bind;
	Bind.m_Default = true;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	m_vBinds.push_back(Bind);
}

void CBindChat::RemoveBindCommand(const char *pCommand)
{
	if(pCommand[0] == '\0')
		return;
	for(auto It = m_vBinds.begin(); It != m_vBinds.end(); ++It)
	{
		if(str_comp(It->m_aCommand, pCommand) == 0)
		{
			m_vBinds.erase(It);
			return;
		}
	}
}

void CBindChat::RemoveBind(const char *pName)
{
	if(pName[0] == '\0')
		return;
	for(auto It = m_vBinds.begin(); It != m_vBinds.end(); ++It)
	{
		if(str_comp(It->m_aName, pName) == 0)
		{
			m_vBinds.erase(It);
			return;
		}
	}
	SortChatBinds();
}

void CBindChat::RemoveBind(int Index)
{
	if(Index >= static_cast<int>(m_vBinds.size()) || Index < 0)
		return;
	auto It = m_vBinds.begin() + Index;
	m_vBinds.erase(It);
}

void CBindChat::RemoveAllBinds()
{
	m_vBinds.clear();
}

int CBindChat::GetBindNoDefault(const char *pCommand)
{
	if(pCommand[0] == '\0')
		return -1;
	for(auto It = m_vBinds.begin(); It != m_vBinds.end(); ++It)
	{
		if(str_comp_nocase(It->m_aCommand, pCommand) == 0 && !It->m_Default)
			return &*It - m_vBinds.data();
	}
	return -1;
}

int CBindChat::GetBind(const char *pCommand)
{
	if(pCommand[0] == '\0')
		return -1;
	for(auto It = m_vBinds.begin(); It != m_vBinds.end(); ++It)
	{
		if(str_comp_nocase(It->m_aCommand, pCommand) == 0)
			return &*It - m_vBinds.data();
	}
	return -1;
}

CBindChat::CBind *CBindChat::Get(int Index)
{
	if(Index < 0 || Index >= (int)m_vBinds.size())
		return nullptr;
	return &m_vBinds[Index];
}

void CBindChat::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::TCLIENTCHATBINDS);

	Console()->Register("bindchat", "s[name] r[command]", CFGFLAG_CLIENT, ConAddBindchat, this, "Add a chat bind");
	Console()->Register("bindchats", "?s[name]", CFGFLAG_CLIENT, ConBindchats, this, "Print command executed by this name or all chat binds");
	Console()->Register("unbindchat", "s[name] r[command]", CFGFLAG_CLIENT, ConRemoveBindchat, this, "Remove a chat bind");
	Console()->Register("unbindchatall", "", CFGFLAG_CLIENT, ConRemoveBindchatAll, this, "Removes all chat binds");
	Console()->Register("bindchatdefaults", "", CFGFLAG_CLIENT, ConBindchatDefaults, this, "Adds default chat binds");

	AddBindDefault("/specid", "specid");

	auto &&AddDefaultBind = [this](const char *pName, const char *pCommand) {
		for(int p = 0; p < 2; p++)
		{
			const char Prefix = p == 0 ? '.' : '!';
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "%c%s", Prefix, pName);
			AddBindDefault(aBuf, pCommand);
		}
	};

	AddDefaultBind("help", "exec data/entity/binds/.help.cfg");
	AddDefaultBind("extra", "exec data/entity/binds/.extra.cfg");
	AddDefaultBind("votekick", "votekick");
	AddDefaultBind("onlineinfo", "OnlineInfo");
	AddDefaultBind("playerinfo", "PlayerInfo");
	AddBindDefault("github", "view_link https://github.com/qxdFox/Entity-Client");
	AddDefaultBind("r", "reply_last");

	AddDefaultBind("friend", "add_friend");
	AddDefaultBind("unfriend", "remove_friend");

	AddDefaultBind("restoreskin", "restoreskin");
	AddDefaultBind("saveskin", "saveskin");

	AddDefaultBind("restore", "restoreskin");
	AddDefaultBind("save", "saveskin");

	AddDefaultBind("tempwar", "addtempwar");
	AddDefaultBind("untempwar", "deltempwar");
	AddDefaultBind("deltempwar", "deltempwar");

	AddDefaultBind("temphelper", "addtemphelper");
	AddDefaultBind("untemphelper", "deltemphelper");
	AddDefaultBind("deltemphelper", "deltemphelper");

	AddDefaultBind("tempmute", "addtempmute");
	AddDefaultBind("untempmute", "deltempmute");

	AddDefaultBind("war", "war_name_index 1");
	AddDefaultBind("delwar", "remove_war_name_index 1");
	AddDefaultBind("unwar", "remove_war_name_index 1");

	AddDefaultBind("team", "war_name_index 2");
	AddDefaultBind("delteam", "remove_war_name_index 2");
	AddDefaultBind("unteam", "remove_war_name_index 2");

	AddDefaultBind("helper", "war_name_index 3");
	AddDefaultBind("delhelper", "remove_war_name_index 3");
	AddDefaultBind("unhelper", "remove_war_name_index 3");

	AddDefaultBind("mute", "addmute");
	AddDefaultBind("delmute", "delmute");
	AddDefaultBind("unmute", "delmute");

	AddDefaultBind("clanwar", "war_clan_index 1");
	AddDefaultBind("delclanwar", "remove_war_clan_index 1");
	AddDefaultBind("unclanwar", "remove_war_clan_index 1");

	AddDefaultBind("clanteam", "war_clan_index 2");
	AddDefaultBind("delclanteam", "remove_war_clan_index 2");
	AddDefaultBind("unclanteam", "remove_war_clan_index 2");
}

void CBindChat::OnInit()
{
	CacheChatCommands();
	SortChatBinds();
}

void CBindChat::ExecuteBind(int Bind, const char *pArgs)
{
	char aBuf[BINDCHAT_MAX_CMD] = "";
	str_append(aBuf, m_vBinds[Bind].m_aCommand);
	if(pArgs)
	{
		str_append(aBuf, " ");
		str_append(aBuf, pArgs);
	}
	Console()->ExecuteLine(aBuf);
}

bool CBindChat::CheckBindChat(const char *pText)
{
	const char *pSpace = str_find(pText, " ");
	size_t SpaceIndex = pSpace ? pSpace - pText : strlen(pText);
	for(const CBind &Bind : m_vBinds)
	{
		if(str_comp_nocase_num(pText, Bind.m_aName, SpaceIndex) == 0)
			return true;
	}
	return false;
}

bool CBindChat::ChatDoBinds(const char *pText)
{
	if(pText[0] == ' ' || pText[0] == '\0' || pText[1] == '\0')
		return false;

	const bool IsExclemataion = str_startswith(pText, "!") && g_Config.m_ClSendExclamation;

	CChat &Chat = GameClient()->m_Chat;
	const char *pSpace = str_find(pText, " ");
	size_t SpaceIndex = pSpace ? pSpace - pText : strlen(pText);
	for(const CBind &Bind : m_vBinds)
	{
		const bool SendsMessage = str_find(Bind.m_aCommand, "say") ||
					  str_find(Bind.m_aCommand, "say_team") || str_find(Bind.m_aCommand, "reply_last");
		if(str_startswith_nocase(pText, Bind.m_aName) &&
			str_comp_nocase_num(pText, Bind.m_aName, SpaceIndex) == 0)
		{
			ExecuteBind(&Bind - m_vBinds.data(), pSpace ? pSpace + 1 : nullptr);
			// Add to history (see CChat::SendChatQueued)
			const int Length = str_length(pText);
			CChat::CHistoryEntry *pEntry = Chat.m_History.Allocate(sizeof(CChat::CHistoryEntry) + Length);
			pEntry->m_Team = 0; // All
			str_copy(pEntry->m_aText, pText, Length + 1);
			if(IsExclemataion && !SendsMessage)
				return false;
			return true;
		}
	}
	return false;
}

bool CBindChat::ChatDoAutocomplete(bool ShiftPressed)
{
	CChat &Chat = GameClient()->m_Chat;

	if(!ValidPrefix(Chat.m_aCompletionBuffer[0]))
		return false;

	if(m_vChatCommands.size() == 0)
		return false;

	const CChat::CCommand *pCompletionCommand = nullptr;
	int InitialCompletionChosen = Chat.m_CompletionChosen;
	int InitialCompletionUsed = Chat.m_CompletionUsed;

	if(ShiftPressed && Chat.m_CompletionUsed)
		Chat.m_CompletionChosen--;
	else if(!ShiftPressed)
		Chat.m_CompletionChosen++;
	Chat.m_CompletionChosen = (Chat.m_CompletionChosen + m_vChatCommands.size()) % m_vChatCommands.size(); // size != 0

	Chat.m_CompletionUsed = true;
	int Index = Chat.m_CompletionChosen;
	for(size_t i = 0; i < m_vChatCommands.size(); i++)
	{
		int CommandIndex = (Index + i) % m_vChatCommands.size();
		if(str_startswith_nocase(m_vChatCommands.at(CommandIndex).m_aName, Chat.m_aCompletionBuffer))
		{
			pCompletionCommand = &m_vChatCommands.at(CommandIndex);
			Chat.m_CompletionChosen = CommandIndex;
			break;
		}
	}

	// insert the command
	if(pCompletionCommand)
	{
		char aBuf[MAX_LINE_LENGTH];
		// add part before the name
		str_truncate(aBuf, sizeof(aBuf), Chat.m_Input.GetString(), Chat.m_PlaceholderOffset);

		// add the command
		str_append(aBuf, pCompletionCommand->m_aName);

		// add separator
		// TODO: figure out if the command would accept an extra param
		// char commandBuf[128];
		// str_next_token(pCompletionBind->m_aCommand, " ", commandBuf, sizeof(commandBuf));
		// CCommandInfo *pInfo = GameClient()->Console()->GetCommandInfo(commandBuf, CFGFLAG_CLIENT, false);
		// if(pInfo && pInfo->m_pParams != '\0')
		const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
		str_append(aBuf, pSeparator);

		// add part after the name
		str_append(aBuf, Chat.m_Input.GetString() + Chat.m_PlaceholderOffset + Chat.m_PlaceholderLength);

		Chat.m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName);
		Chat.m_Input.Set(aBuf);
		Chat.m_Input.SetCursorOffset(Chat.m_PlaceholderOffset + Chat.m_PlaceholderLength);
	}
	else
	{
		Chat.m_CompletionChosen = InitialCompletionChosen;
		Chat.m_CompletionUsed = InitialCompletionUsed;
	}

	return pCompletionCommand != nullptr;
}

void CBindChat::CacheChatCommands()
{
	m_vChatCommands.clear();

	CChat &Chat = GameClient()->m_Chat;

	for(const auto &ServerCommand : Chat.m_vServerCommands)
	{
		char Temp[64];
		str_format(Temp, sizeof(Temp), "/%s", ServerCommand.m_aName);
		CChat::CCommand Command(Temp, ServerCommand.m_aParams, ServerCommand.m_aHelpText);
		Command.m_Prefix = '/';
		m_vChatCommands.push_back(Command);
	}

	for(const auto &ChatBind : GameClient()->m_Bindchat.m_vBinds)
	{
		if(!ChatBind.m_aName[0])
			continue;

		char CommandBuf[128];
		str_next_token(ChatBind.m_aCommand, " ", CommandBuf, sizeof(CommandBuf));
		const IConsole::CCommandInfo *pInfo = GameClient()->Console()->GetCommandInfo(CommandBuf, CFGFLAG_CLIENT, false);
		CChat::CCommand Command(ChatBind.m_aName, pInfo->m_pParams, pInfo->m_pHelp);
		Command.m_Prefix = ChatBind.m_aName[0];
		bool Found = false;
		for(const auto &ChatCommand : m_vChatCommands)
		{
			if(!str_comp(ChatCommand.m_aName, ChatBind.m_aName))
			{
				Found = true;
				break;
			}
		}
		if(!Found)
			m_vChatCommands.push_back(Command);
	}
}

void CBindChat::SortChatBinds()
{
	std::sort(m_vChatCommands.begin(), m_vChatCommands.end());
}

bool CBindChat::ValidPrefix(char Prefix) const
{
	for(const auto &Command : m_vChatCommands)
	{
		if(Command.m_Prefix == '\0')
			continue;

		if(Prefix == Command.m_Prefix)
			return true;
	}
	return false;
}

void CBindChat::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindChat *pThis = (CBindChat *)pUserData;
	bool Failed = false;

	for(CBind &Bind : pThis->m_vBinds)
	{
		// Default binds do not need to be saved because they will get added on launch
		if(Bind.m_Default)
			continue;

		char aBuf[BINDCHAT_MAX_CMD * 2] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "bindchat \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aName, pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aCommand, pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::TCLIENTCHATBINDS);
	}
}