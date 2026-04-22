#include "chat_bubbles.h"

#include <base/color.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/teamscore.h>

#include <algorithm>
#include <cstdint>

constexpr float NameplateOffset = 10.0f;
constexpr float CharacterMinOffset = 40.0f;
constexpr float MarginBetween = 1.0f;
constexpr float LineWidth = 500.0f;

CChat *CChatBubbles::Chat() const
{
	return &GameClient()->m_Chat;
}

float CChatBubbles::GetOffset(int ClientId)
{
	float Offset = GameClient()->m_NamePlates.GetNamePlateOffset(ClientId) + NameplateOffset;
	if(Offset < CharacterMinOffset)
		Offset = CharacterMinOffset;

	return Offset;
}

void CChatBubbles::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(!g_Config.m_ClChatBubbles)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		AddBubble(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CChatBubbles::UpdateBubbleOffsets(int ClientId, float InputBubbleHeight)
{
	float Offset = 0.0f;
	if(InputBubbleHeight > 0.0f)
		Offset += InputBubbleHeight + MarginBetween;

	for(CBubble &Bubble : m_avChatBubbles[ClientId])
	{
		SetupTextcontainer(Bubble);
		STextBoundingBox BoundingBox = TextRender()->GetBoundingBoxTextContainer(Bubble.m_TextContainerIndex);
		Bubble.m_TargetOffsetY = Offset;
		Offset += BoundingBox.m_H + g_Config.m_ClChatBubbleSize + MarginBetween;
	}
}

void CChatBubbles::AddBubble(int ClientId, int Team, const char *pText)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !pText)
		return;

	if(Chat()->ChatDetection(ClientId, Team, pText))
		return;

	if(*pText == 0)
		return;
	if(GameClient()->m_aClients[ClientId].m_aName[0] == '\0')
		return;
	if(GameClient()->m_aClients[ClientId].m_ChatIgnore)
		return;
	if(GameClient()->m_Snap.m_LocalClientId != ClientId)
	{
		if(g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend)
			return;
		if(g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK)
			return;
		if(GameClient()->m_aClients[ClientId].m_Foe)
			return;
		if(GameClient()->m_WarList.m_WarPlayers[ClientId].m_IsMuted)
			return;
		else if(g_Config.m_ClWarList && g_Config.m_ClHideEnemyChat && GameClient()->m_WarList.GetWarData(ClientId).m_WarGroupMatches[1])
			return;
	}

	CTextCursor Cursor;

	// Create Text at default zoom
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y);

	SetupTextCursor(Cursor);

	CBubble Bubble(pText, Cursor, time_get());

	ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	if(Chat()->LineHighlighted(ClientId, pText))
		Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	else if(Team == 1)
		Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	else if(Team == TEAM_WHISPER_RECV)
		Color = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
	else if(Team == TEAM_WHISPER_SEND)
	{
		Color = ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
		ClientId = GameClient()->m_Snap.m_LocalClientId; // Set ClientId to local client for whisper send
	}
	else // regular message
		Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));

	TextRender()->TextColor(Color);

	TextRender()->ColorParsing(pText, &Cursor, Color, &Bubble.m_TextContainerIndex);

	m_avChatBubbles[ClientId].insert(m_avChatBubbles[ClientId].begin(), Bubble);

	UpdateBubbleOffsets(ClientId);
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

void CChatBubbles::RemoveBubble(int ClientId, const CBubble &Bubble)
{
	for(auto It = m_avChatBubbles[ClientId].begin(); It != m_avChatBubbles[ClientId].end(); ++It)
	{
		if(*It == Bubble)
		{
			TextRender()->DeleteTextContainer(It->m_TextContainerIndex);
			It->m_TextContainerIndex.Reset();
			m_avChatBubbles[ClientId].erase(It);
			UpdateBubbleOffsets(ClientId);
			return;
		}
	}
}

