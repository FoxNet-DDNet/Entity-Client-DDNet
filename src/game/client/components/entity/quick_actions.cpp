#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "quick_actions.h"
#include <engine/shared/protocol.h>
#include <base/vmath.h>
#include <generated/client_data.h>
#include <generated/protocol.h>
#include <engine/textrender.h>
#include <array>

CQuickActions::CQuickActions()
{
	OnReset();
}

vec2 CQuickActions::GetCursorWorldPos() const
{
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
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

		vec2 PlayerPos = GameClient()->m_aClients[ClientId].m_RenderPos;
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
		if(pThis->GameClient()->m_Emoticon.IsActive() || pThis->GameClient()->m_Bindwheel.IsActive())
		{
			pThis->m_Active = false;
		}
		else
		{
			const vec2 Pos = pThis->GetCursorWorldPos();
			pThis->m_QuickActionId = pThis->GetClosetClientId(Pos);
			if(pThis->m_QuickActionId < 0 || pThis->m_QuickActionId >= MAX_CLIENTS)
				pThis->m_QuickActionId = -1;

			pThis->m_Active = pResult->GetInteger(0) != 0;
		}
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
	AddBind("Add Clanwar", "war_clan_index 1 \"%s\"");
	AddBind("Add Clanteam", "war_clan_index 2 \"%s\"");
	AddBind("Remove Entry", "remove_entry_name \"%s\" ");
	AddBind("Player Info", "playerinfo \"%s\"");
	AddBind("Message", "set_input %s: ");
	AddBind("Whisper", "set_input /w \"%s\" ");
	AddBind("Mute", "addmute \"%s\"");
}

void CQuickActions::OnInit()
{
	if(m_vBinds.empty())
		AddDefaultBinds();
}

void CQuickActions::OnStateChange(int NewState, int OldState)
{
	if(OldState == NewState)
	{
		m_Active = false;
		m_WasActive = false;
		m_SelectedBind = -1;
		m_QuickActionId = -1;
	}
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
	GameClient()->m_Emoticon.m_SelectorMouse += vec2(x, y);
	return true;
}

