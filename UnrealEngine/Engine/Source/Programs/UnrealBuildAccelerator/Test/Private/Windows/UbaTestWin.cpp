// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTestAll.h"

int wmain(int argc, wchar_t* argv[])
{
	using namespace uba;

	if (!RunTests(argc, argv))
		return -1;
	return 0;
}
