#include "system.h"

#include "str.h"

#include <cctype> // std::toupper
#include <cstdlib> // std::malloc, std::free

void str_to_uppercase(const char *src, char *dst, int dst_size)
{
	int i = 0;
	for(; src[i] && i < dst_size - 1; ++i)
		dst[i] = str_uppercase(src[i]);
	dst[i] = '\0';
}

const char *str_to_uppercase(const char *src)
{
	int len = str_length(src);
	char *dst = (char *)malloc(len + 1);
	if(!dst)
		return nullptr;
	for(int i = 0; i < len; ++i)
		dst[i] = str_uppercase(src[i]);
	dst[len] = '\0';
	return dst;
}

bool str_isalluppercase(const char *str)
{
	for(; *str; ++str)
	{
		// Only check alphabetic characters
		if(*str >= 'a' && *str <= 'z')
			return false;
	}
	return true;
}

void SetFlag(int32_t &Flags, int n, bool Value)
{
	if(Value)
		Flags |= (1 << n);
	else
		Flags &= ~(1 << n);
}

bool IsFlagSet(int32_t Flags, int n)
{
	return (Flags & (1 << n)) != 0;
}