void CChatBubbles::RenderCurInput(float y)
{
	const int FontSize = g_Config.m_ClChatBubbleSize;
	const char *pText = Chat()->m_Input.GetString();
	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	vec2 Position = GameClient()->m_aClients[LocalId].m_RenderPos;

	CTextCursor Cursor;
	STextContainerIndex TextContainerIndex;

	// Create Text at default zoom
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y);

	SetupTextCursor(Cursor);

	TextRender()->CreateOrAppendTextContainer(TextContainerIndex, &Cursor, pText);

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);

	if(TextContainerIndex.Valid())
	{
		STextBoundingBox BoundingBox = TextRender()->GetBoundingBoxTextContainer(TextContainerIndex);

		Position.x -= BoundingBox.m_W / 2.0f + g_Config.m_ClChatBubbleSize / 15.0f;
		float InputBubbleHeight = BoundingBox.m_H + FontSize;

		float TargetY = y - InputBubbleHeight;

		Graphics()->DrawRect(Position.x - FontSize / 2.0f, TargetY - FontSize / 2.0f,
			BoundingBox.m_W + FontSize * 1.20f, BoundingBox.m_H + FontSize,
			ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, g_Config.m_ClChatBubbleSize / 4.5f);

		TextRender()->RenderTextContainer(TextContainerIndex, ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f), ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), Position.x, TargetY);

		UpdateBubbleOffsets(LocalId, InputBubbleHeight);
	}
	else
		UpdateBubbleOffsets(LocalId);
	TextRender()->DeleteTextContainer(TextContainerIndex);
}

void CChatBubbles::RenderChatBubbles(int ClientId)
{
	const float ShowTime = g_Config.m_ClChatBubbleShowTime / 100.0f;
	const int FontSize = g_Config.m_ClChatBubbleSize;
	vec2 Position = GameClient()->m_aClients[ClientId].m_RenderPos;
	float BaseY = Position.y - GetOffset(ClientId) - NameplateOffset;

	if(ClientId == GameClient()->m_Snap.m_LocalClientId)
		RenderCurInput(BaseY);

	for(CBubble &Bubble : m_avChatBubbles[ClientId])
	{
		float Alpha = 1.0f;
		if(GameClient()->IsOtherTeam(ClientId))
			Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;

		Alpha *= GetAlpha(Bubble.m_Time);

		if(Bubble.m_Time + time_freq() * ShowTime < time_get())
		{
			RemoveBubble(ClientId, Bubble);
			continue;
		}

		if(Alpha <= 0.01f)
			continue;

		//if(!g_Config.m_ClChatBubblePushOut)
		//	Bubble.m_TargetPushOffset = vec2(0.0f, 0.0f);

		Bubble.m_OffsetY += (Bubble.m_TargetOffsetY - Bubble.m_OffsetY) * 0.05f / 10.0f;

		Bubble.m_PushOffset.x += (Bubble.m_TargetPushOffset.x - Bubble.m_PushOffset.x) * 0.06f;
		Bubble.m_PushOffset.y += (Bubble.m_TargetPushOffset.y - Bubble.m_PushOffset.y) * 0.06f;

		SetupTextcontainer(Bubble);

		ColorRGBA BgColor(0.0f, 0.0f, 0.0f, 0.25f * Alpha);
		ColorRGBA TextColor(1, 1, 1, Alpha);
		ColorRGBA OutlineColor(0.0f, 0.0f, 0.0f, 0.5f * Alpha);

		if(Bubble.m_TextContainerIndex.Valid())
		{
			STextBoundingBox BoundingBox = TextRender()->GetBoundingBoxTextContainer(Bubble.m_TextContainerIndex);

			float x = Position.x - (BoundingBox.m_W / 2.0f + g_Config.m_ClChatBubbleSize / 15.0f);
			float y = BaseY - Bubble.m_OffsetY - BoundingBox.m_H - FontSize;

			x += Bubble.m_PushOffset.x;
			y += Bubble.m_PushOffset.y;

			Graphics()->DrawRect((x - FontSize / 2.0f), y - FontSize / 2.0f,
				BoundingBox.m_W + FontSize * 1.20f, BoundingBox.m_H + FontSize,
				BgColor, IGraphics::CORNER_ALL, g_Config.m_ClChatBubbleSize / 4.5f);

			TextRender()->RenderTextContainer(Bubble.m_TextContainerIndex, TextColor, OutlineColor, x, y);
		}
	}
}

