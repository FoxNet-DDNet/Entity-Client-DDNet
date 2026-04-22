#ifndef GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#define GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#include <engine/shared/http.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>

class CEntityInfo : public CComponent
{
public:
	std::shared_ptr<CHttpRequest> m_pEClientInfoTask = nullptr;
	void FetchEClientInfo();
	void FinishEClientInfo();
	void ResetEClientInfoTask();

	bool m_Retried = false;

	char m_aVersionStr[10] = "0";
	char m_aNews[5000] = "";
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnInit() override;
};

#endif
