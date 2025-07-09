// I have no clue why I thought that this is needed but here we are

#include <game/generated/protocol.h>

enum
{
	EFFECT_OFF = 0,
	EFFECT_SPARKLE,
	EFFECT_FIRETRAIL,
	EFFECT_SWITCH,

	SILENT_MSG = TEAM_ALL - 2,
	ECLIENT_MSG,
};