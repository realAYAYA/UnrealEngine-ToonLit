// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include COMPILED_PLATFORM_HEADER_WITH_PREFIX(Platform,TestRunner.h)

#include "CommandLineUtil.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Logging/LogSuppressionInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Templates/SharedPointer.h"
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005)
#endif

#if __cplusplus
	extern "C" const char* GetProcessExecutablePath();
#else
	const char* GetProcessExecutablePath();
#endif
const char* GetCacheDirectory();

DECLARE_DELEGATE(FGlobalSetupHandler);

class FTestDelegates
{
public:
	inline static TSharedPtr<FGlobalSetupHandler, ESPMode::ThreadSafe> GlobalSetup = MakeShareable(new FGlobalSetupHandler());
	inline static TSharedPtr<FGlobalSetupHandler, ESPMode::ThreadSafe> GlobalTeardown = MakeShareable(new FGlobalSetupHandler());
};

int RunTests(int argc, const char* argv[]);