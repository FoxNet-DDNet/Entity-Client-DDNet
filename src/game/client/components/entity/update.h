#ifndef GAME_CLIENT_COMPONENTS_ENTITY_UPDATE_H
#define GAME_CLIENT_COMPONENTS_ENTITY_UPDATE_H
#include <game/client/component.h>
#include <engine/shared/http.h>
#include <memory>
#include <cstdint>

class CUpdate : public CComponent
{
public:
	std::shared_ptr<CHttpRequest> m_pAClientVerTask = nullptr;
	void FetchAClientInfo();
	void FinishAClientInfo();
	void ResetAClientInfoTask();

	char m_aVersionStr[10] = "0";
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif