#ifndef ENGINE_SHARED_NIGHT_SHIFT_H
#define ENGINE_SHARED_NIGHT_SHIFT_H

#include <base/color.h>

enum
{
	NIGHT_SHIFT_SCHEDULE_ALWAYS = 0,
	NIGHT_SHIFT_SCHEDULE_CUSTOM,
	NIGHT_SHIFT_SCHEDULE_SUN,
	NUM_NIGHT_SHIFT_SCHEDULES,
};

// Warms the colors of the whole screen after dark, like the night shift filter on phones.
// The client multiplies the finished frame with the tint from this class, so everything
// the frame contains is warmed at once instead of every drawing site having to opt in.
class CNightShift
{
public:
	enum ESunState
	{
		SUN_NORMAL = 0,
		SUN_ALWAYS_UP, // polar day, the sun does not set on this date
		SUN_ALWAYS_DOWN, // polar night, the sun does not rise on this date
		SUN_UNAVAILABLE, // the local date could not be determined
	};

	// How far the shift has faded in right now, 0.0f when it is off and 1.0f at full warmth.
	static float Strength();

	// The color to multiply the frame with, white while the shift is inactive.
	static ColorRGBA Tint();

	// The tint the shift would use at the given strength, for previews in the settings.
	static ColorRGBA TintForStrength(float Strength);

	// Sunrise and sunset for the configured coordinates today, as minutes since local midnight.
	static ESunState SunTimes(int *pSunriseMinute, int *pSunsetMinute);

	// The current local time as minutes since midnight, or -1 when it is not available.
	static int LocalMinuteOfDay();
};

#endif // ENGINE_SHARED_NIGHT_SHIFT_H
