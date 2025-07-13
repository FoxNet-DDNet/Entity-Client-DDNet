#ifndef GAME_CLIENT_COMPONENTS_ENTITY_UPDATE_H
#define GAME_CLIENT_COMPONENTS_ENTITY_UPDATE_H
#include <cstdint>
#include <engine/shared/http.h>
#include <game/client/component.h>
#include <memory>

class CUpdate : public CComponent
{
public:
	std::shared_ptr<CHttpRequest> m_pAClientVerTask = nullptr;
	void FetchEClientInfo();
	void FinishEClientInfo();
	void ResetEClientInfoTask();

	char m_aVersionStr[10] = "0";
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
