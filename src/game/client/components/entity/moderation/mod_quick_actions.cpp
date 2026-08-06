#include "mod_quick_actions.h"

#include <base/str.h>

#include <engine/config.h>
#include <engine/shared/config.h>

#include <algorithm>

void CModQuickActions::ConAddModAction(IConsole::IResult *pResult, void *pUserData)
{
	CModQuickActions *pThis = static_cast<CModQuickActions *>(pUserData);
	pThis->AddAction(pResult->GetString(0), pResult->GetString(1));
}

void CModQuickActions::ConRemoveModAction(IConsole::IResult *pResult, void *pUserData)
{
	CModQuickActions *pThis = static_cast<CModQuickActions *>(pUserData);
	pThis->RemoveAction(pResult->GetString(0), pResult->GetString(1));
}

void CModQuickActions::ConRemoveAllModActions(IConsole::IResult *pResult, void *pUserData)
{
	CModQuickActions *pThis = static_cast<CModQuickActions *>(pUserData);
	pThis->RemoveAllActions();
}

void CModQuickActions::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYMODACTIONS);

	Console()->Register("add_mod_action", "s[name] r[command]", CFGFLAG_CLIENT, ConAddModAction, this, "Add a quick action to the moderation menu");
	Console()->Register("remove_mod_action", "s[name] r[command]", CFGFLAG_CLIENT, ConRemoveModAction, this, "Remove a quick action from the moderation menu");
	Console()->Register("delete_all_mod_actions", "", CFGFLAG_CLIENT, ConRemoveAllModActions, this, "Removes all moderation menu quick actions");
}

int CModQuickActions::AddAction(const char *pName, const char *pCommand)
{
	if(m_vActions.size() >= MODQUICKACTION_MAX_ACTIONS)
		return -1;

	CAction Action;
	str_copy(Action.m_aName, pName);
	str_copy(Action.m_aCommand, pCommand);
	m_vActions.push_back(Action);
	return static_cast<int>(m_vActions.size()) - 1;
}

void CModQuickActions::RemoveAction(const char *pName, const char *pCommand)
{
	CAction Action;
	str_copy(Action.m_aName, pName);
	str_copy(Action.m_aCommand, pCommand);
	auto It = std::find(m_vActions.begin(), m_vActions.end(), Action);
	if(It != m_vActions.end())
		m_vActions.erase(It);
}

void CModQuickActions::RemoveAction(int Index)
{
	if(Index < 0 || Index >= static_cast<int>(m_vActions.size()))
		return;
	m_vActions.erase(m_vActions.begin() + Index);
}

void CModQuickActions::RemoveAllActions()
{
	m_vActions.clear();
}

void CModQuickActions::MoveAction(int Index, int NewIndex)
{
	const int NumActions = static_cast<int>(m_vActions.size());
	if(Index < 0 || Index >= NumActions || NewIndex < 0 || NewIndex >= NumActions || Index == NewIndex)
		return;

	const CAction Action = m_vActions[Index];
	m_vActions.erase(m_vActions.begin() + Index);
	m_vActions.insert(m_vActions.begin() + NewIndex, Action);
}

void CModQuickActions::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CModQuickActions *pThis = static_cast<CModQuickActions *>(pUserData);

	for(const CAction &Action : pThis->m_vActions)
	{
		char aBuf[(MODQUICKACTION_MAX_NAME + MODQUICKACTION_MAX_CMD) * 2 + 32] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_mod_action \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Action.m_aName, pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Action.m_aCommand, pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYMODACTIONS);
	}
}