bool CQuickActions::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CQuickActions::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// DrawDebugLines();

	if(m_QuickActionId < 0 || m_QuickActionId >= MAX_CLIENTS)
		return;

	static const auto QuadEaseInOut = [](float t) -> float {
		if(t == 0.0f)
			return 0.0f;
		if(t == 1.0f)
			return 1.0f;
		return (t < 0.5f) ? (2.0f * t * t) : (1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f);
	};
	static const auto PositiveMod = [](float x, float y) -> float {
		return std::fmod(x + y, y);
	};

	const bool BindsEmpty = m_vBinds.empty();

	static const float s_InnerOuterMouseBoundaryRadius = 110.0f;
	static const float s_OuterMouseLimitRadius = 170.0f;
	static const float s_OuterItemRadius = 140.0f; // 10.0f less than emoticons for extra text space
	static const float s_OuterCircleRadius = 190.0f;
	static const float s_InnerCircleRadius = 100.0f;
	static const float s_FontSize = 12.0f;
	static const float s_FontSizeSelected = 18.0f;

	const float AnimationTime = (float)g_Config.m_ClAnimateWheelTime / 1000.0f;
	const float ItemAnimationTime = AnimationTime / 2.0f;

	if(AnimationTime != 0.0f)
	{
		for(float &Time : m_aAnimationTimeItems)
			Time = std::max(0.0f, Time - Client()->RenderFrameTime());
	}

	if(!m_Active)
	{
		if(m_WasActive)
		{
			if(!BindsEmpty)
			{
				if(g_Config.m_ClResetQuickActionMouse)
					GameClient()->m_Emoticon.m_SelectorMouse = vec2(0.0f, 0.0f);
				if(m_SelectedBind != -1)
					ExecuteBind(m_SelectedBind);
			}
		}
		m_WasActive = false;

		if(AnimationTime == 0.0f)
			return;

		m_AnimationTime -= Client()->RenderFrameTime() * 3.0f; // Close animation 3x faster
		if(m_AnimationTime <= 0.0f)
		{
			m_AnimationTime = 0.0f;
			return;
		}
	}
	else
	{
		m_AnimationTime += Client()->RenderFrameTime();
		if(m_AnimationTime > AnimationTime)
			m_AnimationTime = AnimationTime;
		m_WasActive = true;
	}

	const CUIRect Screen = *Ui()->Screen();

	const bool WasTouchPressed = GameClient()->m_Emoticon.m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(GameClient()->m_Emoticon.m_TouchState);
	if(GameClient()->m_Emoticon.m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (GameClient()->m_Emoticon.m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
		const float TouchCenterDistance = length(TouchPos);
		if(TouchCenterDistance <= s_OuterMouseLimitRadius)
		{
			GameClient()->m_Emoticon.m_SelectorMouse = TouchPos;
		}
		else if(TouchCenterDistance > s_OuterCircleRadius)
		{
			GameClient()->m_Emoticon.m_TouchPressedOutside = true;
		}
	}
	else if(WasTouchPressed)
	{
		m_Active = false;
	}

	std::array<float, 3> aAnimationPhase;
	if(AnimationTime == 0.0f)
	{
		aAnimationPhase.fill(1.0f);
	}
	else
	{
		aAnimationPhase[0] = QuadEaseInOut(m_AnimationTime / AnimationTime);
		aAnimationPhase[1] = aAnimationPhase[0] * aAnimationPhase[0];
		aAnimationPhase[2] = aAnimationPhase[1] * aAnimationPhase[1];
	}

	if(length(GameClient()->m_Emoticon.m_SelectorMouse) > s_OuterMouseLimitRadius)
		GameClient()->m_Emoticon.m_SelectorMouse = normalize(GameClient()->m_Emoticon.m_SelectorMouse) * s_OuterMouseLimitRadius;

	int SegmentCount = m_vBinds.size();
	if(SegmentCount == 0)
	{
		m_SelectedBind = -1;
	}
	else
	{
		const float SelectedAngle = angle(GameClient()->m_Emoticon.m_SelectorMouse);
		if(length(GameClient()->m_Emoticon.m_SelectorMouse) > s_InnerOuterMouseBoundaryRadius)
			m_SelectedBind = PositiveMod(std::round(SelectedAngle / (2.0f * pi) * SegmentCount), SegmentCount);
		else
			m_SelectedBind = -1;
	}

	if(m_SelectedBind != -1)
	{
		m_aAnimationTimeItems[m_SelectedBind] += Client()->RenderFrameTime() * 2.0f; // To counteract earlier decrement
		if(m_aAnimationTimeItems[m_SelectedBind] >= ItemAnimationTime)
			m_aAnimationTimeItems[m_SelectedBind] = ItemAnimationTime;
	}

	Ui()->MapScreen();

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f * aAnimationPhase[0]);
	Graphics()->DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, s_OuterCircleRadius * aAnimationPhase[0], 64);
	Graphics()->QuadsEnd();

	Graphics()->WrapClamp();

	if(BindsEmpty)
	{
		float Size = 20.0f;
		TextRender()->Text(Screen.w / 2.0f - TextRender()->TextWidth(Size, "Empty") / 2.0f, Screen.h / 2.0f - Size / 2, Size, "Empty");
	}

	const float Theta = pi * 2.0f / std::max<float>(1.0f, m_vBinds.size()); // Prevent divide by 0
	for(int i = 0; i < static_cast<int>(m_vBinds.size()); i++)
	{
		const CBind &Bind = m_vBinds[i];
		const float Angle = Theta * i;
		const float Phase = ItemAnimationTime == 0.0f ? (i == m_SelectedBind ? 1.0f : 0.0f) : QuadEaseInOut(m_aAnimationTimeItems[i] / ItemAnimationTime);
		const float FontSize = (s_FontSize + Phase * (s_FontSizeSelected - s_FontSize)) * aAnimationPhase[1];
		const char *pName = Bind.m_aName;
		if(pName[0] == '\0')
		{
			pName = "Empty";
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, aAnimationPhase[1]);
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, aAnimationPhase[1]);

		}
		else
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, aAnimationPhase[1]);
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, aAnimationPhase[1]);
		}
		const float Width = TextRender()->TextWidth(FontSize, pName);
		const vec2 Pos = direction(Angle) * s_OuterItemRadius * aAnimationPhase[1];
		TextRender()->Text(Screen.w / 2.0f + Pos.x - Width / 2.0f, Screen.h / 2.0f + Pos.y - FontSize / 2.0f, FontSize, pName);
	}
	Graphics()->WrapNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.10f * aAnimationPhase[2]);
	Graphics()->DrawCircle(Screen.w / 2.0f, Screen.h / 2.0f, s_InnerCircleRadius * aAnimationPhase[2], 64);
	Graphics()->QuadsEnd();

	const float FontSize = 20.0f * aAnimationPhase[2];
	if(BindsEmpty)
	{
		TextRender()->Text(Screen.w / 2.0f - TextRender()->TextWidth(FontSize, "Empty") / 2.0f, Screen.h / 2.0f - FontSize / 2, FontSize, "Empty");
	}
	else if(m_QuickActionId >= 0 && m_QuickActionId < MAX_CLIENTS)
	{
		const CGameClient::CClientData Target = GameClient()->m_aClients[m_QuickActionId];
		const CNetObj_Character TargetRender = GameClient()->m_aClients[m_QuickActionId].m_RenderCur;

		const CNetObj_DDNetCharacter *pExtendedData = &GameClient()->m_Snap.m_aCharacters[m_QuickActionId].m_ExtendedData;
		const vec2 Direction = vec2(pExtendedData->m_TargetX, pExtendedData->m_TargetY);
		const vec2 Middle = vec2(Screen.w / 2.0f, Screen.h / 2.0f) + vec2(0.0f, 10.0f) * aAnimationPhase[2];
		CAnimState State;
		State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);

		// bool Sitting = Target.m_Afk || Target.m_Paused;
		// if(Sitting)
		// State.Add(Direction.x < 0 ? &g_pData->m_aAnimations[ANIM_SIT_LEFT] : &g_pData->m_aAnimations[ANIM_SIT_RIGHT], 0, 1.0f);
		CTeeRenderInfo SkinInfo = Target.m_RenderInfo;
		SkinInfo.m_Size *= aAnimationPhase[2];

		RenderTools()->RenderTee(&State, &SkinInfo, TargetRender.m_Emote, normalize(Direction), Middle, aAnimationPhase[2]);

		const char *pName = Target.m_aName;
		const float LineWidth = 175.0f * aAnimationPhase[2];
		const float NameWidth = std::min(TextRender()->TextWidth(FontSize, pName), LineWidth);

		CTextCursor Cursor;
		const float NameOffset = 50.0f * aAnimationPhase[2];
		const vec2 NamePos = vec2(Screen.x, Screen.y) + vec2(Screen.w - NameWidth, Screen.h) / 2.0f - vec2(0.0f, NameOffset);
		Cursor.SetPosition(NamePos);
		Cursor.m_FontSize = FontSize;
		Cursor.m_Flags |= TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.m_LineWidth = LineWidth;

		TextRender()->TextEx(&Cursor, pName);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

	RenderTools()->RenderCursor(GameClient()->m_Emoticon.m_SelectorMouse + vec2(Screen.w, Screen.h) / 2.0f, 24.0f, aAnimationPhase[0]);
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
