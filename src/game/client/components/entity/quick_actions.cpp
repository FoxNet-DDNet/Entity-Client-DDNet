#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>

#include "../chat.h"
#include "../emoticon.h"

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "quick_actions.h"
#include <game/client/gameclient.h>

CQuickActions::CQuickActions()
{
	OnReset();
}

vec2 CQuickActions::GetCursorWorldPos() const
{
	if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW)
		return GameClient()->m_Camera.m_Center;

	vec2 Target = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];

	vec2 TargetCameraOffset(0, 0);
	float l = length(Target);

	if(l > 0.0001f) // make sure that this isn't 0
	{
		float OffsetAmount = maximum(l - GameClient()->m_Snap.m_SpecInfo.m_Deadzone, 0.0f) * (GameClient()->m_Snap.m_SpecInfo.m_FollowFactor / 100.0f);
		TargetCameraOffset = normalize(Target) * OffsetAmount;
	}

	vec2 Position = GameClient()->m_CursorInfo.Position();

	const float Zoom = (GameClient()->m_Camera.m_Zooming && GameClient()->m_Camera.m_AutoSpecCameraZooming) ? GameClient()->m_Camera.m_Zoom : GameClient()->m_Snap.m_SpecInfo.m_Zoom;
	vec2 WorldTarget = Position + (Target - TargetCameraOffset) * Zoom + TargetCameraOffset;

	return WorldTarget;
}

int CQuickActions::GetClosetClientId(vec2 Pos)
{
	int ClosestId = -1;
	if(GameClient()->m_Snap.m_LocalClientId < 0)
		return ClosestId;

	float ClosestDistance = std::numeric_limits<float>::max();

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == GameClient()->m_Snap.m_LocalClientId)
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;

		if(!GameClient()->m_aClients[ClientId].m_Active)
			continue;
		if(!GameClient()->m_aClients[ClientId].m_RenderInfo.Valid())
			continue;

		vec2 PlayerPos = GameClient()->m_Snap.m_aCharacters[ClientId].m_Position;
		float Distance = distance(Pos, PlayerPos);

		if(Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestId = ClientId;
		}
	}

	return ClosestId;
}

void CQuickActions::ConOpenQuickActionMenu(IConsole::IResult *pResult, void *pUserData)
{
	CQuickActions *pThis = (CQuickActions *)pUserData;
	if(pThis->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(pThis->m_QuickActionId < 0)
		{
			vec2 Pos = pThis->GetCursorWorldPos();
			pThis->m_QuickActionId = pThis->GetClosetClientId(Pos);
			if(pThis->m_QuickActionId < 0 || pThis->m_QuickActionId >= MAX_CLIENTS)
				pThis->m_QuickActionId = -1;
		}

		pThis->m_Active = pResult->GetInteger(0) != 0;
	}
}

void CQuickActions::ConAddQuickAction(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CQuickActions *pThis = static_cast<CQuickActions *>(pUserData);
	pThis->AddBind(aName, aCommand);
}

void CQuickActions::ConRemoveQuickAction(IConsole::IResult *pResult, void *pUserData)
{
	const char *aName = pResult->GetString(0);
	const char *aCommand = pResult->GetString(1);

	CQuickActions *pThis = static_cast<CQuickActions *>(pUserData);
	pThis->RemoveBind(aName, aCommand);
}

void CQuickActions::ConResetAllQuickActions(IConsole::IResult *pResult, void *pUserData)
{
	CQuickActions *pThis = static_cast<CQuickActions *>(pUserData);
	pThis->RemoveAllBinds();
	pThis->AddDefaultBinds();
}

void CQuickActions::ConRemoveAllQuickActions(IConsole::IResult *pResult, void *pUserData)
{
	CQuickActions *pThis = static_cast<CQuickActions *>(pUserData);
	pThis->RemoveAllBinds();
}

void CQuickActions::AddBind(const char *pName, const char *pCommand)
{
	if((pName[0] == '\0' && pCommand[0] == '\0') || m_vBinds.size() >= BINDWHEEL_MAX_BINDS)
		return;

	CBind Bind;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	m_vBinds.push_back(Bind);
}

