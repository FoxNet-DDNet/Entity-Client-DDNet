#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/gameclient.h>

#include <base/system.h>
#include <base/vmath.h>

#include "chat_bubbles.h"

CChat *CChatBubbles::Chat() const
{
	return &GameClient()->m_Chat;
}

float CChatBubbles::GetOffset(int ClientId)
{
	return GameClient()->m_NamePlates.GetNamePlateOffset(ClientId);
}

void CChatBubbles::Update(CTextCursor &pCursor, float FontSize)
{
	const char *pText = Chat()->m_Input.GetString();

	if(!str_comp(m_aText, pText) && m_aFontSize != FontSize)
		return;

	str_copy(m_aText, pText, sizeof(m_aText));
	m_aFontSize = FontSize;

	TextRender()->DeleteTextContainer(m_TextContainerIndex);
	m_TextContainerIndex.Reset();

	TextRender()->SetCursor(&pCursor, 0, 0, FontSize, TEXTFLAG_RENDER);
	pCursor.m_LineWidth = 500.0f - FontSize * 2.0f;
	TextRender()->CreateOrAppendTextContainer(m_TextContainerIndex, &pCursor, m_aText);
}

void CChatBubbles::RenderOwn()
{
	float ExtraPadding = 2.0f;
	float FontSize = 24.0f;

	int LocalId = GameClient()->m_Snap.m_LocalClientId;
	vec2 Position = GameClient()->m_aClients[LocalId].m_RenderPos;
	Position.y -= GetOffset(LocalId) + m_aFontSize;

	CTextCursor pCursor;
	Update(pCursor, FontSize);

	if(!m_TextContainerIndex.Valid())
		return;

	STextBoundingBox BoundingBox = TextRender()->GetBoundingBoxTextContainer(m_TextContainerIndex);

	Position.x -= BoundingBox.m_W / 2.0f - ExtraPadding;
	Position.y -= BoundingBox.m_H;

	Graphics()->DrawRect(Position.x - 5.0f, Position.y - 5.0f, BoundingBox.m_W + 15.0f - ExtraPadding, BoundingBox.m_H + 10.0f, ColorRGBA(0, 0, 0, 0.25f), IGraphics::CORNER_ALL, 6.0f);

	TextRender()->RenderTextContainer(m_TextContainerIndex, ColorRGBA(1, 1, 1, 1), ColorRGBA(0, 0, 0, 0.5f), Position.x, Position.y);
}

void CChatBubbles::OnRender()
{
	if(Client()->State() != Client()->STATE_ONLINE) // Gotta be online
		return;

	if(!GameClient()->m_Snap.m_pLocalCharacter)
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	RenderOwn();

	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}