#ifndef GAME_CLIENT_COMPONENTS_ENTITY_CHAT_BUBBLES_H
#define GAME_CLIENT_COMPONENTS_ENTITY_CHAT_BUBBLES_H

#include <base/str.h>
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/components/chat.h>

#include <cstdint>
#include <utility>
#include <vector>

class CBubble
{
public:
	char m_aText[MAX_LINE_LENGTH] = "";
	int64_t m_Time = 0;

	STextContainerIndex m_TextContainerIndex;
	CTextCursor m_Cursor;
	vec2 m_RenderPos = vec2(0, 0);
	float m_OffsetY = 0.0f;
	float m_TargetOffsetY = 0.0f;

	vec2 m_PushOffset = vec2(0, 0);
	vec2 m_TargetPushOffset = vec2(0, 0);

	CBubble(const char *pText, CTextCursor pCursor, int64_t pTime)
	{
		str_copy(m_aText, pText, sizeof(m_aText));
		m_Cursor = std::move(pCursor);
		m_Time = pTime;
		m_OffsetY = 0.0f;
		m_TargetOffsetY = 0.0f;
	}

	bool operator==(const CBubble &Other) const
	{
		bool MatchText = str_comp(m_aText, Other.m_aText) == 0 && str_comp(m_aText, "") != 0;
		bool MatchTime = m_Time == Other.m_Time && m_Time > 0;
		return MatchText && MatchTime;
	}
};

class CChatBubbles : public CComponent
{
	CChat *Chat() const;

	std::vector<CBubble> m_avChatBubbles[MAX_CLIENTS];

	void RenderCurInput(float y);
	void RenderChatBubbles(int ClientId);

	float GetOffset(int ClientId);
	float GetAlpha(int64_t Time);

	void UpdateBubbleOffsets(int ClientId, float InputBubbleHeight = 0.0f);

	void AddBubble(int ClientId, int Team, const char *pText);
	void RemoveBubble(int ClientId, const CBubble &Bubble);

	void ShiftBubbles();

	int m_UseChatBubbles = 0;

	void Reset();

	void SetupTextCursor(CTextCursor &Cursor);
	void SetupTextcontainer(CBubble &Bubble);

public:
	void OnMessage(int MsgType, void *pRawMsg) override;
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnStateChange(int NewState, int OldState) override;

	void OnWindowResize() override; // so it resets when font is changed
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_CHAT_BUBBLES_H
