#include "night_shift.h"

#include "config.h"

#include <base/system.h>

#include <algorithm>
#include <cmath>
#include <ctime>

namespace
{

	constexpr double MINUTES_PER_DAY = 24.0 * 60.0;
	// Temperature that leaves the picture alone, matching the white point of a normal display.
	constexpr float NEUTRAL_TEMPERATURE = 6500.0f;
	// The sun counts as risen once its center is this many degrees above the horizon,
	// which is where it appears to be on the horizon after atmospheric refraction.
	constexpr double SUN_ALTITUDE = -0.833;
	constexpr double EARTH_OBLIQUITY = 23.4397;
	constexpr double PI = 3.14159265358979323846;

	double ToRadians(double Degrees)
	{
		return Degrees * PI / 180.0;
	}

	double ToDegrees(double Radians)
	{
		return Radians * 180.0 / PI;
	}

	bool LocalTime(std::tm *pTm)
	{
		const std::time_t Now = std::time(nullptr);
#if defined(CONF_FAMILY_WINDOWS)
		return localtime_s(pTm, &Now) == 0;
#else
		return localtime_r(&Now, pTm) != nullptr;
#endif
	}

	// How far local time is ahead of UTC in minutes, daylight saving included.
	int UtcOffsetMinutes()
	{
		const std::time_t Now = std::time(nullptr);
		std::tm Utc{};
#if defined(CONF_FAMILY_WINDOWS)
		if(gmtime_s(&Utc, &Now) != 0)
			return 0;
#else
		if(gmtime_r(&Now, &Utc) == nullptr)
			return 0;
#endif
		// Reading the UTC wall clock back as if it were a local one tells us how far the two are apart.
		Utc.tm_isdst = -1;
		const std::time_t AsLocal = std::mktime(&Utc);
		if(AsLocal == (std::time_t)-1)
			return 0;
		return (int)std::lround(std::difftime(Now, AsLocal) / 60.0);
	}

	// Julian day number of a Gregorian date, which as a Julian date is noon UTC on that day.
	double JulianDayNumber(int Year, int Month, int Day)
	{
		const int A = (14 - Month) / 12;
		const int Y = Year + 4800 - A;
		const int M = Month + 12 * A - 3;
		return Day + (153 * M + 2) / 5 + 365 * Y + Y / 4 - Y / 100 + Y / 400 - 32045;
	}

	// Fraction of the UTC day a Julian date falls on, shifted into local minutes since midnight.
	int JulianDateToLocalMinute(double JulianDate, int UtcOffset)
	{
		double DayFraction = JulianDate + 0.5;
		DayFraction -= std::floor(DayFraction);
		double Minutes = std::fmod(DayFraction * MINUTES_PER_DAY + UtcOffset, MINUTES_PER_DAY);
		if(Minutes < 0.0)
			Minutes += MINUTES_PER_DAY;
		return (int)std::lround(Minutes) % (int)MINUTES_PER_DAY;
	}

	// Approximate white point of a black body at the given temperature, normalized to 0..1.
	// Based on the widely used piecewise fit of the Planckian locus.
	ColorRGBA BlackBodyWhitePoint(float Kelvin)
	{
		const double T = std::clamp(Kelvin, 1000.0f, 40000.0f) / 100.0f;

		double Red;
		if(T <= 66.0)
			Red = 255.0;
		else
			Red = 329.698727446 * std::pow(T - 60.0, -0.1332047592);

		double Green;
		if(T <= 66.0)
			Green = 99.4708025861 * std::log(T) - 161.1195681661;
		else
			Green = 288.1221695283 * std::pow(T - 60.0, -0.0755148492);

		double Blue;
		if(T >= 66.0)
			Blue = 255.0;
		else if(T <= 19.0)
			Blue = 0.0;
		else
			Blue = 138.5177312231 * std::log(T - 10.0) - 305.0447927307;

		return ColorRGBA(
			std::clamp((float)Red / 255.0f, 0.0f, 1.0f),
			std::clamp((float)Green / 255.0f, 0.0f, 1.0f),
			std::clamp((float)Blue / 255.0f, 0.0f, 1.0f),
			1.0f);
	}

