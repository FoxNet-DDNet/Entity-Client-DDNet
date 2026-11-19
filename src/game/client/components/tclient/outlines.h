#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_OUTLINES_H

#include <game/client/component.h>

#include <cstdint>

class CTile;
class CTeleTile;

class COutlines : public CComponent
{
private:
	ivec2 m_MapDataSize;
	// One entry per tile: the outline type in the low bits and, above them, which of the eight
	// surrounding tiles outline at least as strongly. The neighbourhood only depends on the map,
	// so it is resolved once here instead of being looked up again for every visible tile of
	// every frame.
	uint16_t *m_pTileInfo = nullptr;

	static constexpr int TYPE_BITS = 3;
	static constexpr uint16_t TYPE_MASK = (1 << TYPE_BITS) - 1;
	static constexpr int NEIGHBOR_SHIFT = 8;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnMapLoad() override;
	void OnRender() override;
	~COutlines() override { delete[] m_pTileInfo; }
};

#endif
