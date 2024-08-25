// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTestAll.h"

int main(int argc, char* argv[])
{
	using namespace uba;

	if (!RunTests(argc, argv))
		return -1;
	return 0;
}