	// Fades the shift in over the first and out over the last minutes of a window that may
	// wrap around midnight. Returns how much of the shift applies at the given time.
	float WindowStrength(double Now, int Start, int End, int Transition)
	{
		const int Day = (int)MINUTES_PER_DAY;
		const int Duration = ((End - Start) % Day + Day) % Day;
		if(Duration == 0)
			return 0.0f;

		double Since = std::fmod(Now - Start, MINUTES_PER_DAY);
		if(Since < 0.0)
			Since += MINUTES_PER_DAY;
		if(Since >= Duration)
			return 0.0f;

		if(Transition <= 0)
			return 1.0f;

		const double FadeIn = Since / Transition;
		const double FadeOut = (Duration - Since) / Transition;
		const float Linear = (float)std::clamp(std::min(FadeIn, FadeOut), 0.0, 1.0);
		return Linear * Linear * (3.0f - 2.0f * Linear);
	}

	// The local time of day including seconds, so that the fades run smoothly instead of
	// stepping once a minute. Negative when the local time could not be determined.
	double LocalTimeOfDay()
	{
		std::tm Local{};
		if(!LocalTime(&Local))
			return -1.0;
		return Local.tm_hour * 60.0 + Local.tm_min + Local.tm_sec / 60.0;
	}

	float ComputeStrength()
	{
		if(!g_Config.m_ClNightShift)
			return 0.0f;

		if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_ALWAYS)
			return 1.0f;

		const double Now = LocalTimeOfDay();
		if(Now < 0.0)
			return 0.0f;

		int Start = g_Config.m_ClNightShiftFrom;
		int End = g_Config.m_ClNightShiftTo;
		if(g_Config.m_ClNightShiftSchedule == NIGHT_SHIFT_SCHEDULE_SUN)
		{
			int Sunrise, Sunset;
			switch(CNightShift::SunTimes(&Sunrise, &Sunset))
			{
			case CNightShift::SUN_NORMAL:
				Start = Sunset;
				End = Sunrise;
				break;
			case CNightShift::SUN_ALWAYS_DOWN:
				return 1.0f;
			case CNightShift::SUN_ALWAYS_UP:
			case CNightShift::SUN_UNAVAILABLE:
				return 0.0f;
			}
		}

		return WindowStrength(Now, Start, End, g_Config.m_ClNightShiftTransition);
	}

} // namespace

float CNightShift::Strength()
{
	if(!g_Config.m_ClNightShift)
		return 0.0f;

	// The strength only moves over minutes, so recomputing it every frame would waste
	// a time zone lookup and the sun math on every single swap.
	static int64_t s_NextUpdate = 0;
	static float s_Strength = 0.0f;
	const int64_t Now = time_get();
	if(Now >= s_NextUpdate)
	{
		s_NextUpdate = Now + time_freq() / 5;
		s_Strength = ComputeStrength();
	}
	return s_Strength;
}

ColorRGBA CNightShift::TintForStrength(float Strength)
{
	Strength = std::clamp(Strength, 0.0f, 1.0f);
	if(Strength <= 0.0f)
		return ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	// Dividing by the neutral white point keeps the brightest channel at full intensity, so
	// the shift only removes blue and green instead of dimming the whole picture.
	const ColorRGBA Target = BlackBodyWhitePoint(g_Config.m_ClNightShiftTemperature);
	const ColorRGBA Neutral = BlackBodyWhitePoint(NEUTRAL_TEMPERATURE);
	const float Red = std::clamp(Target.r / Neutral.r, 0.0f, 1.0f);
	const float Green = std::clamp(Target.g / Neutral.g, 0.0f, 1.0f);
	const float Blue = std::clamp(Target.b / Neutral.b, 0.0f, 1.0f);

	return ColorRGBA(
		1.0f + (Red - 1.0f) * Strength,
		1.0f + (Green - 1.0f) * Strength,
		1.0f + (Blue - 1.0f) * Strength,
		1.0f);
}

ColorRGBA CNightShift::Tint()
{
	return TintForStrength(Strength());
}

