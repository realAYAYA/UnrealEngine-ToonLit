// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

// BEGIN EPIC MOD
//#include PCH_INCLUDE
// END EPIC MOD
#include "LC_UtcTime.h"
// BEGIN EPIC MOD
#include "Windows/WindowsHWrapper.h"
// END EPIC MOD

uint64_t utcTime::GetCurrent(void)
{
	FILETIME fileTime = {};
	::GetSystemTimeAsFileTime(&fileTime);

	ULARGE_INTEGER asLargeInteger = {};
	asLargeInteger.LowPart = fileTime.dwLowDateTime;
	asLargeInteger.HighPart = fileTime.dwHighDateTime;

	return asLargeInteger.QuadPart;
}
