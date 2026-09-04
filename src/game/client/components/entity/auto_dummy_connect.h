#ifndef GAME_CLIENT_COMPONENTS_ENTITY_AUTO_DUMMY_CONNECT_H
#define GAME_CLIENT_COMPONENTS_ENTITY_AUTO_DUMMY_CONNECT_H

#include <game/client/component.h>

#include <cstdint>

class CAutoDummyConnect : public CComponent
{
	bool m_WarnedNotAllowed = false;

	int64_t m_NextAttempt = 0;

	void AdoptDummyLocalId();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_AUTO_DUMMY_CONNECT_H
