#include "tolower_data.h"

int str_utf8_tolower_codepoint(int code)
{
	auto It = UPPER_TO_LOWER_CODEPOINT_MAP.find(code);
	return It == UPPER_TO_LOWER_CODEPOINT_MAP.end() ? code : It->second;
}
