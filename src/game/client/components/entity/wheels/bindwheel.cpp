#include "bindwheel.h"

#include "wheel.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

static std::function<void(CGameClient &)> CommandCallback(const char *pCommand)
{
	if(pCommand[0] == '\0')
		return nullptr;

	std::string CommandCopy = pCommand;

	return [Command = std::move(CommandCopy)](CGameClient &GameClient) {
		GameClient.Console()->ExecuteLine(Command.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
	};
}

CBindWheel::CBindWheel()
{
	OnReset();
}

void CBindWheel::ConBindwheelExecuteHover(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pThis = (CBindWheel *)pUserData;
	pThis->m_Wheel.ExecuteHoveredItem();
}

void CBindWheel::ConOpenBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pThis = (CBindWheel *)pUserData;
	if(pThis->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(pThis->GameClient()->m_Emoticon.IsActive() || pThis->GameClient()->m_PlayerActions.IsActive())
			pThis->m_Wheel.m_Active = false;
		else
			pThis->m_Wheel.m_Active = pResult->GetInteger(0) != 0;
	}
}

void CBindWheel::ConAddBindwheelLegacy(IConsole::IResult *pResult, void *pUserData)
{
	int BindPos = pResult->GetInteger(0);
	if(BindPos < 0 || BindPos >= BINDWHEEL_MAX_BINDS)
		return;

	char aName[BINDWHEEL_MAX_NAME] = "";
	char aCommand[BINDWHEEL_MAX_CMD] = "";
	str_copy(aName, pResult->GetString(1));
	str_copy(aCommand, pResult->GetString(2));

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	if(pThis->m_Wheel.m_vItems.size() <= (size_t)BindPos)
		pThis->m_Wheel.m_vItems.resize((size_t)BindPos + 1);

	pThis->m_Wheel.m_vItems[BindPos].m_Name = aName;
	pThis->m_Wheel.m_vItems[BindPos].m_Id = aCommand;
	pThis->m_Wheel.m_vItems[BindPos].m_Execute = CommandCallback(aCommand);
}

void CBindWheel::ConAddBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *pName = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->AddBind(pName, pCommand);
}

void CBindWheel::ConRemoveBindwheel(IConsole::IResult *pResult, void *pUserData)
{
	const char *pName = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->m_Wheel.RemoveItem(pName, pCommand);
}

void CBindWheel::ConRemoveAllBindwheelBinds(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pThis = static_cast<CBindWheel *>(pUserData);
	pThis->m_Wheel.RemoveAllItems();
}

void CBindWheel::AddBind(const char *pName, const char *pCommand)
{
	char aName[BINDWHEEL_MAX_NAME] = "";
	char aCommand[BINDWHEEL_MAX_CMD] = "";
	str_copy(aName, pName);
	str_copy(aCommand, pCommand);
	m_Wheel.AddItem(aName, aCommand, CommandCallback(aCommand));
}

void CBindWheel::UpdateBind(int Index, const char *pName, const char *pCommand)
{
	if(pName[0] == '\0')
		return;

	char aName[BINDWHEEL_MAX_NAME] = "";
	char aCommand[BINDWHEEL_MAX_CMD] = "";
	str_copy(aName, pName);
	str_copy(aCommand, pCommand);

	m_Wheel.m_vItems[Index].m_Name = aName;
	m_Wheel.m_vItems[Index].m_Id = aCommand;
	m_Wheel.m_vItems[Index].m_Execute = CommandCallback(aCommand);
}

void CBindWheel::OnConsoleInit()
{
	m_Wheel.SetData(GameClient(), BINDWHEEL_MAX_BINDS);

	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::ENTITYBINDHWEEL);

	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConOpenBindwheel, this, "Open bindwheel selector");
	Console()->Register("+bindwheel_execute_hover", "", CFGFLAG_CLIENT, ConBindwheelExecuteHover, this, "Execute hovered bindwheel bind");

	Console()->Register("bindwheel", "i[index] s[name] r[command]", CFGFLAG_CLIENT, ConAddBindwheelLegacy, this, "DONT USE THIS! USE add_bindwheel INSTEAD!");
	Console()->Register("add_bindwheel", "s[name] r[command]", CFGFLAG_CLIENT, ConAddBindwheel, this, "Add a bind to the bindwheel");
	Console()->Register("remove_bindwheel", "s[name] r[command]", CFGFLAG_CLIENT, ConRemoveBindwheel, this, "Remove a bind from the bindwheel");
	Console()->Register("delete_all_bindwheel_binds", "", CFGFLAG_CLIENT, ConRemoveAllBindwheelBinds, this, "Removes all bindwheel binds");
}

void CBindWheel::OnReset()
{
	m_Wheel.Reset();
}

void CBindWheel::OnRelease()
{
	m_Wheel.m_Active = false;
}

bool CBindWheel::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Wheel.m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_Wheel.m_SelectorMouse += vec2(x, y);
	return true;
}

bool CBindWheel::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CBindWheel::OnRender()
{
	if(!m_Wheel.Render(g_Config.m_ClResetBindWheelMouse))
		return;

	const CUIRect &Screen = *Ui()->Screen();
	RenderTools()->RenderCursor(m_Wheel.m_SelectorMouse + vec2(Screen.w, Screen.h) / 2.0f, 24.0f);
}

void CBindWheel::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CBindWheel *pThis = (CBindWheel *)pUserData;

	for(CWheelItem &Bind : pThis->m_Wheel.m_vItems)
	{
		char aBuf[BINDWHEEL_MAX_CMD * 2] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_bindwheel \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_Name.c_str(), pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_Id.c_str(), pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf, ConfigDomain::ENTITYBINDHWEEL);
	}
}