void CChatBubbles::ShiftBubbles()
{
	// @qxdFox: ToDo
}

float CChatBubbles::GetAlpha(int64_t Time)
{
	const float FadeOutTime = g_Config.m_ClChatBubbleFadeOut / 100.0f;
	const float FadeInTime = g_Config.m_ClChatBubbleFadeIn / 100.0f;
	const float ShowTime = g_Config.m_ClChatBubbleShowTime / 100.0f;

	int64_t Now = time_get();
	float LineAge = (Now - Time) / (float)time_freq();

	// Fade in
	if(LineAge < FadeInTime)
		return std::clamp(LineAge / FadeInTime, 0.0f, 1.0f);

	float FadeOutProgress = (LineAge - (ShowTime - FadeOutTime)) / FadeOutTime;
	return std::clamp(1.0f - FadeOutProgress, 0.0f, 1.0f);
}

void CChatBubbles::OnRender()
{
	if(m_UseChatBubbles != g_Config.m_ClChatBubbles)
	{
		Reset();
		m_UseChatBubbles = g_Config.m_ClChatBubbles;
	}

	if(!g_Config.m_ClChatBubbles)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId])
			continue;
		const CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		if(!ClientData.m_Active || !ClientData.m_RenderInfo.Valid())
			continue;
		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
			continue;
		ShiftBubbles();
		RenderChatBubbles(ClientId);
	}
}

void CChatBubbles::Reset()
{
	for(auto &vChatBubbles : m_avChatBubbles)
	{
		for(auto &Bubble : vChatBubbles)
		{
			if(Bubble.m_TextContainerIndex.Valid())
			{
				TextRender()->DeleteTextContainer(Bubble.m_TextContainerIndex);
				Bubble.m_TextContainerIndex.Reset();
			}
			Bubble.m_Cursor.m_FontSize = 0;
		}
		vChatBubbles.clear();
	}
}

void CChatBubbles::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChatBubbles::OnWindowResize()
{
	Reset();
}

void CChatBubbles::SetupTextCursor(CTextCursor &Cursor)
{
	Cursor.SetPosition(vec2(0, 0));
	Cursor.m_FontSize = g_Config.m_ClChatBubbleSize;
	Cursor.m_Flags = TEXTFLAG_RENDER;
	Cursor.m_LineWidth = LineWidth - Cursor.m_FontSize * 2.0f;
}

void CChatBubbles::SetupTextcontainer(CBubble &Bubble)
{
	const int FontSize = g_Config.m_ClChatBubbleSize;

	if(!Bubble.m_TextContainerIndex.Valid() || Bubble.m_Cursor.m_FontSize != FontSize)
	{
		if(Bubble.m_TextContainerIndex.Valid())
		{
			TextRender()->DeleteTextContainer(Bubble.m_TextContainerIndex);
			Bubble.m_TextContainerIndex.Reset();
		}

		CTextCursor Cursor;
		SetupTextCursor(Cursor);

		TextRender()->CreateOrAppendTextContainer(Bubble.m_TextContainerIndex, &Cursor, Bubble.m_aText);
		Bubble.m_Cursor.m_FontSize = FontSize;

		TextRender()->ColorParsing(Bubble.m_aText, &Cursor, ColorRGBA(1.0f, 1.0f, 1.0f), &Bubble.m_TextContainerIndex);
	}
}
