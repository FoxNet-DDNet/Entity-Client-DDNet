#ifndef GAME_CLIENT_COMPONENTS_ENTITY_CHATBUBBLES_H
#define GAME_CLIENT_COMPONENTS_ENTITY_CHATBUBBLES_H
#include <game/client/component.h>
#include <game/client/components/chat.h>

class CChatBubbles : public CComponent
{
	CChat *Chat() const;

	float GetOffset(int ClientId);

	char m_aText[CChat::MAX_LINE_LENGTH] = {0};
	char m_aFontSize;
	void RenderOwn();

	void Update(CTextCursor &pCursor, float FontSize);

	STextContainerIndex m_TextContainerIndex;
	int m_QuadContainerIndex;

public:

	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
};

#endif