#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_WEBHOOK_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_WEBHOOK_H

#include <engine/http.h>

#include <game/client/component.h>

#include <memory>
#include <string>
#include <vector>

class CWebhook : public CComponent
{
private:
	static const size_t BUFFER_MAX_SIZE = 10 * 8192;
	std::string m_Buffer;
	float m_TimeSinceLastRequest = 0.0f;
	float m_TimeSinceBufferStart = 0.0f;
	std::vector<std::shared_ptr<IHttpRequest>> m_vpHttpRequests;
	void FlushBuffer();

public:
	CWebhook();
	~CWebhook();

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnRender() override;

	void ConsoleLine(int Type, const char *pLine);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_MODERATION_WEBHOOK_H
