// Copyright Epic Games, Inc. All Rights Reserved.

#if !EXPLICIT_TESTS_TARGET && WITH_LOW_LEVEL_TESTS
#include "TestCommon/Initialization.h"

#include <catch2/catch_test_macros.hpp>

struct StaticInit {
	StaticInit()
	{
		FCommandLine::Set(TEXT(""));
	}
} GStaticInit;

GROUP_BEFORE_GLOBAL(Catch::DefaultGroup)
{
	InitAll(true, true);
}

GROUP_AFTER_GLOBAL(Catch::DefaultGroup)
{
	CleanupAll();
}
#endif // !EXPLICIT_TESTS_TARGET && WITH_LOW_LEVEL_TESTS