void CQuickActions::RemoveBind(const char *pName, const char *pCommand)
{
	CBind Bind;
	str_copy(Bind.m_aName, pName);
	str_copy(Bind.m_aCommand, pCommand);
	auto it = std::find(m_vBinds.begin(), m_vBinds.end(), Bind);
	if(it != m_vBinds.end())
		m_vBinds.erase(it);
}

void CQuickActions::RemoveBind(int Index)
{
	if(Index >= static_cast<int>(m_vBinds.size()) || Index < 0)
		return;
	auto Pos = m_vBinds.begin() + Index;
	m_vBinds.erase(Pos);
}

void CQuickActions::RemoveAllBinds()
{
	m_vBinds.clear();
}

void CQuickActions::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterECallback(ConfigSaveCallback, this);

	Console()->Register("+quickactions", "", CFGFLAG_CLIENT, ConOpenQuickActionMenu, this, "Open quick action menu");

	Console()->Register("add_quickaction", "s[name] s[command]", CFGFLAG_CLIENT, ConAddQuickAction, this, "Add a bind to the quick action menu");
	Console()->Register("remove_quickaction", "s[name] s[command]", CFGFLAG_CLIENT, ConRemoveQuickAction, this, "Remove a bind from the quick action menu");
	Console()->Register("reset_all_quickactions", "", CFGFLAG_CLIENT, ConResetAllQuickActions, this, "Resets quick actions to the default ones");
	Console()->Register("delete_all_quickactions", "", CFGFLAG_CLIENT, ConRemoveAllQuickActions, this, "Removes all quick actions");
}

void CQuickActions::AddDefaultBinds()
{
	AddBind("Add War", "war_name_index 1 \"%s\"");
	AddBind("Add Team", "war_name_index 2 \"%s\"");
	AddBind("Add Helper", "war_name_index 3 \"%s\"");
	AddBind("Add Clan War", "war_clan_index 1 \"%s\"");
	AddBind("Add Clan Team", "war_clan_index 2 \"%s\"");
	AddBind("Player Info", "playerinfo \"%s\"");
	AddBind("Message Player", "set_input %s: ");
	AddBind("Whisper Player", "set_input /w \"%s\" ");
	AddBind("Mute", "addmute \"%s\"");
}

void CQuickActions::OnInit()
{
	if(m_vBinds.empty())
		AddDefaultBinds();
}

void CQuickActions::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedBind = -1;
}

void CQuickActions::OnRelease()
{
	m_Active = false;
}

bool CQuickActions::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;
	if(m_QuickActionId < 0 || m_QuickActionId >= MAX_CLIENTS)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}


void CQuickActions::DrawCircle(float x, float y, float r, int Segments)
{
	Graphics()->DrawCircle(x, y, r, Segments);
}

