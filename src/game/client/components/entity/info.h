#ifndef GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#define GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#include <engine/http.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <string>

typedef struct _json_value json_value;

class CEntityInfo : public CComponent
{
public:
	std::shared_ptr<IHttpRequest> m_pEClientInfoTask = nullptr;
	void FetchEClientInfo();
	void FinishEClientInfo();
	void ResetEClientInfoTask();

	bool m_Retried = false;

	char m_aVersionStr[10] = "0";
	std::string m_News;
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnInit() override;

private:
	void UpdateNews(const json_value &Json);
};

#endif
