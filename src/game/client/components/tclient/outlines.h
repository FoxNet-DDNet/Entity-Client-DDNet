
#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H
#include <game/client/component.h>

class COutlines : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnMapLoad() override; 

private:
	CTile *m_pGameTiles = nullptr;
	CTeleTile *m_pTeleTiles = nullptr;
	int m_Width = 0, m_Height = 0;
};

#endif
