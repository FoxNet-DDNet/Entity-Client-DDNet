#ifndef GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#define GAME_CLIENT_COMPONENTS_FREEZEBARS_H

#include <base/color.h>

#include <game/client/component.h>

class CFreezeBars : public CComponent
{
	bool RenderKillBar();
	void RenderFreezeBar(int ClientId);
	void RenderFreezeBarPos(float x, float y, float Width, float Height, float Progress, ColorRGBA Color);
	bool IsPlayerInfoAvailable(int ClientId) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
