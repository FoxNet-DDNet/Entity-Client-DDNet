#ifndef BASE_SYSTEM_H
#define BASE_SYSTEM_H

#include "dbg.h"
#include "detect.h"
#include "fs.h"
#include "io.h"
#include "mem.h"
#include "secure.h"
#include "str.h"
#include "time.h"
#include "types.h"

// EClient
const char *str_to_uppercase(const char *src);
bool str_isalluppercase(const char *str);

void SetFlag(int32_t &Flags, int n, bool Value);
bool IsFlagSet(int32_t Flags, int n);

#endif // BASE_SYSTEM_H