CNightShift::ESunState CNightShift::SunTimes(int *pSunriseMinute, int *pSunsetMinute)
{
	std::tm Local{};
	if(!LocalTime(&Local))
		return SUN_UNAVAILABLE;

	// The sun only moves by minutes a day, so the result is reused until the date or the
	// configured place changes.
	static int s_CachedDay = -1;
	static int s_CachedLatitude = 0;
	static int s_CachedLongitude = 0;
	static int s_CachedSunrise = 0;
	static int s_CachedSunset = 0;
	static ESunState s_CachedState = SUN_UNAVAILABLE;

	const int Day = (Local.tm_year + 1900) * 1000 + Local.tm_yday;
	if(Day == s_CachedDay &&
		g_Config.m_ClNightShiftLatitude == s_CachedLatitude &&
		g_Config.m_ClNightShiftLongitude == s_CachedLongitude)
	{
		if(pSunriseMinute)
			*pSunriseMinute = s_CachedSunrise;
		if(pSunsetMinute)
			*pSunsetMinute = s_CachedSunset;
		return s_CachedState;
	}

	const double Latitude = g_Config.m_ClNightShiftLatitude / 100.0;
	const double Longitude = g_Config.m_ClNightShiftLongitude / 100.0;

	// Sunrise equation, accurate to roughly a minute away from the poles.
	const double DaysSinceEpoch = JulianDayNumber(Local.tm_year + 1900, Local.tm_mon + 1, Local.tm_mday) - 2451545.0 + 0.0008;
	const double MeanSolarDay = DaysSinceEpoch - Longitude / 360.0;
	const double MeanAnomaly = std::fmod(357.5291 + 0.98560028 * MeanSolarDay, 360.0);
	const double MeanAnomalyRad = ToRadians(MeanAnomaly);
	const double Center = 1.9148 * std::sin(MeanAnomalyRad) + 0.02 * std::sin(2.0 * MeanAnomalyRad) + 0.0003 * std::sin(3.0 * MeanAnomalyRad);
	const double EclipticLongitude = std::fmod(MeanAnomaly + Center + 102.9372 + 180.0, 360.0);
	const double EclipticLongitudeRad = ToRadians(EclipticLongitude);
	const double SolarNoon = 2451545.0 + MeanSolarDay + 0.0053 * std::sin(MeanAnomalyRad) - 0.0069 * std::sin(2.0 * EclipticLongitudeRad);

	const double SinDeclination = std::sin(EclipticLongitudeRad) * std::sin(ToRadians(EARTH_OBLIQUITY));
	const double Declination = std::asin(SinDeclination);
	const double LatitudeRad = ToRadians(Latitude);
	const double CosHourAngle = (std::sin(ToRadians(SUN_ALTITUDE)) - std::sin(LatitudeRad) * SinDeclination) /
				    (std::cos(LatitudeRad) * std::cos(Declination));

	ESunState State = SUN_NORMAL;
	int Sunrise = 0;
	int Sunset = 0;
	if(CosHourAngle > 1.0)
	{
		State = SUN_ALWAYS_DOWN;
	}
	else if(CosHourAngle < -1.0)
	{
		State = SUN_ALWAYS_UP;
	}
	else
	{
		const int UtcOffset = UtcOffsetMinutes();
		const double HourAngle = ToDegrees(std::acos(CosHourAngle));
		Sunrise = JulianDateToLocalMinute(SolarNoon - HourAngle / 360.0, UtcOffset);
		Sunset = JulianDateToLocalMinute(SolarNoon + HourAngle / 360.0, UtcOffset);
	}

	s_CachedDay = Day;
	s_CachedLatitude = g_Config.m_ClNightShiftLatitude;
	s_CachedLongitude = g_Config.m_ClNightShiftLongitude;
	s_CachedSunrise = Sunrise;
	s_CachedSunset = Sunset;
	s_CachedState = State;

	if(pSunriseMinute)
		*pSunriseMinute = Sunrise;
	if(pSunsetMinute)
		*pSunsetMinute = Sunset;
	return State;
}

int CNightShift::LocalMinuteOfDay()
{
	const double Now = LocalTimeOfDay();
	if(Now < 0.0)
		return -1;
	return (int)Now;
}
