#ifndef GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#define GAME_CLIENT_COMPONENTS_ENTITY_INFO_H
#include <cstdint>
#include <engine/shared/http.h>
#include <game/client/component.h>
#include <memory>

class CEntityInfo : public CComponent
{
public:
	std::shared_ptr<CHttpRequest> m_pEClientInfoTask = nullptr;
	void FetchEClientInfo();
	void FinishEClientInfo();
	void ResetEClientInfoTask();

	char m_aVersionStr[10] = "0";
	char m_aNews[3000] = "";
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnInit() override;
};

#endif
