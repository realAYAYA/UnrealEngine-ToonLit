// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestCommon/Initialization.h"

#include <catch2/catch_test_macros.hpp>

GROUP_BEFORE_GLOBAL(Catch::DefaultGroup)
{
	InitAll(/*bAllowLogging=*/true, /*bMultithreaded=*/false);
}

GROUP_AFTER_GLOBAL(Catch::DefaultGroup)
{
	CleanupAll();
}