void CQuickActions::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const bool BindsEmpty = m_vBinds.empty();

	// DrawDebugLines();

	if(!m_Active)
	{
		if(!BindsEmpty)
		{
			if(g_Config.m_ClResetQuickActionMouse)
				m_SelectorMouse = vec2(0.0f, 0.0f);
			if(m_WasActive && m_SelectedBind != -1)
				ExecuteBind(m_SelectedBind);
		}
		m_WasActive = false;
		m_QuickActionId = -1;
		return;
	}
	m_WasActive = true;

	if(m_QuickActionId < 0 || m_QuickActionId >= MAX_CLIENTS)
		return;

	CGameClient::CClientData &Target = GameClient()->m_aClients[m_QuickActionId];
	CNetObj_Character TargetRender = GameClient()->m_aClients[m_QuickActionId].m_RenderCur;

	if(length(m_SelectorMouse) > 170.0f)
		m_SelectorMouse = normalize(m_SelectorMouse) * 170.0f;

	int SegmentCount = m_vBinds.size();
	float SegmentAngle = 2.0f * pi / SegmentCount;

	float SelectedAngle = angle(m_SelectorMouse) + SegmentAngle / 2.0f;
	if(SelectedAngle < 0.0f)
		SelectedAngle += 2.0f * pi;
	if(length(m_SelectorMouse) > 110.0f)
		m_SelectedBind = (int)(SelectedAngle / (2.0f * pi) * SegmentCount);
	else
		m_SelectedBind = -1;

	CUIRect Screen = *Ui()->Screen();

	Ui()->MapScreen();

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, 190.0f, 64);
	Graphics()->QuadsEnd();
	Graphics()->WrapClamp();

	const float Theta = pi * 2.0f / m_vBinds.size();
	for(int i = 0; i < static_cast<int>(m_vBinds.size()); i++)
	{
		const CBind &Bind = m_vBinds[i];
		const float Angle = Theta * i;
		vec2 Pos = direction(Angle);
		Pos *= 140.0f;
		const float FontSize = (i == m_SelectedBind) ? 20.0f : 12.0f;
		float Width = TextRender()->TextWidth(FontSize, Bind.m_aName);
		TextRender()->Text(Screen.w / 2.0f + Pos.x - Width / 2.0f, Screen.h / 2.0f + Pos.y - FontSize / 2.0f, FontSize, Bind.m_aName);
	}

	Graphics()->WrapNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.3f);
	DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, 100.0f, 64);
	Graphics()->QuadsEnd();

	if(BindsEmpty)
	{
		float Size = 20.0f;
		TextRender()->Text(Screen.w / 2.0f - TextRender()->TextWidth(Size, "Empty") / 2.0f, Screen.h / 2.0f - Size / 2, Size, "Empty");
	}
	else
	{
		CNetObj_DDNetCharacter *pExtendedData = &GameClient()->m_Snap.m_aCharacters[m_QuickActionId].m_ExtendedData;
		vec2 Direction = vec2(pExtendedData->m_TargetX, pExtendedData->m_TargetY);
		vec2 Middle = vec2(Screen.w / 2.0f, Screen.h / 2.0f);
		CAnimState State;
		State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);

		// bool Sitting = Target.m_Afk || Target.m_Paused;
		// if(Sitting)
		//	State.Add(Direction.x < 0 ? &g_pData->m_aAnimations[ANIM_SIT_LEFT] : &g_pData->m_aAnimations[ANIM_SIT_RIGHT], 0, 1.0f);
		
		RenderTools()->RenderTee(&State, &Target.m_RenderInfo, TargetRender.m_Emote, normalize(Direction), Middle, 1.0f);
	}

	RenderTools()->RenderCursor(m_SelectorMouse + vec2(Screen.w, Screen.h) / 2.0f, 24.0f);
}

void CQuickActions::ExecuteBind(int Bind)
{
	char aCmd[(int)BINDWHEEL_MAX_CMD + (int)MAX_NAME_LENGTH] = "";

	if(m_QuickActionId < 0 || m_QuickActionId >= MAX_CLIENTS)
		return;
	char pTargetName[32];

	str_copy(pTargetName, GameClient()->m_aClients[m_QuickActionId].m_aName);

	str_format(aCmd, sizeof(aCmd), m_vBinds[Bind].m_aCommand, pTargetName);
	Console()->ExecuteLine(aCmd);
}

void CQuickActions::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CQuickActions *pThis = (CQuickActions *)pUserData;

	for(CBind &Bind : pThis->m_vBinds)
	{
		char aBuf[BINDWHEEL_MAX_CMD * 2] = "";
		char *pEnd = aBuf + sizeof(aBuf);
		char *pDst;
		str_append(aBuf, "add_quickaction \"");
		// Escape name
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aName, pEnd);
		str_append(aBuf, "\" \"");
		// Escape command
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, Bind.m_aCommand, pEnd);
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf);
	}
}

void CQuickActions::DrawDebugLines()
{
	vec2 TargetPos = GetCursorWorldPos();
	int Id = GetClosetClientId(TargetPos);
	if(Id < 0 || Id >= MAX_CLIENTS)
		return;

	CGameClient::CClientData &Target = GameClient()->m_aClients[Id];

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	const IGraphics::CLineItem Test(Target.m_RenderPos.x, Target.m_RenderPos.y, TargetPos.x, TargetPos.y);

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->LinesDraw(&Test, 1);
	Graphics()->LinesEnd();

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}
