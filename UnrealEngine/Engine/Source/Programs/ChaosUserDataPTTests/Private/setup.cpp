// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "TestCommon/Initialization.h"
#include "TestHarness.h"

TEST_CASE("Module setup")
{
	InitAll(/*bAllowLogging=*/true, /*bMultithreaded=*/false);
}
