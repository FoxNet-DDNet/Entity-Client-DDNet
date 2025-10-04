#ifndef GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#define GAME_CLIENT_COMPONENTS_FREEZEBARS_H

#include <game/client/component.h>

#include <base/color.h>

class CFreezeBars : public CComponent
{
	bool RenderKillBar();
	void RenderFreezeBar(const int ClientId);
	void RenderFreezeBarPos(float x, float y, float Width, float Height, float Progress, ColorRGBA Color);
	bool IsPlayerInfoAvailable(int ClientId) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
